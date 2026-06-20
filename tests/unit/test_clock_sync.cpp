///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_clock_sync.cpp
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Unit tests for cbdev::ClockSync (probes-only with configurable α)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cbdev/clock_sync.h"
#include "cbdev/device_session.h"  // deviceTimestampToNs
#include <chrono>
#include <cmath>
#include <cstdint>

using namespace cbdev;
using SteadyTP = ClockSync::time_point;

namespace {

// Helper: create a time_point from a raw nanosecond count
SteadyTP tp_from_ns(int64_t ns) {
    return SteadyTP(std::chrono::nanoseconds(ns));
}

// Helper: convert time_point to nanoseconds
int64_t tp_to_ns(SteadyTP tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
// Basic State
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, InitiallyNoSyncData) {
    ClockSync sync;

    EXPECT_FALSE(sync.hasSyncData());
    EXPECT_FALSE(sync.toLocalTime(1000).has_value());
    EXPECT_FALSE(sync.toDeviceTime(tp_from_ns(1000)).has_value());
    EXPECT_FALSE(sync.getOffsetNs().has_value());
    EXPECT_FALSE(sync.getUncertaintyNs().has_value());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Probe Samples (default α = 0.5, NTP midpoint)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ProbeSampleBasics) {
    ClockSync sync;

    // T1 = 1000 ns, T3 = 2500 ns, T4 = 2000 ns
    // RTT = 1000, α = 0.5 (default)
    // offset = T3 - T1 - α * RTT = 2500 - 1000 - 500 = 1000
    // uncertainty = RTT/2 = 500
    sync.addProbeSample(tp_from_ns(1000), 2500, tp_from_ns(2000));

    EXPECT_TRUE(sync.hasSyncData());
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 1000);
    ASSERT_TRUE(sync.getUncertaintyNs().has_value());
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

TEST(ClockSyncTest, CustomAlpha) {
    // With α = 2/3, the forward delay fraction shifts the estimate
    ClockSync::Config config;
    config.forward_delay_fraction = 2.0 / 3.0;
    ClockSync sync(config);

    // Simulate known asymmetry: D1 = 1750 μs, D2 = 850 μs
    // True offset = 100,000,000 ns (100 ms)
    // T1 = 0, T3 = D1 + offset = 101,750,000, T4 = D1 + D2 = 2,600,000
    // RTT = 2,600,000, α = 2/3
    // offset = 101,750,000 - 0 - round(2/3 * 2,600,000) = 100,016,667
    // True offset is 100,000,000, error ≈ 16.7 μs
    sync.addProbeSample(tp_from_ns(0), 101'750'000, tp_from_ns(2'600'000));

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    const int64_t estimated = *sync.getOffsetNs();
    const int64_t true_offset = 100'000'000;
    const int64_t error = std::abs(estimated - true_offset);
    EXPECT_LT(error, 20'000);

    EXPECT_EQ(*sync.getUncertaintyNs(), 1'300'000);
}

TEST(ClockSyncTest, SymmetricPath) {
    // With symmetric delays (D1 = D2), default α = 0.5 gives exact offset
    ClockSync sync;

    // True offset = 100,000,000, D1 = D2 = 1,000,000
    // T1 = 0, T3 = D1 + offset = 101,000,000, T4 = D1 + D2 = 2,000,000
    // offset = 101,000,000 - 0 - 0.5 * 2,000,000 = 100,000,000 (exact)
    sync.addProbeSample(tp_from_ns(0), 101'000'000, tp_from_ns(2'000'000));

    EXPECT_EQ(*sync.getOffsetNs(), 100'000'000);
    EXPECT_EQ(*sync.getUncertaintyNs(), 1'000'000);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Conversion Round-Trip
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ConversionRoundTrip) {
    ClockSync sync;

    // Establish an offset with default α = 0.5
    // T1 = 10000, T3 = 15000, T4 = 11000
    // RTT = 1000, offset = 15000 - 10000 - 0.5 * 1000 = 4500
    sync.addProbeSample(tp_from_ns(10000), 15000, tp_from_ns(11000));

    const uint64_t device_ns = 20000;
    auto local = sync.toLocalTime(device_ns);
    ASSERT_TRUE(local.has_value());

    auto back = sync.toDeviceTime(*local);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, device_ns);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Probe Expiry
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ProbeExpiry) {
    ClockSync::Config config;
    config.max_probe_age = std::chrono::seconds(1);  // 1s expiry for testing
    ClockSync sync(config);

    // Add probe at t=0
    sync.addProbeSample(tp_from_ns(0), 5100, tp_from_ns(200));
    EXPECT_TRUE(sync.hasSyncData());

    // Add probe at t=2s (past the 1s expiry) — old probe should be pruned
    const auto t2s = tp_from_ns(2'000'000'000);
    sync.addProbeSample(t2s, uint64_t(2'000'005'000), tp_from_ns(2'000'000'400));

    // Only the new probe remains
    // RTT = 400, offset = 5000 - 0.5 * 400 = 4800
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 4800);
    EXPECT_EQ(*sync.getUncertaintyNs(), 200);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Best Probe Selection (maximum offset)
//
// The algorithm selects the probe with the largest computed offset (after gap-based glitch
// rejection). For symmetric network paths this is equivalent to minimum RTT: the probe with the
// least processing delay has the largest offset estimate. For asymmetric paths, max-offset is
// the correct criterion because a higher offset means less queuing/processing bias in T3.
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, MaxOffsetSelection) {
    ClockSync sync;

    // Probe 1: RTT = 1000, offset = 5000 — higher offset, should be selected
    // T1 = 1000, T3 = 6500, T4 = 2000
    // offset = 6500 - 1000 - 0.5 * 1000 = 5000
    sync.addProbeSample(tp_from_ns(1000), 6500, tp_from_ns(2000));
    EXPECT_EQ(*sync.getOffsetNs(), 5000);

    // Probe 2: RTT = 200, offset = 4900 — lower RTT but also lower offset, not selected
    // T1 = 3000, T3 = 8000, T4 = 3200
    // offset = 8000 - 3000 - 0.5 * 200 = 4900
    sync.addProbeSample(tp_from_ns(3000), 8000, tp_from_ns(3200));

    // Best probe is probe 1 (highest offset), not probe 2 (lowest RTT)
    EXPECT_EQ(*sync.getOffsetNs(), 5000);
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

TEST(ClockSyncTest, BestProbeIsMaxOffset) {
    ClockSync sync;

    // Add 3 probes. The max-offset probe should win regardless of insertion order.
    // In this case the max-offset probe also happens to have the minimum RTT
    // (consistent with symmetric-path assumptions).

    // Probe A: RTT = 500, offset = 9750
    sync.addProbeSample(tp_from_ns(0), 10000, tp_from_ns(500));

    // Probe B: RTT = 100, offset = 9950  (max offset — should be selected)
    sync.addProbeSample(tp_from_ns(1000), 11000, tp_from_ns(1100));

    // Probe C: RTT = 300, offset = 9850
    sync.addProbeSample(tp_from_ns(2000), 12000, tp_from_ns(2300));

    // Probe B should be selected (highest offset = 9950)
    EXPECT_EQ(*sync.getOffsetNs(), 9950);
    EXPECT_EQ(*sync.getUncertaintyNs(), 50);  // RTT/2 = 100/2 = 50
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Multi-device clock-sync bug coverage
//
// These tests pin down the device(PTP)->host time mapping invariants that the
// Gemini NSP/HUB shared-clock bug violates.  Each test is labelled GREEN
// (guards existing correct behavior) or RED (encodes the INTENDED invariant and
// is EXPECTED TO FAIL on the current code, documenting the defect).
//
// Realistic magnitudes: a Gemini PTP clock reports wall-clock nanoseconds
// (~1.77e18 ns ≈ mid-2026), while the host std::chrono::steady_clock counts
// nanoseconds since boot (~5e12 ns ≈ 5000 s uptime).  The true offset
// (device_ns - steady_ns) is therefore ~1.77e18.  A "raw device clock leak"
// means toLocalTime() returns ~device_ns itself (offset ≈ 0 applied), i.e. a
// time_point ~1.77e9 s from the steady epoch instead of the ~5000 s host value.
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

// Host steady_clock "now", in ns since the steady epoch (~5000 s uptime).
constexpr int64_t HOST_NOW_NS = 5'000'000'000'000LL;
// True device(PTP) - host offset (~1.77e18 ns ≈ mid-2026 wall clock).
constexpr int64_t TRUE_OFFSET_NS = 1'770'000'000'000'000'000LL;
// Device(PTP) timestamp corresponding to HOST_NOW_NS.
constexpr uint64_t DEVICE_NOW_NS =
    static_cast<uint64_t>(HOST_NOW_NS + TRUE_OFFSET_NS);

} // anonymous namespace

// (A1) GREEN — Device(PTP)->host sanity for the probe path.
// After establishing a large true offset from a symmetric probe around host
// instant HOST_NOW, toLocalTime(latest device_ns) must land near HOST_NOW and
// MUST NOT leak the raw device clock (must be far from device_ns).
TEST(ClockSyncMultiDeviceTest, ProbeMappingDoesNotLeakRawDeviceClock) {
    ClockSync sync;

    // Symmetric probe centered on HOST_NOW: RTT = 2000 ns, midpoint = HOST_NOW.
    // offset = T3 - T1 - 0.5*RTT = DEVICE_NOW - (HOST_NOW-1000) - 1000 = TRUE_OFFSET.
    sync.addProbeSample(tp_from_ns(HOST_NOW_NS - 1000),
                        DEVICE_NOW_NS,
                        tp_from_ns(HOST_NOW_NS + 1000));

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), TRUE_OFFSET_NS);

