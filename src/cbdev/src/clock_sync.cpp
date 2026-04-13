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
#include <numeric>
#include <vector>

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
    m_data_samples.clear();
    m_data_floor_ns = std::nullopt;
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
    // raw_offset = device_time - recv_time = true_offset - one_way_delay.
    // The maximum raw_offset (minimum one-way delay) is closest to the
    // true offset, regardless of clock epoch.
    const int64_t offset_ns = static_cast<int64_t>(device_time_ns) - recv_ns;

    // Detect clock jumps (same logic as addProbeSample)
    if (m_current_offset_ns && std::abs(offset_ns - *m_current_offset_ns) > 1'000'000'000LL) {
        m_data_samples.clear();
        m_data_floor_ns = std::nullopt;
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

    // Compute fallback offset: max raw_offset with gap-based glitch
    // rejection, plus a small correction for residual network delay.
    // Recomputed from the current window each time (no permanent floor).
    if (!m_data_samples.empty()) {
        constexpr int64_t ONE_WAY_DELAY_ESTIMATE_NS = 700'000;  // 0.7 ms
        constexpr int64_t DATA_GAP_THRESHOLD_NS = 5'000'000;    // 5 ms

        // Sort offsets to find the glitch-filtered max.
        std::vector<int64_t> offsets;
        offsets.reserve(m_data_samples.size());
        for (const auto& s : m_data_samples)
            offsets.push_back(s.offset_ns);
        std::sort(offsets.begin(), offsets.end());

        // Strip isolated upward outliers (same gap logic as probes).
        size_t top = offsets.size();
        while (top > 1) {
            if (offsets[top - 1] - offsets[top - 2] > DATA_GAP_THRESHOLD_NS)
                --top;
            else
                break;
        }

        m_data_floor_ns = offsets[top - 1] + ONE_WAY_DELAY_ESTIMATE_NS;
    }

    recomputeEstimate();
}

void ClockSync::recomputeEstimate() {
    // Strategy:
    //   1. If probes are reliable (tight spread), use glitch-filtered
    //      max-offset probe.  This is the HUB1 path.
    //   2. Otherwise, if data-packet samples are available, use the
    //      glitch-filtered max raw_offset from data packets.  This is
    //      the NSP path — data-packet timestamps (from the ADC/PTP
    //      clock) are more stable than probe header->time on the NSP.
    //   3. If neither is available, use probes anyway (unreliable but
    //      better than nothing).

    // Check if probes are reliable enough to use directly.
    if (!m_probe_samples.empty() && probeSpreadOk()) {
        // Probes are tight — use glitch-filtered max-offset.
        std::vector<size_t> indices(m_probe_samples.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
                  [this](size_t a, size_t b) {
                      return m_probe_samples[a].offset_ns
                           < m_probe_samples[b].offset_ns;
                  });

        constexpr int64_t GAP_THRESHOLD_NS = 10'000'000;  // 10 ms
        size_t top = indices.size();
        while (top > 1) {
            if (m_probe_samples[indices[top - 1]].offset_ns -
                m_probe_samples[indices[top - 2]].offset_ns > GAP_THRESHOLD_NS)
                --top;
            else
                break;
        }

        const auto& best = m_probe_samples[indices[top - 1]];
        m_current_offset_ns = best.offset_ns;
        m_current_uncertainty_ns = best.rtt_ns / 2;
        return;
    }

    // Probes unreliable or absent — use data-packet fallback.
    if (m_data_floor_ns.has_value()) {
        m_current_offset_ns = *m_data_floor_ns;
        m_current_uncertainty_ns = 700'000;  // ONE_WAY_DELAY_ESTIMATE_NS
        return;
    }

    // Last resort: use probes even though they're unreliable.
    if (!m_probe_samples.empty()) {
        // Same glitch-filtered max-offset as above.
        std::vector<size_t> indices(m_probe_samples.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
                  [this](size_t a, size_t b) {
                      return m_probe_samples[a].offset_ns
                           < m_probe_samples[b].offset_ns;
                  });

        constexpr int64_t GAP_THRESHOLD_NS = 10'000'000;
        size_t top = indices.size();
        while (top > 1) {
            if (m_probe_samples[indices[top - 1]].offset_ns -
                m_probe_samples[indices[top - 2]].offset_ns > GAP_THRESHOLD_NS)
                --top;
            else
                break;
        }

        const auto& best = m_probe_samples[indices[top - 1]];
        m_current_offset_ns = best.offset_ns;
        m_current_uncertainty_ns = best.rtt_ns / 2;
        return;
    }

    m_current_offset_ns = std::nullopt;
    m_current_uncertainty_ns = std::nullopt;
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
