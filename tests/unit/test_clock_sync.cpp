///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_clock_sync.cpp
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Unit tests for cbdev::ClockSync (probes-only with asymmetry correction)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "clock_sync.h"
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
// Probe Samples with Asymmetry Correction
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ProbeSampleBasics) {
    // Use α = 2/3 (default)
    ClockSync sync;

    // T1 = 1000 ns, T3 = 2500 ns, T4 = 2000 ns
    // RTT = 2000 - 1000 = 1000
    // offset = T3 - T1 - α * RTT = 2500 - 1000 - round(2/3 * 1000) = 2500 - 1000 - 667 = 833
    // uncertainty = RTT/2 = 500
    sync.addProbeSample(tp_from_ns(1000), 2500, tp_from_ns(2000));

    EXPECT_TRUE(sync.hasSyncData());
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 833);
    ASSERT_TRUE(sync.getUncertaintyNs().has_value());
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

TEST(ClockSyncTest, AsymmetryCorrection) {
    // Simulate known asymmetry: D1 = 1750 μs, D2 = 850 μs
    // True offset = 100,000,000 ns (100 ms)
    // T1 = 0, T3 = T1 + D1 + offset = 1,750,000 + 100,000,000 = 101,750,000
    // T4 = T1 + D1 + D2 = 2,600,000
    // RTT = 2,600,000, α = 2/3
    // offset = T3 - T1 - α * RTT = 101,750,000 - 0 - round(2/3 * 2,600,000)
    //        = 101,750,000 - 1,733,333 = 100,016,667
    // True offset is 100,000,000, so error = 16,667 ns ≈ 16.7 μs
    ClockSync sync;
    sync.addProbeSample(tp_from_ns(0), 101'750'000, tp_from_ns(2'600'000));

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    const int64_t estimated = *sync.getOffsetNs();
    const int64_t true_offset = 100'000'000;
    const int64_t error = std::abs(estimated - true_offset);
    // Error should be small (< 20 μs)
    EXPECT_LT(error, 20'000);

    // Uncertainty = RTT/2 = 1,300,000
    EXPECT_EQ(*sync.getUncertaintyNs(), 1'300'000);
}

TEST(ClockSyncTest, SymmetricFallback) {
    // With α = 0.5, should give standard NTP midpoint formula
    ClockSync::Config config;
    config.forward_delay_fraction = 0.5;
    ClockSync sync(config);

    // T1 = 1000, T3 = 2500, T4 = 2000
    // RTT = 1000, α = 0.5
    // offset = 2500 - 1000 - round(0.5 * 1000) = 2500 - 1000 - 500 = 1000
    // NTP midpoint: T3 - (T1 + T4)/2 = 2500 - 1500 = 1000 ✓
    sync.addProbeSample(tp_from_ns(1000), 2500, tp_from_ns(2000));

    EXPECT_EQ(*sync.getOffsetNs(), 1000);
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Conversion Round-Trip
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ConversionRoundTrip) {
    ClockSync sync;

    // Establish an offset with default α = 2/3
    // T1 = 10000, T3 = 15000, T4 = 11000
    // RTT = 1000, offset = 15000 - 10000 - round(2/3 * 1000) = 15000 - 10000 - 667 = 4333
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
    // RTT = 400, offset = 5000 - round(2/3 * 400) = 5000 - 267 = 4733
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 4733);
    EXPECT_EQ(*sync.getUncertaintyNs(), 200);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Best Probe Selection (minimum RTT)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, MinRTTSelection) {
    ClockSync sync;

    // Probe 1: RTT = 1000 (worse)
    // T1 = 1000, T3 = 6500, T4 = 2000
    // offset = 6500 - 1000 - round(2/3 * 1000) = 6500 - 1000 - 667 = 4833
    sync.addProbeSample(tp_from_ns(1000), 6500, tp_from_ns(2000));
    EXPECT_EQ(*sync.getOffsetNs(), 4833);

    // Probe 2: RTT = 200 (better — should be selected)
    // T1 = 3000, T3 = 8000, T4 = 3200
    // offset = 8000 - 3000 - round(2/3 * 200) = 8000 - 3000 - 133 = 4867
    sync.addProbeSample(tp_from_ns(3000), 8000, tp_from_ns(3200));

    // Best probe is probe 2 (lowest RTT)
    EXPECT_EQ(*sync.getOffsetNs(), 4867);
    EXPECT_EQ(*sync.getUncertaintyNs(), 100);
}

TEST(ClockSyncTest, BestProbeIsMinimumRTT) {
    ClockSync sync;

    // Add 3 probes with different RTTs. The min-RTT probe should win
    // regardless of insertion order.

    // Probe A: RTT = 500
    sync.addProbeSample(tp_from_ns(0), 10000, tp_from_ns(500));
    // offset = 10000 - 0 - round(2/3 * 500) = 10000 - 333 = 9667

    // Probe B: RTT = 100 (best)
    sync.addProbeSample(tp_from_ns(1000), 11000, tp_from_ns(1100));
    // offset = 11000 - 1000 - round(2/3 * 100) = 11000 - 1000 - 67 = 9933

    // Probe C: RTT = 300
    sync.addProbeSample(tp_from_ns(2000), 12000, tp_from_ns(2300));
    // offset = 12000 - 2000 - round(2/3 * 300) = 12000 - 2000 - 200 = 9800

    // Probe B should be selected (RTT = 100)
    EXPECT_EQ(*sync.getOffsetNs(), 9933);
    EXPECT_EQ(*sync.getUncertaintyNs(), 50);  // RTT/2 = 100/2 = 50
}
