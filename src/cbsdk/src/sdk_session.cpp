///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   sdk_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  SDK session implementation
///
/// Implements the two-stage pipeline:
///   Device → cbdev receive thread → cbshm (fast!) → queue → callback thread → user callback
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include "cbsdk/sdk_session.h"
#include "cbdev/device_factory.h"
#include "cbshm/shmem_session.h"
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
    std::unique_ptr<cbdev::IDeviceSession> device_session;
    std::optional<cbshm::ShmemSession> shmem_session;

    // Packet queue (receive thread → callback thread)
    SPSCQueue<cbPKT_GENERIC, 16384> packet_queue;  // Fixed size for now (TODO: make configurable)

    // Callback thread
    std::unique_ptr<std::thread> callback_thread;
    std::atomic<bool> callback_thread_running{false};
    std::atomic<bool> callback_thread_waiting{false};
    std::mutex callback_mutex;
    std::condition_variable callback_cv;

    // Device send thread (STANDALONE mode only)
    // Note: device receive thread is now managed by device_session->startReceiveThread()
    std::unique_ptr<std::thread> device_send_thread;
    std::atomic<bool> device_send_thread_running{false};

    // Callback handles for device receive thread
    cbdev::CallbackHandle receive_callback_handle = 0;
    cbdev::CallbackHandle datagram_callback_handle = 0;

    // Shared memory receive thread (CLIENT mode only)
    std::unique_ptr<std::thread> shmem_receive_thread;
    std::atomic<bool> shmem_receive_thread_running{false};

    // Handshake state (for performStartupHandshake)
    std::atomic<uint32_t> device_runlevel{0};
    std::atomic<bool> received_sysrep{false};
    std::mutex handshake_mutex;
    std::condition_variable handshake_cv;

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
        // Stop device receive thread (managed by DeviceSession)
        if (device_session) {
            device_session->stopReceiveThread();
            // Unregister callbacks
            if (receive_callback_handle != 0) {
                device_session->unregisterCallback(receive_callback_handle);
            }
            if (datagram_callback_handle != 0) {
                device_session->unregisterCallback(datagram_callback_handle);
            }
        }
        // Stop device send thread
        if (device_send_thread_running.load()) {
            device_send_thread_running.store(false);
            if (device_send_thread && device_send_thread->joinable()) {
                device_send_thread->join();
            }
        }
        // Stop callback thread
        if (callback_thread_running.load()) {
            callback_thread_running.store(false);
            callback_cv.notify_one();
            if (callback_thread && callback_thread->joinable()) {
                callback_thread->join();
            }
        }
        // Stop shmem receive thread (CLIENT mode)
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
        case DeviceType::NSP:
            return 0;  // NSP is always instance 0
        case DeviceType::HUB1:
            return 1;
        case DeviceType::HUB2:
            return 2;
        case DeviceType::HUB3:
            return 3;
        case DeviceType::NPLAY:
            return 0;  // nPlay uses instance 0
        default:
            return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Central-compatible shared memory naming
// Names match Central's naming convention: base name + optional instance suffix
///////////////////////////////////////////////////////////////////////////////////////////////////

// Helper function to get Central-compatible shared memory names
// Returns config buffer name (e.g., "cbCFGbuffer" or "cbCFGbuffer1")
static std::string getCentralConfigBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbCFGbuffer";
    } else {
        return "cbCFGbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible transmit buffer name
// Returns transmit buffer name (e.g., "XmtGlobal" or "XmtGlobal1")
static std::string getCentralTransmitBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "XmtGlobal";
    } else {
        return "XmtGlobal" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible receive buffer name
// Returns receive buffer name (e.g., "cbRECbuffer" or "cbRECbuffer1")
static std::string getCentralReceiveBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbRECbuffer";
    } else {
        return "cbRECbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible local transmit buffer name
// Returns local transmit buffer name (e.g., "XmtLocal" or "XmtLocal1")
static std::string getCentralLocalTransmitBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "XmtLocal";
    } else {
        return "XmtLocal" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible status buffer name
// Returns status buffer name (e.g., "cbSTATUSbuffer" or "cbSTATUSbuffer1")
static std::string getCentralStatusBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSTATUSbuffer";
    } else {
        return "cbSTATUSbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible spike cache buffer name
// Returns spike cache buffer name (e.g., "cbSPKbuffer" or "cbSPKbuffer1")
static std::string getCentralSpikeBufferName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSPKbuffer";
    } else {
        return "cbSPKbuffer" + std::to_string(instance);
    }
}

