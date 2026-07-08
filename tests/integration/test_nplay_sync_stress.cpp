///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_nplay_sync_stress.cpp
/// @author CereLink Development Team
///
/// @brief  Stress tests for nPlayServer clock synchronization
///
/// nPlayServer replays a recording (or an LSL stream) and stamps each packet
/// with a DEVICE clock that is NOT a real wall clock: for file playback it is
/// "samples since the start of the file", which RESETS to ~0 every time the
/// short test file wraps.  CereLink's ClockSync maps that device clock onto the
/// host steady_clock; downstream consumers (e.g. ezmsg-blackrock RESAMPLE/MERGE)
/// then require the resulting HOST-mapped timeline to be MONOTONIC.
///
/// The failure mode these tests guard against (observed in the cursor pipeline,
/// 2026-06): nPlay's host-mapped timestamps jump BACKWARD by ~0.6 s *within* a
/// single playback pass — the offset estimate recalibrates against a replay
/// clock that does not advance at true wall-rate — which stalls any consumer
/// that assumes monotonic time.  A real Hub on a stable PTP clock does not do
/// this, so the symptom only appears when nPlay feeds a monotonic-time consumer.
///
/// Two scenarios:
///   1. SINGLE nPlay (always runs when the nPlayServer binary + test data are
///      available, like the other integration tests): stamp every data batch
///      with toLocalTime(first_ts) — exactly as a consumer does — over a long
///      run that crosses many file wraps, then assert the host-mapped timeline
///      is monotonic WITHIN each no-wrap segment (the wrap itself is a legitimate
///      device-clock reset and is excluded).
///
///   2. nPlay + a LIVE Hub2 (hardware-gated, opt-in): the actual cross-device
///      merge topology.  Brings up an nPlay session AND a real Hub2 session
///      concurrently and asserts BOTH host-mapped timelines stay monotonic and
///      that each device's current time maps to ~now() — so nPlay's replay clock
///      cannot silently diverge from the real Hub while a merge consumes both.
///      SKIPPED unless CERELINK_HW_MULTIDEVICE is set (bringing up a live Hub
///      runs a handshake that can HARDRESET it — disruptive on a recording rig).
///
/// The wrap-aware, segmented monotonicity check is intentional: a naive "offset
/// is constant" assertion would false-positive at every file wrap, where the
/// committed offset MUST step by the file length to keep current device time
/// mapped to ~now.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "nplay_fixture.h"

#include <cbproto/cbproto.h>
#include <cbsdk/sdk_session.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace cbsdk;

