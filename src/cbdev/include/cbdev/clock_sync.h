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

        // Minimum number of probes before probesAreReliable() can report true.
        // A single (or a wild pair of) probe is not enough evidence to commit
        // an offset from the probe path.
        size_t min_reliable_probes = 3;

        // Plausibility bound for an externally-injected (peer-borrowed) offset.
        // An external offset is only adopted if it agrees with the device's own
        // internal evidence (probe/data) to within
        //   max(external_plausibility_band_ns, external_unc_k * internal_uncertainty).
        // This separates a legitimate sub-second offset difference from a
        // wrong-epoch / raw-PTP-clock value (off by ~1.77e18 ns).
        int64_t external_plausibility_band_ns = 1'000'000'000; // 1 s
        double  external_unc_k = 4.0;

        // Offset-commit discipline for the internal (probe/data) estimate, to
        // damp per-sample jitter and transient jumps.  A change within
        // commit_deadband_ns is applied as-is; a larger change up to slew_max_ns
        // is slewed by slew_gain of the residual per sample; a change beyond
        // slew_max_ns is treated as a step and applied only after it persists
        // for step_persist consecutive samples.
        int64_t commit_deadband_ns = 1'000'000;   // 1 ms
        int64_t slew_max_ns        = 50'000'000;  // 50 ms
        double  slew_gain          = 0.2;
        int     step_persist       = 3;

        // Backstop: if the committed offset stays more than commit_deadband_ns
        // away from incoming evidence for this many consecutive samples (slew
        // and step having failed to reconcile it), re-acquire to the current
        // estimate.  Recovers a committed offset that would otherwise stay
        // stuck when there is no peer to break the tie.
        int     stepout_samples    = 25;
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

    /// Offset from this device's OWN evidence only (probes/data), ignoring any
    /// injected external/peer offset.  Provides an independent vote for
    /// cross-device clock consensus.
    [[nodiscard]] std::optional<int64_t> getInternalOffsetNs() const;

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

    /// Inject an externally-determined offset (e.g., from a peer device's
    /// shared memory).  The external offset is treated as a CANDIDATE, not an
    /// unconditional override: it is adopted only when it is sanity-consistent
    /// with the device's own internal evidence (see Config::external_*), and is
    /// re-evaluated against fresh internal evidence on every update so it cannot
    /// latch to a stale value.  An implausible external offset (e.g. a stale
    /// wrong-epoch value that would leak the raw device clock) is rejected and
    /// the internal estimate is used instead.  Call with nullopt to clear.
    void setExternalOffset(std::optional<int64_t> offset_ns,
                           std::optional<int64_t> uncertainty_ns = std::nullopt);

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
    int m_pending_step_count = 0;       // consecutive large-jump samples seen
    int m_nonconverged_streak = 0;      // consecutive samples not within deadband
    bool m_committed_from_external = false;  // committed value came from a peer offset

    // External offset injected by setExternalOffset() — takes priority
    // over internal estimates when set.
    std::optional<int64_t> m_external_offset_ns;
    std::optional<int64_t> m_external_uncertainty_ns;

    // Internal (own-evidence) estimate, independent of any external offset.
    struct InternalEstimate {
        std::optional<int64_t> offset_ns;
        int64_t uncertainty_ns = 0;
    };

    void recomputeEstimate();              // called with lock held
    void commitDisciplined(int64_t target_offset_ns, int64_t uncertainty_ns);  // lock held
    void resetDiscipline();                // zero the commit-discipline counters
    InternalEstimate computeInternalEstimate() const;  // called with lock held
    bool externalPlausible(int64_t external_ns,
                           const InternalEstimate& internal) const;  // lock held
    void pruneExpired(time_point now);     // called with lock held
    bool probeSpreadOk() const;            // called with lock held
};

} // namespace cbdev

#endif // CBDEV_CLOCK_SYNC_H