// Helper function to get Central-compatible signal event name
// Returns signal event name (e.g., "cbSIGNALevent" or "cbSIGNALevent1")
static std::string getCentralSignalEventName(DeviceType type) {
    int instance = getInstanceNumber(type);
    if (instance == 0) {
        return "cbSIGNALevent";
    } else {
        return "cbSIGNALevent" + std::to_string(instance);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Native-mode shared memory naming
// Names use per-device segments: "cbshm_{device}_{segment}"
///////////////////////////////////////////////////////////////////////////////////////////////////

static const char* getNativeDeviceName(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP: return "legacy_nsp";
        case DeviceType::NSP:        return "nsp";
        case DeviceType::HUB1:       return "hub1";
        case DeviceType::HUB2:       return "hub2";
        case DeviceType::HUB3:       return "hub3";
        case DeviceType::NPLAY:      return "nplay";
        default:                     return "unknown";
    }
}

static std::string getNativeSegmentName(DeviceType type, const std::string& segment) {
    return std::string("cbshm_") + getNativeDeviceName(type) + "_" + segment;
}

/// @brief Map DeviceType to Central's instrument index (GEMSTART==2 mapping)
///
/// Central hardcodes instrument assignments at compile time.
/// With GEMSTART==2 (current build): Hub1=0, Hub2=1, Hub3=2, NSP=3
/// With single-device (non-Gemini): instrument 0
///
static int32_t getCentralInstrumentIndex(DeviceType type) {
    switch (type) {
        case DeviceType::HUB1:       return 0;
        case DeviceType::HUB2:       return 1;
        case DeviceType::HUB3:       return 2;
        case DeviceType::NSP:        return 3;
        case DeviceType::LEGACY_NSP: return 0;  // Non-Gemini, single instrument
        default:                     return -1;  // No filter
    }
}

Result<SdkSession> SdkSession::create(const SdkConfig& config) {
    SdkSession session;
    session.m_impl->config = config;

    // Three-way shared memory detection:
    // 1. Try Central compat CLIENT: attach to existing Central-named segments
    // 2. Try native CLIENT: attach to existing native-named segments
    // 3. Fall back to native STANDALONE: create new native-mode segments
    bool is_standalone = false;

    // --- Attempt 1: Central-compatible CLIENT mode ---
    // Try to attach to Central's shared memory (Central is running)
    std::string central_cfg = getCentralConfigBufferName(config.device_type);
    std::string central_rec = getCentralReceiveBufferName(config.device_type);
    std::string central_xmt = getCentralTransmitBufferName(config.device_type);
    std::string central_xmt_local = getCentralLocalTransmitBufferName(config.device_type);
    std::string central_status = getCentralStatusBufferName(config.device_type);
    std::string central_spk = getCentralSpikeBufferName(config.device_type);
    std::string central_signal = getCentralSignalEventName(config.device_type);

    auto shmem_result = cbshm::ShmemSession::create(
        central_cfg, central_rec, central_xmt, central_xmt_local,
        central_status, central_spk, central_signal,
        cbshm::Mode::CLIENT, cbshm::ShmemLayout::CENTRAL_COMPAT);

    if (shmem_result.isOk()) {
        // Set instrument filter for CENTRAL_COMPAT mode (Central's receive buffer
        // contains packets from ALL instruments; we only want our device's packets)
        int32_t inst_idx = getCentralInstrumentIndex(config.device_type);
        shmem_result.value().setInstrumentFilter(inst_idx);
    }

    if (shmem_result.isError()) {
        // --- Attempt 2: Native CLIENT mode ---
        // Try to attach to an existing CereLink STANDALONE's native segments
        std::string native_cfg = getNativeSegmentName(config.device_type, "config");
        std::string native_rec = getNativeSegmentName(config.device_type, "receive");
        std::string native_xmt = getNativeSegmentName(config.device_type, "xmt_global");
        std::string native_xmt_local = getNativeSegmentName(config.device_type, "xmt_local");
        std::string native_status = getNativeSegmentName(config.device_type, "status");
        std::string native_spk = getNativeSegmentName(config.device_type, "spike");
        std::string native_signal = getNativeSegmentName(config.device_type, "signal");

        shmem_result = cbshm::ShmemSession::create(
            native_cfg, native_rec, native_xmt, native_xmt_local,
            native_status, native_spk, native_signal,
            cbshm::Mode::CLIENT, cbshm::ShmemLayout::NATIVE);

        if (shmem_result.isError()) {
            // --- Attempt 3: Native STANDALONE mode ---
            // No existing shared memory found, create new native-mode segments
            shmem_result = cbshm::ShmemSession::create(
                native_cfg, native_rec, native_xmt, native_xmt_local,
                native_status, native_spk, native_signal,
                cbshm::Mode::STANDALONE, cbshm::ShmemLayout::NATIVE);

            if (shmem_result.isError()) {
                return Result<SdkSession>::error("Failed to create shared memory: " + shmem_result.error());
            }
            is_standalone = true;
        }
    }

    session.m_impl->shmem_session = std::move(shmem_result.value());

    // Create device session only in STANDALONE mode
    if (is_standalone) {
        // Map SDK DeviceType to cbdev DeviceType
        cbdev::DeviceType dev_type;
        switch (config.device_type) {
            case DeviceType::LEGACY_NSP:
                dev_type = cbdev::DeviceType::LEGACY_NSP;
                break;
            case DeviceType::NSP:
                dev_type = cbdev::DeviceType::NSP;
                break;
            case DeviceType::HUB1:
                dev_type = cbdev::DeviceType::HUB1;
                break;
            case DeviceType::HUB2:
                dev_type = cbdev::DeviceType::HUB2;
                break;
            case DeviceType::HUB3:
                dev_type = cbdev::DeviceType::HUB3;
                break;
            case DeviceType::NPLAY:
                dev_type = cbdev::DeviceType::NPLAY;
                break;
            default:
                return Result<SdkSession>::error("Invalid device type");
        }

        // Create device config from device type (uses predefined addresses/ports)
        cbdev::ConnectionParams dev_config = cbdev::ConnectionParams::forDevice(dev_type);

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

        auto dev_result = cbdev::createDeviceSession(dev_config);
        if (dev_result.isError()) {
            return Result<SdkSession>::error("Failed to create device session: " + dev_result.error());
        }
        session.m_impl->device_session = std::move(dev_result.value());

        // TODO [Phase 3]: Config parsing now happens in SDK receive thread, not DeviceSession
        // DeviceSession no longer has setConfigBuffer() method
        // Config buffer management is now SDK's responsibility

        // Start the session (starts receive/send threads)
        // Start session (for STANDALONE mode, this also connects to device and performs handshake)
        auto start_result = session.start();
        if (start_result.isError()) {
            return Result<SdkSession>::error("Failed to start session: " + start_result.error());
        }
    }

    return Result<SdkSession>::ok(std::move(session));
}

Result<void> SdkSession::start() {
    if (m_impl->is_running.load()) {
        return Result<void>::error("Session is already running");
    }

    // Set up device callbacks (if in STANDALONE mode)
    if (m_impl->device_session) {
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

        // Register receive callback - handles each packet from device
        m_impl->receive_callback_handle = m_impl->device_session->registerReceiveCallback(
            [impl](const cbPKT_GENERIC& pkt) {
                // Check for SYSREP packets (handshake responses)
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                    const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
                    impl->device_runlevel.store(sysinfo->runlevel, std::memory_order_release);
                    impl->received_sysrep.store(true, std::memory_order_release);
                    impl->handshake_cv.notify_all();
                }

                // Update stats
                {
                    std::lock_guard<std::mutex> lock(impl->stats_mutex);
                    impl->stats.packets_received_from_device++;
                }

                // Store to shared memory
                auto store_result = impl->shmem_session->storePacket(pkt);
                if (store_result.isOk()) {
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
            });

        // Register datagram complete callback - signals after all packets in a datagram are processed
        m_impl->datagram_callback_handle = m_impl->device_session->registerDatagramCompleteCallback(
            [impl]() {
                // Signal CLIENT processes that new data is available
                impl->shmem_session->signalData();

                // Wake callback thread if waiting
                if (impl->callback_thread_waiting.load(std::memory_order_relaxed)) {
                    impl->callback_cv.notify_one();
                }
            });

        // Start device receive thread (managed by DeviceSession)
        auto recv_start_result = m_impl->device_session->startReceiveThread();
        if (recv_start_result.isError()) {
            // Failed to start receive thread - clean up
            m_impl->callback_thread_running.store(false);
            m_impl->callback_cv.notify_one();
            if (m_impl->callback_thread && m_impl->callback_thread->joinable()) {
                m_impl->callback_thread->join();
            }
            return Result<void>::error("Failed to start device receive thread: " + recv_start_result.error());
        }

        // Start device send thread - dequeues from shmem and sends to device
        m_impl->device_send_thread_running.store(true);
        m_impl->device_send_thread = std::make_unique<std::thread>([impl]() {
            while (impl->device_send_thread_running.load()) {
                bool has_packets = false;

                // Try to dequeue and send all available packets
                while (true) {
                    cbPKT_GENERIC pkt = {};

                    // Dequeue packet from shared memory transmit buffer
                    auto result = impl->shmem_session->dequeuePacket(pkt);
                    if (result.isError() || !result.value()) {
                        break;  // Error or no more packets
                    }

                    has_packets = true;

                    // Send packet to device
                    auto send_result = impl->device_session->sendPacket(pkt);
                    if (send_result.isError()) {
                        std::lock_guard<std::mutex> lock(impl->stats_mutex);
                        impl->stats.send_errors++;
                    } else {
                        std::lock_guard<std::mutex> lock(impl->stats_mutex);
                        impl->stats.packets_sent_to_device++;
                    }
                }

                if (has_packets) {
                    // Had packets - check again quickly
                    std::this_thread::yield();
                } else {
                    // No packets - wait briefly before checking again
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });

        // Perform handshaking based on autorun flag
        Result<void> handshake_result;
        if (m_impl->config.autorun) {
            // Fully start device to RUNNING state (includes requestConfiguration)
            handshake_result = performStartupHandshake(500);
        } else {
            // Just request configuration without changing runlevel
            handshake_result = requestConfiguration(500);
        }

        if (handshake_result.isError()) {
            // Clean up device receive thread (managed by DeviceSession)
            m_impl->device_session->stopReceiveThread();
            m_impl->device_session->unregisterCallback(m_impl->receive_callback_handle);
            m_impl->device_session->unregisterCallback(m_impl->datagram_callback_handle);
            m_impl->receive_callback_handle = 0;
            m_impl->datagram_callback_handle = 0;
            // Clean up device send thread
            m_impl->device_send_thread_running.store(false);
            if (m_impl->device_send_thread && m_impl->device_send_thread->joinable()) {
                m_impl->device_send_thread->join();
            }
            // Clean up callback thread
            m_impl->callback_thread_running.store(false);
            m_impl->callback_cv.notify_one();
            if (m_impl->callback_thread && m_impl->callback_thread->joinable()) {
                m_impl->callback_thread->join();
            }
            return Result<void>::error("Handshake failed: " + handshake_result.error());
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
    if (m_impl->device_session) {
        // Stop device receive thread (managed by DeviceSession)
        m_impl->device_session->stopReceiveThread();
        // Unregister callbacks
        if (m_impl->receive_callback_handle != 0) {
            m_impl->device_session->unregisterCallback(m_impl->receive_callback_handle);
            m_impl->receive_callback_handle = 0;
        }
        if (m_impl->datagram_callback_handle != 0) {
            m_impl->device_session->unregisterCallback(m_impl->datagram_callback_handle);
            m_impl->datagram_callback_handle = 0;
        }
        // Stop device send thread
        if (m_impl->device_send_thread_running.load()) {
            m_impl->device_send_thread_running.store(false);
            if (m_impl->device_send_thread && m_impl->device_send_thread->joinable()) {
                m_impl->device_send_thread->join();
            }
        }
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
        if (m_impl->device_session) {
            // Notify send thread that packets are available
            // (device_session checks hasTransmitPackets via callback)
        }
        return Result<void>::ok();
    } else {
        return Result<void>::error(result.error());
    }
}

///--------------------------------------------------------------------------------------------
/// Helper: Wait for SYSREP packet
///--------------------------------------------------------------------------------------------

bool SdkSession::waitForSysrep(uint32_t timeout_ms, uint32_t expected_runlevel) const {
    // Wait for SYSREP packet with optional expected runlevel
    // If expected_runlevel is 0, accept any SYSREP
    // If expected_runlevel is non-zero, wait for that specific runlevel
    std::unique_lock<std::mutex> lock(m_impl->handshake_mutex);
    return m_impl->handshake_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this, expected_runlevel] {
            if (!m_impl->received_sysrep.load(std::memory_order_acquire)) {
                return false;  // Haven't received SYSREP yet
            }
            if (expected_runlevel == 0) {
                return true;  // Accept any SYSREP
            }
            // Check if runlevel matches
            return m_impl->device_runlevel.load(std::memory_order_acquire) == expected_runlevel;
        });
}

///--------------------------------------------------------------------------------------------
/// Device Handshaking Methods (STANDALONE mode only)
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) {
    return setSystemRunLevel(runlevel, resetque, runflags, 0, 500);
}

Result<void> SdkSession::setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                           uint32_t wait_for_runlevel, uint32_t timeout_ms) {
    if (!m_impl->device_session) {
        return Result<void>::error("setSystemRunLevel() only available in STANDALONE mode");
    }

    // Reset handshake state before sending
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);

    // Send the runlevel packet to device (packet creation handled by DeviceSession)
    auto send_result = m_impl->device_session->setSystemRunLevel(runlevel, resetque, runflags);
    if (send_result.isError()) {
        return Result<void>::error(send_result.error());
    }

    // Wait for SYSREP response (synchronous behavior)
    // wait_for_runlevel: 0 = any SYSREP, non-zero = wait for specific runlevel
    if (!waitForSysrep(timeout_ms, wait_for_runlevel)) {
        if (wait_for_runlevel != 0) {
            return Result<void>::error("No SYSREP response with expected runlevel " + std::to_string(wait_for_runlevel));
        }
        return Result<void>::error("No SYSREP response received for setSystemRunLevel");
    }

    return Result<void>::ok();
}

