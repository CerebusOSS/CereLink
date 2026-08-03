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
#ifdef _WIN32
#include <mmsystem.h>
#endif

#include "cbsdk/sdk_session.h"
#include "cmp_parser.h"
#include "cbdev/device_factory.h"
#include "cbdev/connection.h"
#include "cbshm/shmem_session.h"
#include <cbproto/gemini.h>
#include <ccfutils/ccf_config.h>
#include <CCFUtils.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <utility>
#include "cbdev/clock_sync.h"
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace {

/// High-resolution microsecond delay.
/// On Windows, std::this_thread::sleep_for rounds up to ~15 ms which is far
/// too coarse for the 50 µs inter-packet pacing the send thread needs.
/// Use a QPC spin-wait instead (mirrors cbdev's hr_sleep_us).
inline void hr_sleep_us([[maybe_unused]] uint64_t microseconds) {
#ifdef _WIN32
    if (microseconds == 0) return;
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const double ticks_per_us = static_cast<double>(freq.QuadPart) / 1'000'000.0;
    const LONGLONG delta = static_cast<LONGLONG>(microseconds * ticks_per_us);
    LARGE_INTEGER target;
    target.QuadPart = start.QuadPart + delta;
    for (;;) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (now.QuadPart >= target.QuadPart) break;
    }
#else
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int64_t>(microseconds)));
#endif
}

/// Lightweight read-only reader for a peer device's clock sync fields
/// in shared memory.  Opens only the config segment via shm_open/mmap
/// (not a full ShmemSession).  Used by the NSP to borrow a HUB's
/// probe-based clock offset when its own probes are unreliable.
struct PeerClockReader {
#ifndef _WIN32
    int fd = -1;
    void* mapped = nullptr;
    size_t mapped_size = 0;

    bool isOpen() const { return mapped != nullptr && mapped != MAP_FAILED; }

    bool tryOpen(const std::string& segment_name) {
        close();
        std::string posix_name = "/" + segment_name;
        fd = shm_open(posix_name.c_str(), O_RDONLY, 0);
        if (fd < 0)
            return false;
        struct stat st{};
        if (fstat(fd, &st) < 0 || st.st_size < static_cast<off_t>(sizeof(cbshm::NativeConfigBuffer))) {
            ::close(fd);
            fd = -1;
            return false;
        }
        mapped_size = static_cast<size_t>(st.st_size);
        mapped = mmap(nullptr, mapped_size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapped == MAP_FAILED) {
            mapped = nullptr;
            ::close(fd);
            fd = -1;
            return false;
        }
        return true;
    }

    std::optional<int64_t> getClockOffsetNs() const {
        if (!isOpen()) return std::nullopt;
        const auto* cfg = static_cast<const cbshm::NativeConfigBuffer*>(mapped);
        if (!cfg->clock_sync_valid) return std::nullopt;
        // Liveness check: is the owning process still alive?
        if (cfg->owner_pid != 0 && kill(static_cast<pid_t>(cfg->owner_pid), 0) != 0)
            return std::nullopt;
        return cfg->clock_offset_ns;
    }

    /// Peer's own (pre-consensus) estimate — the value to use for cross-device
    /// consensus voting (clock_offset_ns is the peer's post-consensus value).
    std::optional<int64_t> getRawOffsetNs() const {
        if (!isOpen()) return std::nullopt;
        const auto* cfg = static_cast<const cbshm::NativeConfigBuffer*>(mapped);
        if (!cfg->clock_raw_valid) return std::nullopt;
        if (cfg->owner_pid != 0 && kill(static_cast<pid_t>(cfg->owner_pid), 0) != 0)
            return std::nullopt;
        return cfg->clock_raw_offset_ns;
    }

    std::optional<int64_t> getClockUncertaintyNs() const {
        if (!isOpen()) return std::nullopt;
        const auto* cfg = static_cast<const cbshm::NativeConfigBuffer*>(mapped);
        if (!cfg->clock_sync_valid) return std::nullopt;
        if (cfg->owner_pid != 0 && kill(static_cast<pid_t>(cfg->owner_pid), 0) != 0)
            return std::nullopt;
        return cfg->clock_uncertainty_ns;
    }

    void close() {
        if (mapped && mapped != MAP_FAILED)
            munmap(mapped, mapped_size);
        mapped = nullptr;
        mapped_size = 0;
        if (fd >= 0)
            ::close(fd);
        fd = -1;
    }

    ~PeerClockReader() { close(); }
#else
    // Windows stub — not yet implemented
    bool isOpen() const { return false; }
    bool tryOpen(const std::string&) { return false; }
    std::optional<int64_t> getClockOffsetNs() const { return std::nullopt; }
    std::optional<int64_t> getRawOffsetNs() const { return std::nullopt; }
    std::optional<int64_t> getClockUncertaintyNs() const { return std::nullopt; }
    void close() {}
#endif

