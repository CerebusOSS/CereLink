///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   sdk_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  SDK session implementation
///
/// Implements the two-stage pipeline:
///   Device → cbdev receive thread → cbshmem (fast!) → queue → callback thread → user callback
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cbsdk_v2/sdk_session.h"
#include "cbdev/device_session.h"
#include "cbshmem/shmem_session.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <iostream>

namespace cbsdk {

///////////////////////////////////////////////////////////////////////////////////////////////////
// SdkSession::Impl - Internal Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

struct SdkSession::Impl {
    // Configuration
    SdkConfig config;

    // Sub-components
    std::optional<cbdev::DeviceSession> device_session;
    std::optional<cbshmem::ShmemSession> shmem_session;

    // Packet queue (receive thread → callback thread)
    SPSCQueue<cbPKT_GENERIC, 16384> packet_queue;  // Fixed size for now (TODO: make configurable)

    // Callback thread
    std::unique_ptr<std::thread> callback_thread;
    std::atomic<bool> callback_thread_running{false};
    std::atomic<bool> callback_thread_waiting{false};
    std::mutex callback_mutex;
    std::condition_variable callback_cv;

    // Shared memory receive thread (CLIENT mode only)
    std::unique_ptr<std::thread> shmem_receive_thread;
    std::atomic<bool> shmem_receive_thread_running{false};

    // User callbacks
    PacketCallback packet_callback;
    ErrorCallback error_callback;
    std::mutex user_callback_mutex;

    // Statistics
    SdkStats stats;
    std::mutex stats_mutex;

    // Running state
    std::atomic<bool> is_running{false};

    // Device handshake state (for connect() method)
    std::atomic<uint32_t> device_runlevel{0};      // Current runlevel from SYSREP
    std::atomic<bool> received_sysrep{false};       // Have we received any SYSREP?
    std::atomic<uint64_t> packets_received{0};      // Total packets received
    std::mutex handshake_mutex;
    std::condition_variable handshake_cv;

