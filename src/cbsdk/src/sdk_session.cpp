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
#include "cmp_parser.h"
#include "cbdev/device_factory.h"
#include "cbdev/connection.h"
#include "cbshm/shmem_session.h"
#include <ccfutils/ccf_config.h>
#include <CCFUtils.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <array>
#include <vector>

namespace cbsdk {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel type classification helper (capability-based)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Classify a channel using its capability flags from device config.
/// This matches the logic in cbdev::DeviceSession::channelMatchesType and pycbsdk.
static ChannelType classifyChannelByCaps(const cbPKT_CHANINFO& chaninfo) {
    const uint32_t caps = chaninfo.chancaps;

    // Channel must exist and be connected
    if ((cbCHAN_EXISTS | cbCHAN_CONNECTED) != (caps & (cbCHAN_EXISTS | cbCHAN_CONNECTED)))
        return ChannelType::ANY;

    // Front-end: analog input + isolated
    if ((cbCHAN_AINP | cbCHAN_ISOLATED) == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED)))
        return ChannelType::FRONTEND;

    // Analog input (not isolated)
    if (cbCHAN_AINP == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED)))
        return ChannelType::ANALOG_IN;

    // Analog output: check audio flag to distinguish
    if (cbCHAN_AOUT == (caps & cbCHAN_AOUT)) {
        if (cbAOUT_AUDIO == (chaninfo.aoutcaps & cbAOUT_AUDIO))
            return ChannelType::AUDIO;
        return ChannelType::ANALOG_OUT;
    }

    // Digital input: check serial vs regular
    if (cbCHAN_DINP == (caps & cbCHAN_DINP)) {
        if (chaninfo.dinpcaps & cbDINP_SERIALMASK)
            return ChannelType::SERIAL;
        return ChannelType::DIGITAL_IN;
    }

    // Digital output
    if (cbCHAN_DOUT == (caps & cbCHAN_DOUT))
        return ChannelType::DIGITAL_OUT;

    return ChannelType::ANY;
}

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

    // User callbacks — per-type vectors for O(1) dispatch (Phase 2, Fix 8)
    // Registered rarely (user thread), dispatched at 30k/s (callback thread).
    struct PacketCB { CallbackHandle handle; PacketCallback cb; };
    struct EventCB  { CallbackHandle handle; ChannelType channel_type; EventCallback cb; };
    struct GroupCB  { CallbackHandle handle; uint8_t group_id; GroupCallback cb; };
    struct ConfigCB { CallbackHandle handle; uint16_t packet_type; ConfigCallback cb; };

    std::vector<PacketCB> packet_callbacks;
    std::vector<EventCB>  event_callbacks;
    std::vector<GroupCB>  group_callbacks;
    std::vector<ConfigCB> config_callbacks;

    CallbackHandle next_callback_handle = 1;
    ErrorCallback error_callback;
    std::mutex user_callback_mutex;

    // Channel type cache — pre-computed at config time, avoids per-packet getChanInfo() (Phase 3, Fix 10)
    std::array<ChannelType, cbMAXCHANS> channel_type_cache;
    bool channel_cache_valid = false;

    // CMP (channel mapping) position overlay
    // Keyed by cmpKey(bank, term) → position[4] = {col, row, bank_letter, electrode}
    CmpPositionMap cmp_positions;
    std::mutex cmp_mutex;  // protects cmp_positions (loaded rarely, read on every CHANREP)

    void rebuildChannelTypeCache() {
        for (uint32_t ch = 0; ch < cbMAXCHANS; ++ch) {
            const cbPKT_CHANINFO* ci = nullptr;
            if (device_session) {
                ci = device_session->getChanInfo(ch + 1);
            } else if (shmem_session) {
                const auto* native = shmem_session->getNativeConfigBuffer();
                if (native) ci = &native->chaninfo[ch];
            }
            channel_type_cache[ch] = ci ? classifyChannelByCaps(*ci) : ChannelType::ANY;
        }
        channel_cache_valid = true;
    }

    /// Get chaninfo pointer for a 0-based channel index (works for both STANDALONE and CLIENT)
    const cbPKT_CHANINFO* getChanInfoPtr(uint32_t idx) const;

    // Clock sync periodic probing (STANDALONE mode)
    std::chrono::steady_clock::time_point last_clock_probe_time{};

    // Statistics — atomic counters, no mutex needed (Phase 2, Fix 9)
    struct AtomicStats {
        std::atomic<uint64_t> packets_received_from_device{0};
        std::atomic<uint64_t> bytes_received_from_device{0};
        std::atomic<uint64_t> packets_stored_to_shmem{0};
        std::atomic<uint64_t> packets_queued_for_callback{0};
        std::atomic<uint64_t> packets_delivered_to_callback{0};
        std::atomic<uint64_t> packets_dropped{0};
        std::atomic<uint64_t> queue_max_depth{0};
        std::atomic<uint64_t> packets_sent_to_device{0};
        std::atomic<uint64_t> shmem_store_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> send_errors{0};

        void reset() {
            packets_received_from_device.store(0, std::memory_order_relaxed);
            bytes_received_from_device.store(0, std::memory_order_relaxed);
            packets_stored_to_shmem.store(0, std::memory_order_relaxed);
            packets_queued_for_callback.store(0, std::memory_order_relaxed);
            packets_delivered_to_callback.store(0, std::memory_order_relaxed);
            packets_dropped.store(0, std::memory_order_relaxed);
            queue_max_depth.store(0, std::memory_order_relaxed);
            packets_sent_to_device.store(0, std::memory_order_relaxed);
            shmem_store_errors.store(0, std::memory_order_relaxed);
            receive_errors.store(0, std::memory_order_relaxed);
            send_errors.store(0, std::memory_order_relaxed);
        }

        SdkStats snapshot() const {
            SdkStats s;
            s.packets_received_from_device = packets_received_from_device.load(std::memory_order_relaxed);
            s.bytes_received_from_device = bytes_received_from_device.load(std::memory_order_relaxed);
            s.packets_stored_to_shmem = packets_stored_to_shmem.load(std::memory_order_relaxed);
            s.packets_queued_for_callback = packets_queued_for_callback.load(std::memory_order_relaxed);
            s.packets_delivered_to_callback = packets_delivered_to_callback.load(std::memory_order_relaxed);
            s.packets_dropped = packets_dropped.load(std::memory_order_relaxed);
            s.queue_max_depth = queue_max_depth.load(std::memory_order_relaxed);
            s.packets_sent_to_device = packets_sent_to_device.load(std::memory_order_relaxed);
            s.shmem_store_errors = shmem_store_errors.load(std::memory_order_relaxed);
            s.receive_errors = receive_errors.load(std::memory_order_relaxed);
            s.send_errors = send_errors.load(std::memory_order_relaxed);
            return s;
        }
    };
    AtomicStats stats;

    // Running state
    std::atomic<bool> is_running{false};

    // Shutdown guard — set during stop()/~Impl() to make callbacks bail out immediately.
    // Separate from is_running because callbacks must work before is_running is set
    // (e.g., during the handshake phase in start()).
    std::atomic<bool> shutting_down{false};

    /// Apply CMP position overlay to a CHANREP packet's channel in device_config.
    /// Called from the receive path when a CHANREP is received.
    void applyCmpOverlay(const cbPKT_CHANINFO& chaninfo) {
        std::lock_guard<std::mutex> lock(cmp_mutex);
        if (cmp_positions.empty()) return;

        uint64_t key = cmpKey(chaninfo.bank, chaninfo.term);
        auto it = cmp_positions.find(key);
        if (it == cmp_positions.end()) return;

        const auto& pos = it->second;

        // Overlay in device_config (STANDALONE mode primary config source)
        if (device_session) {
            const uint32_t chan = chaninfo.chan;
            if (chan >= 1 && chan <= cbMAXCHANS) {
                // getChanInfo returns const, but we need to write — use getDeviceConfig
                auto& config = const_cast<cbproto::DeviceConfig&>(device_session->getDeviceConfig());
                std::memcpy(config.chaninfo[chan - 1].position, pos.data(), sizeof(int32_t) * 4);
            }
        }

        // Also overlay in shmem (so CLIENTs see correct positions)
        if (shmem_session) {
            const uint32_t chan = chaninfo.chan;
            if (chan >= 1 && chan <= cbMAXCHANS) {
                auto ci_result = shmem_session->getChanInfo(chan - 1);
                if (ci_result.isOk()) {
                    auto ci = ci_result.value();
                    std::memcpy(ci.position, pos.data(), sizeof(int32_t) * 4);
                    shmem_session->setChanInfo(chan - 1, ci);
                }
            }
        }
    }

    /// Apply CMP positions to all existing chaninfo entries.
    /// Called after loading a CMP file to overlay positions immediately.
    void applyCmpToAllChannels() {
        for (uint32_t ch = 0; ch < cbMAXCHANS; ++ch) {
            const cbPKT_CHANINFO* ci = nullptr;
            if (device_session) {
                ci = device_session->getChanInfo(ch + 1);
            } else if (shmem_session) {
                const auto* native = shmem_session->getNativeConfigBuffer();
                if (native) ci = &native->chaninfo[ch];
            }
            if (ci && ci->chan > 0) {
                applyCmpOverlay(*ci);
            }
        }
    }

    /// Dispatch a single packet to all matching typed callbacks.
    /// Called on the callback thread (off the queue).
    /// Snapshots each callback vector under lock, then dispatches without lock (Phase 2, Fix 6).
    void dispatchPacket(const cbPKT_GENERIC& pkt) {
        const uint16_t chid = pkt.cbpkt_header.chid;

        // Snapshot callback vectors under lock (fast: just copies a few pointers+sizes)
        std::vector<PacketCB> snap_packet;
        std::vector<EventCB>  snap_event;
        std::vector<GroupCB>  snap_group;
        std::vector<ConfigCB> snap_config;
        {
            std::lock_guard<std::mutex> lock(user_callback_mutex);
            snap_packet = packet_callbacks;
            // Only snapshot the vectors we'll actually need for this packet type
            if (chid != 0 && !(chid & cbPKTCHAN_CONFIGURATION)) {
                snap_event = event_callbacks;
            } else if (chid == 0) {
                snap_group = group_callbacks;
            } else if (chid & cbPKTCHAN_CONFIGURATION) {
                snap_config = config_callbacks;
            }
        }

        // Dispatch without holding the lock — user callbacks can take arbitrary time
        for (const auto& cb : snap_packet) {
            if (cb.cb) cb.cb(pkt);
        }

        if (chid != 0 && !(chid & cbPKTCHAN_CONFIGURATION)) {
            // Look up cached channel type (Phase 3, Fix 10)
            ChannelType pkt_chan_type = ChannelType::ANY;
            if (channel_cache_valid && chid >= 1 && chid <= cbMAXCHANS) {
                pkt_chan_type = channel_type_cache[chid - 1];
            }
            for (const auto& cb : snap_event) {
                if (cb.channel_type == ChannelType::ANY || cb.channel_type == pkt_chan_type) {
                    if (cb.cb) cb.cb(pkt);
                }
            }
        } else if (chid == 0) {
            for (const auto& cb : snap_group) {
                if (pkt.cbpkt_header.type == cb.group_id) {
                    if (cb.cb) cb.cb(reinterpret_cast<const cbPKT_GROUP&>(pkt));
                }
            }
        } else if (chid & cbPKTCHAN_CONFIGURATION) {
            for (const auto& cb : snap_config) {
                if (pkt.cbpkt_header.type == cb.packet_type) {
                    if (cb.cb) cb.cb(pkt);
                }
            }
        }
    }

    ~Impl() {
        // Mark as shutting down so callbacks bail out immediately
        shutting_down.store(true, std::memory_order_release);
        is_running.store(false);

        if (device_session) {
            // Unregister callbacks first (blocks until any in-progress callback completes)
            if (receive_callback_handle != 0) {
                device_session->unregisterCallback(receive_callback_handle);
            }
            if (datagram_callback_handle != 0) {
                device_session->unregisterCallback(datagram_callback_handle);
            }
            // Then stop device receive thread
            device_session->stopReceiveThread();
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

// Helper function to map DeviceType to shared memory instance number.
// Central creates a SINGLE shared memory instance (0) for ALL instruments in a
// Gemini system. The different instruments (Hub1-3, NSP) share the same buffers
// and are distinguished by instrument INDEX within the buffers, not by separate
// shared memory instances. Therefore all device types map to instance 0.
static int getInstanceNumber(DeviceType /*type*/) {
    return 0;
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
        case DeviceType::GEMINI_NPLAY: return "gemini_nplay";
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
            case DeviceType::GEMINI_NPLAY:
                dev_type = cbdev::DeviceType::GEMINI_NPLAY;
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
    } else {
        // CLIENT mode - start the shmem receive thread (no device session needed)
        auto start_result = session.start();
        if (start_result.isError()) {
            return Result<SdkSession>::error("Failed to start CLIENT session: " + start_result.error());
        }
        // Build channel type cache from existing shmem config
        session.m_impl->rebuildChannelTypeCache();
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
            constexpr size_t MAX_BATCH = 32;
            cbPKT_GENERIC packets[MAX_BATCH];

            while (impl->callback_thread_running.load()) {
                size_t count = 0;

                // Drain available packets from queue (non-blocking)
                while (count < MAX_BATCH && impl->packet_queue.pop(packets[count])) {
                    count++;
                }

                if (count > 0) {
                    impl->callback_thread_waiting.store(false, std::memory_order_relaxed);

                    impl->stats.packets_delivered_to_callback.fetch_add(count, std::memory_order_relaxed);

                    // Dispatch each packet to matching typed callbacks
                    for (size_t i = 0; i < count; i++) {
                        impl->dispatchPacket(packets[i]);
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
                // Guard: bail out if session is shutting down to avoid accessing
                // members during destruction (prevents intermittent SIGSEGV in debug mode)
                if (impl->shutting_down.load(std::memory_order_acquire)) {
                    return;
                }

                // Check for SYSREP packets (handshake responses)
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                    const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
                    impl->device_runlevel.store(sysinfo->runlevel, std::memory_order_release);
                    impl->received_sysrep.store(true, std::memory_order_release);
                    impl->handshake_cv.notify_all();
                }

                // Store to shared memory
                auto store_result = impl->shmem_session->storePacket(pkt);

                // Apply CMP position overlay for CHANREP packets
                // DeviceSession::updateConfigFromBuffer already ran (before this callback),
                // so device_config has the new chaninfo. We overlay positions in both
                // device_config and shmem so all readers see correct positions.
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANREP) {
                    const auto* chaninfo = reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
                    impl->applyCmpOverlay(*chaninfo);
                }

                // Queue for callback
                bool queued = impl->packet_queue.push(pkt);

                // Update stats with atomic increments (no mutex needed)
                impl->stats.packets_received_from_device.fetch_add(1, std::memory_order_relaxed);
                if (store_result.isOk()) {
                    impl->stats.packets_stored_to_shmem.fetch_add(1, std::memory_order_relaxed);
                } else {
                    impl->stats.shmem_store_errors.fetch_add(1, std::memory_order_relaxed);
                }
                if (queued) {
                    impl->stats.packets_queued_for_callback.fetch_add(1, std::memory_order_relaxed);
                    uint64_t current_depth = impl->packet_queue.size();
                    uint64_t prev_max = impl->stats.queue_max_depth.load(std::memory_order_relaxed);
                    while (current_depth > prev_max &&
                           !impl->stats.queue_max_depth.compare_exchange_weak(
                               prev_max, current_depth, std::memory_order_relaxed)) {}
                } else {
                    impl->stats.packets_dropped.fetch_add(1, std::memory_order_relaxed);
                }

                if (!queued) {
                    std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                    if (impl->error_callback) {
                        impl->error_callback("Packet queue overflow - dropping packets");
                    }
                }
            });

        // Register datagram complete callback - signals after all packets in a datagram are processed
        m_impl->datagram_callback_handle = m_impl->device_session->registerDatagramCompleteCallback(
            [impl]() {
                // Guard: bail out if session is shutting down
                if (impl->shutting_down.load(std::memory_order_acquire)) {
                    return;
                }

                // Periodic clock sync probing
                auto now = std::chrono::steady_clock::now();
                if (now - impl->last_clock_probe_time > std::chrono::seconds(5)) {
                    impl->device_session->sendClockProbe();
                    impl->last_clock_probe_time = now;
                }

                // Propagate clock sync offset to shmem for CLIENT mode readers
                if (auto offset = impl->device_session->getOffsetNs()) {
                    auto uncertainty = impl->device_session->getUncertaintyNs().value_or(0);
                    impl->shmem_session->setClockSync(*offset, uncertainty);
                }

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
                        impl->stats.send_errors.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        impl->stats.packets_sent_to_device.fetch_add(1, std::memory_order_relaxed);
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
        Impl* impl = m_impl.get();
        m_impl->shmem_receive_thread = std::make_unique<std::thread>([impl]() {
            // CLIENT mode receive thread: reads from Central's cbRECbuffer, dispatches to callbacks
            constexpr size_t MAX_BATCH = 128;
            cbPKT_GENERIC packets[MAX_BATCH];

            while (impl->shmem_receive_thread_running.load()) {
                auto wait_result = impl->shmem_session->waitForData(250);
                if (wait_result.isError()) {
                    std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                    if (impl->error_callback) {
                        impl->error_callback("Error waiting for shared memory signal: " + wait_result.error());
                    }
                    continue;
                }

                if (!wait_result.value()) {
                    continue;  // Timeout
                }

                // Drain all available packets from the ring buffer (not just one batch).
                // This is important when the signal fires infrequently (e.g., Central
                // signals ~100 times/sec) — reading only one batch per wake-up would
                // cap throughput at signal_rate × batch_size.
                bool had_error = false;
                size_t packets_read = 0;
                do {
                    packets_read = 0;
                    auto read_result = impl->shmem_session->readReceiveBuffer(packets, MAX_BATCH, packets_read);
                    if (read_result.isError()) {
                        impl->stats.shmem_store_errors.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                        if (impl->error_callback) {
                            impl->error_callback("Error reading from shared memory: " + read_result.error());
                        }
                        had_error = true;
                        break;
                    }

                    if (packets_read > 0) {
                        impl->stats.packets_delivered_to_callback.fetch_add(packets_read, std::memory_order_relaxed);
                        for (size_t i = 0; i < packets_read; i++) {
                            // CLIENT mode: apply CMP position overlay for CHANREP packets
                            if ((packets[i].cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANREP) {
                                const auto* chaninfo = reinterpret_cast<const cbPKT_CHANINFO*>(&packets[i]);
                                impl->applyCmpOverlay(*chaninfo);
                            }
                            impl->dispatchPacket(packets[i]);
                        }
                    }
                } while (packets_read == MAX_BATCH && impl->shmem_receive_thread_running.load());
                if (had_error) continue;
            }
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
    m_impl->shutting_down.store(true, std::memory_order_release);

    // Stop device threads (if STANDALONE mode)
    if (m_impl->device_session) {
        // Unregister callbacks FIRST — this blocks until any in-progress callback
        // completes (via callback_mutex), then removes them so no new invocations
        // can start. Combined with the shutting_down guard in the callbacks themselves,
        // this ensures no callback accesses Impl members during/after shutdown.
        if (m_impl->receive_callback_handle != 0) {
            m_impl->device_session->unregisterCallback(m_impl->receive_callback_handle);
            m_impl->receive_callback_handle = 0;
        }
        if (m_impl->datagram_callback_handle != 0) {
            m_impl->device_session->unregisterCallback(m_impl->datagram_callback_handle);
            m_impl->datagram_callback_handle = 0;
        }
        // Stop device receive thread (managed by DeviceSession)
        m_impl->device_session->stopReceiveThread();
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

CallbackHandle SdkSession::registerPacketCallback(PacketCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->packet_callbacks.push_back({handle, std::move(callback)});
    return handle;
}

CallbackHandle SdkSession::registerEventCallback(const ChannelType channel_type, EventCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->event_callbacks.push_back({handle, channel_type, std::move(callback)});
    return handle;
}

CallbackHandle SdkSession::registerGroupCallback(const SampleRate rate, GroupCallback callback) const {
    const uint8_t group_id = static_cast<uint8_t>(rate);
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->group_callbacks.push_back({handle, group_id, std::move(callback)});
    return handle;
}

CallbackHandle SdkSession::registerConfigCallback(const uint16_t packet_type, ConfigCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->config_callbacks.push_back({handle, packet_type, std::move(callback)});
    return handle;
}

void SdkSession::unregisterCallback(CallbackHandle handle) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    auto erase_by_handle = [handle](auto& vec) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [handle](const auto& cb) { return cb.handle == handle; }),
            vec.end());
    };
    erase_by_handle(m_impl->packet_callbacks);
    erase_by_handle(m_impl->event_callbacks);
    erase_by_handle(m_impl->group_callbacks);
    erase_by_handle(m_impl->config_callbacks);
}

void SdkSession::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    m_impl->error_callback = std::move(callback);
}

SdkStats SdkSession::getStats() const {
    SdkStats stats = m_impl->stats.snapshot();
    stats.queue_current_depth = m_impl->packet_queue.size();
    return stats;
}

void SdkSession::resetStats() {
    m_impl->stats.reset();
}

const SdkConfig& SdkSession::getConfig() const {
    return m_impl->config;
}

const cbPKT_SYSINFO* SdkSession::getSysInfo() const {
    if (m_impl->device_session)
        return &m_impl->device_session->getSysInfo();
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native)
            return &native->sysinfo;
    }
    return nullptr;
}

const cbPKT_CHANINFO* SdkSession::getChanInfo(const uint32_t chan_id) const {
    if (m_impl->device_session)
        return m_impl->device_session->getChanInfo(chan_id);
    if (m_impl->shmem_session && chan_id >= 1 && chan_id <= cbMAXCHANS) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native)
            return &native->chaninfo[chan_id - 1];
    }
    return nullptr;
}