    auto local = sync.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(local.has_value());
    const int64_t local_ns = tp_to_ns(*local);

    // Lands near the true host instant (within the ~1us probe RTT).
    EXPECT_NEAR(static_cast<double>(local_ns),
                static_cast<double>(HOST_NOW_NS), 1'000.0);

    // Core invariant: result must NOT be ~= raw device_ns.  The gap equals the
    // (huge) true offset, far more than 1 ms.
    EXPECT_GT(std::llabs(static_cast<long long>(DEVICE_NOW_NS) - local_ns),
              1'000'000LL)
        << "toLocalTime leaked the raw device/PTP clock";
}

// (A2) GREEN — Data-packet fallback path.
// With no probes, feed data-packet samples carrying a known device-host offset
// plus per-packet network delay.  The data-floor estimate must yield
// toLocalTime within the documented ONE_WAY_DELAY/uncertainty (700 us) of truth,
// and the reported uncertainty must be the data-fallback value.
TEST(ClockSyncMultiDeviceTest, DataFallbackMappingWithinUncertainty) {
    ClockSync sync;

    // Feed 8 data packets.  raw_offset = device_ts - recv = TRUE_OFFSET - delay.
    // Min one-way delay is 300 us, so max raw_offset = TRUE_OFFSET - 300us, and
    // the estimate (max raw + 700us correction) = TRUE_OFFSET + 400us.
    constexpr int64_t MIN_DELAY_NS = 300'000;
    int64_t last_recv_ns = 0;
    uint64_t last_device_ns = 0;
    for (int k = 0; k < 8; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;  // 1 ms apart
        // Vary delay >= MIN_DELAY_NS; k==3 hits the minimum.
        const int64_t delay_ns = MIN_DELAY_NS + std::llabs(3 - k) * 50'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - delay_ns);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
        last_recv_ns = recv_ns;
        last_device_ns = device_ns;
    }

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    ASSERT_TRUE(sync.getUncertaintyNs().has_value());
    EXPECT_EQ(*sync.getUncertaintyNs(), 700'000)
        << "data fallback should report the ONE_WAY_DELAY uncertainty";

    auto local = sync.toLocalTime(last_device_ns);
    ASSERT_TRUE(local.has_value());
    const int64_t local_ns = tp_to_ns(*local);

    // The mapped host time must be within ~1 ms (one-way delay + correction) of
    // when that packet was actually received.
    EXPECT_NEAR(static_cast<double>(local_ns),
                static_cast<double>(last_recv_ns), 1'000'000.0);

    // And it must not leak the raw device clock.
    EXPECT_GT(std::llabs(static_cast<long long>(last_device_ns) - local_ns),
              1'000'000'000LL)
        << "data fallback leaked the raw device/PTP clock";
}

// (A3a) GREEN — setExternalOffset is used while set, and clearing (nullopt)
// reverts to the internal (data-derived) estimate.
TEST(ClockSyncMultiDeviceTest, ExternalOffsetUsedThenClearedRevertsToInternal) {
    ClockSync sync;

    // Establish an internal data-derived estimate ≈ TRUE_OFFSET + 400us.
    for (int k = 0; k < 8; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - 300'000);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
    }
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    const int64_t internal = *sync.getOffsetNs();

    // Inject a plausible external offset 50 ms away — it must take effect.
    const int64_t external = TRUE_OFFSET_NS + 50'000'000LL;
    sync.setExternalOffset(external, 1'000'000);
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), external);

    // Clearing reverts to the internal estimate.
    sync.setExternalOffset(std::nullopt);
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), internal);
}