    PeerClockReader() = default;
    PeerClockReader(const PeerClockReader&) = delete;
    PeerClockReader& operator=(const PeerClockReader&) = delete;
};

} // anonymous namespace

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

    // Connection mode
    bool standalone = false;

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
    // Sticky flag set when a SYSREPRUNLEV (0x12) is observed since the last
    // reset.  Stays true even if a later SYSREP (0x10) heartbeat arrives, so
    // sync()'s wait can't be raced by a heartbeat clobbering a "last type"
    // field.  Reset alongside received_sysrep before each send.
    std::atomic<bool> received_sysrepRunlev{false};
    std::mutex handshake_mutex;
    std::condition_variable handshake_cv;

    // Running count of configuration replies (CHANREP/PROCREP/GROUPREP).
    // requestConfiguration() uses this to tell "the dump arrived but its
    // terminating SYSREP did not" apart from "the device never answered".
    std::atomic<uint64_t> config_replies{0};

    // User callbacks — per-type vectors for O(1) dispatch (Phase 2, Fix 8)
    // Registered rarely (user thread), dispatched at 30k/s (callback thread).
    struct PacketCB     { CallbackHandle handle; PacketCallback cb; };
    struct EventCB      { CallbackHandle handle; ChannelType channel_type; EventCallback cb; };
    struct GroupCB       { CallbackHandle handle; uint8_t group_id; GroupCallback cb; };
    struct GroupBatchCB  { CallbackHandle handle; uint8_t group_id; GroupBatchCallback cb; };
    struct ConfigCB     { CallbackHandle handle; uint16_t packet_type; ConfigCallback cb; };
    struct RunlevelCB   { CallbackHandle handle; RunlevelCallback cb; };

    std::vector<PacketCB>     packet_callbacks;
    std::vector<EventCB>      event_callbacks;
    std::vector<GroupCB>       group_callbacks;
    std::vector<GroupBatchCB>  group_batch_callbacks;
    std::vector<ConfigCB>     config_callbacks;
    std::vector<RunlevelCB>   runlevel_callbacks;

    /// Atomically update device_runlevel; fire registered callbacks if the
    /// value changed.  Called from the receive thread (STANDALONE) or the
    /// shmem-receive thread (CLIENT) — both paths converge here.
    void updateRunlevel(uint32_t new_runlevel) {
        const uint32_t prev = device_runlevel.exchange(new_runlevel, std::memory_order_acq_rel);
        if (prev == new_runlevel) return;
        std::vector<RunlevelCB> snap;
        {
            std::lock_guard<std::mutex> lock(user_callback_mutex);
            snap = runlevel_callbacks;
        }
        for (const auto& cb : snap) {
            if (cb.cb) cb.cb(new_runlevel);
        }
    }

    CallbackHandle next_callback_handle = 1;
    ErrorCallback error_callback;
    std::mutex user_callback_mutex;

    ///////////////////////////////////////////////////////////////////////////
    // Per-instrument channel window
    //
    // Central numbers channels globally across every instrument (Hub1 1-256,
    // Hub2 257-512, ...), but the public API presents each device as an
    // independent 1-based device so that CENTRAL CLIENT and STANDALONE look
    // identical to a caller: HUB2's channel 1 is 1, not 257.  These segments
    // map this session's local ids onto Central's global ids.
    //
    // Local numbering follows the cbproto *wire* layout (FE 1..256, then ANAIN,
    // ANAOUT, AUDOUT, DIGIN, SERIAL, DIGOUT), which is exactly one instrument's
    // complement.  On a single-instrument system the map is the identity, so
    // nothing changes for legacy setups.
    struct ChanSeg {
        uint32_t local_start;   ///< 1-based, inclusive
        uint32_t count;
        uint32_t global_start;  ///< 1-based, inclusive
    };
    // Mutable so an unresolved window can still be resolved from const
    // accessors -- see ensureWindowResolved().
    mutable std::vector<ChanSeg> chan_segs;         ///< the mapped ranges
    mutable bool window_resolved = false;           ///< false => identity map (unknown)
    mutable uint32_t local_max_chans = cbMAXCHANS;  ///< channels this device has
    uint32_t central_instrument = 0;                ///< instrument this session speaks for

    /// @brief Resolve the window on first use if it could not be resolved yet
    ///
    /// procinfo can lag session setup by an unpredictable amount -- a Gemini
    /// NSP resolved on three opens out of four with a one-second wait -- and an
    /// unresolved window silently reports the whole wire space. Retrying at the
    /// point of use removes the timing sensitivity entirely; once resolved this
    /// is a single bool test.
    void ensureWindowResolved() const {
        if (!window_resolved) {
            const_cast<Impl*>(this)->buildChannelWindow(central_instrument);
        }
    }

    /// @brief Map a local (per-device) channel id onto Central's global id
    /// @return the global id, or 0 if this device has no such channel
    uint32_t toGlobalChan(const uint32_t local) const {
        if (local < 1 || local > local_max_chans) return 0;
        // An unresolved window is the identity; a resolved but empty one means
        // the device is not present and nothing is addressable.
        if (!window_resolved) return local;
        for (const auto& s : chan_segs) {
            if (local >= s.local_start && local < s.local_start + s.count)
                return s.global_start + (local - s.local_start);
        }
        return 0;
    }

    /// @brief Inverse of toGlobalChan()
    ///
    /// Returns 0 when the global id belongs to a different instrument, which is
    /// how packets for other devices are recognised and ignored.
    uint32_t toLocalChan(const uint32_t global) const {
        if (global < 1) return 0;
        if (!window_resolved) return global <= local_max_chans ? global : 0;
        for (const auto& s : chan_segs) {
            if (global >= s.global_start && global < s.global_start + s.count)
                return s.local_start + (global - s.global_start);
        }
        return 0;
    }

    /// @brief Build the local->global window for this instrument
    ///
    /// The mapping cannot be computed from compile-time constants.  Central
    /// packs the connected instruments densely into one channel space and
    /// renumbers them whenever that set changes: with two hubs the NSP's
    /// analog inputs start at 513, with one hub at 257, with none at 1.  So the
    /// window is discovered at runtime from procinfo, which Central maintains.
    ///
    /// Each instrument's channels are contiguous, so the window is a single
    /// range: local 1..chancount maps to global base..base+chancount-1, where
    /// base is 1 plus the running sum of the preceding instruments' chancount.
    /// A device reporting chancount 0 is not present.
    void buildChannelWindow(const uint32_t instrument) {
        chan_segs.clear();
        window_resolved = false;
        local_max_chans = cbMAXCHANS;
        if (!shmem_session) return;

        const uint32_t max_procs = std::max<uint32_t>(shmem_session->getMaxProcs(), 1);
        if (instrument >= max_procs) return;

        uint32_t base = 1;
        uint32_t count = 0;
        uint32_t total = 0;
        for (uint32_t i = 0; i < max_procs; ++i) {
            auto pi = shmem_session->getProcInfoAt(i);
            const uint32_t cc = pi.isOk() ? pi.value().chancount : 0;
            total += cc;
            if (i < instrument) base += cc;
            else if (i == instrument) count = cc;
        }

        // Every instrument reporting zero means procinfo has not been populated
        // yet rather than that nothing is connected.  Leave the window
        // unresolved so the session degrades to the wire space instead of
        // having no addressable channels at all.
        if (total == 0) return;

        // Resolved.  A zero count here means this instrument really is absent,
        // so the window stays empty and every channel id is rejected — a
        // session for a disconnected device must not read another one's data.
        window_resolved = true;
        if (count == 0) {
            local_max_chans = 0;
            return;
        }
        chan_segs.push_back({1, count, base});
        local_max_chans = count;
    }

    /// @brief Resolve the channel window, retrying briefly if procinfo lags
    ///
    /// procinfo.chancount only becomes meaningful once the device has answered
    /// REQCONFIGALL (STANDALONE) or the owner has published it (CLIENT), and
    /// both can lag the point where a session is otherwise ready. Observed on a
    /// Gemini NSP, which resolved on some session opens and not others.
    /// An unresolved window silently reports the whole wire space, so it is
    /// worth a bounded wait here rather than a wrong answer for the session's
    /// lifetime.
    void resolveChannelWindow(const int attempts = 20) {
        for (int i = 0; i < attempts; ++i) {
            buildChannelWindow(central_instrument);
            if (window_resolved) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Channel type cache — pre-computed at config time, avoids per-packet getChanInfo() (Phase 3, Fix 10)
    std::array<ChannelType, cbMAXCHANS> channel_type_cache;
    bool channel_cache_valid = false;

    // CMP (channel mapping) overlay. Keyed by cmpKey(bank, term) →
    // {x, y, size, headstage, label}. Loaded rarely, read on every CHANREP.
    CmpEntries cmp_entries;
    std::mutex cmp_mutex;

    void rebuildChannelTypeCache() {
        channel_type_cache.fill(ChannelType::ANY);
        for (uint32_t ch = 0; ch < local_max_chans; ++ch) {
            auto ci = getChanInfo(ch + 1);
            channel_type_cache[ch] = ci.isOk() ? classifyChannelByCaps(ci.value()) : ChannelType::ANY;
        }
        channel_cache_valid = true;
    }

    // Helper: get chaninfo for a 1-based *local* channel ID
    Result<cbPKT_CHANINFO> getChanInfo(const uint32_t chan_id) const {
        // Reject ids this device does not have before choosing a backing
        // store, so the answer cannot depend on which one replies.  A hub has
        // no I/O channels, but the wire layout still reserves slots at 257..,
        // and the device happily returns the empty chaninfo sitting there.
        ensureWindowResolved();
        const uint32_t global = toGlobalChan(chan_id);
        if (!global) {
            return Result<cbPKT_CHANINFO>::error("Channel not present on this device");
        }
        // Prefer shmem over device_config because CMP position overlays are
        // written to shmem (device_config is owned by the receive thread and
        // we avoid writing to it from other threads).
        //
        // chaninfo[] is indexed in Central's global space, so the translated id
        // is the right index there; device_session talks to the device
        // directly, which numbers channels locally.
        if (shmem_session) {
            return shmem_session->getChanInfo(global - 1);
        }
        // Fallback to device_config (no shmem available)
        if (device_session) {
            const auto* chaninfo = device_session->getChanInfo(chan_id);
            if (!chaninfo) {
                return Result<cbPKT_CHANINFO>::error("Failed to get channel information");
            }
            return Result<cbPKT_CHANINFO>::ok(*chaninfo);
        }
        return Result<cbPKT_CHANINFO>::error("Channel information not available");
    }

    // Clock sync periodic probing
    std::chrono::steady_clock::time_point last_clock_probe_time{};

    // CLIENT-mode clock sync (used when no device_session is available)
    cbdev::ClockSync client_clock_sync;

    // Peer-device clock sync readers.  When this device (NSP) has unreliable
    // probes, it borrows a peer HUB's clock offset from the HUB's shared memory
    // config segment.  All known HUB segments are kept open and refreshed each
    // cycle; the borrowed offset is sanity-checked inside ClockSync before use.
    struct PeerHub {
        std::string segment;
        std::unique_ptr<PeerClockReader> reader;
    };
    std::vector<PeerHub> peer_hubs;
    bool peer_hubs_init = false;
    struct PendingClockProbe {
        std::chrono::steady_clock::time_point t1_local;
        bool active = false;
    };
    PendingClockProbe pending_clock_probe;
    std::mutex clock_probe_mutex;

    // CENTRAL-mode cross-instrument clock estimates.
    //
    // A STANDALONE owner borrows a peer's offset through the peer's own shared
    // memory (see peer_hubs above) -- notably a Gemini NSP borrowing a hub,
    // because the NSP transmits in ~8192-sample bursts and its probe replies
    // are latched to those boundaries, quantising every offset by up to 273 ms.
    // Under CENTRAL there is no peer CereLink process publishing anything, so
    // that mechanism finds nothing and the NSP is stuck with its own unusable
    // probes.
    //
    // Central's ring already carries every instrument's packets though, so the
    // estimates can be built here instead: the packet observer sees the other
    // instruments' probe replies before the demux filter discards them, and the
    // same vote/borrow rules are applied to the result. The library resolves
    // this itself -- callers should never have to assemble a clock estimate.
    struct PeerInstrumentClock {
        cbdev::ClockSync sync;
        PendingClockProbe pending;
    };
    std::map<uint32_t, PeerInstrumentClock> peer_instrument_clocks;
    std::mutex peer_instrument_mutex;

    /// @brief Feed a probe reply belonging to another instrument
    ///
    /// Called from the packet observer, i.e. on the reading thread, for replies
    /// the demux filter is about to discard.
    void addPeerProbeReply(uint32_t instrument, uint64_t device_time_ns,
                           std::chrono::steady_clock::time_point t4) {
        std::lock_guard<std::mutex> lk(peer_instrument_mutex);
        auto it = peer_instrument_clocks.find(instrument);
        if (it == peer_instrument_clocks.end() || !it->second.pending.active) return;
        it->second.sync.addProbeSample(it->second.pending.t1_local, device_time_ns, t4);
        it->second.pending.active = false;
    }

    /// @brief Vote across instruments and commit the result to this session
    ///
    /// Mirrors the STANDALONE rules deliberately, so the two modes agree:
    /// three or more independent estimates elect a median; below that an
    /// instrument with no usable estimate of its own borrows the
    /// lowest-uncertainty peer. ClockSync::setExternalOffset sanity-checks the
    /// value before adopting it, which is what keeps this safe on hardware
    /// whose instruments are not commonly disciplined -- a peer that disagrees
    /// is rejected rather than believed.
    void applyCrossInstrumentConsensus() {
        std::vector<int64_t> votes;
        std::optional<int64_t> best_peer_offset;
        std::optional<int64_t> best_peer_uncert;
        int64_t best_uncert_val = INT64_MAX;
        {
            std::lock_guard<std::mutex> lk(peer_instrument_mutex);
            for (auto& [inst, pc] : peer_instrument_clocks) {
                auto off = pc.sync.getOffsetNs();
                if (!off) continue;
                votes.push_back(*off);
                const int64_t unc = pc.sync.getUncertaintyNs().value_or(INT64_MAX);
                if (!best_peer_offset || unc < best_uncert_val) {
                    best_peer_offset = off;
                    best_peer_uncert = pc.sync.getUncertaintyNs();
                    best_uncert_val = unc;
                }
            }
        }
        const auto own = client_clock_sync.getOffsetNs();
        if (own) votes.push_back(*own);

        if (votes.size() >= 3) {
            std::sort(votes.begin(), votes.end());
            client_clock_sync.setExternalOffset(votes[votes.size() / 2]);
        } else if (best_peer_offset) {
            client_clock_sync.setExternalOffset(best_peer_offset, best_peer_uncert);
        } else {
            client_clock_sync.setExternalOffset(std::nullopt);
        }
    }

    /// Instruments present in Central, discovered from procinfo.chancount.
    std::vector<uint32_t> presentInstruments() const {
        std::vector<uint32_t> out;
        if (!shmem_session) return out;
        const uint32_t max_procs = std::max<uint32_t>(shmem_session->getMaxProcs(), 1);
        for (uint32_t i = 0; i < max_procs; ++i) {
            auto pi = shmem_session->getProcInfoAt(i);
            if (pi.isOk() && pi.value().chancount > 0) out.push_back(i);
        }
        return out;
    }

    // Per-stream monotonic-conversion state (see toLocalTimeBatch).  Each stream
    // keeps its own non-decreasing floor and the discontinuity epoch it last
    // saw; on an epoch change the floor is reset (a genuine clock re-sync), else
    // a backward step is clamped to the floor.  Lazily created per stream_id.
    struct MonoState {
        int64_t  floor_ns  = 0;
        uint64_t last_epoch = 0;
        bool     seen       = false;
    };
    std::mutex mono_mutex;
    std::unordered_map<int64_t, MonoState> mono_streams;

    // CLIENT-mode (shmem) discontinuity-epoch approximation.  The shmem offset
    // path has no local ClockSync, so we derive an epoch by watching the
    // peer/Central offset for jumps larger than a smooth slew (Q1=b — best
    // effort until the shmem layout carries a real epoch field).  Guarded by
    // mono_mutex (only touched from the monotonic conversion path).
    std::optional<int64_t> client_epoch_offset;
    uint64_t client_epoch = 0;

    uint64_t deriveClientEpoch(int64_t offset_ns) {  // mono_mutex held
        constexpr int64_t kStepNs = 50'000'000;  // ~ClockSync slew_max_ns
        if (client_epoch_offset &&
            std::llabs(offset_ns - *client_epoch_offset) > kStepNs)
            ++client_epoch;
        client_epoch_offset = offset_ns;
        return client_epoch;
    }

    // Current (offset, epoch) from whichever source getClockOffsetNs() would
    // use, read together so a batch converts against one consistent regime.
    std::optional<std::pair<int64_t, uint64_t>> currentOffsetAndEpoch() {  // mono_mutex held
        if (device_session) {
            auto off = device_session->getOffsetNs();
            if (off) return std::make_pair(*off, device_session->syncEpoch());
            return std::nullopt;
        }
        if (shmem_session) {
            auto off = shmem_session->getClockOffsetNs();
            if (off) return std::make_pair(*off, deriveClientEpoch(*off));
        }
        auto off = client_clock_sync.getOffsetNs();
        if (off) return std::make_pair(*off, client_clock_sync.syncEpoch());
        return std::nullopt;
    }

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
        std::atomic<uint64_t> shmem_overruns{0};

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
            shmem_overruns.store(0, std::memory_order_relaxed);
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
            s.shmem_overruns = shmem_overruns.load(std::memory_order_relaxed);
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

    /// Apply CMP overlay (position + label) to every channel whose device
    /// (bank, term) matches a loaded CMP entry. Called after loading a CMP
    /// file. Writes the updated chaninfo to shmem. Label push to the device
    /// is done separately by SdkSession::loadChannelMap.
    void applyCmpToAllChannels() {
        std::lock_guard<std::mutex> lock(cmp_mutex);
        if (cmp_entries.empty()) return;

        for (uint32_t chan_id = 1; chan_id <= local_max_chans; ++chan_id) {
            // Snapshot chaninfo to avoid racing with the receive thread
            cbPKT_CHANINFO ci{};
            bool valid = false;
            if (device_session) {
                const auto* p = device_session->getChanInfo(chan_id);
                if (p && p->chan > 0) { ci = *p; valid = true; }
            } else if (shmem_session) {
                auto r = shmem_session->getChanInfo(chan_id - 1);
                if (r.isOk() && r.value().chan > 0) { ci = r.value(); valid = true; }
            }
            if (!valid) continue;

            // Match the CMP row by the channel's own (bank, term).
            auto it = cmp_entries.find(cmpKey(ci.bank, ci.term));
            if (it == cmp_entries.end()) continue;

            ci.position[0] = it->second.x;
            ci.position[1] = it->second.y;
            ci.position[2] = it->second.size;
            ci.position[3] = it->second.headstage;
            std::strncpy(ci.label, it->second.label.c_str(), sizeof(ci.label) - 1);
            ci.label[sizeof(ci.label) - 1] = '\0';

            if (shmem_session) {
                shmem_session->setChanInfo(chan_id - 1, ci);
            }
        }
    }

    /// Dispatch a batch of packets: first fire batch group callbacks, then per-packet callbacks.
    /// Called from both STANDALONE callback thread and CLIENT shmem receive thread.
    void dispatchBatch(cbPKT_GENERIC* packets, size_t count) {
        // Central stamps packet chids in its global space (Hub2's channel 1
        // arrives as 257), so rewrite them into this device's local space
        // before any dispatch.  Doing it here covers the batch and per-packet
        // paths at once and keeps callbacks identical to STANDALONE.
        //
        // An identity map — NATIVE, STANDALONE, or a single-instrument
        // Central — skips the loop entirely, so the hot path is unaffected.
        if (!chan_segs.empty()) {
            for (size_t i = 0; i < count; i++) {
                const uint16_t chid = packets[i].cbpkt_header.chid;
                // chid 0 is a sample group and the high bit marks configuration
                // packets; neither is a channel id.
                if (chid == 0 || (chid & cbPKTCHAN_CONFIGURATION)) continue;
                const uint32_t local = toLocalChan(chid);
                // 0 means the channel belongs to another instrument, which the
                // receive-buffer instrument filter should already have dropped.
                // Leave it alone rather than rewriting it to 0, which would
                // masquerade as a sample group.
                if (local) packets[i].cbpkt_header.chid = static_cast<uint16_t>(local);
            }
        }

        // Phase 1: batch group callbacks (one invocation per group_id per batch)
        std::vector<GroupBatchCB> snap_batch;
        {
            std::lock_guard<std::mutex> lock(user_callback_mutex);
            snap_batch = group_batch_callbacks;
        }

        if (!snap_batch.empty()) {
            // Temp buffers — sized for max batch (128 packets × 272 channels)
            // ~70KB on stack, well within typical thread stack limits.
            int16_t sample_buf[128 * cbNUM_ANALOG_CHANS];
            uint64_t ts_buf[128];

            for (const auto& bcb : snap_batch) {
                size_t n = 0;
                size_t n_channels = 0;

                for (size_t i = 0; i < count; i++) {
                    if (packets[i].cbpkt_header.chid == 0 &&
                        packets[i].cbpkt_header.type == bcb.group_id) {
                        const auto& grp = reinterpret_cast<const cbPKT_GROUP&>(packets[i]);
                        size_t nc = static_cast<size_t>(grp.cbpkt_header.dlen) * 2;
                        if (nc == 0) continue;
                        if (n == 0) n_channels = nc;
                        else if (nc != n_channels) continue;  // skip mismatched (shouldn't happen)
                        std::memcpy(&sample_buf[n * n_channels], grp.data, n_channels * sizeof(int16_t));
                        ts_buf[n] = grp.cbpkt_header.time;
                        n++;
                    }
                }

                if (n > 0 && bcb.cb) {
                    bcb.cb(sample_buf, n, n_channels, ts_buf);
                }
            }
        }

        // Phase 2: per-packet dispatch (existing behavior, unchanged)
        for (size_t i = 0; i < count; i++) {
            dispatchPacket(packets[i]);
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
            if (channel_cache_valid && chid >= 1 && chid <= local_max_chans) {
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
        // No CENTRAL instrument mapping (e.g. NPLAY). -1 yields an invalid
        // InstrumentId, so CENTRAL-mode create() fails and the caller falls
        // back to NATIVE. This is NOT a "match all instruments" sentinel.
        default:                     return -1;
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

    // Device token used to build NATIVE segment names. The SDK always targets
    // Central's primary instance, so the CENTRAL instance suffix is empty.
    std::string device_tag = getNativeDeviceName(config.device_type);

    // --- Attempt 1: Central-compatible CLIENT mode ---
    // Try to attach to Central's shared memory (Central is running)
    auto inst = cbproto::InstrumentId::fromIndex(getCentralInstrumentIndex(config.device_type));
    auto shmem_result = cbshm::ShmemSession::create(
        cbshm::Mode::CLIENT, cbshm::ShmemLayout::CENTRAL, /*instance=*/"", inst);

    if (shmem_result.isError()) {
        // --- Attempt 2: Native CLIENT mode ---
        // Try to attach to an existing CereLink STANDALONE's native segments
        shmem_result = cbshm::ShmemSession::create(
            cbshm::Mode::CLIENT, cbshm::ShmemLayout::NATIVE, device_tag,
            cbproto::InstrumentId::fromIndex(0));

        // Liveness check: reject stale segments from a dead STANDALONE process.
        // The ShmemSession destructor (triggered by reassignment) unmaps the segments;
        // the subsequent STANDALONE creation path will shm_unlink + recreate them.
        if (shmem_result.isOk() && !shmem_result.value().isOwnerAlive()) {
            shmem_result = cbshm::Result<cbshm::ShmemSession>::error(
                "Stale shared memory detected (owner process dead)");
        }

        if (shmem_result.isError()) {
            // --- Attempt 3: Native STANDALONE mode ---
            // No existing shared memory found, create new native-mode segments
            shmem_result = cbshm::ShmemSession::create(
                cbshm::Mode::STANDALONE, cbshm::ShmemLayout::NATIVE, device_tag,
                cbproto::InstrumentId::fromIndex(0));

            if (shmem_result.isError()) {
                return Result<SdkSession>::error("Failed to create shared memory: " + shmem_result.error());
            }
            is_standalone = true;
        }
    }

    session.m_impl->shmem_session = std::move(shmem_result.value());
    session.m_impl->standalone = is_standalone;

    // Establish this session's local->global channel window.  NATIVE and
    // STANDALONE own a single instrument, so index 0 is theirs.
    //
    // For CENTRAL the config is already sitting in shared memory, so this
    // resolves immediately.  A STANDALONE session has not downloaded the
    // device's config yet and will resolve to the identity map here; it is
    // rebuilt after the handshake, once procinfo is populated.
    {
        // Only the CENTRAL layout aggregates instruments, so only there does
        // the device type select a non-zero index.  NATIVE shared memory holds
        // a single instrument whichever device produced it -- including for a
        // NATIVE CLIENT, which is not standalone but is still single-instrument.
        const int32_t inst_idx = getCentralInstrumentIndex(config.device_type);
        const bool central_layout =
            session.m_impl->shmem_session->getLayout() == cbshm::ShmemLayout::CENTRAL;
        session.m_impl->central_instrument =
            central_layout && inst_idx >= 0 ? static_cast<uint32_t>(inst_idx) : 0u;
        session.m_impl->buildChannelWindow(session.m_impl->central_instrument);
    }

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
    } else {
        // CLIENT mode - start the shmem receive thread (no device session needed)
        auto start_result = session.start();
        if (start_result.isError()) {
            return Result<SdkSession>::error("Failed to start CLIENT session: " + start_result.error());
        }
        // Resolve the channel window before the type cache, which it sizes.
        session.m_impl->resolveChannelWindow();
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

                    // Dispatch batch (fires batch group callbacks, then per-packet callbacks)
                    impl->dispatchBatch(packets, count);
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

                // Track configuration replies so requestConfiguration() can
                // recognise a config dump that arrived without its terminator.
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANREP
                    || pkt.cbpkt_header.type == cbPKTTYPE_PROCREP
                    || pkt.cbpkt_header.type == cbPKTTYPE_GROUPREP) {
                    impl->config_replies.fetch_add(1, std::memory_order_relaxed);
                }

                // Check for SYSREP packets (handshake responses)
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                    const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
                    impl->updateRunlevel(sysinfo->runlevel);
                    if (pkt.cbpkt_header.type == cbPKTTYPE_SYSREPRUNLEV) {
                        impl->received_sysrepRunlev.store(true, std::memory_order_release);
                    }
                    impl->received_sysrep.store(true, std::memory_order_release);
                    impl->handshake_cv.notify_all();
                }

                // Store to shared memory
                auto store_result = impl->shmem_session->storePacket(pkt);

                // Mirror config reply packets to shmem so CLIENT processes
                // can read device configuration (chaninfo, procinfo, sysinfo, groupinfo).
                if (pkt.cbpkt_header.type == cbPKTTYPE_PROCREP) {
                    const auto* procinfo = reinterpret_cast<const cbPKT_PROCINFO*>(&pkt);
                    impl->shmem_session->setProcInfo(*procinfo);
                    impl->shmem_session->setGeminiSystem(cbproto::procInfoIsGemini(*procinfo));
                }
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                    const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
                    impl->shmem_session->setSysInfo(*sysinfo);
                }
                if (pkt.cbpkt_header.type == cbPKTTYPE_GROUPREP) {
                    const auto* groupinfo = reinterpret_cast<const cbPKT_GROUPINFO*>(&pkt);
                    if (groupinfo->group >= 1 && groupinfo->group <= cbMAXGROUPS) {
                        impl->shmem_session->setGroupInfo(groupinfo->group - 1, *groupinfo);
                    }
                }
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANREP) {
                    auto chaninfo_copy = *reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
                    // Apply CMP overlay (position + label) before writing to
                    // shmem so locally-supplied geometry and labels survive
                    // even when the device sends a fresh CHANREP.
                    {
                        std::lock_guard<std::mutex> lock(impl->cmp_mutex);
                        if (!impl->cmp_entries.empty()) {
                            auto it = impl->cmp_entries.find(
                                cmpKey(chaninfo_copy.bank, chaninfo_copy.term));
                            if (it != impl->cmp_entries.end()) {
                                chaninfo_copy.position[0] = it->second.x;
                                chaninfo_copy.position[1] = it->second.y;
                                chaninfo_copy.position[2] = it->second.size;
                                chaninfo_copy.position[3] = it->second.headstage;
                                std::strncpy(chaninfo_copy.label,
                                             it->second.label.c_str(),
                                             sizeof(chaninfo_copy.label) - 1);
                                chaninfo_copy.label[sizeof(chaninfo_copy.label) - 1] = '\0';
                            }
                        }
                    }
                    if (chaninfo_copy.chan >= 1 && chaninfo_copy.chan <= cbMAXCHANS) {
                        impl->shmem_session->setChanInfo(chaninfo_copy.chan - 1, chaninfo_copy);
                    }
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
                if (now - impl->last_clock_probe_time > std::chrono::milliseconds(100)) {
                    impl->device_session->sendClockProbe();
                    impl->last_clock_probe_time = now;
                }

                // Cross-device clock consensus.  Devices that share one PTP
                // clock should report the same device->host offset.  Each device
                // publishes its own (independent) estimate; here we combine this
                // device's estimate with every peer's and use the median, so a
                // transiently-biased device is outvoted instead of skewing time
                // conversion.  All participants read the same set of published
                // estimates and therefore converge on the same median.
                // Consensus needs >=3 participants to reject one outlier; with
                // fewer, an NSP still borrows a HUB's offset (its own probes are
                // unreliable) and other device types keep their own estimate.
                //
                // Only Gemini devices (NSP + HUBs) share one PTP clock.
                // Non-Gemini devices (legacy NSP, nPlay, custom) have
                // independent clocks and must not be averaged together, so they
                // skip consensus/borrow entirely.
                const DeviceType self_type = impl->config.device_type;
                const bool shares_ptp_clock =
                    self_type == DeviceType::NSP  ||
                    self_type == DeviceType::HUB1 || self_type == DeviceType::HUB2 ||
                    self_type == DeviceType::HUB3;
                if (shares_ptp_clock) {
                    if (!impl->peer_hubs_init) {
                        impl->peer_hubs_init = true;
                        for (auto dt : {DeviceType::NSP, DeviceType::HUB1,
                                        DeviceType::HUB2, DeviceType::HUB3}) {
                            if (dt == impl->config.device_type)
                                continue;  // skip self
                            impl->peer_hubs.push_back(
                                {getNativeSegmentName(dt, "config"),
                                 std::make_unique<PeerClockReader>()});
                        }
                    }

                    // Collect peer votes; track the lowest-uncertainty peer for
                    // the <3-participant fallback.
                    std::vector<int64_t> votes;
                    std::optional<int64_t> best_peer_offset;
                    std::optional<int64_t> best_peer_uncert;
                    int64_t best_peer_uncert_val = INT64_MAX;
                    for (auto& ph : impl->peer_hubs) {
                        if (!ph.reader->isOpen())
                            ph.reader->tryOpen(ph.segment);
                        // Vote on the peer's own (pre-consensus) estimate so the
                        // median can track real common-mode drift.
                        auto offset = ph.reader->getRawOffsetNs();
                        if (!offset)
                            continue;
                        votes.push_back(*offset);
                        auto uncert = ph.reader->getClockUncertaintyNs();
                        const int64_t uncert_val = uncert ? *uncert : INT64_MAX;
                        if (!best_peer_offset || uncert_val < best_peer_uncert_val) {
                            best_peer_offset = offset;
                            best_peer_uncert = uncert;
                            best_peer_uncert_val = uncert_val;
                        }
                    }
                    // This device's own independent vote.
                    if (auto own = impl->device_session->getInternalOffsetNs())
                        votes.push_back(*own);

                    if (votes.size() >= 3) {
                        std::sort(votes.begin(), votes.end());
                        const int64_t median = votes[votes.size() / 2];
                        impl->device_session->setExternalClockOffset(median);
                    } else if (impl->config.device_type == DeviceType::NSP &&
                               best_peer_offset) {
                        // Too few for consensus: a Gemini NSP still borrows a HUB.
                        impl->device_session->setExternalClockOffset(best_peer_offset, best_peer_uncert);
                    } else {
                        impl->device_session->setExternalClockOffset(std::nullopt);
                    }
                }

                // Publish two offsets: the committed (post-consensus) value in
                // clock_offset_ns for CLIENT-mode readers, and this device's own
                // (pre-consensus) estimate in clock_raw_offset_ns for peers to
                // vote on.
                {
                    auto uncertainty = impl->device_session->getUncertaintyNs().value_or(0);
                    if (auto committed = impl->device_session->getOffsetNs())
                        impl->shmem_session->setClockSync(*committed, uncertainty);
                    if (auto internal = impl->device_session->getInternalOffsetNs())
                        impl->shmem_session->setClockRawOffset(*internal);
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
#ifdef _WIN32
                bool timer_raised = false;
#endif
                while (true) {
                    cbPKT_GENERIC pkt = {};

                    // Dequeue packet from shared memory transmit buffer
                    auto result = impl->shmem_session->dequeuePacket(pkt);
                    if (result.isError() || !result.value()) {
                        break;  // Error or no more packets
                    }

                    has_packets = true;
#ifdef _WIN32
                    if (!timer_raised) { timeBeginPeriod(1); timer_raised = true; }
#endif

                    // Send packet to device
                    auto send_result = impl->device_session->sendPacket(pkt);
                    if (send_result.isError()) {
                        impl->stats.send_errors.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        impl->stats.packets_sent_to_device.fetch_add(1, std::memory_order_relaxed);
                    }

                    // Pace sends to avoid overflowing the device's kernel
                    // UDP receive buffer (~8 KB on Windows = ~8 packets).
                    if ((impl->stats.packets_sent_to_device.load(std::memory_order_relaxed) % 8) == 0) {
#ifdef _WIN32
                        Sleep(1);
#else
                        std::this_thread::sleep_for(std::chrono::microseconds(50));
#endif
                    }
                }
#ifdef _WIN32
                if (timer_raised) timeEndPeriod(1);
#endif

                if (!has_packets) {
                    // No packets - wait briefly before checking again
                    hr_sleep_us(100);
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

        // Send initial clock probe immediately after handshake so clock
        // offset is available as soon as possible (don't wait for the
        // periodic 2-second timer to fire).
        if (handshake_result.isOk() && m_impl->device_session) {
            m_impl->device_session->sendClockProbe();
            m_impl->last_clock_probe_time = std::chrono::steady_clock::now();
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

        // Catch the other instruments' probe replies before the demux filter
        // discards them, so this session can build a cross-device estimate
        // without a peer CereLink process to borrow one from. Only replies are
        // taken; the filter still decides what the session actually receives,
        // so no foreign data reaches callbacks.
        if (m_impl->shmem_session->getLayout() == cbshm::ShmemLayout::CENTRAL) {
            const uint32_t self_inst = m_impl->shmem_session->getInstrument().toIndex();
            m_impl->shmem_session->setPacketObserver(
                [impl, self_inst](const cbPKT_GENERIC& pkt) {
                    if (pkt.cbpkt_header.type != cbPKTTYPE_NPLAYREP) return;
                    const uint32_t inst = pkt.cbpkt_header.instrument;
                    if (inst == self_inst) return;  // own reply: handled below
                    constexpr uint64_t STALENESS_CORRECTION_NS = 165000;
                    impl->addPeerProbeReply(inst,
                                            pkt.cbpkt_header.time + STALENESS_CORRECTION_NS,
                                            std::chrono::steady_clock::now());
                });
        }

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
                        // Separate "we lost data" from "the read could not run at
                        // all".  ShmemSession marks the former (overrun / desync)
                        // with "data lost"; the latter (not open, bad args) has no
                        // effect on the stream.  packets_dropped is NOT reused --
                        // that counts callback-queue overflow.
                        if (read_result.error().find("data lost") != std::string::npos) {
                            impl->stats.shmem_overruns.fetch_add(1, std::memory_order_relaxed);
                        }
                        std::lock_guard<std::mutex> lock(impl->user_callback_mutex);
                        if (impl->error_callback) {
                            impl->error_callback("Error reading from shared memory: " + read_result.error());
                        }
                        had_error = true;
                        break;
                    }

                    if (packets_read > 0) {
                        auto t4 = std::chrono::steady_clock::now();
                        uint64_t last_data_time = 0;
                        impl->stats.packets_delivered_to_callback.fetch_add(packets_read, std::memory_order_relaxed);
                        impl->stats.packets_received_from_device.fetch_add(packets_read, std::memory_order_relaxed);
                        uint64_t batch_bytes = 0;
                        // CLIENT mode: scan packets for clock sync replies and CMP overlays
                        for (size_t i = 0; i < packets_read; i++) {
                            batch_bytes += static_cast<uint64_t>(
                                cbPKT_HEADER_32SIZE + packets[i].cbpkt_header.dlen) * 4;
                            // Remember the newest data-packet timestamp for the
                            // fallback estimator (fed once per batch below).
                            if (!(packets[i].cbpkt_header.chid & cbPKTCHAN_CONFIGURATION) &&
                                packets[i].cbpkt_header.time != 0) {
                                last_data_time = packets[i].cbpkt_header.time;
                            }
                            if (packets[i].cbpkt_header.type == cbPKTTYPE_NPLAYREP) {
                                // Complete pending clock sync probe
                                constexpr uint64_t STALENESS_CORRECTION_NS = 165000;
                                std::lock_guard<std::mutex> lock(impl->clock_probe_mutex);
                                if (impl->pending_clock_probe.active) {
                                    // Use header.time as T3 — already converted to ns
                                    // by readReceiveBuffer for all device types.
                                    // Add staleness correction (header.time is from the
                                    // device's previous main-loop iteration).
                                    uint64_t device_time_ns = packets[i].cbpkt_header.time
                                                              + STALENESS_CORRECTION_NS;
                                    impl->client_clock_sync.addProbeSample(
                                        impl->pending_clock_probe.t1_local,
                                        device_time_ns, t4);
                                    impl->pending_clock_probe.active = false;
                                }
                            }
                            // Check for SYSREP packets (handshake responses)
                            if ((packets[i].cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                                const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&packets[i]);
                                impl->updateRunlevel(sysinfo->runlevel);
                                if (packets[i].cbpkt_header.type == cbPKTTYPE_SYSREPRUNLEV) {
                                    impl->received_sysrepRunlev.store(true, std::memory_order_release);
                                }
                                impl->received_sysrep.store(true, std::memory_order_release);
                                impl->handshake_cv.notify_all();
                            }
                            // Note: CMP positions are applied by the STANDALONE session
                            // when it writes chaninfo to shmem. CLIENT doesn't need to
                            // re-apply them.
                        }
                        impl->stats.bytes_received_from_device.fetch_add(
                            batch_bytes, std::memory_order_relaxed);

                        // Feed the fallback estimator once per batch, mirroring
                        // DeviceSession. This is the only recourse for a device
                        // whose probe replies are latched to transmit blocks
                        // (a Gemini NSP quantises every offset by up to 273 ms)
                        // when there is no peer instrument to borrow from --
                        // an NSP alone under Central. No tick->ns conversion is
                        // needed here: readReceiveBuffer already normalises
                        // CLIENT timestamps for every device type.
                        //
                        // Selection order makes this self-limiting: reliable
                        // probes still win, so a device with good probes is
                        // unaffected and only one with unusable probes gains
                        // the floor.
                        if (last_data_time != 0) {
                            impl->client_clock_sync.addDataPacketSample(last_data_time, t4);
                        }

                        // Periodic clock sync probing (~every 5 seconds)
                        if (t4 - impl->last_clock_probe_time > std::chrono::seconds(2)) {
                            // Build and enqueue probe via shmem xmt buffer.
                            // Can't call sendClockProbe() (needs SdkSession*), so inline it.
                            {
                                std::lock_guard<std::mutex> lock(impl->clock_probe_mutex);
                                impl->pending_clock_probe.t1_local = t4;
                                impl->pending_clock_probe.active = true;
                            }
                            cbPKT_NPLAY probe{};
                            probe.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
                            probe.cbpkt_header.type = cbPKTTYPE_NPLAYSET;
                            probe.cbpkt_header.dlen = cbPKTDLEN_NPLAY;
                            // Address the probe to this session's instrument so
                            // the fabric routes it to the correct hub and the
                            // echoed NPLAYREP carries the matching instrument
                            // index — otherwise a non-index-0 session's reply is
                            // dropped by readReceiveBuffer's instrument filter.
                            // Use the session's bound instrument (0 for NATIVE)
                            // so the probe and the filter compare the same value
                            // by construction.
                            probe.cbpkt_header.instrument =
                                impl->shmem_session->getInstrument().toPacketField();
                            probe.mode = 0xFFFF;
                            probe.stime = static_cast<uint64_t>(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    t4.time_since_epoch()).count());
                            // Stamp with current device time (enqueuePacket needs non-zero time)
                            PROCTIME last_t = impl->shmem_session->getLastTime();
                            probe.cbpkt_header.time = (last_t != 0) ? last_t : 1;
                            impl->shmem_session->enqueuePacket(
                                *reinterpret_cast<const cbPKT_GENERIC*>(&probe));
                            impl->last_clock_probe_time = t4;

                            // CENTRAL: also probe the other instruments, so this
                            // session can build its own cross-device estimate.
                            // Their replies come back on the same ring and are
                            // caught by the packet observer before the demux
                            // filter drops them.
                            if (impl->shmem_session->getLayout() == cbshm::ShmemLayout::CENTRAL) {
                                const uint32_t self = impl->shmem_session->getInstrument().toIndex();
                                for (uint32_t inst : impl->presentInstruments()) {
                                    if (inst == self) continue;
                                    {
                                        std::lock_guard<std::mutex> lk(impl->peer_instrument_mutex);
                                        auto& pc = impl->peer_instrument_clocks[inst];
                                        pc.pending.t1_local = t4;
                                        pc.pending.active = true;
                                    }
                                    cbPKT_NPLAY peer_probe = probe;
                                    peer_probe.cbpkt_header.instrument =
                                        cbproto::InstrumentId::fromIndex(
                                            static_cast<int32_t>(inst)).toPacketField();
                                    impl->shmem_session->enqueuePacket(
                                        *reinterpret_cast<const cbPKT_GENERIC*>(&peer_probe));
                                }
                                impl->applyCrossInstrumentConsensus();
                            }
                        }

                        // Dispatch batch (fires batch group callbacks, then per-packet callbacks)
                        impl->dispatchBatch(packets, packets_read);
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

CallbackHandle SdkSession::registerGroupBatchCallback(const SampleRate rate, GroupBatchCallback callback) const {
    const uint8_t group_id = static_cast<uint8_t>(rate);
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->group_batch_callbacks.push_back({handle, group_id, std::move(callback)});
    return handle;
}

CallbackHandle SdkSession::registerConfigCallback(const uint16_t packet_type, ConfigCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->config_callbacks.push_back({handle, packet_type, std::move(callback)});
    return handle;
}

CallbackHandle SdkSession::registerRunlevelChangeCallback(RunlevelCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    const auto handle = m_impl->next_callback_handle++;
    m_impl->runlevel_callbacks.push_back({handle, std::move(callback)});
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
    erase_by_handle(m_impl->group_batch_callbacks);
    erase_by_handle(m_impl->config_callbacks);
    erase_by_handle(m_impl->runlevel_callbacks);
}

void SdkSession::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->user_callback_mutex);
    m_impl->error_callback = std::move(callback);
}

SdkStats SdkSession::getStats() const {
    SdkStats stats = m_impl->stats.snapshot();
    stats.queue_current_depth = m_impl->packet_queue.size();
    // Live value, like queue_current_depth: the producer owns this counter in the
    // ring, so a CLIENT can compare it against packets_received to size the loss
    // an overrun cost it.  Exact only for a single-instrument ring -- a Central
    // ring counts every instrument's packets, so treat it as an upper bound.
    if (m_impl->shmem_session) {
        auto produced = m_impl->shmem_session->getReceivedPacketCount();
        if (produced.isOk()) {
            stats.packets_produced = produced.value();
        }
    }
    return stats;
}

void SdkSession::resetStats() {
    m_impl->stats.reset();
}

const SdkConfig& SdkSession::getConfig() const {
    return m_impl->config;
}

Result<cbPKT_SYSINFO> SdkSession::getSysInfo() const {
    if (m_impl->device_session)
        return Result<cbPKT_SYSINFO>::ok(m_impl->device_session->getSysInfo());
    if (m_impl->shmem_session)
        return m_impl->shmem_session->getSysInfo();
    return Result<cbPKT_SYSINFO>::error("System information not available");
}

Result<cbPKT_CHANINFO> SdkSession::getChanInfo(const uint32_t chan_id) const {
    return m_impl->getChanInfo(chan_id);
}

Result<cbPKT_GROUPINFO> SdkSession::getGroupInfo(uint32_t group_id) const {
    if (group_id == 0 || group_id > cbMAXGROUPS)
        return Result<cbPKT_GROUPINFO>::error("Invalid group ID");
    if (m_impl->device_session) {
        const auto& config = m_impl->device_session->getDeviceConfig();
        return Result<cbPKT_GROUPINFO>::ok(config.groupinfo[group_id - 1]);
    }
    if (m_impl->shmem_session) {
        return m_impl->shmem_session->getGroupInfo(group_id - 1);
    }
    return Result<cbPKT_GROUPINFO>::error("Group information not available");
}

uint32_t SdkSession::getGroupChannelList(uint32_t group_id, uint16_t* list, uint32_t max_count) const {
    if (group_id == 0 || group_id > cbMAXGROUPS || !list || max_count == 0)
        return 0;

    uint32_t count = 0;
    const bool is_raw = (group_id == cbRAWGROUP);

    for (uint32_t chan = 1; chan <= cbNUM_ANALOG_CHANS && count < max_count; ++chan) {
        // Prefer device_config (updated in updateConfigFromBuffer before the
        // sendAndWait / SYSREP sync barrier fires).  shmem chaninfo may lag
        // slightly because SDK callbacks run after the sync notification.
        cbPKT_CHANINFO ci{};
        if (m_impl->device_session) {
            const auto* res = m_impl->device_session->getChanInfo(chan);
            if (!res) continue;
            ci = *res;
        } else {
            auto res = getChanInfo(chan);
            if (res.isError()) continue;
            ci = res.value();
        }

        bool in_group;
        if (is_raw)
            in_group = (ci.ainpopts & cbAINP_RAWSTREAM) != 0;
        else
            in_group = (ci.smpgroup == group_id);

        if (in_group)
            list[count++] = static_cast<uint16_t>(chan);
    }
    return count;
}

Result<cbPKT_FILTINFO> SdkSession::getFilterInfo(const uint32_t filter_id) const {
    if (filter_id >= cbMAXFILTS)
        return Result<cbPKT_FILTINFO>::error("Invalid filter ID");
    if (m_impl->device_session) {
        const auto& config = m_impl->device_session->getDeviceConfig();
        return Result<cbPKT_FILTINFO>::ok(config.filtinfo[filter_id]);
    }
    if (m_impl->shmem_session) {
        return m_impl->shmem_session->getFilterInfo(filter_id + 1);
    }
    return Result<cbPKT_FILTINFO>::error("Filter information not available");
}

uint32_t SdkSession::getMaxChans() const {
    m_impl->ensureWindowResolved();
    return m_impl->local_max_chans;
}

uint32_t SdkSession::getRunLevel() const {
    uint32_t rl = m_impl->device_runlevel.load(std::memory_order_acquire);
    if (rl != 0) return rl;
    // Fall back to the SYSINFO mirrored into shmem by the STANDALONE owner.
    // In CLIENT mode the device only emits SYSREP on runlevel-set commands,
    // so this session's receive ring may never see one in steady state.
    auto si = getSysInfo();
    if (si.isOk()) return si.value().runlevel;
    return 0;
}

bool SdkSession::isStandalone() const {
    return m_impl && m_impl->standalone;
}

uint32_t SdkSession::getProtocolVersion() const {
    if (m_impl->device_session)
        return static_cast<uint32_t>(m_impl->device_session->getProtocolVersion());
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native)
            return CBPROTO_PROTOCOL_CURRENT;  // NATIVE layout always uses current protocol
        else
            return m_impl->shmem_session->getCompatProtocolVersion();
    }
    return 0;
}