    ~Impl() {
        // Ensure threads are stopped
        if (callback_thread_running.load()) {
            callback_thread_running.store(false);
            callback_cv.notify_one();
            if (callback_thread && callback_thread->joinable()) {
                callback_thread->join();
            }
        }
        if (shmem_receive_thread_running.load()) {
            shmem_receive_thread_running.store(false);
            if (shmem_receive_thread && shmem_receive_thread->joinable()) {
                shmem_receive_thread->join();
            }
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// SdkSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

SdkSession::SdkSession()
    : m_impl(std::make_unique<Impl>()) {
}

SdkSession::SdkSession(SdkSession&&) noexcept = default;
SdkSession& SdkSession::operator=(SdkSession&&) noexcept = default;

SdkSession::~SdkSession() {
    if (m_impl) {  // Check if moved-from
        stop();
    }
}

// Helper function to map DeviceType to instance number
// Central uses instance numbers to distinguish multiple devices:
// - Instance 0: NSP (first device)
// - Instance 1: Hub1 (second device)
// - Instance 2: Hub2 (third device)
// - Instance 3: Hub3 (fourth device)
static int getInstanceNumber(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP:
            return 0;
        case DeviceType::GEMINI_NSP:
            return 0;  // NSP is always instance 0
        case DeviceType::GEMINI_HUB1:
            return 1;
        case DeviceType::GEMINI_HUB2:
            return 2;
        case DeviceType::GEMINI_HUB3:
            return 3;
        case DeviceType::NPLAY:
            return 0;  // nPlay uses instance 0
        default:
            return 0;
    }
}

// Helper function to get Central-compatible shared memory names
// Returns config buffer name (e.g., "cbCFGbuffer" or "cbCFGbuffer1")
static std::string getConfigBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbCFGbuffer";
    } else {
        return "cbCFGbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible transmit buffer name
// Returns transmit buffer name (e.g., "XmtGlobal" or "XmtGlobal1")
static std::string getTransmitBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "XmtGlobal";
    } else {
        return "XmtGlobal" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible receive buffer name
// Returns receive buffer name (e.g., "cbRECbuffer" or "cbRECbuffer1")
static std::string getReceiveBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbRECbuffer";
    } else {
        return "cbRECbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible local transmit buffer name
// Returns local transmit buffer name (e.g., "XmtLocal" or "XmtLocal1")
static std::string getLocalTransmitBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "XmtLocal";
    } else {
        return "XmtLocal" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible status buffer name
// Returns status buffer name (e.g., "cbSTATUSbuffer" or "cbSTATUSbuffer1")
static std::string getStatusBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSTATUSbuffer";
    } else {
        return "cbSTATUSbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible spike cache buffer name
// Returns spike cache buffer name (e.g., "cbSPKbuffer" or "cbSPKbuffer1")
static std::string getSpikeBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSPKbuffer";
    } else {
        return "cbSPKbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible signal event name
// Returns signal event name (e.g., "cbSIGNALevent" or "cbSIGNALevent1")
static std::string getSignalEventName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSIGNALevent";
    } else {
        return "cbSIGNALevent" + std::to_string(instance);
    }
}

Result<SdkSession> SdkSession::create(const SdkConfig& config) {
    SdkSession session;
    session.m_impl->config = config;

    // Automatically determine shared memory names from device type (Central-compatible)
    std::string cfg_name = getConfigBufferName(config.device_type);
    std::string rec_name = getReceiveBufferName(config.device_type);
    std::string xmt_name = getTransmitBufferName(config.device_type);
    std::string xmt_local_name = getLocalTransmitBufferName(config.device_type);
    std::string status_name = getStatusBufferName(config.device_type);
    std::string spk_name = getSpikeBufferName(config.device_type);
    std::string signal_event_name = getSignalEventName(config.device_type);

    // Auto-detect mode: Try CLIENT first (attach to existing), fall back to STANDALONE (create new)
    bool is_standalone = false;

    // Try to attach to existing shared memory (CLIENT mode)
    auto shmem_result = cbshmem::ShmemSession::create(cfg_name, rec_name, xmt_name, xmt_local_name,
                                                       status_name, spk_name, signal_event_name, cbshmem::Mode::CLIENT);

    if (shmem_result.isError()) {
        // No existing shared memory, create new (STANDALONE mode)
        shmem_result = cbshmem::ShmemSession::create(cfg_name, rec_name, xmt_name, xmt_local_name,
                                                      status_name, spk_name, signal_event_name, cbshmem::Mode::STANDALONE);
        if (shmem_result.isError()) {
            return Result<SdkSession>::error("Failed to create shared memory: " + shmem_result.error());
        }
        is_standalone = true;
    }

    session.m_impl->shmem_session = std::move(shmem_result.value());

    // Create device session only in STANDALONE mode
    if (is_standalone) {
        // Map SDK DeviceType to cbdev DeviceType
        cbdev::DeviceType dev_type;
        switch (config.device_type) {
            case DeviceType::LEGACY_NSP:
                dev_type = cbdev::DeviceType::NSP;
                break;
            case DeviceType::GEMINI_NSP:
                dev_type = cbdev::DeviceType::GEMINI;
                break;
            case DeviceType::GEMINI_HUB1:
                dev_type = cbdev::DeviceType::HUB1;
                break;
            case DeviceType::GEMINI_HUB2:
                dev_type = cbdev::DeviceType::HUB2;
                break;
            case DeviceType::GEMINI_HUB3:
                dev_type = cbdev::DeviceType::HUB3;
                break;
            case DeviceType::NPLAY:
                dev_type = cbdev::DeviceType::NPLAY;
                break;
            default:
                return Result<SdkSession>::error("Invalid device type");
        }

        // Create device config from device type (uses predefined addresses/ports)
        cbdev::DeviceConfig dev_config = cbdev::DeviceConfig::forDevice(dev_type);

        // Apply custom addresses/ports if specified (overrides device type defaults)
        if (config.custom_device_address.has_value()) {
            dev_config.device_address = config.custom_device_address.value();
        }
        if (config.custom_client_address.has_value()) {
            dev_config.client_address = config.custom_client_address.value();
        }
        if (config.custom_device_port.has_value()) {
            dev_config.send_port = config.custom_device_port.value();
        }
        if (config.custom_client_port.has_value()) {
            dev_config.recv_port = config.custom_client_port.value();
        }

        dev_config.recv_buffer_size = config.recv_buffer_size;
        dev_config.non_blocking = config.non_blocking;

        auto dev_result = cbdev::DeviceSession::create(dev_config);
        if (dev_result.isError()) {
            return Result<SdkSession>::error("Failed to create device session: " + dev_result.error());
        }
        session.m_impl->device_session = std::move(dev_result.value());

        // Start the session (starts receive/send threads)
        auto start_result = session.start();
        if (start_result.isError()) {
            return Result<SdkSession>::error("Failed to start session: " + start_result.error());
        }

        // Connect to device and verify it's responding
        // (performs handshake based on config.auto_run setting)
        auto connect_result = session.connect(500);  // 500ms timeout per step
        if (connect_result.isError()) {
            session.stop();  // Clean up threads
            return Result<SdkSession>::error("Device not responding: " + connect_result.error());
        }
    }

    return Result<SdkSession>::ok(std::move(session));
}

Result<void> SdkSession::start() {
    if (m_impl->is_running.load()) {
        return Result<void>::error("Session is already running");
    }

    // Set up device callbacks (if in STANDALONE mode)
    if (m_impl->device_session.has_value()) {
        // STANDALONE mode - start callback thread + device threads
        // In STANDALONE mode, we need the callback thread to decouple fast UDP receive from slow user callbacks

        // Start callback thread
        m_impl->callback_thread_running.store(true);
        m_impl->callback_thread = std::make_unique<std::thread>([this]() {
            callbackThreadLoop();
        });

        // Packet receive callback
        m_impl->device_session->setPacketCallback([this](const cbPKT_GENERIC* pkts, size_t count) {
            onPacketsReceivedFromDevice(pkts, count);
        });

        // Transmit callback (for send thread to dequeue packets from shared memory)
        m_impl->device_session->setTransmitCallback([this](cbPKT_GENERIC& pkt) -> bool {
            // Dequeue packet from shared memory transmit buffer
            auto result = m_impl->shmem_session->dequeuePacket(pkt);
            if (result.isError()) {
                return false;  // Error - treat as empty
            }
            return result.value();  // Returns true if packet was dequeued, false if queue empty
        });

        // Start device receive thread
        auto recv_result = m_impl->device_session->startReceiveThread();
        if (recv_result.isError()) {
            // Clean up callback thread
            m_impl->callback_thread_running.store(false);
            m_impl->callback_cv.notify_one();
            if (m_impl->callback_thread && m_impl->callback_thread->joinable()) {
                m_impl->callback_thread->join();
            }
            return Result<void>::error("Failed to start device receive thread: " + recv_result.error());
        }

        // Start device send thread
        auto send_result = m_impl->device_session->startSendThread();
        if (send_result.isError()) {
            // Clean up receive thread and callback thread
            m_impl->device_session->stopReceiveThread();
            m_impl->callback_thread_running.store(false);
            m_impl->callback_cv.notify_one();
            if (m_impl->callback_thread && m_impl->callback_thread->joinable()) {
                m_impl->callback_thread->join();
            }
            return Result<void>::error("Failed to start device send thread: " + send_result.error());
        }
    } else {
        // CLIENT mode - start shared memory receive thread only
        // In CLIENT mode, we don't need a separate callback thread because reading from
        // cbRECbuffer is not time-critical (200MB buffer provides ample buffering)
        // This eliminates an extra data copy and thread overhead

        m_impl->shmem_receive_thread_running.store(true);
        m_impl->shmem_receive_thread = std::make_unique<std::thread>([this]() {
            shmemReceiveThreadLoop();
        });
    }

    m_impl->is_running.store(true);
    return Result<void>::ok();
}

void SdkSession::stop() {
    if (!m_impl || !m_impl->is_running.load()) {
        return;
    }

    m_impl->is_running.store(false);

    // Stop device threads (if STANDALONE mode)
    if (m_impl->device_session.has_value()) {
        m_impl->device_session->stopReceiveThread();
        m_impl->device_session->stopSendThread();
    }

    // Stop shared memory receive thread (if CLIENT mode)
    if (m_impl->shmem_receive_thread_running.load()) {
        m_impl->shmem_receive_thread_running.store(false);
        if (m_impl->shmem_receive_thread && m_impl->shmem_receive_thread->joinable()) {
            m_impl->shmem_receive_thread->join();
        }
    }

    // Stop callback thread
    m_impl->callback_thread_running.store(false);
    m_impl->callback_cv.notify_one();
    if (m_impl->callback_thread && m_impl->callback_thread->joinable()) {
        m_impl->callback_thread->join();
    }
}

bool SdkSession::isRunning() const {
    return m_impl && m_impl->is_running.load();
}

void SdkSession::setPacketCallback(PacketCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    m_impl->packet_callback = std::move(callback);
}

void SdkSession::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    m_impl->error_callback = std::move(callback);
}