// (A3b) RED — An external offset that maps the latest device_ns to an
// implausible host time (decades from the device's own data-packet evidence)
// must NOT be adopted with unconditional priority.  Intended invariant: an
// adopted external offset must be sanity-consistent with the device's own data
// evidence, else it is rejected/flagged.
//
// Current code (clock_sync.cpp:191-198 recomputeEstimate / 112-125
// setExternalOffset) adopts ANY external offset unconditionally, so an external
// offset of 0 collapses toLocalTime() to the raw device clock.  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, ImplausibleExternalOffsetRejected) {
    ClockSync sync;

    // Strong internal data evidence: offset ≈ TRUE_OFFSET.
    for (int k = 0; k < 8; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - 300'000);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
    }
    ASSERT_TRUE(sync.toLocalTime(DEVICE_NOW_NS).has_value());

    // Inject a wildly inconsistent external offset (0 → maps device_ns to the
    // raw PTP clock, ~1.77e18 ns from the data-derived host time).
    sync.setExternalOffset(0, 1'000'000);

    auto local = sync.toLocalTime(DEVICE_NOW_NS);
    ASSERT_TRUE(local.has_value());
    const int64_t local_ns = tp_to_ns(*local);

    // INTENDED: the implausible external offset is rejected, so the mapping
    // stays near the device's own (data-derived) host estimate ≈ HOST_NOW.
    EXPECT_LT(std::llabs(local_ns - HOST_NOW_NS), 1'000'000'000LL)
        << "implausible external offset was blindly adopted; toLocalTime="
        << local_ns << " (raw device_ns=" << DEVICE_NOW_NS << ")";
}

