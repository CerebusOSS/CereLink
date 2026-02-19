///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   clock_sync.h
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Device clock to host steady_clock offset estimator
///
/// Estimates the offset between the device's nanosecond clock and the host's
/// std::chrono::steady_clock using a combination of request-response probes and
/// passive one-way monitoring (inspired by NTP).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_CLOCK_SYNC_H
#define CBDEV_CLOCK_SYNC_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

namespace cbdev {

class ClockSync {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    struct Config {
        std::chrono::milliseconds one_way_window = std::chrono::milliseconds{10000};
        size_t max_probe_samples = 20;
        std::chrono::seconds max_probe_age = std::chrono::seconds{300};
    };

    ClockSync();
    explicit ClockSync(Config config);

    /// Add a request-response probe sample.
    /// @param t1_local  Host time just before sending the probe request
    /// @param t3_device_ns  Device timestamp from the response header (nanoseconds)
    /// @param t4_local  Host time just after receiving the response
    void addProbeSample(time_point t1_local, uint64_t t3_device_ns, time_point t4_local);

    /// Add a passive one-way sample (e.g. from SYSPROTOCOLMONITOR).
    /// @param device_time_ns  Device timestamp from the packet header (nanoseconds)
    /// @param local_recv_time  Host time when the packet was received
    void addOneWaySample(uint64_t device_time_ns, time_point local_recv_time);

    /// Convert device timestamp to host steady_clock time_point.
    [[nodiscard]] std::optional<time_point> toLocalTime(uint64_t device_time_ns) const;

    /// Convert host steady_clock time_point to device timestamp (nanoseconds).
    [[nodiscard]] std::optional<uint64_t> toDeviceTime(time_point local_time) const;

    /// Returns true if at least one sample has been ingested.
    [[nodiscard]] bool hasSyncData() const;

    /// Current offset estimate: device_ns - local_ns.
    [[nodiscard]] std::optional<int64_t> getOffsetNs() const;

    /// Uncertainty (half-RTT) from the best probe, or INT64_MAX for one-way only.
    [[nodiscard]] std::optional<int64_t> getUncertaintyNs() const;

private:
    mutable std::mutex m_mutex;
    Config m_config;

    struct ProbeSample {
        int64_t offset_ns;
        int64_t uncertainty_ns;
        time_point when;
    };

    struct OneWaySample {
        int64_t raw_offset_ns;
        time_point when;
    };

    std::deque<ProbeSample> m_probe_samples;
    std::deque<OneWaySample> m_one_way_samples;

    std::optional<int64_t> m_current_offset_ns;
    std::optional<int64_t> m_current_uncertainty_ns;

    void recomputeEstimate();   // called with lock held
    void pruneExpired(time_point now);  // called with lock held
};

} // namespace cbdev

#endif // CBDEV_CLOCK_SYNC_H
