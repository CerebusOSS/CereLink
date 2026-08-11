///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   rx_bench.cpp
/// @brief  STANDALONE UDP reception benchmark.
///
/// Streams continuous sample-group data from one or more devices and attributes
/// packet loss to a layer, using two independent measures:
///
///   * firmware reconciliation — every cbPKT_SYSPROTOCOLMONITOR reports how
///     many packets the device sent; summing whole monitor-to-monitor intervals
///     and comparing against what arrived measures loss on the wire and in the
///     kernel.  Per-interval deficits and surpluses are reported separately:
///     a packet that straddles a monitor boundary shows up as a deficit in one
///     interval and a matching surplus in the next, which is NOT loss.
///   * timestamp contiguity — gaps in the group-packet timestamp sequence.
///
///   * callback-queue loss — SdkStats::packets_dropped, i.e. loss inside the
///     SDK rather than below it.
///
/// A second instance run against a device that already has an owner attaches in
/// CLIENT mode, which exercises the shared-memory ring instead of the socket.
///
/// Usage:
///   rx_bench [--device HUB1[,HUB2,...]] [--seconds 30] [--group 6]
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk/sdk_session.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

/// Records group-packet timestamps during the measurement window and derives
/// the loss figures afterwards.  Post-hoc analysis (rather than streaming
/// inference) keeps the period estimate robust: it is the median over the whole
/// run, not over whatever the first few dozen packets happened to look like.
struct GapTracker {
    std::vector<uint64_t> times;   // appended by the callback thread only

    // Results, filled by analyze()
    uint64_t period = 0;
    uint64_t gap_events = 0;
    uint64_t missing = 0;
    uint64_t backward = 0;
    uint64_t worst_gap = 0;

    void reserveFor(double seconds) { times.reserve(static_cast<size_t>(seconds * 31000.0) + 1024); }

    void analyze() {
        if (times.size() < 3) return;

        std::vector<int64_t> deltas;
        deltas.reserve(times.size() - 1);
        for (size_t i = 1; i < times.size(); ++i) {
            deltas.push_back(static_cast<int64_t>(times[i]) - static_cast<int64_t>(times[i - 1]));
        }

        std::vector<int64_t> sorted = deltas;
        std::sort(sorted.begin(), sorted.end());
        period = static_cast<uint64_t>(std::max<int64_t>(sorted[sorted.size() / 2], 1));

        // Integer tick->ns conversion makes 30 kHz deltas alternate 33333/33334,
        // so anything within one period plus a few ns of jitter is contiguous.
        const int64_t p = static_cast<int64_t>(period);
        for (const int64_t d : deltas) {
            if (d <= 0) { ++backward; continue; }
            if (d <= p + 2) continue;
            // Round to the nearest whole number of periods.
            const int64_t n = (d + p / 2) / p;
            if (n <= 1) continue;   // jitter, not a dropped packet
            ++gap_events;
            missing += static_cast<uint64_t>(n - 1);
            worst_gap = std::max<uint64_t>(worst_gap, static_cast<uint64_t>(d));
        }
    }
};

struct DeviceRun {
    std::string name;
    cbsdk::DeviceType type;
    std::unique_ptr<cbsdk::SdkSession> session;
    GapTracker gaps;
    std::atomic<bool> measuring{false};
    std::atomic<uint64_t> group_packets{0};
    std::atomic<uint64_t> overflow_errors{0};

    // Ground truth from the device: every cbPKT_SYSPROTOCOLMONITOR reports how
    // many packets the firmware sent since the previous one.  Summing those and
    // comparing against what we actually received measures loss on the wire and
    // in the kernel, independent of any timestamp reasoning.
    std::atomic<uint64_t> device_sent{0};
    std::atomic<uint64_t> all_packets{0};
    std::atomic<uint64_t> monitors{0};
    // Callback-thread-only state.  Kept here rather than in a `mutable` lambda
    // capture: SdkSession stores callbacks as std::function and dispatches from
    // a snapshot, so per-invocation mutations to a capture are not guaranteed to
    // persist.
    uint64_t since_monitor = 0;
    bool armed = false;
    std::vector<int64_t> interval_deltas;   // received - sent, per monitor interval

    std::optional<int64_t> clock_offset_first;
    std::optional<int64_t> clock_offset_ns;
    std::optional<int64_t> clock_uncertainty_ns;
};