const cbPKT_GROUPINFO* SdkSession::getGroupInfo(uint32_t group_id) const {
    if (group_id == 0 || group_id > cbMAXGROUPS)
        return nullptr;
    if (m_impl->device_session) {
        const auto& config = m_impl->device_session->getDeviceConfig();
        return &config.groupinfo[group_id - 1];
    }
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native)
            return &native->groupinfo[group_id - 1];
    }
    return nullptr;
}

const cbPKT_FILTINFO* SdkSession::getFilterInfo(const uint32_t filter_id) const {
    if (filter_id >= cbMAXFILTS)
        return nullptr;
    if (m_impl->device_session) {
        const auto& config = m_impl->device_session->getDeviceConfig();
        return &config.filtinfo[filter_id];
    }
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native)
            return &native->filtinfo[filter_id];
    }
    return nullptr;
}

uint32_t SdkSession::getRunLevel() const {
    return m_impl->device_runlevel.load(std::memory_order_acquire);
}

uint32_t SdkSession::getProtocolVersion() const {
    if (m_impl->device_session)
        return static_cast<uint32_t>(m_impl->device_session->getProtocolVersion());
    return 0;  // CLIENT mode — no protocol version available
}

uint32_t SdkSession::getSpikeLength() const {
    const auto* si = getSysInfo();
    return si ? si->spikelen : 0;
}