std::string SdkSession::getProcIdent() const {
    if (m_impl->device_session) {
        const auto& config = m_impl->device_session->getDeviceConfig();
        return std::string(config.procinfo.ident,
            strnlen(config.procinfo.ident, sizeof(config.procinfo.ident)));
    }
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native) {
            return std::string(native->procinfo.ident,
                strnlen(native->procinfo.ident, sizeof(native->procinfo.ident)));
        } else {
            auto procinfo = m_impl->shmem_session->getProcInfo();
            if (procinfo.isError()) {
                return {};
            }
            return std::string(procinfo.value().ident, strnlen(procinfo.value().ident, sizeof(procinfo.value().ident)));
        }
    }
    return {};
}

Result<cbPKT_PROCINFO> SdkSession::getProcInfo() const {
    if (m_impl->device_session) {
        return Result<cbPKT_PROCINFO>::ok(m_impl->device_session->getDeviceConfig().procinfo);
    }
    if (m_impl->shmem_session) {
        const auto* native = m_impl->shmem_session->getNativeConfigBuffer();
        if (native) {
            return Result<cbPKT_PROCINFO>::ok(native->procinfo);
        }
        return m_impl->shmem_session->getProcInfo();
    }
    return Result<cbPKT_PROCINFO>::error("Processor information not available");
}