SdkStats SdkSession::getStats() const {
    std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
    SdkStats stats = m_impl->stats;

    // Update queue depth (lock-free read)
    stats.queue_current_depth = m_impl->packet_queue.size();

    return stats;
}

void SdkSession::resetStats() {
    std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
    m_impl->stats.reset();
}

const SdkConfig& SdkSession::getConfig() const {
    return m_impl->config;
}

Result<void> SdkSession::sendPacket(const cbPKT_GENERIC& pkt) {
    // Enqueue packet to shared memory transmit buffer
    // Works in both STANDALONE and CLIENT modes:
    // - STANDALONE: send thread will dequeue and transmit
    // - CLIENT: STANDALONE process's send thread will pick it up
    auto result = m_impl->shmem_session->enqueuePacket(pkt);
    if (result.isOk()) {
        // Wake up send thread if in STANDALONE mode
        if (m_impl->device_session.has_value()) {
            // Notify send thread that packets are available
            // (device_session checks hasTransmitPackets via callback)
        }
        return Result<void>::ok();
    } else {
        return Result<void>::error(result.error());
    }
}

Result<void> SdkSession::setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) {
    // Create runlevel command packet
    cbPKT_SYSINFO sysinfo;
    std::memset(&sysinfo, 0, sizeof(sysinfo));

    // Fill header
    sysinfo.cbpkt_header.time = 1;
    sysinfo.cbpkt_header.chid = 0x8000;  // cbPKTCHAN_CONFIGURATION
    sysinfo.cbpkt_header.type = 0x92;    // cbPKTTYPE_SYSSETRUNLEV
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro (accounts for 64-bit PROCTIME)
    sysinfo.cbpkt_header.instrument = 0;

    // Fill payload
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = runflags;

    // Cast to generic packet and send
    cbPKT_GENERIC pkt;
    std::memcpy(&pkt, &sysinfo, sizeof(sysinfo));

    return sendPacket(pkt);
}