uint32_t SdkSession::getSpikePretrigger() const {
    const auto* si = getSysInfo();
    return si ? si->spikepre : 0;
}

Result<void> SdkSession::setSpikeLength(uint32_t spikelen, uint32_t spikepre) {
    const auto* si = getSysInfo();
    if (!si)
        return Result<void>::error("System info not available");
    cbPKT_SYSINFO pkt = *si;
    pkt.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
    pkt.spikelen = spikelen;
    pkt.spikepre = spikepre;
    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

uint64_t SdkSession::getTime() const {
    if (!m_impl || !m_impl->shmem_session)
        return 0;
    return m_impl->shmem_session->getLastTime();
}

///--------------------------------------------------------------------------------------------
/// Channel Configuration
///--------------------------------------------------------------------------------------------

/// Helper: extract a numeric field from a cbPKT_CHANINFO
static int64_t extractChanInfoField(const cbPKT_CHANINFO& ci, ChanInfoField field) {
    switch (field) {
        case ChanInfoField::SMPGROUP:    return ci.smpgroup;
        case ChanInfoField::SMPFILTER:   return ci.smpfilter;
        case ChanInfoField::SPKFILTER:   return ci.spkfilter;
        case ChanInfoField::AINPOPTS:    return ci.ainpopts;
        case ChanInfoField::SPKOPTS:     return ci.spkopts;
        case ChanInfoField::SPKTHRLEVEL: return ci.spkthrlevel;
        case ChanInfoField::LNCRATE:     return ci.lncrate;
        case ChanInfoField::REFELECCHAN: return ci.refelecchan;
        case ChanInfoField::AMPLREJPOS:  return ci.amplrejpos;
        case ChanInfoField::AMPLREJNEG:  return ci.amplrejneg;
        case ChanInfoField::CHANCAPS:    return ci.chancaps;
        case ChanInfoField::BANK:        return ci.bank;
        case ChanInfoField::TERM:        return ci.term;
        default: return 0;
    }
}

/// Helper: get chaninfo pointer for a 0-based channel index
const cbPKT_CHANINFO* SdkSession::Impl::getChanInfoPtr(uint32_t idx) const {
    if (device_session) {
        return device_session->getChanInfo(idx + 1);
    } else if (shmem_session) {
        const auto* native = shmem_session->getNativeConfigBuffer();
        if (native) return &native->chaninfo[idx];
    }
    return nullptr;
}

Result<int64_t> SdkSession::getChannelField(uint32_t chanId, ChanInfoField field) const {
    if (chanId == 0 || chanId > cbMAXCHANS)
        return Result<int64_t>::error("Invalid channel ID");
    const auto* ci = m_impl->getChanInfoPtr(chanId - 1);
    if (!ci)
        return Result<int64_t>::error("Channel info unavailable");
    return Result<int64_t>::ok(extractChanInfoField(*ci, field));
}

Result<std::vector<uint32_t>> SdkSession::getMatchingChannelIds(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<uint32_t> ids;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        const auto* ci = m_impl->getChanInfoPtr(ch);
        if (!ci) continue;
        if (classifyChannelByCaps(*ci) != chanType) continue;
        ids.push_back(ch + 1);
        count++;
    }
    return Result<std::vector<uint32_t>>::ok(std::move(ids));
}

