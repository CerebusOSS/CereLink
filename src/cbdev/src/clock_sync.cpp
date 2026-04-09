///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   clock_sync.cpp
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Implementation of ClockSync offset estimator (probes-only with asymmetry correction)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cbdev/clock_sync.h"
#include <algorithm>
#include <cmath>

namespace cbdev {

namespace {

inline int64_t to_ns(ClockSync::time_point tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

inline ClockSync::time_point from_ns(int64_t ns) {
    return ClockSync::time_point(std::chrono::nanoseconds(ns));
}

} // anonymous namespace

ClockSync::ClockSync()
    : m_config{} {}

ClockSync::ClockSync(Config config)
    : m_config(std::move(config)) {}

void ClockSync::addProbeSample(time_point t1_local, uint64_t t3_device_ns, time_point t4_local) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const int64_t t1_ns = to_ns(t1_local);
    const int64_t t4_ns = to_ns(t4_local);
    const int64_t rtt_ns = t4_ns - t1_ns;

    // offset = T3 - T1 - α * RTT
    const int64_t offset_ns = static_cast<int64_t>(t3_device_ns) - t1_ns
        - static_cast<int64_t>(std::round(m_config.forward_delay_fraction * rtt_ns));

    // Detect device clock wrap (e.g., nPlayServer file loop).  If the new
    // offset differs from the current estimate by more than 1 second, the
    // device clock has jumped and all old probes are stale.
    if (m_current_offset_ns) {
        const int64_t drift = offset_ns - *m_current_offset_ns;
        if (std::abs(drift) > 1'000'000'000LL) {
            m_probe_samples.clear();
        }
    }

    ProbeSample sample;
    sample.offset_ns = offset_ns;
    sample.rtt_ns = rtt_ns;
    sample.when = t4_local;

    m_probe_samples.push_back(sample);

    // Cap the number of stored probes
    while (m_probe_samples.size() > m_config.max_probe_samples) {
        m_probe_samples.pop_front();
    }

    pruneExpired(t4_local);
    recomputeEstimate();
}

void ClockSync::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_probe_samples.clear();
    m_current_offset_ns = std::nullopt;
    m_current_uncertainty_ns = std::nullopt;
}

std::optional<ClockSync::time_point> ClockSync::toLocalTime(uint64_t device_time_ns) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current_offset_ns)
        return std::nullopt;
    return from_ns(static_cast<int64_t>(device_time_ns) - *m_current_offset_ns);
}

std::optional<uint64_t> ClockSync::toDeviceTime(time_point local_time) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current_offset_ns)
        return std::nullopt;
    return static_cast<uint64_t>(to_ns(local_time) + *m_current_offset_ns);
}

bool ClockSync::hasSyncData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_offset_ns.has_value();
}

std::optional<int64_t> ClockSync::getOffsetNs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_offset_ns;
}

std::optional<int64_t> ClockSync::getUncertaintyNs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_uncertainty_ns;
}

bool ClockSync::probesAreReliable() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return probeSpreadOk();
}

void ClockSync::addDataPacketSample(const uint64_t device_time_ns, const time_point recv_local) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const int64_t recv_ns = to_ns(recv_local);
    // offset = device_time - recv_time.
    // This overestimates the true offset by the one-way network delay
    // (host→device path isn't measured).  Over many samples the minimum
    // offset will approach the true value because the minimum corresponds
    // to the least-queued packet.
    const int64_t offset_ns = static_cast<int64_t>(device_time_ns) - recv_ns;

    // Detect clock jumps (same logic as addProbeSample)
    if (m_current_offset_ns && std::abs(offset_ns - *m_current_offset_ns) > 1'000'000'000LL) {
        m_data_samples.clear();
    }

    DataSample ds;
    ds.offset_ns = offset_ns;
    ds.when = recv_local;
    m_data_samples.push_back(ds);

    while (m_data_samples.size() > m_config.max_probe_samples * 4) {
        m_data_samples.pop_front();
    }

    // Prune old entries
    const auto cutoff = recv_local - m_config.max_probe_age;
    while (!m_data_samples.empty() && m_data_samples.front().when < cutoff) {
        m_data_samples.pop_front();
    }

    // Pick minimum offset (closest to true offset = least network delay).
    // Add a constant to compensate for the one-way acquisition-to-
    // transmission delay that this method can't measure.  Without
    // this correction the converted timestamps appear in the future.
    // 0.7 ms is the observed lower bound on the device's capture-to-
    // send latency (ADC sample → UDP datagram on the wire).
    if (!m_data_samples.empty()) {
        constexpr int64_t ONE_WAY_DELAY_ESTIMATE_NS = 700'000;  // 0.7 ms
        int64_t best = m_data_samples.front().offset_ns;
        for (const auto& s : m_data_samples)
            best = std::min(best, s.offset_ns);
        m_current_offset_ns = best + ONE_WAY_DELAY_ESTIMATE_NS;
        m_current_uncertainty_ns = ONE_WAY_DELAY_ESTIMATE_NS;
    }
}

void ClockSync::recomputeEstimate() {
    // If data-packet fallback is active, don't overwrite it with bad probe data.
    if (!m_data_samples.empty() && !probeSpreadOk())
        return;

    // Find probe with minimum RTT — least queuing/jitter gives most reliable estimate
    const ProbeSample* best_probe = nullptr;
    for (const auto& p : m_probe_samples) {
        if (!best_probe || p.rtt_ns < best_probe->rtt_ns) {
            best_probe = &p;
        }
    }

    if (best_probe) {
        m_current_offset_ns = best_probe->offset_ns;
        m_current_uncertainty_ns = best_probe->rtt_ns / 2;
    } else if (m_data_samples.empty()) {
        // Only clear if no data-packet fallback either
        m_current_offset_ns = std::nullopt;
        m_current_uncertainty_ns = std::nullopt;
    }
}

bool ClockSync::probeSpreadOk() const {
    // Internal version — called with m_mutex already held.
    if (m_probe_samples.size() < 3)
        return true;
    int64_t lo = m_probe_samples.front().offset_ns;
    int64_t hi = lo;
    for (const auto& p : m_probe_samples) {
        lo = std::min(lo, p.offset_ns);
        hi = std::max(hi, p.offset_ns);
    }
    constexpr int64_t RELIABLE_THRESHOLD_NS = 10'000'000;  // 10 ms
    return (hi - lo) < RELIABLE_THRESHOLD_NS;
}

void ClockSync::pruneExpired(time_point now) {
    const auto probe_cutoff = now - m_config.max_probe_age;
    while (!m_probe_samples.empty() && m_probe_samples.front().when < probe_cutoff) {
        m_probe_samples.pop_front();
    }
}

} // namespace cbdev