Result<void> SdkSession::requestConfiguration() {
    // Create REQCONFIGALL packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    // Fill header
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = 0x8000;  // cbPKTCHAN_CONFIGURATION
    pkt.cbpkt_header.type = 0x88;    // cbPKTTYPE_REQCONFIGALL
    pkt.cbpkt_header.dlen = 0;       // No payload
    pkt.cbpkt_header.instrument = 0;

    return sendPacket(pkt);
}

Result<void> SdkSession::performStartupHandshake(uint32_t timeout_ms) {
    // This implements the complete device startup sequence

    // TODO: This is a stub implementation that needs proper state machine with:
    // 1. SYSREP packet tracking to monitor runlevel changes
    // 2. Timeout handling for each step
    // 3. Proper sequencing with waits
    //
    // For now, we'll just send the basic sequence without waiting

    // Step 1: Send cbRUNLEVEL_RUNNING
    auto result = setSystemRunLevel(cbRUNLEVEL_RUNNING);
    if (result.isError()) {
        return result;
    }

    // TODO: Wait for SYSREP or timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));

    // Step 2: Send cbRUNLEVEL_HARDRESET (if device not running)
    result = setSystemRunLevel(cbRUNLEVEL_HARDRESET);
    if (result.isError()) {
        return result;
    }

    // TODO: Wait for SYSREP or timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));

    // Step 3: Request all configuration
    result = requestConfiguration();
    if (result.isError()) {
        return result;
    }

    // TODO: Wait for config flood (> 1000 packets ending with SYSREP)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Step 4: Send cbRUNLEVEL_RESET (if still not running)
    result = setSystemRunLevel(cbRUNLEVEL_RESET);
    if (result.isError()) {
        return result;
    }

    // TODO: Wait for device to transition to RUNNING
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> SdkSession::connect(uint32_t timeout_ms) {
    // Connect to device and perform handshake to verify it's present and responsive
    // This is called from create() in STANDALONE mode only
    //
    // Handshake sequence (when auto_run = true):
    // 1. Send cbRUNLEVEL_RUNNING - wait for SYSREP or timeout (0.5s)
    // 2. If not running, send cbRUNLEVEL_HARDRESET - wait (0.5s)
    // 3. Send REQCONFIGALL - wait for config flood (should see many packets)
    // 4. Send cbRUNLEVEL_RUNNING - transition device to RUNNING state
    //
    // When auto_run = false, only REQCONFIGALL is sent (step 3 only)

    // Reset handshake state
    m_impl->packets_received.store(0, std::memory_order_relaxed);
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    m_impl->device_runlevel.store(0, std::memory_order_relaxed);

    Result<void> result;

    if (m_impl->config.auto_run) {
        // Full handshake: perform runlevel commands to start device

        // Helper lambda to wait for SYSREP with timeout
        auto waitForSysrep = [this](uint32_t timeout_ms) -> bool {
            std::unique_lock<std::mutex> lock(m_impl->handshake_mutex);
            return m_impl->handshake_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                [this] { return m_impl->received_sysrep.load(std::memory_order_acquire); });
        };

        // Step 1: Send cbRUNLEVEL_RUNNING to check if device is already running
        result = setSystemRunLevel(cbRUNLEVEL_RUNNING);
        if (result.isError()) {
            return Result<void>::error("Failed to send RUNNING command: " + result.error());
        }

        // Wait for SYSREP response
        if (!waitForSysrep(timeout_ms)) {
            // No response - device might be in deep standby, try HARDRESET
            goto try_hardreset;
        }

        // Got SYSREP - check if device is running
        if (m_impl->device_runlevel.load(std::memory_order_acquire) == cbRUNLEVEL_RUNNING) {
            // Device is already running - request config and we're done
            goto request_config;
        }

    try_hardreset:
        // Step 2: Device not running - send HARDRESET
        m_impl->received_sysrep.store(false, std::memory_order_relaxed);
        result = setSystemRunLevel(cbRUNLEVEL_HARDRESET);
        if (result.isError()) {
            return Result<void>::error("Failed to send HARDRESET command: " + result.error());
        }

        // Wait for SYSREP response
        if (!waitForSysrep(timeout_ms)) {
            return Result<void>::error("Device not responding to HARDRESET (no SYSREP received)");
        }
    }

request_config:
    // Helper lambda to wait for SYSREP with timeout (defined here for both paths)
    auto waitForSysrep = [this](uint32_t timeout_ms) -> bool {
        std::unique_lock<std::mutex> lock(m_impl->handshake_mutex);
        return m_impl->handshake_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this] { return m_impl->received_sysrep.load(std::memory_order_acquire); });
    };

    // Step 3: Request all configuration (always performed)
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    result = requestConfiguration();
    if (result.isError()) {
        return Result<void>::error("Failed to send REQCONFIGALL: " + result.error());
    }

    // Wait for final SYSREP packet from config flood
    // The device sends many config packets and finishes with a SYSREP containing current runlevel
    if (!waitForSysrep(timeout_ms)) {
        return Result<void>::error("Device not responding to REQCONFIGALL (no final SYSREP received)");
    }

    // Step 4: Get current runlevel from the SYSREP response
    uint32_t current_runlevel = m_impl->device_runlevel.load(std::memory_order_acquire);

    if (m_impl->config.auto_run && current_runlevel != cbRUNLEVEL_RUNNING) {
        // Step 5: Send RUNNING to complete handshake
        // Device is in STANDBY (30) after REQCONFIGALL - transition to RUNNING (50)
        m_impl->received_sysrep.store(false, std::memory_order_relaxed);
        result = setSystemRunLevel(cbRUNLEVEL_RUNNING);
        if (result.isError()) {
            return Result<void>::error("Failed to send final RUNNING command: " + result.error());
        }

        // Wait for device to transition to RUNNING
        if (!waitForSysrep(timeout_ms)) {
            return Result<void>::error("Device not responding to final RUNNING command (no SYSREP received)");
        }
    }

    // Success - device is connected and responding
    return Result<void>::ok();
}