Result<std::vector<int64_t>> SdkSession::getChannelField(
        const size_t nChans, const ChannelType chanType, const ChanInfoField field) const {
    std::vector<int64_t> values;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        const auto* ci = m_impl->getChanInfoPtr(ch);
        if (!ci) continue;
        if (classifyChannelByCaps(*ci) != chanType) continue;
        values.push_back(extractChanInfoField(*ci, field));
        count++;
    }
    return Result<std::vector<int64_t>>::ok(std::move(values));
}

Result<std::vector<std::string>> SdkSession::getChannelLabels(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<std::string> labels;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        const auto* ci = m_impl->getChanInfoPtr(ch);
        if (!ci) continue;
        if (classifyChannelByCaps(*ci) != chanType) continue;
        labels.emplace_back(ci->label);
        count++;
    }
    return Result<std::vector<std::string>>::ok(std::move(labels));
}

Result<std::vector<int32_t>> SdkSession::getChannelPositions(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<int32_t> positions;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        const auto* ci = m_impl->getChanInfoPtr(ch);
        if (!ci) continue;
        if (classifyChannelByCaps(*ci) != chanType) continue;
        positions.push_back(ci->position[0]);
        positions.push_back(ci->position[1]);
        positions.push_back(ci->position[2]);
        positions.push_back(ci->position[3]);
        count++;
    }
    return Result<std::vector<int32_t>>::ok(std::move(positions));
}

