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

// Helper function to map DeviceType to shared memory name
// This ensures compatibility with Central's naming convention
static std::string getSharedMemoryName(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP:
            return "cbsdk_default";
        case DeviceType::GEMINI_NSP:
            return "cbsdk_gemini_nsp";
        case DeviceType::GEMINI_HUB1:
            return "cbsdk_gemini_hub1";
        case DeviceType::GEMINI_HUB2:
            return "cbsdk_gemini_hub2";
        case DeviceType::GEMINI_HUB3:
            return "cbsdk_gemini_hub3";
        case DeviceType::NPLAY:
            return "cbsdk_nplay";
        default:
            return "cbsdk_default";
    }
}

Result<SdkSession> SdkSession::create(const SdkConfig& config) {
    SdkSession session;
    session.m_impl->config = config;

    // Automatically determine shared memory name from device type
    std::string shmem_name = getSharedMemoryName(config.device_type);

    // Auto-detect mode: Try CLIENT first (attach to existing), fall back to STANDALONE (create new)
    bool is_standalone = false;

    // Try to attach to existing shared memory (CLIENT mode)
    auto shmem_result = cbshmem::ShmemSession::create(shmem_name, cbshmem::Mode::CLIENT);

    if (shmem_result.isError()) {
        // No existing shared memory, create new (STANDALONE mode)
        shmem_result = cbshmem::ShmemSession::create(shmem_name, cbshmem::Mode::STANDALONE);
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
    }

    return Result<SdkSession>::ok(std::move(session));
}

Result<void> SdkSession::start() {
    if (m_impl->is_running.load()) {
        return Result<void>::error("Session is already running");
    }

    // Start callback thread first
    m_impl->callback_thread_running.store(true);
    m_impl->callback_thread = std::make_unique<std::thread>([this]() {
        callbackThreadLoop();
    });

    // Set up device callbacks (if in STANDALONE mode)
    if (m_impl->device_session.has_value()) {
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
    }

    m_impl->is_running.store(true);
    return Result<void>::ok();
}

void SdkSession::stop() {
    if (!m_impl || !m_impl->is_running.load()) {
        return;
    }

    m_impl->is_running.store(false);

    // Stop device threads
    if (m_impl->device_session.has_value()) {
        m_impl->device_session->stopReceiveThread();
        m_impl->device_session->stopSendThread();
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
    sysinfo.cbpkt_header.dlen = ((sizeof(cbPKT_SYSINFO)/4) - 2);  // cbPKTDLEN_SYSINFO
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// Private Methods
///////////////////////////////////////////////////////////////////////////////////////////////////

void SdkSession::onPacketsReceivedFromDevice(const cbPKT_GENERIC* pkts, size_t count) {
    // This runs on the cbdev receive thread - MUST BE FAST!

    for (size_t i = 0; i < count; ++i) {
        const auto& pkt = pkts[i];

        // Update stats (atomic or quick increment)
        {
            std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
            m_impl->stats.packets_received_from_device++;
            // bytes_received tracked by cbdev
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

    // Wake callback thread if it's waiting
    if (m_impl->callback_thread_waiting.load(std::memory_order_relaxed)) {
        m_impl->callback_cv.notify_one();
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
