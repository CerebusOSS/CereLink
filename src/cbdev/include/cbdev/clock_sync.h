///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   clock_sync.h
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Device clock to host steady_clock offset estimator
///
/// Estimates the offset between the device's nanosecond clock and the host's
/// std::chrono::steady_clock using request-response probes.
///
/// Each probe gives T1 (host send), T3 (device timestamp), T4 (host recv).
/// Offset = T3 - T1 - α * (T4 - T1), where α is the forward delay fraction
/// (default 0.5 = symmetric NTP midpoint). The probe with minimum RTT is
/// selected as the best estimate, since minimum RTT implies the least
/// queuing/jitter.
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
        double forward_delay_fraction = 0.5; // α: assumed D1/(D1+D2)
        size_t max_probe_samples = 80;
        std::chrono::seconds max_probe_age = std::chrono::seconds{15};
    };

    ClockSync();
    explicit ClockSync(Config config);

    /// Add a request-response probe sample.
    /// @param t1_local  Host time just before sending the probe request
    /// @param t3_device_ns  Device timestamp from the response (nanoseconds)
    /// @param t4_local  Host time just after receiving the response
    void addProbeSample(time_point t1_local, uint64_t t3_device_ns, time_point t4_local);

    /// Convert device timestamp to host steady_clock time_point.
    [[nodiscard]] std::optional<time_point> toLocalTime(uint64_t device_time_ns) const;

    /// Convert host steady_clock time_point to device timestamp (nanoseconds).
    [[nodiscard]] std::optional<uint64_t> toDeviceTime(time_point local_time) const;

    /// Discard all probe samples and reset the offset estimate.
    void reset();

    /// Returns true if at least one probe sample has been ingested.
    [[nodiscard]] bool hasSyncData() const;

    /// Current offset estimate: device_ns - local_ns.
    [[nodiscard]] std::optional<int64_t> getOffsetNs() const;

    /// Uncertainty (RTT/2) from the best probe.
    [[nodiscard]] std::optional<int64_t> getUncertaintyNs() const;

    /// True if probes are producing consistent offsets (spread < threshold).
    /// When false, the caller should feed data-packet timestamps as a fallback.
    [[nodiscard]] bool probesAreReliable() const;

    /// Add a data-packet observation for fallback clock sync.
    /// @param device_time_ns  Device timestamp from a data packet header
    /// @param recv_local      Host time when the datagram was received
    /// Used only when probesAreReliable() returns false.
    void addDataPacketSample(uint64_t device_time_ns, time_point recv_local);

private:
    mutable std::mutex m_mutex;
    Config m_config;

    struct ProbeSample {
        int64_t offset_ns;       // T3 - T1 - α * RTT
        int64_t rtt_ns;          // T4 - T1
        time_point when;
    };

    std::deque<ProbeSample> m_probe_samples;

    struct DataSample {
        int64_t offset_ns;   // device_ns - recv_host_ns
        time_point when;
    };
    std::deque<DataSample> m_data_samples;
    std::optional<int64_t> m_data_floor_ns;  // monotonic floor for data fallback

    std::optional<int64_t> m_current_offset_ns;
    std::optional<int64_t> m_current_uncertainty_ns;

    void recomputeEstimate();       // called with lock held
    void pruneExpired(time_point now);  // called with lock held
    bool probeSpreadOk() const;     // called with lock held
};

} // namespace cbdev

#endif // CBDEV_CLOCK_SYNC_H