/// Helper: map cbsdk::ChannelType to cbdev::ChannelType
static cbdev::ChannelType toDevChannelType(const ChannelType chanType) {
    switch (chanType) {
        case ChannelType::FRONTEND:    return cbdev::ChannelType::FRONTEND;
        case ChannelType::ANALOG_IN:   return cbdev::ChannelType::ANALOG_IN;
        case ChannelType::ANALOG_OUT:  return cbdev::ChannelType::ANALOG_OUT;
        case ChannelType::AUDIO:       return cbdev::ChannelType::AUDIO;
        case ChannelType::DIGITAL_IN:  return cbdev::ChannelType::DIGITAL_IN;
        case ChannelType::SERIAL:      return cbdev::ChannelType::SERIAL;
        case ChannelType::DIGITAL_OUT: return cbdev::ChannelType::DIGITAL_OUT;
        default:                       return cbdev::ChannelType::FRONTEND;
    }
}


Result<void> SdkSession::setChannelSampleGroup(const size_t nChans, const ChannelType chanType,
                                                const SampleRate rate, const bool disableOthers) {
    const uint32_t group_id = static_cast<uint32_t>(rate);
    // STANDALONE mode: delegate to device session (has full config + direct send)
    if (m_impl->device_session) {
        const auto r = m_impl->device_session->setChannelsGroupByType(
            nChans, toDevChannelType(chanType), static_cast<cbdev::DeviceRate>(group_id), disableOthers);
        if (r.isError())
            return Result<void>::error(r.error());
        return Result<void>::ok();
    }

    // CLIENT mode: build packets from shmem chaninfo and send through shmem transmit queue
    if (!m_impl->shmem_session)
        return Result<void>::error("No session available");

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS; ++chan) {
        if (!disableOthers && count >= nChans)
            break;

        auto ci_result = m_impl->shmem_session->getChanInfo(chan - 1);
        if (ci_result.isError())
            continue;
        auto chaninfo = ci_result.value();

        if (classifyChannelByCaps(chaninfo) != chanType)
            continue;

        const auto grp = count < nChans ? group_id : 0u;
        chaninfo.chan = chan;

        if (grp > 0 && grp < 6) {
            chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSETSMP;
            chaninfo.smpgroup = grp;
            constexpr uint32_t filter_map[] = {0, 5, 6, 7, 10, 0, 0};
            chaninfo.smpfilter = filter_map[grp];
            if (grp == 5) {
                chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSET;
                chaninfo.ainpopts &= ~cbAINP_RAWSTREAM;
            }
        } else if (grp == 6) {
            chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
            chaninfo.ainpopts |= cbAINP_RAWSTREAM;
        } else {
            chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSET;
            chaninfo.smpgroup = 0;
            chaninfo.ainpopts &= ~cbAINP_RAWSTREAM;
        }

        auto r = sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(chaninfo));
        if (r.isError())
            return r;
        count++;
    }

    if (count == 0)
        return Result<void>::error("No channels found matching type");
    return Result<void>::ok();
}