// (A4) RED — Latch behavior.  Model the SDK sequence: device on data fallback,
// a stale external offset injected once, then fresh internal evidence keeps
// arriving.  The estimate must be able to RECOVER (refresh off the fresh
// internal evidence) rather than staying permanently pinned to the stale
// external offset.
//
// Current code latches: once m_external_offset_ns is set, recomputeEstimate
// (clock_sync.cpp:191-198) returns it first on every subsequent
// addDataPacketSample, so internal evidence is ignored forever.  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, ExternalOffsetDoesNotLatchAgainstFreshEvidence) {
    ClockSync sync;

    // Establish good internal data evidence ≈ TRUE_OFFSET (data fallback).
    for (int k = 0; k < 8; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - 300'000);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
    }

    // Inject a stale external offset 10 s away from the truth, once — as the
    // SDK does when it borrows a peer HUB offset on data fallback.  An offset
    // this far from the device's own evidence must NOT pin the estimate: it is
    // rejected as implausible (and even if a future change instead adopted it
    // briefly, the estimate must still recover as fresh evidence arrives — the
    // invariant checked at the end of this test).
    const int64_t stale_external = TRUE_OFFSET_NS + 10'000'000'000LL;
    sync.setExternalOffset(stale_external, 700'000);

    // Fresh, consistent internal evidence continues to arrive.
    for (int k = 8; k < 40; ++k) {
        const int64_t recv_ns = HOST_NOW_NS + k * 1'000'000LL;
        const uint64_t device_ns =
            static_cast<uint64_t>(recv_ns + TRUE_OFFSET_NS - 300'000);
        sync.addDataPacketSample(device_ns, tp_from_ns(recv_ns));
    }

    // INTENDED: the estimate recovers toward the fresh internal evidence and is
    // no longer pinned to the stale external offset.
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_LT(std::llabs(*sync.getOffsetNs() - TRUE_OFFSET_NS), 1'000'000LL)
        << "estimate latched to stale external offset " << stale_external
        << "; got " << *sync.getOffsetNs();
}

// (A5a) RED — A single probe must not be reported as "reliable": one probe is
// insufficient evidence to commit an offset.
//
// Current probeSpreadOk() (clock_sync.cpp:277) returns true for < 3 probes, so
// probesAreReliable() reports a lone probe as reliable.  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, SingleProbeNotReliable) {
    ClockSync sync;
    sync.addProbeSample(tp_from_ns(HOST_NOW_NS - 1000),
                        DEVICE_NOW_NS,
                        tp_from_ns(HOST_NOW_NS + 1000));
    EXPECT_FALSE(sync.probesAreReliable())
        << "a single probe should not be treated as reliable";
}