void SdkSession::onPacketsReceivedFromDevice(const cbPKT_GENERIC* pkts, size_t count) {
    // This runs on the cbdev receive thread - MUST BE FAST!

    for (size_t i = 0; i < count; ++i) {
        const auto& pkt = pkts[i];

        // Track total packets received (for connect() verification)
        m_impl->packets_received.fetch_add(1, std::memory_order_relaxed);

        // Update stats (atomic or quick increment)
        {
            std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
            m_impl->stats.packets_received_from_device++;
            // bytes_received tracked by cbdev
        }

        // Track SYSREP packets for handshake state machine
        // SYSREP type is 0x10-0x1F (cbPKTTYPE_SYSREP*)
        if ((pkt.cbpkt_header.type & 0xF0) == 0x10) {
            // This is a SYSREP packet - extract runlevel from cbPKT_SYSINFO structure
            const cbPKT_SYSINFO* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
            m_impl->device_runlevel.store(sysinfo->runlevel, std::memory_order_release);
            m_impl->received_sysrep.store(true, std::memory_order_release);
            m_impl->handshake_cv.notify_all();  // Wake up connect() if waiting
        }

        // Store to shared memory (FAST PATH - just memcpy)
        auto result = m_impl->shmem_session->storePacket(pkt);
        if (result.isOk()) {
            std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
            m_impl->stats.packets_stored_to_shmem++;
        } else {
            std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
            m_impl->stats.shmem_store_errors++;
        }

        // Queue for callback (lock-free push)
        if (m_impl->packet_queue.push(pkt)) {
            std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
            m_impl->stats.packets_queued_for_callback++;

            // Track peak queue depth
            size_t current_depth = m_impl->packet_queue.size();
            if (current_depth > m_impl->stats.queue_max_depth) {
                m_impl->stats.queue_max_depth = current_depth;
            }
        } else {
            // Queue overflow! Drop packet and invoke error callback
            {
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.packets_dropped++;
            }

            // Invoke error callback (if set)
            std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
            if (m_impl->error_callback) {
                m_impl->error_callback("Packet queue overflow - dropping packets");
            }
        }
    }

    // Signal CLIENT processes that new data is available (STANDALONE mode only)
    // This wakes up any CLIENT processes waiting in waitForData()
    m_impl->shmem_session->signalData();

    // Wake callback thread if it's waiting
    if (m_impl->callback_thread_waiting.load(std::memory_order_relaxed)) {
        m_impl->callback_cv.notify_one();
    }
}

