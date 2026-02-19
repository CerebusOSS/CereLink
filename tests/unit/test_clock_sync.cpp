///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_clock_sync.cpp
/// @author CereLink Development Team
/// @date   2025-02-18
///
/// @brief  Unit tests for cbdev::ClockSync
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "clock_sync.h"
#include <chrono>
#include <cstdint>
#include <limits>

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
// One-Way Samples
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, SingleOneWaySample) {
    ClockSync sync;

    // Device at 5000 ns, host recv at 4000 ns → raw_offset = 1000 ns
    sync.addOneWaySample(5000, tp_from_ns(4000));

    EXPECT_TRUE(sync.hasSyncData());
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 1000);
    // One-way only → uncertainty is INT64_MAX
    ASSERT_TRUE(sync.getUncertaintyNs().has_value());
    EXPECT_EQ(*sync.getUncertaintyNs(), std::numeric_limits<int64_t>::max());

    // toLocalTime: local_ns = device_ns - offset = 5000 - 1000 = 4000
    auto local = sync.toLocalTime(5000);
    ASSERT_TRUE(local.has_value());
    EXPECT_EQ(tp_to_ns(*local), 4000);
}

TEST(ClockSyncTest, OneWayMaxFilter) {
    ClockSync sync;

    const auto base = tp_from_ns(1'000'000'000);

    // Multiple samples: max(raw_offset) should be used
    // sample 1: device=1001000000, local=1000000000 → offset=1000000
    sync.addOneWaySample(1'001'000'000, base);

    // sample 2: device=1002000000, local=1000500000 → offset=1500000
    sync.addOneWaySample(1'002'000'000, tp_from_ns(1'000'500'000));

    // sample 3: device=1001500000, local=1001000000 → offset=500000
    sync.addOneWaySample(1'001'500'000, tp_from_ns(1'001'000'000));

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 1'500'000);  // max of the three
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Probe Samples
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ProbeSampleBasics) {
    ClockSync sync;

    // T1 = 1000 ns (host send), T3 = 2500 ns (device), T4 = 2000 ns (host recv)
    // midpoint = (1000 + 2000) / 2 = 1500
    // offset = 2500 - 1500 = 1000
    // RTT = 2000 - 1000 = 1000; uncertainty = 500
    sync.addProbeSample(tp_from_ns(1000), 2500, tp_from_ns(2000));

    EXPECT_TRUE(sync.hasSyncData());
    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 1000);
    ASSERT_TRUE(sync.getUncertaintyNs().has_value());
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

TEST(ClockSyncTest, ProbeOverridesOneWay) {
    ClockSync sync;

    // One-way: device=5000, local=4200 → raw_offset=800
    sync.addOneWaySample(5000, tp_from_ns(4200));
    EXPECT_EQ(*sync.getOffsetNs(), 800);

    // Probe: T1=4000, T3=5100, T4=4200 → midpoint=4100, offset=1000, uncertainty=100
    // Probe offset (1000) > one-way offset (800), so probe wins
    sync.addProbeSample(tp_from_ns(4000), 5100, tp_from_ns(4200));

    ASSERT_TRUE(sync.getOffsetNs().has_value());
    EXPECT_EQ(*sync.getOffsetNs(), 1000);
    EXPECT_EQ(*sync.getUncertaintyNs(), 100);
}

TEST(ClockSyncTest, DriftDetection) {
    ClockSync sync;

    // Start with a probe: offset=1000, uncertainty=500
    sync.addProbeSample(tp_from_ns(1000), 2500, tp_from_ns(2000));
    EXPECT_EQ(*sync.getOffsetNs(), 1000);

    // One-way sample with higher offset (drift detected): raw_offset=1500
    // device=4000, local=2500 → raw_offset=1500
    sync.addOneWaySample(4000, tp_from_ns(2500));

    // offset should be revised upward to max(probe_offset=1000, one_way_max=1500) = 1500
    EXPECT_EQ(*sync.getOffsetNs(), 1500);
    // Uncertainty still from the probe
    EXPECT_EQ(*sync.getUncertaintyNs(), 500);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Conversion Round-Trip
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, ConversionRoundTrip) {
    ClockSync sync;

    // Establish an offset
    sync.addProbeSample(tp_from_ns(10000), 15000, tp_from_ns(11000));
    // midpoint=10500, offset=4500, uncertainty=500

    const uint64_t device_ns = 20000;
    auto local = sync.toLocalTime(device_ns);
    ASSERT_TRUE(local.has_value());

    auto back = sync.toDeviceTime(*local);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, device_ns);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Window Expiry
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, WindowExpiry) {
    ClockSync::Config config;
    config.one_way_window = std::chrono::milliseconds(100);  // 100ms window for testing
    ClockSync sync(config);

    // Add a sample at t=0
    const auto t0 = tp_from_ns(0);
    sync.addOneWaySample(2000, t0);  // raw_offset=2000
    EXPECT_EQ(*sync.getOffsetNs(), 2000);

    // Add a sample at t=200ms (well past the 100ms window)
    // The old sample should be pruned
    const auto t200ms = tp_from_ns(200'000'000);
    sync.addOneWaySample(200'001'000, t200ms);  // raw_offset=1000

    // Only the new sample remains → offset=1000
    EXPECT_EQ(*sync.getOffsetNs(), 1000);
}

TEST(ClockSyncTest, StaleProbeExpiry) {
    ClockSync::Config config;
    config.max_probe_age = std::chrono::seconds(1);  // 1s expiry for testing
    config.one_way_window = std::chrono::milliseconds(500);
    ClockSync sync(config);

    // Add probe at t=0: offset=5000, uncertainty=100
    sync.addProbeSample(tp_from_ns(0), 5100, tp_from_ns(200));
    EXPECT_EQ(*sync.getOffsetNs(), 5000);
    EXPECT_EQ(*sync.getUncertaintyNs(), 100);

    // Add one-way sample at t=2s (past the 1s probe expiry)
    // This should prune the probe
    const auto t2s = tp_from_ns(2'000'000'000);
    sync.addOneWaySample(2'000'003'000, t2s);  // raw_offset=3000

    // Probe is expired, so only one-way remains → offset=3000, uncertainty=INT64_MAX
    EXPECT_EQ(*sync.getOffsetNs(), 3000);
    EXPECT_EQ(*sync.getUncertaintyNs(), std::numeric_limits<int64_t>::max());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Best Probe Selection
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ClockSyncTest, BestProbeIsMinimumRTT) {
    ClockSync sync;

    // Probe 1: RTT=1000, offset=5000, uncertainty=500
    sync.addProbeSample(tp_from_ns(1000), 6500, tp_from_ns(2000));

    // Probe 2: RTT=200, offset=4900, uncertainty=100 (better RTT)
    sync.addProbeSample(tp_from_ns(3000), 8000, tp_from_ns(3200));
    // midpoint=3100, offset=8000-3100=4900, uncertainty=100

    // Best probe is probe 2 (lowest uncertainty/RTT)
    // Probe 1: midpoint=1500, offset=6500-1500=5000, uncertainty=500
    // Probe 2: midpoint=3100, offset=8000-3100=4900, uncertainty=100
    // Algorithm uses best probe's offset = 4900
    EXPECT_EQ(*sync.getOffsetNs(), 4900);
    EXPECT_EQ(*sync.getUncertaintyNs(), 100);
}