// (A5b) RED — Two wildly disagreeing probes must not be reported as "reliable".
// A pair whose offsets differ by 100 ms is clearly inconsistent, but
// probeSpreadOk() short-circuits to true for < 3 probes, so a wild pair (or a
// single wild probe) can define the committed offset.  EXPECTED FAIL.
TEST(ClockSyncMultiDeviceTest, TwoWildlyDisagreeingProbesNotReliable) {
    ClockSync sync;
    // Probe 1: offset ≈ TRUE_OFFSET.
    sync.addProbeSample(tp_from_ns(HOST_NOW_NS - 1000),
                        DEVICE_NOW_NS,
                        tp_from_ns(HOST_NOW_NS + 1000));
    // Probe 2: offset ≈ TRUE_OFFSET + 100 ms (wildly different).
    sync.addProbeSample(tp_from_ns(HOST_NOW_NS - 1000),
                        DEVICE_NOW_NS + 100'000'000ULL,
                        tp_from_ns(HOST_NOW_NS + 1000));
    EXPECT_FALSE(sync.probesAreReliable())
        << "two probes disagreeing by 100 ms should not be treated as reliable";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Commit discipline (deadband / slew / step / stepout)
//
// These drive the committed-offset discipline deterministically via a tailored
// Config, feeding probes whose selected offset is known.  All GREEN — they
// guard the discipline that damps per-sample jitter on the live multi-device
// rig (see tests/integration/test_multidevice_clock_sync.cpp).
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
// A probe whose computed offset is exactly `offset_ns` (symmetric path, RTT
// 2000 ns): offset = T3 - T1 - RTT/2 = (offset+1000) - 0 - 1000.
void addProbeWithOffset(ClockSync& sync, int64_t offset_ns) {
    sync.addProbeSample(tp_from_ns(0),
                        static_cast<uint64_t>(offset_ns + 1000),
                        tp_from_ns(2000));
}
} // anonymous namespace

// A medium change (within slew_max) is damped, not applied in full: the
// committed offset moves by slew_gain of the residual.
TEST(ClockSyncDisciplineTest, MediumChangeIsSlewed) {
    ClockSync::Config cfg;
    cfg.commit_deadband_ns = 100'000;       // 0.1 ms
    cfg.slew_max_ns        = 50'000'000;    // 50 ms
    cfg.slew_gain          = 0.5;
    ClockSync sync(cfg);

    addProbeWithOffset(sync, 100'000'000);  // cold-commit at 100 ms
    ASSERT_EQ(*sync.getOffsetNs(), 100'000'000);

    // Jump the selected offset by +8 ms (deadband < 8 ms < slew_max).
    addProbeWithOffset(sync, 108'000'000);
    // Slewed halfway: 100 ms + 0.5 * 8 ms = 104 ms (not the full 108 ms).
    EXPECT_EQ(*sync.getOffsetNs(), 104'000'000);
}

// A one-off large jump (beyond slew_max) is held, not adopted, until it
// persists for step_persist samples.
TEST(ClockSyncDisciplineTest, LargeJumpHeldThenSteppedAfterPersistence) {
    ClockSync::Config cfg;
    cfg.commit_deadband_ns = 100'000;       // 0.1 ms
    cfg.slew_max_ns        = 2'000'000;     // 2 ms
    cfg.step_persist       = 3;
    cfg.stepout_samples    = 1000;          // keep stepout out of the way
    ClockSync sync(cfg);

    addProbeWithOffset(sync, 100'000'000);  // cold-commit at 100 ms
    ASSERT_EQ(*sync.getOffsetNs(), 100'000'000);

    // +5 ms jump (> slew_max) — held for the first two samples...
    addProbeWithOffset(sync, 105'000'000);
    EXPECT_EQ(*sync.getOffsetNs(), 100'000'000) << "held after 1 large-jump sample";
    addProbeWithOffset(sync, 105'000'000);
    EXPECT_EQ(*sync.getOffsetNs(), 100'000'000) << "held after 2 large-jump samples";
    // ...then accepted on the third (step_persist == 3).
    addProbeWithOffset(sync, 105'000'000);
    EXPECT_EQ(*sync.getOffsetNs(), 105'000'000) << "stepped after persistence";
}