uint32_t SdkSession::getSpikeLength() const {
    auto si = getSysInfo();
    return si.isOk() ? si.value().spikelen : 0;
}

uint32_t SdkSession::getSpikePretrigger() const {
    auto si = getSysInfo();
    return si.isOk() ? si.value().spikepre : 0;
}

Result<void> SdkSession::setSpikeLength(uint32_t spikelen, uint32_t spikepre) {
    auto si = getSysInfo();
    if (si.isError())
        return Result<void>::error(si.error());
    cbPKT_SYSINFO pkt = si.value();
    pkt.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
    pkt.spikelen = spikelen;
    pkt.spikepre = spikepre;

    if (m_impl->device_session) {
        const auto r = m_impl->device_session->sendPacket(
            reinterpret_cast<const cbPKT_GENERIC&>(pkt));
        if (r.isError())
            return Result<void>::error(r.error());
        return Result<void>::ok();
    }
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

Result<int64_t> SdkSession::getChannelField(uint32_t chanId, ChanInfoField field) const {
    if (chanId == 0 || chanId > cbMAXCHANS)
        return Result<int64_t>::error("Invalid channel ID");
    auto ci = getChanInfo(chanId);
    if (ci.isError())
        return Result<int64_t>::error("Channel info unavailable");
    return Result<int64_t>::ok(extractChanInfoField(ci.value(), field));
}

Result<std::vector<uint32_t>> SdkSession::getMatchingChannelIds(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<uint32_t> ids;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        auto ci = getChanInfo(ch + 1);
        if (ci.isError()) continue;
        if (classifyChannelByCaps(ci.value()) != chanType) continue;
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
        auto ci = getChanInfo(ch + 1);
        if (ci.isError()) continue;
        if (classifyChannelByCaps(ci.value()) != chanType) continue;
        values.push_back(extractChanInfoField(ci.value(), field));
        count++;
    }
    return Result<std::vector<int64_t>>::ok(std::move(values));
}

