///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_clock_sync.cpp
/// @brief  Integration tests for clock synchronization
///
/// Tests device timestamps, clock offset estimation, clock probing, and
/// time conversion against a live nPlayServer instance.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "nplay_fixture.h"

#include <cbproto/cbproto.h>
#include <cbsdk/sdk_session.h>

#include <chrono>
#include <cmath>
#include <thread>

using namespace cbsdk;

static Result<SdkSession> createNPlaySession() {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;
    return SdkSession::create(config);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Device Timestamp
///////////////////////////////////////////////////////////////////////////////////////////////////

class DeviceTimeTest : public NPlayFixture {};

TEST_F(DeviceTimeTest, TimeNonzero) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    // Allow time for first packets to arrive and update shmem timestamp
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t t = result.value().getTime();
    EXPECT_GT(t, 0u) << "Device time should be nonzero after handshake";
}

TEST_F(DeviceTimeTest, TimeAdvances) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    uint64_t t1 = session.getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    uint64_t t2 = session.getTime();

    EXPECT_GT(t2, t1) << "Device time did not advance";
}

TEST_F(DeviceTimeTest, TimeRate) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Use a short window to avoid file looping (~2s file)
    uint64_t t1 = session.getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    uint64_t t2 = session.getTime();

    if (t2 <= t1) {
        // File looped — retry
        t1 = t2;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        t2 = session.getTime();
    }

    double delta_s = static_cast<double>(t2 - t1) / 1e9;
    EXPECT_GT(delta_s, 0.1) << "Device time advanced too slowly: " << delta_s << "s";
    EXPECT_LT(delta_s, 1.0) << "Device time advanced too fast: " << delta_s << "s";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Offset
///////////////////////////////////////////////////////////////////////////////////////////////////

class ClockOffsetTest : public NPlayFixture {};

TEST_F(ClockOffsetTest, OffsetTypeCheck) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // nPlayServer may not respond to clock probes — offset may be unavailable.
    // Just verify the call doesn't crash.
    auto offset = session.getClockOffsetNs();
    // offset is either nullopt or a valid int64_t
    (void)offset;
}

TEST_F(ClockOffsetTest, OffsetReasonable) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto offset = session.getClockOffsetNs();
    if (!offset.has_value()) GTEST_SKIP() << "Clock offset not available";

    double offset_s = std::abs(static_cast<double>(*offset)) / 1e9;
    EXPECT_LT(offset_s, 60.0) << "Clock offset " << offset_s << "s — unreasonably large";
}

TEST_F(ClockOffsetTest, UncertaintyTypeCheck) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // nPlayServer may not respond to clock probes — uncertainty may be unavailable.
    auto uncertainty = session.getClockUncertaintyNs();
    (void)uncertainty;
}

TEST_F(ClockOffsetTest, UncertaintySmallOnLocalhost) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto uncertainty = session.getClockUncertaintyNs();
    if (!uncertainty.has_value()) GTEST_SKIP() << "Clock uncertainty not available";

    double uncertainty_ms = std::abs(static_cast<double>(*uncertainty)) / 1e6;
    EXPECT_LT(uncertainty_ms, 100.0) << "Uncertainty " << uncertainty_ms << "ms — too large for localhost";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Probing
///////////////////////////////////////////////////////////////////////////////////////////////////

class ClockProbeTest : public NPlayFixture {};

TEST_F(ClockProbeTest, SendProbeSucceeds) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto probe_result = result.value().sendClockProbe();
    EXPECT_TRUE(probe_result.isOk()) << probe_result.error();
}

TEST_F(ClockProbeTest, ProbesMaintainUncertainty) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto u_before = session.getClockUncertaintyNs();
    if (!u_before.has_value()) GTEST_SKIP() << "No initial uncertainty";

    // Send several probes
    for (int i = 0; i < 5; ++i) {
        session.sendClockProbe();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    auto u_after = session.getClockUncertaintyNs();
    ASSERT_TRUE(u_after.has_value());
    // Uncertainty should not degrade significantly
    EXPECT_LE(*u_after, *u_before * 2 + 1000000)
        << "Uncertainty degraded: " << *u_before << "ns -> " << *u_after << "ns";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Time Conversion
///////////////////////////////////////////////////////////////////////////////////////////////////

class TimeConversionTest : public NPlayFixture {};

TEST_F(TimeConversionTest, ToLocalTimeTypeCheck) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t device_ns = session.getTime();
    auto local = session.toLocalTime(device_ns);
    // nPlayServer may not provide clock sync data — nullopt is acceptable
    if (!local.has_value())
        GTEST_SKIP() << "toLocalTime unavailable (no clock sync data)";
}

TEST_F(TimeConversionTest, ConvertedTimeCloseToNow) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t device_ns = session.getTime();
    auto local = session.toLocalTime(device_ns);
    if (!local.has_value()) GTEST_SKIP() << "No clock sync data";

    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - *local);
    double delta_s = static_cast<double>(delta.count()) / 1000.0;

    // The converted time should be in the recent past (within 5s)
    EXPECT_GT(delta_s, -1.0) << "Converted time is in the future by " << -delta_s << "s";
    EXPECT_LT(delta_s, 5.0) << "Converted time is " << delta_s << "s in the past";
}

TEST_F(TimeConversionTest, RoundTrip) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto now = std::chrono::steady_clock::now();
    auto device_opt = session.toDeviceTime(now);
    if (!device_opt.has_value()) GTEST_SKIP() << "No clock sync data";

    auto local_opt = session.toLocalTime(*device_opt);
    ASSERT_TRUE(local_opt.has_value());

    auto roundtrip_error = std::chrono::duration_cast<std::chrono::microseconds>(
        *local_opt - now);
    // Round-trip should be exact (same offset applied and removed)
    EXPECT_LT(std::abs(roundtrip_error.count()), 1000)
        << "Round-trip error: " << roundtrip_error.count() << "us";
}

TEST_F(TimeConversionTest, MonotonicityWithinPlayback) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Use short interval to stay within one playback pass (~2s file)
    uint64_t t1_dev = session.getTime();
    auto m1 = session.toLocalTime(t1_dev);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint64_t t2_dev = session.getTime();
    auto m2 = session.toLocalTime(t2_dev);

    if (!m1.has_value() || !m2.has_value()) GTEST_SKIP() << "No clock sync data";

    // Only check monotonicity if device time didn't wrap (file loop)
    if (t2_dev > t1_dev) {
        EXPECT_GT(*m2, *m1) << "Converted times not monotonic";
    }
}