// Stepout backstop: when a large jump is held but never reaches step_persist
// (here step_persist is effectively infinite), the committed offset still
// re-acquires after stepout_samples non-converged samples.
TEST(ClockSyncDisciplineTest, StepoutReacquiresAfterPersistentDisagreement) {
    ClockSync::Config cfg;
    cfg.commit_deadband_ns = 100'000;       // 0.1 ms
    cfg.slew_max_ns        = 2'000'000;     // 2 ms
    cfg.step_persist       = 1000;          // step path never accepts
    cfg.stepout_samples    = 5;             // escape after 5 non-converged samples
    ClockSync sync(cfg);

    addProbeWithOffset(sync, 100'000'000);  // cold-commit at 100 ms
    ASSERT_EQ(*sync.getOffsetNs(), 100'000'000);

    // Four held large-jump samples: still pinned (step path won't accept).
    for (int i = 0; i < 4; ++i)
        addProbeWithOffset(sync, 105'000'000);
    EXPECT_EQ(*sync.getOffsetNs(), 100'000'000) << "held before stepout";

    // Fifth non-converged sample trips the stepout escape — re-acquire.
    addProbeWithOffset(sync, 105'000'000);
    EXPECT_EQ(*sync.getOffsetNs(), 105'000'000) << "stepout should re-acquire";
}

// ---------------------------------------------------------------------------
// deviceTimestampToNs: every clock-sync input (NPLAYREP probe, bulk
// data-delivery, and the data-packet fallback) routes raw device timestamps
// through this single helper so they all share one time domain. Regression
// guard for the #185/#186 follow-up bug where the data-packet fallback fed raw
// nPlay ticks as if they were nanoseconds, yielding an offset of ~ -host_clock
// that poisoned the estimate.
// ---------------------------------------------------------------------------

// nPlay reports 30 kHz sample-count ticks; sysfreq 30000 => 1e9/30000 reduces to
// 100000/3. These MUST be scaled to nanoseconds before reaching the clock sync.
TEST(DeviceTimestampToNs, NonGeminiTicksScaleToNanoseconds) {
    EXPECT_EQ(deviceTimestampToNs(30000, /*ts_are_ns=*/false, 100000, 3), 1'000'000'000ULL); // 1 s
    EXPECT_EQ(deviceTimestampToNs(3,     /*ts_are_ns=*/false, 100000, 3),       100'000ULL);  // 3 ticks = 100 us
}

// A large proctime (a long uptime in 30 kHz ticks) must be scaled to ns and must
// NOT pass through as raw ticks -- the exact mistake the data-packet fallback made.
TEST(DeviceTimestampToNs, LargeTicksAreNotPassedThroughRaw) {
    const uint64_t ticks = 71'390'520'562ULL;            // ~2.38e6 s of 30 kHz ticks
    const uint64_t ns = deviceTimestampToNs(ticks, /*ts_are_ns=*/false, 100000, 3);
    EXPECT_EQ(ns, ticks * 100000 / 3);
    EXPECT_NE(ns, ticks);                                // regression guard
}

// Gemini already reports nanoseconds -> pass through unchanged.
TEST(DeviceTimestampToNs, GeminiNanosecondsPassThrough) {
    const uint64_t ptp_ns = 1'771'721'518'000'000'000ULL;
    EXPECT_EQ(deviceTimestampToNs(ptp_ns, /*ts_are_ns=*/true, 100000, 3), ptp_ns);
}

// A trivial denominator (no usable sysfreq) disables conversion regardless of flag.
TEST(DeviceTimestampToNs, TrivialDenominatorDisablesConversion) {
    EXPECT_EQ(deviceTimestampToNs(12345, /*ts_are_ns=*/false, 1, 1), 12345ULL);
}
