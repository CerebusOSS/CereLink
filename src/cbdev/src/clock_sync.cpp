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
            // Device clock jumped (e.g. nPlay file loop) — old estimate is
            // stale.  Drop history and force an immediate re-acquire so the
            // commit discipline doesn't lag the jump.
            m_probe_samples.clear();
            m_current_offset_ns = std::nullopt;
            m_pending_step_count = 0;
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
    m_pending_step_count = 0;
    m_committed_from_external = false;
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

void ClockSync::setExternalOffset(std::optional<int64_t> offset_ns,
                                   std::optional<int64_t> uncertainty_ns) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_external_offset_ns = offset_ns;
    m_external_uncertainty_ns = uncertainty_ns;
    // Re-evaluate: recomputeEstimate() adopts the external offset only when it
    // is sanity-consistent with internal evidence, and reverts to the internal
    // estimate when cleared or when the external offset is implausible.
    recomputeEstimate();
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
        m_current_offset_ns = std::nullopt;  // re-acquire immediately past a jump
        m_pending_step_count = 0;
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

ClockSync::InternalEstimate ClockSync::computeInternalEstimate() const {
    // Glitch-filtered max-offset probe pick (least-queued probe).  Returns
    // {offset, uncertainty=rtt/2} for the selected probe.
    const auto bestProbe = [this]() -> InternalEstimate {
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
        InternalEstimate e;
        e.offset_ns = best.offset_ns;
        e.uncertainty_ns = best.rtt_ns / 2;
        return e;
    };

    // Strategy:
    //   1. If probes are reliable (enough samples, tight spread), use the
    //      glitch-filtered max-offset probe.  This is the HUB1 path.
    //   2. Otherwise, if data-packet samples are available, use the
    //      glitch-filtered max raw_offset from data packets.  This is
    //      the NSP path — data-packet timestamps (from the ADC/PTP
    //      clock) are more stable than probe header->time on the NSP.
    //   3. If neither is available, use probes anyway (unreliable but
    //      better than nothing).
    if (!m_probe_samples.empty() && probeSpreadOk())
        return bestProbe();

    if (m_data_floor_ns.has_value()) {
        InternalEstimate e;
        e.offset_ns = *m_data_floor_ns;
        e.uncertainty_ns = 700'000;  // ONE_WAY_DELAY_ESTIMATE_NS
        return e;
    }

    if (!m_probe_samples.empty())
        return bestProbe();

    return InternalEstimate{};  // offset_ns == nullopt
}

bool ClockSync::externalPlausible(int64_t external_ns,
                                  const InternalEstimate& internal) const {
    // With no own evidence to check against, trust the peer (cold start).
    if (!internal.offset_ns)
        return true;
    const int64_t diff = std::llabs(external_ns - *internal.offset_ns);
    const int64_t band = std::max<int64_t>(
        m_config.external_plausibility_band_ns,
        static_cast<int64_t>(m_config.external_unc_k * internal.uncertainty_ns));
    return diff <= band;
}

void ClockSync::recomputeEstimate() {
    const InternalEstimate internal = computeInternalEstimate();

    // An external (peer-borrowed) offset is treated as a candidate, not an
    // unconditional override.  It is adopted only when it is sanity-consistent
    // with our own evidence, and — because this runs on every probe/data
    // sample — it is re-checked against fresh internal evidence each time, so a
    // stale external offset cannot latch.  An implausible external offset
    // (e.g. a wrong-epoch value that would leak the raw device clock) is
    // rejected and the internal estimate is used instead.
    if (m_external_offset_ns && externalPlausible(*m_external_offset_ns, internal)) {
        // A plausible peer offset is authoritative (and already disciplined at
        // its source), so adopt it directly.
        m_current_offset_ns = m_external_offset_ns;
        m_current_uncertainty_ns = m_external_uncertainty_ns
            ? m_external_uncertainty_ns
            : std::optional<int64_t>(internal.uncertainty_ns);
        m_pending_step_count = 0;
        m_committed_from_external = true;
        return;
    }

    if (internal.offset_ns) {
        // A change of source (external->internal, or first acquisition) is a
        // deliberate event, not jitter — re-acquire directly.  Only the same
        // (internal) source's drift is run through the commit discipline.
        if (m_committed_from_external || !m_current_offset_ns.has_value()) {
            m_current_offset_ns = *internal.offset_ns;
            m_current_uncertainty_ns = internal.uncertainty_ns;
            m_pending_step_count = 0;
        } else {
            commitDisciplined(*internal.offset_ns, internal.uncertainty_ns);
        }
        m_committed_from_external = false;
    } else {
        m_current_offset_ns = std::nullopt;
        m_current_uncertainty_ns = std::nullopt;
        m_pending_step_count = 0;
        m_committed_from_external = false;
    }
}

void ClockSync::commitDisciplined(int64_t target_offset_ns, int64_t uncertainty_ns) {
    // Cold start: nothing committed yet — accept the target as-is.
    if (!m_current_offset_ns) {
        m_current_offset_ns = target_offset_ns;
        m_current_uncertainty_ns = uncertainty_ns;
        m_pending_step_count = 0;
        return;
    }

    const int64_t residual = target_offset_ns - *m_current_offset_ns;
    const int64_t mag = std::llabs(residual);

    if (mag <= m_config.commit_deadband_ns) {
        // Small change: apply exactly.
        m_current_offset_ns = target_offset_ns;
        m_current_uncertainty_ns = uncertainty_ns;
        m_pending_step_count = 0;
    } else if (mag <= m_config.slew_max_ns) {
        // Medium change: slew a fraction of the residual toward the target.
        // A transient spike is damped; a sustained change is tracked over
        // several samples.
        m_current_offset_ns = *m_current_offset_ns
            + static_cast<int64_t>(m_config.slew_gain * static_cast<double>(residual));
        m_current_uncertainty_ns = uncertainty_ns;
        m_pending_step_count = 0;
    } else {
        // Large jump: only accept once it persists, so a one-off outlier does
        // not move the committed offset.
        if (++m_pending_step_count >= m_config.step_persist) {
            m_current_offset_ns = target_offset_ns;
            m_current_uncertainty_ns = uncertainty_ns;
            m_pending_step_count = 0;
        }
        // else: hold the current offset, ignore this sample.
    }
}

bool ClockSync::probeSpreadOk() const {
    // Internal version — called with m_mutex already held.
    // Require a minimum number of probes: one (or a wild pair of) probe is not
    // enough evidence to declare the probe path reliable.
    if (m_probe_samples.size() < m_config.min_reliable_probes)
        return false;
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