Result<void> SdkSession::requestConfiguration(uint32_t timeout_ms) {
    if (!m_impl->device_session) {
        return Result<void>::error("requestConfiguration() only available in STANDALONE mode");
    }

    // Reset handshake state before sending
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);

    // Send the configuration request packet to device (packet creation handled by DeviceSession)
    auto send_result = m_impl->device_session->requestConfiguration();
    if (send_result.isError()) {
        return Result<void>::error(send_result.error());
    }

    // Wait for final SYSREP packet from config flood (synchronous behavior)
    // The device sends many config packets and finishes with a SYSREP containing current runlevel
    if (!waitForSysrep(timeout_ms)) {
        return Result<void>::error("No SYSREP response received for requestConfiguration");
    }

    return Result<void>::ok();
}

Result<void> SdkSession::performStartupHandshake(uint32_t timeout_ms) {
    if (!m_impl->device_session) {
        return Result<void>::error("performStartupHandshake() only available in STANDALONE mode");
    }

    // Complete device startup sequence to transition device from any state to RUNNING
    //
    // Sequence:
    // 1. Quick device presence check (100ms timeout) - fail fast if device not on network
    // 2. Send cbRUNLEVEL_RUNNING - check if device is already running
    // 3. If not running, send cbRUNLEVEL_HARDRESET - wait for STANDBY
    // 4. Send REQCONFIGALL - wait for config flood ending with SYSREP
    // 5. Send cbRUNLEVEL_RESET - wait for device to transition to RUNNING

    // Reset handshake state
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    m_impl->device_runlevel.store(0, std::memory_order_relaxed);

    // Quick presence check - use shorter timeout to fail fast for non-existent devices
    const uint32_t presence_check_timeout = std::min(100u, timeout_ms);

    // Step 1: Quick presence check - send cbRUNLEVEL_RUNNING with short timeout to fail fast
    Result<void> result = setSystemRunLevel(cbRUNLEVEL_RUNNING, 0, 0, 0, presence_check_timeout);
    if (result.isError()) {
        // No response - device not on network
        return Result<void>::error("Device not reachable (no response to initial probe - check network connection and IP address)");
    }

    // Step 2: Got response - check if device is already running
    if (m_impl->device_runlevel.load(std::memory_order_acquire) == cbRUNLEVEL_RUNNING) {
        // Device is already running - request config and we're done
        goto request_config;
    }

    // Step 3: Device responded but not running - send HARDRESET and wait for STANDBY
    // Device responds with HARDRESET, then STANDBY
    result = setSystemRunLevel(cbRUNLEVEL_HARDRESET, 0, 0, cbRUNLEVEL_STANDBY, timeout_ms);
    if (result.isError()) {
        return Result<void>::error("Failed to send HARDRESET command: " + result.error());
    }

request_config:
    // Step 4: Request all configuration (always performed)
    // requestConfiguration() waits internally for final SYSREP
    result = requestConfiguration(timeout_ms);
    if (result.isError()) {
        return Result<void>::error("Failed to send REQCONFIGALL: " + result.error());
    }

    // Step 5: Get current runlevel and transition to RUNNING if needed
    uint32_t current_runlevel = m_impl->device_runlevel.load(std::memory_order_acquire);

    if (current_runlevel != cbRUNLEVEL_RUNNING) {
        // Send RESET to complete handshake
        // Device is in STANDBY (30) after REQCONFIGALL - send RESET which transitions to RUNNING (50)
        // The device responds first with RESET, then on next iteration with RUNNING
        result = setSystemRunLevel(cbRUNLEVEL_RESET, 0, 0, cbRUNLEVEL_RUNNING, timeout_ms);
        if (result.isError()) {
            return Result<void>::error("Failed to send RESET command: " + result.error());
        }
    }

    // Success - device is now in RUNNING state
    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////////////////////////

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