Result<void> SdkSession::setChannelSpikeSorting(const size_t nChans, const ChannelType chanType,
                                                 const uint32_t sortOptions) {
    // STANDALONE mode: delegate to device session
    if (m_impl->device_session) {
        const auto r = m_impl->device_session->setChannelsSpikeSortingByType(
            nChans, toDevChannelType(chanType), sortOptions);
        if (r.isError())
            return Result<void>::error(r.error());
        return Result<void>::ok();
    }

    // CLIENT mode: build packets from shmem chaninfo
    if (!m_impl->shmem_session)
        return Result<void>::error("No session available");

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS && count < nChans; ++chan) {
        auto ci_result = m_impl->shmem_session->getChanInfo(chan - 1);
        if (ci_result.isError())
            continue;
        auto chaninfo = ci_result.value();

        if (classifyChannelByCaps(chaninfo) != chanType)
            continue;

        chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSETSPKTHR;
        chaninfo.chan = chan;
        chaninfo.spkopts &= ~cbAINPSPK_ALLSORT;
        chaninfo.spkopts |= sortOptions;

        auto r = sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(chaninfo));
        if (r.isError())
            return r;
        count++;
    }

    if (count == 0)
        return Result<void>::error("No channels found matching type");
    return Result<void>::ok();
}

Result<void> SdkSession::setACInputCoupling(const size_t nChans, const ChannelType chanType,
                                             const bool enabled) {
    // STANDALONE mode: delegate to device session
    if (m_impl->device_session) {
        const auto r = m_impl->device_session->setChannelsACInputCouplingByType(
            nChans, toDevChannelType(chanType), enabled);
        if (r.isError())
            return Result<void>::error(r.error());
        return Result<void>::ok();
    }

    // CLIENT mode: build packets from shmem chaninfo
    if (!m_impl->shmem_session)
        return Result<void>::error("No session available");

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS && count < nChans; ++chan) {
        auto ci_result = m_impl->shmem_session->getChanInfo(chan - 1);
        if (ci_result.isError())
            continue;
        auto chaninfo = ci_result.value();

        if (classifyChannelByCaps(chaninfo) != chanType)
            continue;

        chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
        chaninfo.chan = chan;
        if (enabled)
            chaninfo.ainpopts |= cbAINP_OFFSET_CORRECT;
        else
            chaninfo.ainpopts &= ~cbAINP_OFFSET_CORRECT;

        auto r = sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(chaninfo));
        if (r.isError())
            return r;
        count++;
    }

    if (count == 0)
        return Result<void>::error("No channels found matching type");
    return Result<void>::ok();
}

Result<void> SdkSession::setChannelConfig(const cbPKT_CHANINFO& chaninfo) {
    if (m_impl->device_session)
        return m_impl->device_session->setChannelConfig(chaninfo);
    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(chaninfo));
}