void SdkSession::shmemReceiveThreadLoop() {
    // This is the shared memory receive thread (CLIENT mode only)
    // Waits for signal from STANDALONE, reads packets from cbRECbuffer, invokes user callback directly
    //
    // Note: In CLIENT mode, we don't need a separate callback dispatcher thread because:
    // 1. Reading from cbRECbuffer is not time-critical (unlike UDP receive which must be fast)
    // 2. The 200MB buffer provides ample buffering even if callbacks are slow
    // 3. This avoids an extra data copy through packet_queue

    constexpr size_t MAX_BATCH = 128;  // Read up to 128 packets at a time
    cbPKT_GENERIC packets[MAX_BATCH];

    while (m_impl->shmem_receive_thread_running.load()) {
        // Wait for signal from STANDALONE (efficient, no polling!)
        auto wait_result = m_impl->shmem_session->waitForData(250);  // 250ms timeout
        if (wait_result.isError()) {
            // Error waiting - invoke error callback
            std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
            if (m_impl->error_callback) {
                m_impl->error_callback("Error waiting for shared memory signal: " + wait_result.error());
            }
            continue;
        }

        if (!wait_result.value()) {
            // Timeout - no signal received, loop again
            continue;
        }

        // Signal received! Read available packets from cbRECbuffer
        size_t packets_read = 0;
        auto read_result = m_impl->shmem_session->readReceiveBuffer(packets, MAX_BATCH, packets_read);
        if (read_result.isError()) {
            // Buffer overrun or other error
            {
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.shmem_store_errors++;  // Reuse this stat for read errors
            }

            // Invoke error callback
            std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
            if (m_impl->error_callback) {
                m_impl->error_callback("Error reading from shared memory: " + read_result.error());
            }
            continue;
        }

        if (packets_read > 0) {
            // Update stats
            {
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.packets_delivered_to_callback += packets_read;
            }

            // Invoke user callback directly (no queueing, no extra copy!)
            std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
            if (m_impl->packet_callback) {
                m_impl->packet_callback(packets, packets_read);
            }
        }
    }
}

