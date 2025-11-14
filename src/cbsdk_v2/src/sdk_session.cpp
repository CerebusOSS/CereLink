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
                dev_type = cbdev::DeviceType::GEMINI_NSP;
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
        // Capture raw pointer to Impl so thread remains valid even if session is moved
        Impl* impl = m_impl.get();
        m_impl->callback_thread = std::make_unique<std::thread>([impl]() {
            // This is the callback thread - runs user callbacks (can be slow)
            constexpr size_t MAX_BATCH = 16;  // Opportunistic batching
            cbPKT_GENERIC packets[MAX_BATCH];

            while (impl->callback_thread_running.load()) {
                size_t count = 0;

                // Drain all available packets from queue (non-blocking)
                while (count < MAX_BATCH && impl->packet_queue.pop(packets[count])) {
                    count++;
                }

                if (count > 0) {
                    // We have packets - mark not waiting and invoke callback
                    impl->callback_thread_waiting.store(false, std::memory_order_relaxed);

                    // Update stats
                    {
                        std::lock_guard<std::mutex> lock(impl->stats_mutex);
                        impl->stats.packets_delivered_to_callback += count;
                    }

                    // Invoke user callback (can be slow!)
                    std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                    if (impl->packet_callback) {
                        impl->packet_callback(packets, count);
                    }
                } else {
                    // No packets available - wait for notification
                    impl->callback_thread_waiting.store(true, std::memory_order_release);

                    std::unique_lock<std::mutex> lock(impl->callback_mutex);
                    impl->callback_cv.wait_for(lock, std::chrono::milliseconds(1),
                        [impl] { return !impl->callback_thread_running.load() || !impl->packet_queue.empty(); });
                }
            }
        });

        // Packet receive callback - capture impl to survive session moves
        m_impl->device_session->setPacketCallback([impl](const cbPKT_GENERIC* pkts, size_t count) {
            // This runs on the cbdev receive thread - MUST BE FAST!
            for (size_t i = 0; i < count; ++i) {
                const auto& pkt = pkts[i];

                // Update stats
                {
                    std::lock_guard<std::mutex> lock(impl->stats_mutex);
                    impl->stats.packets_received_from_device++;
                }

                // Note: SYSREP packet monitoring is now handled by DeviceSession internally

                // Store to shared memory
                auto result = impl->shmem_session->storePacket(pkt);
                if (result.isOk()) {
                    std::lock_guard<std::mutex> lock(impl->stats_mutex);
                    impl->stats.packets_stored_to_shmem++;
                } else {
                    std::lock_guard<std::mutex> lock(impl->stats_mutex);
                    impl->stats.shmem_store_errors++;
                }

                // Queue for callback
                if (impl->packet_queue.push(pkt)) {
                    std::lock_guard<std::mutex> lock(impl->stats_mutex);
                    impl->stats.packets_queued_for_callback++;
                    size_t current_depth = impl->packet_queue.size();
                    if (current_depth > impl->stats.queue_max_depth) {
                        impl->stats.queue_max_depth = current_depth;
                    }
                } else {
                    // Queue overflow
                    {
                        std::lock_guard<std::mutex> lock(impl->stats_mutex);
                        impl->stats.packets_dropped++;
                    }
                    std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                    if (impl->error_callback) {
                        impl->error_callback("Packet queue overflow - dropping packets");
                    }
                }
            }

            // Signal CLIENT processes that new data is available
            impl->shmem_session->signalData();

            // Wake callback thread if waiting
            if (impl->callback_thread_waiting.load(std::memory_order_relaxed)) {
                impl->callback_cv.notify_one();
            }
        });

        // Transmit callback - capture impl to survive session moves
        m_impl->device_session->setTransmitCallback([impl](cbPKT_GENERIC& pkt) -> bool {
            // Dequeue packet from shared memory transmit buffer
            auto result = impl->shmem_session->dequeuePacket(pkt);
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
    // Delegate to device_session (STANDALONE mode only)
    if (!m_impl->device_session.has_value()) {
        return Result<void>::error("setSystemRunLevel() only available in STANDALONE mode");
    }
    auto result = m_impl->device_session->setSystemRunLevel(runlevel, resetque, runflags);
    if (result.isError()) {
        return Result<void>::error(result.error());
    }
    return Result<void>::ok();
}

Result<void> SdkSession::requestConfiguration() {
    // Delegate to device_session (STANDALONE mode only)
    if (!m_impl->device_session.has_value()) {
        return Result<void>::error("requestConfiguration() only available in STANDALONE mode");
    }
    auto result = m_impl->device_session->requestConfiguration();
    if (result.isError()) {
        return Result<void>::error(result.error());
    }
    return Result<void>::ok();
}

Result<void> SdkSession::performStartupHandshake(uint32_t timeout_ms) {
    // Delegate to device_session (STANDALONE mode only)
    if (!m_impl->device_session.has_value()) {
        return Result<void>::error("performStartupHandshake() only available in STANDALONE mode");
    }
    auto result = m_impl->device_session->performStartupHandshake(timeout_ms);
    if (result.isError()) {
        return Result<void>::error(result.error());
    }
    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> SdkSession::connect(uint32_t timeout_ms) {
    // Connect to device and verify it's present and responsive
    // This is called from create() in STANDALONE mode only
    // Delegates to device_session for handshake logic

    if (!m_impl->device_session.has_value()) {
        return Result<void>::error("connect() requires device_session (STANDALONE mode)");
    }

    if (m_impl->config.auto_run) {
        // Full startup handshake - get device to RUNNING state
        auto result = m_impl->device_session->performStartupHandshake(timeout_ms);
        if (result.isError()) {
            return Result<void>::error("Startup handshake failed: " + result.error());
        }
    } else {
        // Minimal check - just request configuration to verify device is present
        // Device stays in whatever state it's currently in
        auto result = m_impl->device_session->requestConfiguration();
        if (result.isError()) {
            return Result<void>::error("Failed to send REQCONFIGALL: " + result.error());
        }

        // Wait for SYSREP response - device_session handles the handshake state internally
        // We need to poll pollPacket to get packets and trigger the handshake state update
        // Actually, the receive thread should already be handling this. We just need to wait for the handshake CV
        // But wait - we don't have access to device_session's handshake state!
        // For now, just sleep briefly to allow device response
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }

    // Success - device is connected and responding
    return Result<void>::ok();
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

} // namespace cbsdk