///--------------------------------------------------------------------------------------------
/// Comments
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::sendComment(const std::string& comment, const uint32_t rgba, const uint8_t charset) {
    if (m_impl->device_session) {
        return m_impl->device_session->sendComment(comment, rgba, charset);
    }

    // CLIENT mode fallback: build packet and route through shmem
    cbPKT_COMMENT pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_COMMENTSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_COMMENT;
    pkt.info.charset = charset;
    pkt.timeStarted = 0;
    pkt.rgba = rgba;

    const size_t len = std::min(comment.size(), static_cast<size_t>(cbMAX_COMMENT - 1));
    std::memcpy(pkt.comment, comment.c_str(), len);
    pkt.comment[len] = '\0';

    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

///--------------------------------------------------------------------------------------------
/// File Recording (Central-only commands, always routed through shmem)
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::sendFileCfgPacket(uint32_t options, uint32_t recording,
                                            const std::string& filename, const std::string& comment) {
    cbPKT_FILECFG pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_SETFILECFG;
    pkt.cbpkt_header.dlen = cbPKTDLEN_FILECFG;
    pkt.options = options;
    pkt.extctrl = 0;
    pkt.recording = recording;

    if (!filename.empty()) {
        const size_t fnlen = std::min(filename.size(), sizeof(pkt.filename) - 1);
        std::memcpy(pkt.filename, filename.c_str(), fnlen);
        pkt.filename[fnlen] = '\0';
    }

    if (!comment.empty()) {
        const size_t cmtlen = std::min(comment.size(), sizeof(pkt.comment) - 1);
        std::memcpy(pkt.comment, comment.c_str(), cmtlen);
        pkt.comment[cmtlen] = '\0';
    }

    // Fill username with computer name (Central uses this to identify the requester)
#ifdef _WIN32
    DWORD cchBuff = sizeof(pkt.username);
    GetComputerNameA(pkt.username, &cchBuff);
#else
    const char* host = getenv("HOSTNAME");
    if (host) {
        strncpy(pkt.username, host, sizeof(pkt.username) - 1);
        pkt.username[sizeof(pkt.username) - 1] = '\0';
    }
#endif

    cbPKT_GENERIC generic = {};
    std::memcpy(&generic, &pkt, sizeof(pkt));
    return sendPacket(generic);
}

Result<void> SdkSession::openCentralFileDialog() {
    return sendFileCfgPacket(cbFILECFG_OPT_OPEN, 0, "", "");
}

Result<void> SdkSession::closeCentralFileDialog() {
    return sendFileCfgPacket(cbFILECFG_OPT_CLOSE, 0, "", "");
}

Result<void> SdkSession::startCentralRecording(const std::string& filename, const std::string& comment) {
    return sendFileCfgPacket(cbFILECFG_OPT_NONE, 1, filename, comment);
}

Result<void> SdkSession::stopCentralRecording() {
    return sendFileCfgPacket(cbFILECFG_OPT_NONE, 0, "", "");
}

///--------------------------------------------------------------------------------------------
/// Patient Information
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::setPatientInfo(const std::string& id,
                                         const std::string& firstname,
                                         const std::string& lastname,
                                         uint32_t dob_month, uint32_t dob_day, uint32_t dob_year) {
    cbPKT_PATIENTINFO pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_SETPATIENTINFO;
    pkt.cbpkt_header.dlen = cbPKTDLEN_PATIENTINFO;

    std::strncpy(pkt.ID, id.c_str(), cbMAX_PATIENTSTRING - 1);
    std::strncpy(pkt.firstname, firstname.c_str(), cbMAX_PATIENTSTRING - 1);
    std::strncpy(pkt.lastname, lastname.c_str(), cbMAX_PATIENTSTRING - 1);
    pkt.DOBMonth = dob_month;
    pkt.DOBDay = dob_day;
    pkt.DOBYear = dob_year;

    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

///--------------------------------------------------------------------------------------------
/// Analog Output Monitoring
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::setAnalogOutputMonitor(uint32_t aout_chan_id, uint32_t monitor_chan_id,
                                                 bool track_last, bool spike_only) {
    if (aout_chan_id < 1 || aout_chan_id > cbMAXCHANS)
        return Result<void>::error("Invalid analog output channel ID");

    const cbPKT_CHANINFO* info = getChanInfo(aout_chan_id);
    if (!info)
        return Result<void>::error("Channel info not available for channel " + std::to_string(aout_chan_id));

    // Copy current config and modify analog output fields
    cbPKT_CHANINFO chaninfo = *info;

    // Set monitor channel
    chaninfo.monchan = static_cast<uint16_t>(monitor_chan_id);

    // Read-modify-write analog output options (preserve existing flags)
    uint32_t opts = chaninfo.aoutopts;
    opts &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK | cbAOUT_TRACK);
    if (spike_only)
        opts |= cbAOUT_MONITORSPK;
    else
        opts |= cbAOUT_MONITORSMP;
    if (track_last)
        opts |= cbAOUT_TRACK;
    chaninfo.aoutopts = opts;

    // Send as CHANSET
    chaninfo.cbpkt_header.type = cbPKTTYPE_CHANSET;
    return setChannelConfig(chaninfo);
}

///--------------------------------------------------------------------------------------------
/// Digital Output
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::setDigitalOutput(const uint32_t chan_id, const uint16_t value) {
    if (m_impl->device_session)
        return m_impl->device_session->setDigitalOutput(chan_id, value);

    // CLIENT mode fallback: build packet and route through shmem
    cbPKT_SET_DOUT pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_SET_DOUTSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SET_DOUT;
    pkt.chan = static_cast<uint16_t>(chan_id);
    pkt.value = value;

    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

///--------------------------------------------------------------------------------------------
/// CCF Configuration Files
///--------------------------------------------------------------------------------------------

/// Populate a cbCCF struct from a NativeConfigBuffer (CLIENT mode).
/// Mirrors the logic in ccf::extractDeviceConfig() but reads from NativeConfigBuffer fields.
static void extractFromNativeConfig(const cbshm::NativeConfigBuffer& native, cbCCF& ccf_data)
{
    // Digital filters: copy the 4 custom filters
    for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
        ccf_data.filtinfo[i] = native.filtinfo[cbFIRST_DIGITAL_FILTER + i];

    // Channel info
    for (int i = 0; i < cbMAXCHANS; ++i)
        ccf_data.isChan[i] = native.chaninfo[i];

    // Adaptive filter
    ccf_data.isAdaptInfo = native.adaptinfo;

    // Spike sorting
    ccf_data.isSS_Detect = native.pktDetect;
    ccf_data.isSS_ArtifactReject = native.pktArtifReject;
    for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
        ccf_data.isSS_NoiseBoundary[i] = native.pktNoiseBoundary[i];
    ccf_data.isSS_Statistics = native.pktStatistics;

    // Spike sorting status: set elapsed minutes to 99 (Central convention)
    ccf_data.isSS_Status = native.pktStatus;
    ccf_data.isSS_Status.cntlNumUnits.fElapsedMinutes = 99;
    ccf_data.isSS_Status.cntlUnitStats.fElapsedMinutes = 99;

    // System info: set type to SYSSETSPKLEN
    ccf_data.isSysInfo = native.sysinfo;
    ccf_data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;

    // N-trodes
    for (int i = 0; i < cbMAXNTRODES; ++i)
        ccf_data.isNTrodeInfo[i] = native.isNTrodeInfo[i];

    // LNC
    ccf_data.isLnc = native.isLnc;

    // Waveforms: copy and clear active flag
    for (int i = 0; i < AOUT_NUM_GAIN_CHANS; ++i)
        for (int j = 0; j < cbMAX_AOUT_TRIGGER; ++j) {
            ccf_data.isWaveform[i][j] = native.isWaveform[i][j];
            ccf_data.isWaveform[i][j].active = 0;
        }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Mapping (CMP) Files
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> SdkSession::loadChannelMap(const std::string& filepath, uint32_t bank_offset) {
    auto parse_result = parseCmpFile(filepath, bank_offset);
    if (parse_result.isError()) {
        return Result<void>::error(parse_result.error());
    }

    // Merge parsed positions into our map (allows multiple loadChannelMap calls for multi-port)
    {
        std::lock_guard<std::mutex> lock(m_impl->cmp_mutex);
        for (auto& [key, pos] : parse_result.value()) {
            m_impl->cmp_positions[key] = pos;
        }
    }

    // Apply positions to all existing chaninfo entries
    m_impl->applyCmpToAllChannels();

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CCF Configuration Files
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> SdkSession::saveCCF(const std::string& filename) {
    cbCCF ccf_data{};

    if (m_impl->device_session) {
        ccf::extractDeviceConfig(m_impl->device_session->getDeviceConfig(), ccf_data);
    } else if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (!native)
            return Result<void>::error("No configuration available in shared memory");
        extractFromNativeConfig(*native, ccf_data);
    } else {
        return Result<void>::error("No session available");
    }

    CCFUtils writer(false, &ccf_data);
    auto result = writer.WriteCCFNoPrompt(filename.c_str());
    if (result != ccf::CCFRESULT_SUCCESS)
        return Result<void>::error("Failed to write CCF file");
    return Result<void>::ok();
}

Result<void> SdkSession::loadCCF(const std::string& filename) {
    cbCCF ccf_data{};
    CCFUtils reader(false, &ccf_data);

    auto result = reader.ReadCCF(filename.c_str(), true);
    if (result < ccf::CCFRESULT_SUCCESS)
        return Result<void>::error("Failed to read CCF file");

    auto packets = ccf::buildConfigPackets(ccf_data);
    for (const auto& pkt : packets) {
        auto send_result = sendPacket(pkt);
        if (send_result.isError())
            return send_result;
    }

    return Result<void>::ok();
}

///--------------------------------------------------------------------------------------------
/// Clock Synchronization
///--------------------------------------------------------------------------------------------

std::optional<std::chrono::steady_clock::time_point>
SdkSession::toLocalTime(uint64_t device_time_ns) const {
    // STANDALONE: delegate to device_session's ClockSync
    if (m_impl->device_session)
        return m_impl->device_session->toLocalTime(device_time_ns);

    // CLIENT: use clock offset from shmem
    if (m_impl->shmem_session) {
        auto offset = m_impl->shmem_session->getClockOffsetNs();
        if (offset) {
            const auto local_ns = static_cast<int64_t>(device_time_ns) - *offset;
            return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(local_ns));
        }
    }
    return std::nullopt;
}

std::optional<uint64_t>
SdkSession::toDeviceTime(std::chrono::steady_clock::time_point local_time) const {
    // STANDALONE: delegate to device_session's ClockSync
    if (m_impl->device_session)
        return m_impl->device_session->toDeviceTime(local_time);

    // CLIENT: use clock offset from shmem
    if (m_impl->shmem_session) {
        auto offset = m_impl->shmem_session->getClockOffsetNs();
        if (offset) {
            const auto local_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                local_time.time_since_epoch()).count();
            return static_cast<uint64_t>(local_ns + *offset);
        }
    }
    return std::nullopt;
}

Result<void> SdkSession::sendClockProbe() {
    if (!m_impl->device_session)
        return Result<void>::error("sendClockProbe() only available in STANDALONE mode");
    auto r = m_impl->device_session->sendClockProbe();
    if (r.isError())
        return Result<void>::error(r.error());
    return Result<void>::ok();
}

std::optional<int64_t> SdkSession::getClockOffsetNs() const {
    // STANDALONE: get from device_session's ClockSync
    if (m_impl->device_session)
        return m_impl->device_session->getOffsetNs();

    // CLIENT: read from shmem
    if (m_impl->shmem_session)
        return m_impl->shmem_session->getClockOffsetNs();

    return std::nullopt;
}

std::optional<int64_t> SdkSession::getClockUncertaintyNs() const {
    // STANDALONE: get from device_session's ClockSync
    if (m_impl->device_session)
        return m_impl->device_session->getUncertaintyNs();

    // CLIENT: read from shmem
    if (m_impl->shmem_session)
        return m_impl->shmem_session->getClockUncertaintyNs();

    return std::nullopt;
}

Result<void> SdkSession::sendPacket(const cbPKT_GENERIC& pkt) {
    // Enqueue packet to shared memory transmit buffer
    // Works in both STANDALONE and CLIENT modes:
    // - STANDALONE: send thread will dequeue and transmit
    // - CLIENT: STANDALONE process's send thread will pick it up
    if (!m_impl) {
        return Result<void>::error("sendPacket: m_impl is null");
    }
    if (!m_impl->shmem_session) {
        return Result<void>::error("sendPacket: shmem_session is null");
    }

    // Stamp packet time from receive buffer (Central's xmt consumer skips packets with time=0)
    cbPKT_GENERIC stamped = pkt;
    PROCTIME t = m_impl->shmem_session->getLastTime();
    stamped.cbpkt_header.time = (t != 0) ? t : 1;

    auto result = m_impl->shmem_session->enqueuePacket(stamped);
    if (result.isOk()) {
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
/// Device Handshaking Methods
///--------------------------------------------------------------------------------------------

Result<void> SdkSession::setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) {
    return setSystemRunLevel(runlevel, resetque, runflags, 0, 500);
}

Result<void> SdkSession::setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                           uint32_t wait_for_runlevel, uint32_t timeout_ms) {
    // Reset handshake state before sending
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);

    // Send the runlevel packet
    Result<void> send_result = Result<void>::error("No session available");
    if (m_impl->device_session) {
        send_result = m_impl->device_session->setSystemRunLevel(runlevel, resetque, runflags);
    } else {
        // CLIENT mode: build packet and send through shmem
        cbPKT_SYSINFO pkt = {};
        pkt.cbpkt_header.time = 1;
        pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
        pkt.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
        pkt.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;
        pkt.runlevel = runlevel;
        pkt.resetque = resetque;
        pkt.runflags = runflags;
        send_result = sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
    }
    if (send_result.isError())
        return send_result;

    // Wait for SYSREP response
    if (!waitForSysrep(timeout_ms, wait_for_runlevel)) {
        if (wait_for_runlevel != 0)
            return Result<void>::error("No SYSREP response with expected runlevel " + std::to_string(wait_for_runlevel));
        return Result<void>::error("No SYSREP response received for setSystemRunLevel");
    }

    return Result<void>::ok();
}

Result<void> SdkSession::requestConfiguration(uint32_t timeout_ms) {
    // Reset handshake state before sending
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);

    // Send REQCONFIGALL
    Result<void> send_result = Result<void>::error("No session available");
    if (m_impl->device_session) {
        send_result = m_impl->device_session->requestConfiguration();
    } else {
        cbPKT_GENERIC pkt = {};
        pkt.cbpkt_header.time = 1;
        pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
        pkt.cbpkt_header.type = cbPKTTYPE_REQCONFIGALL;
        pkt.cbpkt_header.dlen = 0;
        send_result = sendPacket(pkt);
    }
    if (send_result.isError())
        return send_result;

    // Wait for final SYSREP from config flood
    if (!waitForSysrep(timeout_ms))
        return Result<void>::error("No SYSREP response received for requestConfiguration");

    return Result<void>::ok();
}

Result<void> SdkSession::performStartupHandshake(uint32_t timeout_ms) {

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
    // Build channel type cache now that config is populated
    m_impl->rebuildChannelTypeCache();
    return Result<void>::ok();
}

} // namespace cbsdk