void SdkSession::callbackThreadLoop() {
    // This is the callback thread - runs user callbacks (can be slow)

    constexpr size_t MAX_BATCH = 16;  // Opportunistic batching
    cbPKT_GENERIC packets[MAX_BATCH];

    while (m_impl->callback_thread_running.load()) {
        size_t count = 0;

        // Drain all available packets from queue (non-blocking)
        while (count < MAX_BATCH && m_impl->packet_queue.pop(packets[count])) {
            count++;
        }

        if (count > 0) {
            // We have packets - mark not waiting and invoke callback
            m_impl->callback_thread_waiting.store(false, std::memory_order_relaxed);

            // Update stats
            {
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.packets_delivered_to_callback += count;
            }

            // Invoke user callback (can be slow!)
            std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
            if (m_impl->packet_callback) {
                m_impl->packet_callback(packets, count);
            }
        } else {
            // No packets available - wait for notification
            m_impl->callback_thread_waiting.store(true, std::memory_order_release);

            std::unique_lock<std::mutex> lock(m_impl->callback_mutex);
            m_impl->callback_cv.wait_for(lock, std::chrono::milliseconds(1),
                [this] { return !m_impl->packet_queue.empty() || !m_impl->callback_thread_running.load(); });
        }
    }
}

} // namespace cbsdk
