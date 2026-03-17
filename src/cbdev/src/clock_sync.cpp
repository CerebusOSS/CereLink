///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   clock_sync.cpp
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Implementation of ClockSync offset estimator (probes-only with asymmetry correction)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "clock_sync.h"
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

void ClockSync::recomputeEstimate() {
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
    } else {
        m_current_offset_ns = std::nullopt;
        m_current_uncertainty_ns = std::nullopt;
    }
}

void ClockSync::pruneExpired(time_point now) {
    const auto probe_cutoff = now - m_config.max_probe_age;
    while (!m_probe_samples.empty() && m_probe_samples.front().when < probe_cutoff) {
        m_probe_samples.pop_front();
    }
}

} // namespace cbdev