Result<std::vector<std::string>> SdkSession::getChannelLabels(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<std::string> labels;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        auto ci = getChanInfo(ch + 1);
        if (ci.isError()) continue;
        if (classifyChannelByCaps(ci.value()) != chanType) continue;
        labels.emplace_back(ci.value().label);
        count++;
    }
    return Result<std::vector<std::string>>::ok(std::move(labels));
}

Result<std::vector<int32_t>> SdkSession::getChannelPositions(
        const size_t nChans, const ChannelType chanType) const {
    std::vector<int32_t> positions;
    size_t count = 0;
    for (uint32_t ch = 0; ch < cbMAXCHANS && count < nChans; ++ch) {
        auto ci = getChanInfo(ch + 1);
        if (ci.isError()) continue;
        if (classifyChannelByCaps(ci.value()) != chanType) continue;
        positions.push_back(ci.value().position[0]);
        positions.push_back(ci.value().position[1]);
        positions.push_back(ci.value().position[2]);
        positions.push_back(ci.value().position[3]);
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


// Resolve (nChans, chans) into the explicit set of 1-based channel ids to
// configure.  When @p chans is non-null, it's the verbatim list (length
// nChans).  When @p chans is null, walk channel ids in ascending order and
// pick the first @p nChans that match @p chanType; nChans == UINT32_MAX
// selects all matching.
static std::vector<uint32_t> resolveTargetChans(
    const SdkSession& session, uint32_t nChans, ChannelType chanType,
    const uint32_t* chans) {
    if (chans != nullptr) {
        return std::vector<uint32_t>(chans, chans + nChans);
    }
    std::vector<uint32_t> result;
    if (nChans != UINT32_MAX) {
        result.reserve(std::min<uint32_t>(nChans, cbMAXCHANS));
    }
    for (uint32_t chan = 1; chan <= cbMAXCHANS && result.size() < nChans; ++chan) {
        auto ci = session.getChanInfo(chan);
        if (ci.isOk() && classifyChannelByCaps(ci.value()) == chanType) {
            result.push_back(chan);
        }
    }
    return result;
}

Result<void> SdkSession::setSampleGroup(
    const uint32_t nChans, const ChannelType chanType, const SampleRate rate,
    const bool disableOthers, const uint32_t* chans) {
    // Pre-sync: ensure local chaninfo cache is up to date before we seed
    // outgoing CHANSET* packets from it.  Stale cache would re-send obsolete
    // values for fields we don't explicitly modify here.
    if (auto r = sync(5000); r.isError()) return r;

    const uint32_t group_id = static_cast<uint32_t>(rate);
    const std::vector<uint32_t> targets = resolveTargetChans(*this, nChans, chanType, chans);

    // Build a packet for `chan` with `grp` as its target group.
    // Always sends the full chaninfo (seeded from local cache), so a stale
    // CHANREP can't leave us stuck against a concurrent change.
    auto build_packet = [this](uint32_t chan, uint32_t grp) -> std::optional<cbPKT_CHANINFO> {
        auto base = getChanInfo(chan);
        if (base.isError() || base.value().chan == 0) return std::nullopt;
        cbPKT_CHANINFO& chaninfo = base.value();
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
        return chaninfo;
    };

    // Build the packet vector.  Targets first (configured to `rate`), then
    // — if disableOthers — every other matching channel set to disabled.
    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(targets.size() + (disableOthers ? cbMAXCHANS : 0));
    for (uint32_t chan : targets) {
        if (auto pkt = build_packet(chan, group_id); pkt) {
            packets.push_back(reinterpret_cast<const cbPKT_GENERIC&>(*pkt));
        }
    }
    if (disableOthers) {
        std::set<uint32_t> target_set(targets.begin(), targets.end());
        for (uint32_t chan = 1; chan <= cbMAXCHANS; ++chan) {
            if (target_set.count(chan)) continue;
            auto ci = getChanInfo(chan);
            if (ci.isError() || classifyChannelByCaps(ci.value()) != chanType) continue;
            if (auto pkt = build_packet(chan, 0u); pkt) {
                packets.push_back(reinterpret_cast<const cbPKT_GENERIC&>(*pkt));
            }
        }
    }

    return sendBulkPackets(packets);
}

// Bulk-send a vector of packets using the most efficient path:
// - STANDALONE: device_session->sendPackets — direct UDP with built-in
//   pacing.
// - CLIENT: per-packet shmem enqueue, drained in FIFO order by the peer
//   STANDALONE process.
Result<void> SdkSession::sendBulkPackets(const std::vector<cbPKT_GENERIC>& packets) {
    if (packets.empty()) return Result<void>::ok();
    if (m_impl->device_session) {
        return m_impl->device_session->sendPackets(packets);
    }
    if (!m_impl->shmem_session) {
        return Result<void>::error("No session available");
    }
    for (const auto& pkt : packets) {
        if (auto r = sendPacket(pkt); r.isError()) return r;
    }
    return Result<void>::ok();
}

// Helper for the three "configure-each" bulk setters that don't have
// disable_others semantics: setSpikeSorting, setSpikeExtraction,
// setACInputCoupling.  Pre-syncs, iterates the resolved target list,
// builds a packet per chan via @p mutate, and sends.  Fire-and-forget
// on the response side — caller can call sync() if it needs to read
// back state.
template<typename Mutate>
static Result<void> applyBulkSetter(
    SdkSession& session, uint32_t nChans, ChannelType chanType,
    const uint32_t* chans, Mutate&& mutate) {
    if (auto r = session.sync(5000); r.isError()) return r;

    const std::vector<uint32_t> targets = resolveTargetChans(session, nChans, chanType, chans);
    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(targets.size());
    for (uint32_t chan : targets) {
        auto base = session.getChanInfo(chan);
        if (base.isError() || base.value().chan == 0) continue;
        cbPKT_CHANINFO& chaninfo = base.value();
        chaninfo.chan = chan;
        mutate(chaninfo);
        packets.push_back(reinterpret_cast<const cbPKT_GENERIC&>(chaninfo));
    }

    return session.sendBulkPackets(packets);
}

Result<void> SdkSession::setSpikeSorting(
    const uint32_t nChans, const ChannelType chanType, const uint32_t sortOptions,
    const uint32_t* chans) {
    return applyBulkSetter(*this, nChans, chanType, chans,
        [sortOptions](cbPKT_CHANINFO& ci) {
            // Use CHANSET so the firmware applies spkopts.  CHANSETSPKTHR
            // (used by older revisions) only reads spkthrlevel and would
            // silently ignore the spkopts modification.
            ci.cbpkt_header.type = cbPKTTYPE_CHANSET;
            ci.spkopts &= ~cbAINPSPK_ALLSORT;
            ci.spkopts |= sortOptions;
        });
}

Result<void> SdkSession::setSpikeExtraction(
    const uint32_t nChans, const ChannelType chanType, const bool enabled,
    const uint32_t* chans) {
    return applyBulkSetter(*this, nChans, chanType, chans,
        [enabled](cbPKT_CHANINFO& ci) {
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETSPK;
            ci.spkopts &= ~cbAINPSPK_EXTRACT;
            if (enabled) ci.spkopts |= cbAINPSPK_EXTRACT;
        });
}

Result<void> SdkSession::setACInputCoupling(
    const uint32_t nChans, const ChannelType chanType, const bool enabled,
    const uint32_t* chans) {
    return applyBulkSetter(*this, nChans, chanType, chans,
        [enabled](cbPKT_CHANINFO& ci) {
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
            if (enabled) ci.ainpopts |= cbAINP_OFFSET_CORRECT;
            else         ci.ainpopts &= ~cbAINP_OFFSET_CORRECT;
        });
}

Result<void> SdkSession::setChannelConfig(const cbPKT_CHANINFO& chaninfo) {
    // STANDALONE talks to the device directly, where ids are already local.
    if (m_impl->device_session)
        return m_impl->device_session->setChannelConfig(chaninfo);

    // CLIENT: validate against this device's window, but send the channel id
    // unchanged.
    //
    // Only Central's shared-memory chaninfo[] is indexed globally. Central
    // forwards transmit-buffer packets to the instrument network verbatim
    // (InstNetwork drains the buffer straight into Instrument::Send, which is a
    // plain UDP send with no per-instrument routing), so the device receives
    // exactly what we wrote and applies it using its own numbering -- Hub2's
    // channels are 1..256 to Hub2, whatever Central calls them. Routing is by
    // cbpkt_header.instrument, which sendPacket() stamps.
    //
    // Translating the id here would send Hub2 a channel 257 it does not have.
    if (!m_impl->toGlobalChan(chaninfo.chan)) {
        return Result<void>::error("Channel " + std::to_string(chaninfo.chan) +
                                   " is not present on this device");
    }
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

    auto info = getChanInfo(aout_chan_id);
    if (info.isError())
        return Result<void>::error("Channel info not available for channel " + std::to_string(aout_chan_id));

    // Copy current config and modify analog output fields
    cbPKT_CHANINFO& chaninfo = info.value();

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

Result<void> SdkSession::loadChannelMap(
    const std::string& filepath, uint32_t start_chan, uint32_t hs_id) {
    auto parse_result = parseCmpFile(filepath, start_chan, hs_id);
    if (parse_result.isError()) {
        return Result<void>::error(parse_result.error());
    }

    // Merge parsed entries (keyed by device bank/term) into the overlay map.
    {
        std::lock_guard<std::mutex> lock(m_impl->cmp_mutex);
        for (auto& [key, entry] : parse_result.value()) {
            m_impl->cmp_entries[key] = std::move(entry);
        }
    }

    // Apply positions + labels to shmem for any chaninfo already present.
    m_impl->applyCmpToAllChannels();

    // Push labels to the device so they persist in chaninfo and are echoed
    // back in future CHANREP packets. Positions aren't persisted by the
    // device, so no analogous push for position. We discover which channels
    // matched a CMP row by joining live chaninfo on (bank, term) — the same
    // way applyCmpToAllChannels does — then snapshot (chan_id, label) so the
    // network sends happen outside cmp_mutex.
    if (m_impl->device_session || m_impl->shmem_session) {
        std::vector<std::pair<uint32_t, std::string>> labels_to_push;
        {
            std::lock_guard<std::mutex> lock(m_impl->cmp_mutex);
            for (uint32_t chan_id = 1; chan_id <= m_impl->local_max_chans; ++chan_id) {
                auto info = getChanInfo(chan_id);
                if (info.isError() || info.value().chan == 0) continue;
                auto it = m_impl->cmp_entries.find(cmpKey(info.value().bank, info.value().term));
                if (it == m_impl->cmp_entries.end()) continue;
                labels_to_push.emplace_back(chan_id, it->second.label);
            }
        }
        for (const auto& [chan_id, label] : labels_to_push) {
            auto info = getChanInfo(chan_id);
            if (info.isError()) continue;
            cbPKT_CHANINFO& ci = info.value();
            ci.chan = chan_id;
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETLABEL;
            std::strncpy(ci.label, label.c_str(), sizeof(ci.label) - 1);
            ci.label[sizeof(ci.label) - 1] = '\0';
            (void)setChannelConfig(ci);  // best-effort; per-channel failures shouldn't abort
        }
    }

    return Result<void>::ok();
}

Result<void> SdkSession::clearChannelMap() {
    // Find which channels currently match a loaded CMP entry (by bank/term),
    // then drop the overlay so future CHANREPs land in chaninfo as the device
    // sends them.  Any further applyCmpToAllChannels() call is now a no-op.
    std::vector<uint32_t> mapped_chans;
    {
        std::lock_guard<std::mutex> lock(m_impl->cmp_mutex);
        if (!m_impl->cmp_entries.empty()) {
            for (uint32_t chan_id = 1; chan_id <= m_impl->local_max_chans; ++chan_id) {
                auto info = getChanInfo(chan_id);
                if (info.isError() || info.value().chan == 0) continue;
                if (m_impl->cmp_entries.count(cmpKey(info.value().bank, info.value().term))) {
                    mapped_chans.push_back(chan_id);
                }
            }
        }
        m_impl->cmp_entries.clear();
    }

    // Reset shmem positions to zero for previously-mapped channels.  The
    // device doesn't persist positions, so there's no on-device action — just
    // wipe the local overlay we applied in applyCmpToAllChannels().
    if (m_impl->shmem_session) {
        for (uint32_t chan_id : mapped_chans) {
            if (chan_id < 1 || chan_id > cbMAXCHANS) continue;
            auto r = m_impl->shmem_session->getChanInfo(chan_id - 1);
            if (r.isError()) continue;
            cbPKT_CHANINFO ci = r.value();
            std::memset(ci.position, 0, sizeof(ci.position));
            m_impl->shmem_session->setChanInfo(chan_id - 1, ci);
        }
    }

    // Push default labels ("chan{N}") to the device so the device-side label
    // state matches.  Labels were modified by loadChannelMap; positions were
    // local-only.
    if (m_impl->device_session || m_impl->shmem_session) {
        for (uint32_t chan_id : mapped_chans) {
            auto info = getChanInfo(chan_id);
            if (info.isError()) continue;
            cbPKT_CHANINFO& ci = info.value();
            ci.chan = chan_id;
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETLABEL;
            char default_label[16];
            std::snprintf(default_label, sizeof(default_label), "chan%u", chan_id);
            std::strncpy(ci.label, default_label, sizeof(ci.label) - 1);
            ci.label[sizeof(ci.label) - 1] = '\0';
            (void)setChannelConfig(ci);  // best-effort
        }
    }

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
        if (native) {
            extractFromNativeConfig(*native, ccf_data);
        } else {
            // CENTRAL: getNativeConfigBuffer() is NATIVE-only, so a CENTRAL
            // CLIENT had no way to reach its configuration and every save
            // failed. Translate Central's layout into the native form first.
            // Heap-allocated: NativeConfigBuffer is far too large for the stack.
            auto legacy = std::make_unique<cbshm::NativeConfigBuffer>();
            auto r = m_impl->shmem_session->getLegacyConfigBuffer(*legacy);
            if (r.isError())
                return Result<void>::error("No configuration available in shared memory: " +
                                           r.error());
            extractFromNativeConfig(*legacy, ccf_data);

            // The CCF describes the device this session speaks for, matching
            // the rest of the API: this instrument's channels, in this
            // instrument's numbering.
            //
            // extractFromNativeConfig copies the first cbMAXCHANS entries
            // straight out of Central's globally-indexed space. On a
            // multi-instrument Central that is the wrong set: with two hubs it
            // yielded Hub1's 256 channels plus Hub2's first 28 sitting in the
            // slots reserved for analog/Experiment I/O, and silently dropped
            // everything past 284 of Central's 880. Loading such a file back
            // would have written one device's settings onto another's.
            //
            // Everything else in the CCF (filters, sorting, LNC, waveforms,
            // n-trodes, sysinfo) is system-wide and is kept as extracted.
            std::memset(ccf_data.isChan, 0, sizeof(ccf_data.isChan));
            const uint32_t n_local = std::min<uint32_t>(m_impl->local_max_chans, cbMAXCHANS);
            for (uint32_t local = 1; local <= n_local; ++local) {
                auto ci = m_impl->getChanInfo(local);
                if (ci.isError()) continue;
                ccf_data.isChan[local - 1] = ci.value();
                ccf_data.isChan[local - 1].chan = local;
            }
        }
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

    if (m_impl->device_session) {
        // STANDALONE: send directly via DeviceSession (same path as
        // setSampleGroup et al.).  This avoids the unnecessary
        // shmem round-trip and lets sync() / loadCCFSync() use the
        // standard direct-UDP barrier.
        const auto r = m_impl->device_session->sendPackets(packets);
        if (r.isError())
            return Result<void>::error(r.error());
    } else {
        // CLIENT: enqueue to shmem for the STANDALONE process to transmit.
        for (const auto& pkt : packets) {
            auto send_result = sendPacket(pkt);
            if (send_result.isError())
                return send_result;
        }
    }

    return Result<void>::ok();
}

Result<void> SdkSession::loadCCFSync(const std::string& filename, uint32_t timeout_ms) {
    auto result = loadCCF(filename);
    if (result.isError())
        return result;
    return sync(timeout_ms);
}

Result<void> SdkSession::sync(uint32_t timeout_ms) {
    // Send a no-op runlevel SET (current runlevel) as a sync barrier.
    // The device processes packets in order, so the resulting SYSREPRUNLEV
    // confirms all prior configuration packets have been applied.
    //
    // Use getRunLevel() (not the raw atomic): in CLIENT mode the SYSREP
    // ring may be empty (the STANDALONE owner already handshook before we
    // attached), so the atomic stays at 0.  getRunLevel() falls back to the
    // SYSINFO mirror in shmem.  Sending runlevel=0 here would risk
    // perturbing device state, so it must be the real current runlevel.
    uint32_t current = getRunLevel();

    Result<void> result = Result<void>::error("No session available");
    if (m_impl->device_session) {
        // STANDALONE: use DeviceSession::setSystemRunLevelSync which matches
        // the specific SYSREPRUNLEV (type 0x12) response via sendAndWait.
        result = m_impl->device_session->setSystemRunLevelSync(
            current, 0, 0, std::chrono::milliseconds(timeout_ms));
    } else {
        // CLIENT: route through shmem and wait for SYSREPRUNLEV (0x12)
        // specifically — periodic SYSREP heartbeats (0x10) from
        // nPlayServer would otherwise falsely satisfy the wait.
        result = setSystemRunLevel(current, 0, 0, 0, timeout_ms,
                                   cbPKTTYPE_SYSREPRUNLEV);
    }
    if (result.isError())
        return result;

    // The device also sends SYSREP on network error / reset.  If the
    // returned runlevel is HARDRESET or STANDBY the device dropped our
    // config packets and restarted.
    uint32_t rl = getRunLevel();
    if (rl <= cbRUNLEVEL_STANDBY) {
        return Result<void>::error(
            "Device reset during configuration (runlevel " + std::to_string(rl) + ")");
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

bool SdkSession::toLocalTimeBatch(int64_t stream_id,
                                  const uint64_t* device_ns,
                                  int64_t* out_steady_ns,
                                  size_t n) const {
    if (n == 0) return true;
    if (!device_ns || !out_steady_ns) return false;

    // Stateless fast path: one offset, plain subtraction, no per-stream state.
    if (stream_id < 0) {
        const auto offset = getClockOffsetNs();
        if (!offset) return false;
        for (size_t i = 0; i < n; ++i)
            out_steady_ns[i] = static_cast<int64_t>(device_ns[i]) - *offset;
        return true;
    }

    // Monotonic path: take a single (offset, epoch) snapshot and the floor map
    // under one lock so the whole batch sees one consistent clock regime.
    std::lock_guard<std::mutex> lock(m_impl->mono_mutex);
    const auto oe = m_impl->currentOffsetAndEpoch();
    if (!oe) return false;
    const int64_t  offset = oe->first;
    const uint64_t epoch  = oe->second;

    auto& st = m_impl->mono_streams[stream_id];  // lazy-create (seen == false)
    for (size_t i = 0; i < n; ++i) {
        const int64_t raw = static_cast<int64_t>(device_ns[i]) - offset;
        // Reset the floor on the first conversion for this stream or whenever
        // the clock regime changed; otherwise clamp to a non-decreasing floor.
        const int64_t out = (!st.seen || epoch != st.last_epoch)
                                ? raw
                                : std::max(raw, st.floor_ns);
        st.floor_ns = out;
        st.last_epoch = epoch;
        st.seen = true;
        out_steady_ns[i] = out;
    }
    return true;
}

void SdkSession::resetMonotonic(int64_t stream_id) {
    std::lock_guard<std::mutex> lock(m_impl->mono_mutex);
    m_impl->mono_streams.erase(stream_id);
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
    if (m_impl->device_session) {
        // STANDALONE: send via device session (direct UDP)
        auto r = m_impl->device_session->sendClockProbe();
        if (r.isError())
            return Result<void>::error(r.error());
        return Result<void>::ok();
    }

    // CLIENT: send via shmem xmt buffer (Central forwards to device)
    if (!m_impl->shmem_session)
        return Result<void>::error("sendClockProbe: no session available");

    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_impl->clock_probe_mutex);
        m_impl->pending_clock_probe.t1_local = now;
        m_impl->pending_clock_probe.active = true;
    }

    cbPKT_NPLAY pkt{};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_NPLAYSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NPLAY;
    pkt.mode = 0xFFFF;
    pkt.stime = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());

    return sendPacket(*reinterpret_cast<const cbPKT_GENERIC*>(&pkt));
}

std::optional<int64_t> SdkSession::getClockOffsetNs() const {
    // STANDALONE: get from device_session's ClockSync.
    // The peer HUB offset (if active) is already injected into the
    // ClockSync via setExternalOffset() in the periodic update, so
    // this returns the peer's value transparently.
    if (m_impl->device_session)
        return m_impl->device_session->getOffsetNs();

    // CLIENT: try shmem clock sync fields (NATIVE layout)
    if (m_impl->shmem_session) {
        auto offset = m_impl->shmem_session->getClockOffsetNs();
        if (offset.has_value())
            return offset;
    }

    // CLIENT fallback: use local ClockSync (CENTRAL — no shmem clock fields)
    return m_impl->client_clock_sync.getOffsetNs();
}

std::optional<int64_t> SdkSession::getClockUncertaintyNs() const {
    if (m_impl->device_session)
        return m_impl->device_session->getUncertaintyNs();

    // CLIENT: try shmem clock sync fields (NATIVE layout)
    if (m_impl->shmem_session) {
        auto uncert = m_impl->shmem_session->getClockUncertaintyNs();
        if (uncert.has_value())
            return uncert;
    }

    // CLIENT fallback: use local ClockSync (CENTRAL)
    return m_impl->client_clock_sync.getUncertaintyNs();
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

    // Stamp packet with a nanosecond timestamp (Central's xmt consumer skips packets with time=0).
    // Use getLastTime() (always nanoseconds) so that enqueuePacket's per-protocol translation
    // can convert to the format each consumer expects (e.g. 30kHz ticks for 3.11 CENTRAL).
    cbPKT_GENERIC stamped = pkt;
    PROCTIME t = m_impl->shmem_session->getLastTime();
    stamped.cbpkt_header.time = (t != 0) ? t : 1;

    // Address the packet to this session's instrument so the fabric routes it
    // to the right device and any reply carries the matching instrument index.
    // Without this every outbound packet is addressed to instrument 0: a
    // non-zero-index session's reply is then dropped by readReceiveBuffer's
    // instrument filter and every wait times out. That is why sync() -- a
    // runlevel SET plus a wait for SYSREPRUNLEV -- succeeded only for
    // instrument 0, taking every auto_sync setter down with it on the others.
    //
    // Stamping here covers every CLIENT-mode send at once: runlevel,
    // REQCONFIGALL, channel config, comments, digital output and file control.
    // The clock probe already sets the same value explicitly, so it is
    // unchanged; NATIVE is instrument 0 either way.
    stamped.cbpkt_header.instrument =
        m_impl->shmem_session->getInstrument().toPacketField();

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

bool SdkSession::waitForSysrep(uint32_t timeout_ms, uint32_t expected_runlevel,
                               uint16_t expected_type) const {
    // Wait for SYSREP packet, optionally filtered by type and/or runlevel.
    //   expected_type == 0:                    any 0x10..0x1F packet
    //   expected_type == cbPKTTYPE_SYSREPRUNLEV (0x12):
    //                                          requires the sticky 0x12 flag
    //   expected_runlevel == 0:                accept any runlevel
    std::unique_lock<std::mutex> lock(m_impl->handshake_mutex);
    return m_impl->handshake_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this, expected_runlevel, expected_type] {
            if (expected_type == cbPKTTYPE_SYSREPRUNLEV) {
                if (!m_impl->received_sysrepRunlev.load(std::memory_order_acquire))
                    return false;
            } else if (!m_impl->received_sysrep.load(std::memory_order_acquire)) {
                return false;
            }
            if (expected_runlevel == 0) {
                return true;
            }
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
                                           uint32_t wait_for_runlevel, uint32_t timeout_ms,
                                           uint16_t expected_type) {
    // Reset handshake state before sending
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    m_impl->received_sysrepRunlev.store(false, std::memory_order_relaxed);

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
    if (!waitForSysrep(timeout_ms, wait_for_runlevel, expected_type)) {
        if (wait_for_runlevel != 0)
            return Result<void>::error("No SYSREP response with expected runlevel " + std::to_string(wait_for_runlevel));
        return Result<void>::error("No SYSREP response received for setSystemRunLevel");
    }

    return Result<void>::ok();
}

Result<void> SdkSession::requestConfiguration(uint32_t timeout_ms) {
    // Baseline the config-reply count so we can tell whether the device
    // answered this particular request (see the fallback below).
    const uint64_t config_replies_before =
        m_impl->config_replies.load(std::memory_order_acquire);

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
    if (waitForSysrep(timeout_ms))
        return Result<void>::ok();

    // The terminating SYSREP did not arrive.
    //
    // The device queues the SYSREP that ends the config dump, but firmware before
    // 7.5.1 never flushes the queue afterwards, so if nothing else drives it the
    // terminator is never transmitted (firmware commit ae8df07 added the explicit
    // FlushCerPktQueue() for exactly this). Affected: 7.0.x and 7.5.0.
    //
    // Gate on protocol version, which detection has already established by now.
    // It is coarser than the real firmware boundary but errs safe: 4.0 spans both
    // 7.5.0 and 7.5.1+, and the fallback is unreachable on the latter. All 4.1+
    // firmware flushes, so there a missing terminator is a real fault.
    const bool terminator_guaranteed =
        m_impl->device_session
        && m_impl->device_session->getProtocolVersion() >= cbdev::ProtocolVersion::PROTOCOL_410;
    if (terminator_guaranteed)
        return Result<void>::error("No SYSREP response received for requestConfiguration");

    // Treat the dump as complete once config replies have arrived and then gone
    // quiet. If none arrived at all, the device really did not answer.
    if (m_impl->config_replies.load(std::memory_order_acquire) == config_replies_before)
        return Result<void>::error("No SYSREP response received for requestConfiguration");

    constexpr auto kQuietPeriod = std::chrono::milliseconds(200);
    constexpr auto kPollInterval = std::chrono::milliseconds(25);
    uint64_t last_count = m_impl->config_replies.load(std::memory_order_acquire);
    auto quiet_since = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - quiet_since < kQuietPeriod) {
        std::this_thread::sleep_for(kPollInterval);
        // A late terminator is still the best signal available.
        if (m_impl->received_sysrep.load(std::memory_order_acquire))
            return Result<void>::ok();
        const uint64_t count = m_impl->config_replies.load(std::memory_order_acquire);
        if (count != last_count) {
            last_count = count;
            quiet_since = std::chrono::steady_clock::now();
        }
    }

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
    m_impl->received_sysrepRunlev.store(false, std::memory_order_relaxed);
    m_impl->device_runlevel.store(0, std::memory_order_relaxed);

    // These steps are UDP request/response exchanges, which have no delivery
    // guarantee, so a single lost reply must not fail session creation. The
    // query-style steps below are therefore retried.
    //
    // This matters most when the device is already streaming at full rate: a
    // REQCONFIGALL makes the device emit its whole configuration (one CHANREP
    // per channel plus proc/sys/group info), and that burst's terminating
    // SYSREP has to survive alongside ~16 MB/s of continuous sample data.
    // Occasionally it doesn't, and re-requesting is the only recovery -- a
    // longer wait cannot conjure a datagram the kernel already dropped.
    //
    // Only the idempotent steps retry. HARDRESET/RESET below stay single-shot
    // because re-issuing a state transition that already took effect (and whose
    // reply was merely lost) would disturb a device that is mid-transition.
    constexpr int kMaxQueryAttempts = 3;
    constexpr auto kRetryPause = std::chrono::milliseconds(50);

    // Quick presence check - use shorter timeout to fail fast for non-existent devices
    const uint32_t presence_check_timeout = std::min(100u, timeout_ms);

    // Step 1: Quick presence check - send cbRUNLEVEL_RUNNING with short timeout to fail fast
    Result<void> result = Result<void>::error("presence check not attempted");
    for (int attempt = 0; attempt < kMaxQueryAttempts; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(kRetryPause);
        }
        result = setSystemRunLevel(cbRUNLEVEL_RUNNING, 0, 0, 0, presence_check_timeout);
        if (result.isOk()) {
            break;
        }
    }
    if (result.isError()) {
        // No response - device not on network
        return Result<void>::error("Device not reachable (no response to initial probe after "
            + std::to_string(kMaxQueryAttempts)
            + " attempts - check network connection and IP address)");
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
    // requestConfiguration() waits internally for final SYSREP.
    // Retried: a REQCONFIGALL is a pure query, so re-sending it is harmless --
    // the device simply re-reports the same configuration.
    for (int attempt = 0; attempt < kMaxQueryAttempts; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(kRetryPause);
        }
        result = requestConfiguration(timeout_ms);
        if (result.isOk()) {
            break;
        }
    }
    if (result.isError()) {
        return Result<void>::error("Failed to send REQCONFIGALL after "
            + std::to_string(kMaxQueryAttempts) + " attempts: " + result.error());
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
    // Resolve the channel window and build the type cache now that config is
    // populated. procinfo.chancount is only meaningful once the device has
    // answered REQCONFIGALL, so the window built during create() was still the
    // identity fallback.
    m_impl->resolveChannelWindow();
    m_impl->rebuildChannelTypeCache();
    return Result<void>::ok();
}

} // namespace cbsdk