namespace {

/// Channels to stream at 30 kHz while measuring the timeline.
constexpr uint32_t N_CHANS = 8;

/// Seconds to wait after configuring channels for the first clock probe / data
/// fallback to produce an offset (the SDK probes at handshake then every ~2 s).
constexpr int SYNC_WARMUP_S = 3;

/// A backward jump in DEVICE time larger than this marks a file wrap (the replay
/// counter resetting to ~0), not a glitch.  The dnss256 test file is ~2 s, so a
/// wrap drops device time by ~2e9 ns; a within-pass sample step is ~3e4 ns.
constexpr int64_t WRAP_BACK_NS = 50'000'000;  // 50 ms

/// After a wrap the committed offset must STEP to track the reset device clock;
/// the commit discipline (step_persist / stepout_samples) takes a few samples to
/// re-acquire.  Ignore host-mapped samples within this much DEVICE time of the
/// start of a segment so the legitimate re-acquisition is not counted as a bug.
constexpr int64_t POST_WRAP_GUARD_NS = 150'000'000;  // 150 ms

/// Maximum tolerated BACKWARD jump in the host-mapped timeline within a no-wrap
/// segment.  Healthy localhost sync keeps the offset stable to well under a
/// millisecond, so the host-mapped time tracks device time ~1:1 and never steps
/// back.  The guarded-against bug jumps ~600 ms; this threshold sits ~30x below
/// that while tolerating normal sub-deadband offset corrections.
constexpr int64_t MONO_BACKSTEP_NS = 20'000'000;  // 20 ms

/// Only evaluate segments whose post-guard device span exceeds this, so a
/// partial segment captured at the very start/end of the run is not judged.
constexpr int64_t MIN_SEGMENT_SPAN_NS = 300'000'000;  // 300 ms

/// One captured data batch: the first sample's device time and the host time it
/// mapped to, computed live (in the callback) just as a consumer would stamp it.
struct TimelineSample {
    uint64_t dev_ns;     ///< first device timestamp in the batch (nanoseconds)
    int64_t host_ns;     ///< toLocalTime(dev_ns) as ns since the steady epoch
    bool host_valid;     ///< false if no clock offset was available yet
};

/// Result of walking a captured timeline and checking within-segment monotonicity.
struct TimelineStats {
    size_t n_samples = 0;       ///< valid (synced) samples evaluated
    size_t n_wraps = 0;         ///< device-clock resets detected
    size_t n_segments = 0;      ///< no-wrap segments long enough to evaluate
    size_t n_backsteps = 0;     ///< within-segment backward jumps > MONO_BACKSTEP_NS
    int64_t max_backstep_ns = 0;///< largest within-segment backward jump seen (>=0)
};

/// Capture a continuous device→host timeline by stamping each 30 kHz data batch
/// with toLocalTime(first_ts), the way an acquisition consumer does.
class TimelineCapture {
public:
    /// Register the batch callback on @p session.  Stamps run on the callback
    /// thread; both conversion paths are internally locked, so this is safe.
    ///
    /// @param stream_id  <0 (default): stamp with the stateless toLocalTime(),
    ///   the raw mapping a naive consumer uses.  >=0: stamp with the monotonic
    ///   toLocalTimeBatch() on that stream_id, which enforces a non-decreasing
    ///   host timeline (resetting only across a clock re-sync / file wrap).
    explicit TimelineCapture(SdkSession& session, int64_t stream_id = -1) {
        m_handle = session.registerGroupBatchCallback(
            SampleRate::SR_30kHz,
            [this, &session, stream_id](const int16_t*, size_t n_samples, size_t,
                                        const uint64_t* timestamps) {
                if (n_samples == 0) return;
                const uint64_t dev_ns = timestamps[0];
                TimelineSample s;
                s.dev_ns = dev_ns;
                if (stream_id < 0) {
                    auto local = session.toLocalTime(dev_ns);
                    s.host_valid = local.has_value();
                    s.host_ns = local.has_value()
                        ? std::chrono::duration_cast<std::chrono::nanoseconds>(
                              local->time_since_epoch()).count()
                        : 0;
                } else {
                    int64_t out = 0;
                    s.host_valid =
                        session.toLocalTimeBatch(stream_id, &dev_ns, &out, 1);
                    s.host_ns = s.host_valid ? out : 0;
                }
                std::lock_guard<std::mutex> lock(m_mutex);
                m_samples.push_back(s);
            });
    }

    std::vector<TimelineSample> drain() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_samples;
    }

private:
    std::mutex m_mutex;
    std::vector<TimelineSample> m_samples;
    CallbackHandle m_handle = 0;
};

/// Evaluate one no-wrap segment [begin, end) and fold its findings into @p st.
/// (Defined below; forward-declared so analyzeTimeline can call it.)
void analyzeSegment(const std::vector<TimelineSample>& samples,
                    size_t begin, size_t end, TimelineStats& st,
                    int64_t backstep_threshold);