cbsdk::DeviceType parseDevice(const std::string& s, bool& ok) {
    ok = true;
    if (s == "NSP")        return cbsdk::DeviceType::NSP;
    if (s == "HUB1")       return cbsdk::DeviceType::HUB1;
    if (s == "HUB2")       return cbsdk::DeviceType::HUB2;
    if (s == "HUB3")       return cbsdk::DeviceType::HUB3;
    if (s == "LEGACY_NSP") return cbsdk::DeviceType::LEGACY_NSP;
    if (s == "NPLAY")      return cbsdk::DeviceType::NPLAY;
    ok = false;
    return cbsdk::DeviceType::HUB1;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t pos = s.find(sep, start);
        out.push_back(s.substr(start, pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string devices_arg = "HUB1";
    double seconds = 20.0;
    int group = 6;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--device" || a == "-d")       devices_arg = next();
        else if (a == "--seconds" || a == "-s") seconds = std::stod(next());
        else if (a == "--group" || a == "-g")   group = std::stoi(next());
        else {
            std::cerr << "Usage: rx_bench [--device HUB1,HUB2] [--seconds N] [--group 6]\n";
            return 2;
        }
    }

    std::vector<std::unique_ptr<DeviceRun>> runs;
    for (const auto& name : split(devices_arg, ',')) {
        bool ok = false;
        const auto type = parseDevice(name, ok);
        if (!ok) {
            std::cerr << "Unknown device: " << name << "\n";
            return 2;
        }
        auto run = std::make_unique<DeviceRun>();
        run->name = name;
        run->type = type;
        runs.push_back(std::move(run));
    }

    // --- open sessions -------------------------------------------------------
    for (auto& run : runs) {
        cbsdk::SdkConfig cfg;
        cfg.device_type = run->type;
        auto res = cbsdk::SdkSession::create(cfg);
        if (res.isError()) {
            std::cerr << run->name << ": create failed: " << res.error() << "\n";
            return 1;
        }
        run->session = std::make_unique<cbsdk::SdkSession>(std::move(res.value()));

        auto* raw = run.get();
        raw->session->setErrorCallback([raw](const std::string&) {
            raw->overflow_errors.fetch_add(1, std::memory_order_relaxed);
        });
        raw->session->registerGroupCallback(
            static_cast<cbsdk::SampleRate>(group),
            [raw](const cbPKT_GROUP& pkt) {
                if (!raw->measuring.load(std::memory_order_relaxed)) return;
                raw->group_packets.fetch_add(1, std::memory_order_relaxed);
                raw->gaps.times.push_back(pkt.cbpkt_header.time);
            });
        // Catch-all: counts everything and reconciles against the firmware's own
        // sent-packet count at each SYSPROTOCOLMONITOR boundary.  Accumulating
        // only whole monitor-to-monitor intervals keeps the comparison exact —
        // a partial interval at either end of the window would otherwise bias
        // the result by up to one full interval's worth of packets.
        raw->session->registerPacketCallback([raw](const cbPKT_GENERIC& pkt) {
            if (!raw->measuring.load(std::memory_order_relaxed)) {
                raw->armed = false;
                return;
            }
            ++raw->since_monitor;
            if (pkt.cbpkt_header.type != cbPKTTYPE_SYSPROTOCOLMONITOR) return;

            const auto* mon = reinterpret_cast<const cbPKT_SYSPROTOCOLMONITOR*>(&pkt);
            if (raw->armed) {
                raw->monitors.fetch_add(1, std::memory_order_relaxed);
                raw->device_sent.fetch_add(mon->sentpkts, std::memory_order_relaxed);
                raw->all_packets.fetch_add(raw->since_monitor, std::memory_order_relaxed);
                raw->interval_deltas.push_back(static_cast<int64_t>(raw->since_monitor) -
                                               static_cast<int64_t>(mon->sentpkts));
            }
            raw->armed = true;
            raw->since_monitor = 0;
        });
        run->gaps.reserveFor(seconds + 2.0);
        // SdkSession::create() already starts the session (handshake + threads).
        std::cout << run->name << ": session created and running\n";
    }

    // Let the handshake/config dump settle before measuring.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    for (auto& run : runs) {
        run->session->resetStats();
        run->measuring.store(true, std::memory_order_relaxed);
    }

    for (auto& run : runs) run->clock_offset_first = run->session->getClockOffsetNs();

    const auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(t1 - t0).count();

    for (auto& run : runs) run->measuring.store(false, std::memory_order_relaxed);
    for (auto& run : runs) {
        run->clock_offset_ns = run->session->getClockOffsetNs();
        run->clock_uncertainty_ns = run->session->getClockUncertaintyNs();
    }
    for (auto& run : runs) run->session->stop();
    for (auto& run : runs) run->gaps.analyze();

    // --- report --------------------------------------------------------------
    std::cout << "\n================ rx_bench (" << std::fixed << std::setprecision(2)
              << elapsed << " s) ================\n";
    int exit_code = 0;
    for (auto& run : runs) {
        const auto st = run->session->getStats();
        const auto& g = run->gaps;
        const double rate = static_cast<double>(st.packets_received_from_device) / elapsed;

        std::cout << "\n--- " << run->name
                  << (run->session->isStandalone() ? " [STANDALONE] ---\n" : " [CLIENT] ---\n");
        if (!run->session->isStandalone()) {
            std::cout << "  shmem overruns:   " << st.shmem_overruns << "\n";
            std::cout << "  producer count:   " << st.packets_produced << "\n";
        }
        std::cout << "  rx from device:   " << st.packets_received_from_device
                  << "  (" << std::setprecision(1) << rate << " pkt/s)\n";
        std::cout << "  stored to shmem:  " << st.packets_stored_to_shmem << "\n";
        std::cout << "  queued:           " << st.packets_queued_for_callback << "\n";
        std::cout << "  delivered:        " << st.packets_delivered_to_callback << "\n";
        std::cout << "  QUEUE DROPS:      " << st.packets_dropped << "\n";
        std::cout << "  queue peak depth: " << st.queue_max_depth << "\n";
        std::cout << "  shmem errors:     " << st.shmem_store_errors << "\n";
        std::cout << "  recv errors:      " << st.receive_errors << "\n";
        const uint64_t received = g.times.size();
        std::cout << "  group " << group << " packets:  " << received
                  << "  (period " << g.period << " ns)\n";
        std::cout << "  GAP EVENTS:       " << g.gap_events << "\n";
        std::cout << "  MISSING PACKETS:  " << g.missing;
        if (received + g.missing > 0) {
            std::cout << "  (" << std::setprecision(4)
                      << (100.0 * static_cast<double>(g.missing) /
                          static_cast<double>(received + g.missing))
                      << " %)";
        }
        std::cout << "\n";
        std::cout << "  worst gap:        " << g.worst_gap << " ns\n";
        std::cout << "  backward ts:      " << g.backward << "\n";

        const uint64_t sent = run->device_sent.load();
        const uint64_t got = run->all_packets.load();
        std::cout << "  -- firmware reconciliation (" << run->monitors.load()
                  << " monitor intervals) --\n";
        std::cout << "  device sent:      " << sent << "\n";
        std::cout << "  we received:      " << got << "\n";
        if (sent > 0) {
            const int64_t lost = static_cast<int64_t>(sent) - static_cast<int64_t>(got);
            std::cout << "  WIRE LOSS:        " << lost << "  (" << std::setprecision(4)
                      << (100.0 * static_cast<double>(lost) / static_cast<double>(sent))
                      << " %)\n";
            if (lost > 0) exit_code = 1;
        }
        // Per-interval breakdown.  A real loss shows up as a persistent
        // deficit; packets merely straddling a monitor boundary show up as a
        // deficit in one interval and a matching surplus in the next.
        {
            int64_t deficit = 0, surplus = 0, worst = 0;
            size_t n_short = 0, n_over = 0;
            for (const int64_t d : run->interval_deltas) {
                if (d < 0) { deficit += -d; ++n_short; worst = std::min(worst, d); }
                else if (d > 0) { surplus += d; ++n_over; }
            }
            std::cout << "  intervals short:  " << n_short << " (total " << deficit
                      << ", worst " << worst << ")\n";
            std::cout << "  intervals over:   " << n_over << " (total " << surplus << ")\n";
        }
        // Clock sync health.  Reported so a change to the clock-maintenance
        // cadence can be checked for drift or cross-device disagreement.
        if (auto off = run->clock_offset_ns) {
            std::cout << "  clock offset:     " << *off << " ns";
            if (auto u = run->clock_uncertainty_ns) std::cout << "  (uncertainty " << *u << " ns)";
            std::cout << "\n";
            std::cout << "  offset drift:     " << (*off - run->clock_offset_first.value_or(*off))
                      << " ns over the window\n";
        }

        if (g.missing > 0 || st.packets_dropped > 0) exit_code = 1;
    }
    std::cout << "\n";
    return exit_code;
}