/// Walk a captured timeline: split it into no-wrap segments (a wrap is a large
/// backward jump in DEVICE time), skip a short re-acquisition guard at the start
/// of each segment, and count BACKWARD jumps in the host-mapped time that exceed
/// @p backstep_threshold.  Healthy sync produces zero.  The raw mapping uses a
/// tolerant threshold (sub-deadband corrections are allowed); the monotonic
/// mapping is checked with threshold 0 (it must never step back at all).
TimelineStats analyzeTimeline(const std::vector<TimelineSample>& samples,
                              int64_t backstep_threshold = MONO_BACKSTEP_NS) {
    TimelineStats st;

    // First pass: locate segment boundaries from the device-time series alone,
    // independent of whether the offset was available at that instant.
    size_t seg_start = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const bool wrap = i > 0 &&
            static_cast<int64_t>(samples[i].dev_ns) -
            static_cast<int64_t>(samples[i - 1].dev_ns) < -WRAP_BACK_NS;
        if (wrap) {
            ++st.n_wraps;
            analyzeSegment(samples, seg_start, i, st, backstep_threshold);
            seg_start = i;
        }
    }
    analyzeSegment(samples, seg_start, samples.size(), st, backstep_threshold);
    return st;
}

void analyzeSegment(const std::vector<TimelineSample>& samples,
                    size_t begin, size_t end, TimelineStats& st,
                    int64_t backstep_threshold) {
    if (end <= begin) return;
    const uint64_t seg_dev_start = samples[begin].dev_ns;

    // Span check uses the guarded portion only.
    bool have_prev = false;
    int64_t prev_host = 0;
    int64_t seg_first_eval = -1, seg_last_eval = -1;
    size_t seg_samples = 0, seg_backsteps = 0;
    int64_t seg_max_backstep = 0;

    for (size_t i = begin; i < end; ++i) {
        const auto& s = samples[i];
        if (!s.host_valid) { have_prev = false; continue; }
        // Skip the post-wrap re-acquisition window.
        if (static_cast<int64_t>(s.dev_ns - seg_dev_start) < POST_WRAP_GUARD_NS) {
            have_prev = false;
            continue;
        }
        if (seg_first_eval < 0) seg_first_eval = static_cast<int64_t>(s.dev_ns);
        seg_last_eval = static_cast<int64_t>(s.dev_ns);
        ++seg_samples;

        if (have_prev) {
            const int64_t backstep = prev_host - s.host_ns;  // >0 = went backward
            if (backstep > seg_max_backstep) seg_max_backstep = backstep;
            if (backstep > backstep_threshold) ++seg_backsteps;
        }
        prev_host = s.host_ns;
        have_prev = true;
    }

    // Ignore segments too short (post-guard) to judge meaningfully.
    if (seg_first_eval < 0 || (seg_last_eval - seg_first_eval) < MIN_SEGMENT_SPAN_NS)
        return;

    ++st.n_segments;
    st.n_samples += seg_samples;
    st.n_backsteps += seg_backsteps;
    if (seg_max_backstep > st.max_backstep_ns) st.max_backstep_ns = seg_max_backstep;
}

Result<SdkSession> createSession(DeviceType type) {
    SdkConfig config;
    config.device_type = type;
    return SdkSession::create(config);
}

bool hardwareHub2Enabled() {
    const char* v = std::getenv("CERELINK_HW_MULTIDEVICE");
    return v != nullptr && v[0] != '\0';
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
// Scenario 1: single nPlay — host-mapped timeline monotonic across a long run
///////////////////////////////////////////////////////////////////////////////////////////////////

class NPlaySyncStressTest : public NPlayFixture {};

// Stamp every 30 kHz batch with toLocalTime(first_ts) over ~20 s of playback
// (many file wraps) and require the host-mapped timeline to be monotonic within
// each no-wrap segment.  This is the direct regression guard for the ±0.6 s
// backward jumps that stall downstream RESAMPLE/MERGE.
TEST_F(NPlaySyncStressTest, HostMappedTimelineMonotonicAcrossLongPlayback) {
    auto result = createSession(DeviceType::NPLAY);
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    ASSERT_TRUE(session.setSampleGroup(N_CHANS, ChannelType::FRONTEND,
                                       SampleRate::SR_30kHz, true).isOk());
    // Let channels configure and the first offset settle before capturing.
    std::this_thread::sleep_for(std::chrono::seconds(SYNC_WARMUP_S));

    TimelineCapture capture(session);
    std::this_thread::sleep_for(std::chrono::seconds(20));
    session.stop();

    const auto samples = capture.drain();
    ASSERT_GT(samples.size(), 1000u)
        << "Too few batches captured (" << samples.size() << ") — no data flowing?";

    const TimelineStats st = analyzeTimeline(samples);

    std::printf("=== nPlay timeline: %zu batches, %zu wraps, %zu segments, "
                "%zu backstep(s) > %lld ms, max backstep = %.3f ms ===\n",
                samples.size(), st.n_wraps, st.n_segments, st.n_backsteps,
                static_cast<long long>(MONO_BACKSTEP_NS / 1'000'000),
                static_cast<double>(st.max_backstep_ns) / 1e6);
    std::fflush(stdout);

    // Sanity: the run must actually exercise wraps and at least one real segment,
    // otherwise the monotonicity assertion below is vacuous.
    EXPECT_GT(st.n_wraps, 0u) << "Expected the short test file to wrap during 20 s";
    ASSERT_GT(st.n_segments, 0u) << "No evaluable no-wrap segment captured";

    EXPECT_EQ(st.n_backsteps, 0u)
        << st.n_backsteps << " within-pass backward jump(s) in the host-mapped "
        << "timeline (max " << static_cast<double>(st.max_backstep_ns) / 1e6
        << " ms). nPlay's offset is recalibrating against the replay clock — "
        << "this is the non-monotonic-timestamp bug that stalls RESAMPLE/MERGE.";
}

// Same long playback, but stamped through the MONOTONIC batch API
// (toLocalTimeBatch with a stream_id).  Unlike the raw mapping above — which is
// only monotonic WITHIN a no-wrap segment and tolerates sub-deadband wiggle —
// the monotonic stream must NEVER step backward within a segment (threshold 0).
// At each file wrap the discontinuity epoch advances and the floor resets, so a
// single backward step per wrap is expected and excluded by the segmentation.
TEST_F(NPlaySyncStressTest, MonotonicStreamNeverStepsBackWithinSegment) {
    auto result = createSession(DeviceType::NPLAY);
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    ASSERT_TRUE(session.setSampleGroup(N_CHANS, ChannelType::FRONTEND,
                                       SampleRate::SR_30kHz, true).isOk());
    std::this_thread::sleep_for(std::chrono::seconds(SYNC_WARMUP_S));

    constexpr int64_t kMonoStream = 1;  // 30 kHz raw stream
    TimelineCapture capture(session, kMonoStream);
    std::this_thread::sleep_for(std::chrono::seconds(20));
    session.stop();

    const auto samples = capture.drain();
    ASSERT_GT(samples.size(), 1000u)
        << "Too few batches captured (" << samples.size() << ") — no data flowing?";

    // Threshold 0: the monotonic timeline must be exactly non-decreasing.
    const TimelineStats st = analyzeTimeline(samples, /*backstep_threshold=*/0);

    std::printf("=== nPlay MONOTONIC timeline: %zu batches, %zu wraps, %zu segments, "
                "%zu backstep(s), max backstep = %.6f ms ===\n",
                samples.size(), st.n_wraps, st.n_segments, st.n_backsteps,
                static_cast<double>(st.max_backstep_ns) / 1e6);
    std::fflush(stdout);

    EXPECT_GT(st.n_wraps, 0u) << "Expected the short test file to wrap during 20 s";
    ASSERT_GT(st.n_segments, 0u) << "No evaluable no-wrap segment captured";

    EXPECT_EQ(st.n_backsteps, 0u)
        << st.n_backsteps << " backward step(s) in the MONOTONIC host timeline "
        << "(max " << static_cast<double>(st.max_backstep_ns) / 1e6 << " ms). "
        << "toLocalTimeBatch(stream_id) must clamp every within-segment backstep.";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Scenario 2: nPlay + a LIVE Hub2 — both timelines stay monotonic and host-anchored
///////////////////////////////////////////////////////////////////////////////////////////////////

// The real cross-device merge topology: nPlay (first/reference device) plus a
// real Hub2.  If nPlay's host-mapped timeline goes non-monotonic while Hub2's
// stays clean, a consumer resampling one onto the other stalls.  Hardware-gated.
TEST_F(NPlaySyncStressTest, NPlayAndLiveHub2TimelinesStayMonotonic) {
    if (!hardwareHub2Enabled()) {
        GTEST_SKIP() << "Set CERELINK_HW_MULTIDEVICE=1 to run against a live Hub2 "
                        "alongside nPlay (the handshake may HARDRESET the Hub).";
    }

    auto nplay = createSession(DeviceType::NPLAY);
    ASSERT_TRUE(nplay.isOk()) << "nPlay: " << nplay.error();
    auto hub2 = createSession(DeviceType::HUB2);
    ASSERT_TRUE(hub2.isOk()) << "Hub2: " << hub2.error();

    auto& nplay_s = nplay.value();
    auto& hub2_s = hub2.value();

    ASSERT_TRUE(nplay_s.setSampleGroup(N_CHANS, ChannelType::FRONTEND,
                                       SampleRate::SR_30kHz, true).isOk());
    ASSERT_TRUE(hub2_s.setSampleGroup(N_CHANS, ChannelType::FRONTEND,
                                      SampleRate::SR_30kHz, true).isOk());
    std::this_thread::sleep_for(std::chrono::seconds(SYNC_WARMUP_S));

    TimelineCapture nplay_cap(nplay_s);
    TimelineCapture hub2_cap(hub2_s);
    std::this_thread::sleep_for(std::chrono::seconds(15));
    nplay_s.stop();
    hub2_s.stop();

    const auto nplay_samples = nplay_cap.drain();
    const auto hub2_samples = hub2_cap.drain();
    ASSERT_GT(nplay_samples.size(), 1000u) << "No nPlay data flowing";
    ASSERT_GT(hub2_samples.size(), 1000u) << "No Hub2 data flowing";

    const TimelineStats ns = analyzeTimeline(nplay_samples);
    const TimelineStats hs = analyzeTimeline(hub2_samples);

    std::printf("=== nPlay: %zu seg, %zu backstep(s), max %.3f ms | "
                "Hub2: %zu seg, %zu backstep(s), max %.3f ms ===\n",
                ns.n_segments, ns.n_backsteps,
                static_cast<double>(ns.max_backstep_ns) / 1e6,
                hs.n_segments, hs.n_backsteps,
                static_cast<double>(hs.max_backstep_ns) / 1e6);
    std::fflush(stdout);

    ASSERT_GT(ns.n_segments, 0u) << "No evaluable nPlay segment";
    ASSERT_GT(hs.n_segments, 0u) << "No evaluable Hub2 segment";

    EXPECT_EQ(hs.n_backsteps, 0u)
        << "Live Hub2 host-mapped timeline went non-monotonic (max "
        << static_cast<double>(hs.max_backstep_ns) / 1e6 << " ms) — unexpected "
        << "for a real PTP clock; check the rig before blaming nPlay.";
    EXPECT_EQ(ns.n_backsteps, 0u)
        << ns.n_backsteps << " within-pass backward jump(s) in nPlay's host-mapped "
        << "timeline (max " << static_cast<double>(ns.max_backstep_ns) / 1e6
        << " ms) while a real Hub2 stayed monotonic — the cross-device merge "
        << "stall reproduces here.";
}
