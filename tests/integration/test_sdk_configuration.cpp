///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_configuration.cpp
/// @brief  Integration tests for C++ SdkSession device configuration
///
/// These tests run against a live nPlayServer instance and exercise
/// channel configuration, CCF/CMP loading, and session properties.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "nplay_fixture.h"

#include <cbproto/cbproto.h>
#include <cbsdk/sdk_session.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <io.h>
#  define mkstemp_helper(tmpl) _mktemp_s(tmpl, strlen(tmpl) + 1)
#else
#  include <unistd.h>
#endif

static std::string makeTempCcfPath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    std::string path = std::string(buf) + "cerelink_test_XXXXXX.ccf";
    _mktemp_s(&path[0], path.size() + 1);
    return path;
#else
    char tmpl[] = "/tmp/cerelink_test_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) close(fd);
    std::string path = std::string(tmpl) + ".ccf";
    std::rename(tmpl, path.c_str());
    return path;
#endif
}

using namespace cbsdk;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper: create a session connected to nPlayServer
///////////////////////////////////////////////////////////////////////////////////////////////////

static Result<SdkSession> createNPlaySession(bool autorun = true) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;
    config.autorun = autorun;
    return SdkSession::create(config);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle
///////////////////////////////////////////////////////////////////////////////////////////////////

class SessionLifecycleTest : public NPlayFixture {};

TEST_F(SessionLifecycleTest, ConnectsToNPlay) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();
    EXPECT_TRUE(session.isRunning());
}

TEST_F(SessionLifecycleTest, IsStandalone) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    EXPECT_TRUE(result.value().isStandalone());
}

TEST_F(SessionLifecycleTest, ProtocolVersion) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto version = result.value().getProtocolVersion();
    EXPECT_NE(version, 0u);  // Should not be UNKNOWN
}

TEST_F(SessionLifecycleTest, ProcIdent) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto ident = result.value().getProcIdent();
    // nPlayServer should report an ident string
    EXPECT_FALSE(ident.empty());
}

TEST_F(SessionLifecycleTest, SysInfo) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto* sysinfo = result.value().getSysInfo();
    EXPECT_NE(sysinfo, nullptr);
    if (sysinfo) {
        EXPECT_GT(sysinfo->sysfreq, 0u);
    }
}

TEST_F(SessionLifecycleTest, StopCleanly) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    EXPECT_TRUE(session.isRunning());
    session.stop();
    EXPECT_FALSE(session.isRunning());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Sample Group Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

class ChannelSampleGroupTest : public NPlayFixture {};

TEST_F(ChannelSampleGroupTest, SetFrontend30kHz) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    auto set_result = session.setChannelSampleGroup(
        8, ChannelType::FRONTEND, SampleRate::SR_30kHz, true);
    EXPECT_TRUE(set_result.isOk()) << set_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify via group info
    auto* group_info = session.getGroupInfo(static_cast<uint32_t>(SampleRate::SR_30kHz));
    EXPECT_NE(group_info, nullptr);
}

TEST_F(ChannelSampleGroupTest, SetAndVerifyViaChannelField) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    const size_t n_chans = 4;
    auto set_result = session.setChannelSampleGroup(
        n_chans, ChannelType::FRONTEND, SampleRate::SR_10kHz, true);
    EXPECT_TRUE(set_result.isOk()) << set_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Query channel fields
    auto fields_result = session.getChannelField(n_chans, ChannelType::FRONTEND,
                                                  ChanInfoField::SMPGROUP);
    ASSERT_TRUE(fields_result.isOk()) << fields_result.error();
    auto& fields = fields_result.value();

    for (size_t i = 0; i < fields.size(); ++i) {
        EXPECT_EQ(fields[i], static_cast<int64_t>(SampleRate::SR_10kHz))
            << "Channel " << i << " has wrong sample group";
    }
}

TEST_F(ChannelSampleGroupTest, DisableOthers) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Enable 96 channels at 30kHz
    session.setChannelSampleGroup(96, ChannelType::FRONTEND, SampleRate::SR_30kHz, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Now set 8 at 1kHz with disable_others
    session.setChannelSampleGroup(8, ChannelType::FRONTEND, SampleRate::SR_1kHz, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 30kHz group should now be empty
    auto ids_30k = session.getMatchingChannelIds(cbMAXCHANS, ChannelType::FRONTEND);
    if (ids_30k.isOk()) {
        int count_30k = 0;
        for (auto id : ids_30k.value()) {
            auto f = session.getChannelField(id, ChanInfoField::SMPGROUP);
            if (f.isOk() && f.value() == static_cast<int64_t>(SampleRate::SR_30kHz))
                count_30k++;
        }
        EXPECT_EQ(count_30k, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Info Accessors
///////////////////////////////////////////////////////////////////////////////////////////////////

class ChannelInfoTest : public NPlayFixture {};

TEST_F(ChannelInfoTest, GetChanInfo) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    const cbPKT_CHANINFO* info = result.value().getChanInfo(1);
    ASSERT_NE(info, nullptr);
    // Channel 1 should be a front-end channel with capabilities
    EXPECT_GT(info->chancaps, 0u);
}

TEST_F(ChannelInfoTest, GetChanInfoInvalid) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    // Channel 0 is invalid (1-based)
    EXPECT_EQ(result.value().getChanInfo(0), nullptr);
}

TEST_F(ChannelInfoTest, GetChannelLabels) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto labels_result = result.value().getChannelLabels(4, ChannelType::FRONTEND);
    ASSERT_TRUE(labels_result.isOk()) << labels_result.error();
    auto& labels = labels_result.value();

    EXPECT_EQ(labels.size(), 4u);
    for (const auto& label : labels) {
        EXPECT_FALSE(label.empty());
    }
}

TEST_F(ChannelInfoTest, GetMatchingChannelIds) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto ids_result = result.value().getMatchingChannelIds(
        cbMAXCHANS, ChannelType::FRONTEND);
    ASSERT_TRUE(ids_result.isOk()) << ids_result.error();

    auto& ids = ids_result.value();
    EXPECT_GT(ids.size(), 0u);
    // IDs should be 1-based
    for (auto id : ids) {
        EXPECT_GE(id, 1u);
    }
}

TEST_F(ChannelInfoTest, GetChannelPositions) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto pos_result = result.value().getChannelPositions(4, ChannelType::FRONTEND);
    ASSERT_TRUE(pos_result.isOk()) << pos_result.error();
    // 4 values per channel
    EXPECT_EQ(pos_result.value().size(), 4u * 4u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// AC Input Coupling
///////////////////////////////////////////////////////////////////////////////////////////////////

class ACCouplingTest : public NPlayFixture {};

TEST_F(ACCouplingTest, SetACCoupling) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto ac_result = result.value().setACInputCoupling(
        8, ChannelType::FRONTEND, true);
    EXPECT_TRUE(ac_result.isOk()) << ac_result.error();
}

TEST_F(ACCouplingTest, SetDCCoupling) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto dc_result = result.value().setACInputCoupling(
        8, ChannelType::FRONTEND, false);
    EXPECT_TRUE(dc_result.isOk()) << dc_result.error();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Sorting
///////////////////////////////////////////////////////////////////////////////////////////////////

class SpikeSortingTest : public NPlayFixture {};

TEST_F(SpikeSortingTest, SetSpikeSorting) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto sort_result = result.value().setChannelSpikeSorting(
        8, ChannelType::FRONTEND, 0);
    EXPECT_TRUE(sort_result.isOk()) << sort_result.error();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Length
///////////////////////////////////////////////////////////////////////////////////////////////////

class SpikeLengthTest : public NPlayFixture {};

TEST_F(SpikeLengthTest, GetSpikeLength) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto length = result.value().getSpikeLength();
    EXPECT_GT(length, 0u);
}

TEST_F(SpikeLengthTest, SetSpikeLength) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    auto set_result = session.setSpikeLength(48, 12);
    EXPECT_TRUE(set_result.isOk()) << set_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(session.getSpikeLength(), 48u);
    EXPECT_EQ(session.getSpikePretrigger(), 12u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Filter Info
///////////////////////////////////////////////////////////////////////////////////////////////////

class FilterInfoTest : public NPlayFixture {};

TEST_F(FilterInfoTest, GetFilterInfo) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    // Filter 0 exists (even if empty)
    const cbPKT_FILTINFO* info = result.value().getFilterInfo(0);
    // May be nullptr if no filters configured, but should not crash
}

TEST_F(FilterInfoTest, GetFilterInfoInvalid) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    EXPECT_EQ(result.value().getFilterInfo(9999), nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CCF Files
///////////////////////////////////////////////////////////////////////////////////////////////////

class CCFTest : public NPlayFixture {};

TEST_F(CCFTest, SaveCCF) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    std::string tmp = makeTempCcfPath();

    auto save_result = result.value().saveCCF(tmp);
    EXPECT_TRUE(save_result.isOk()) << save_result.error();

    // File should exist and be non-empty
    std::ifstream f(tmp, std::ios::ate);
    EXPECT_TRUE(f.is_open());
    EXPECT_GT(f.tellg(), 0);
    f.close();
    std::remove(tmp.c_str());
}

TEST_F(CCFTest, LoadCCF) {
    std::string ccf = getCcfPath();
    if (ccf.empty()) GTEST_SKIP() << "No CCF test file available";

    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto load_result = result.value().loadCCF(ccf);
    EXPECT_TRUE(load_result.isOk()) << load_result.error();
}

TEST_F(CCFTest, SaveLoadRoundtrip) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Configure some channels
    session.setChannelSampleGroup(8, ChannelType::FRONTEND, SampleRate::SR_30kHz, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Save
    std::string tmp = makeTempCcfPath();

    auto save_result = session.saveCCF(tmp);
    ASSERT_TRUE(save_result.isOk()) << save_result.error();

    // Load back
    auto load_result = session.loadCCF(tmp);
    EXPECT_TRUE(load_result.isOk()) << load_result.error();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::remove(tmp.c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CMP Files
///////////////////////////////////////////////////////////////////////////////////////////////////

class CMPTest : public NPlayFixture {};

TEST_F(CMPTest, LoadChannelMap) {
    std::string cmp = getCmpPath();
    if (cmp.empty()) GTEST_SKIP() << "No CMP test file available";

    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto load_result = result.value().loadChannelMap(cmp);
    EXPECT_TRUE(load_result.isOk()) << load_result.error();
}

TEST_F(CMPTest, LoadChannelMapWithBankOffset) {
    std::string cmp = getCmpPath();
    if (cmp.empty()) GTEST_SKIP() << "No CMP test file available";

    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    auto load_result = result.value().loadChannelMap(cmp, 4);
    EXPECT_TRUE(load_result.isOk()) << load_result.error();
}

TEST_F(CMPTest, PositionsAfterCMPLoad) {
    std::string cmp = getCmpPath();
    if (cmp.empty()) GTEST_SKIP() << "No CMP test file available";

    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    auto load_result = session.loadChannelMap(cmp);
    ASSERT_TRUE(load_result.isOk()) << load_result.error();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto pos_result = session.getChannelPositions(4, ChannelType::FRONTEND);
    ASSERT_TRUE(pos_result.isOk()) << pos_result.error();

    // After loading CMP, some positions should be non-zero
    const auto& positions = pos_result.value();
    bool has_nonzero = false;
    for (size_t i = 0; i < positions.size(); i += 4) {
        if (positions[i] != 0 || positions[i+1] != 0 ||
            positions[i+2] != 0 || positions[i+3] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "No non-zero positions found after CMP load";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics
///////////////////////////////////////////////////////////////////////////////////////////////////

class StatsTest : public NPlayFixture {};

TEST_F(StatsTest, NonZeroAfterReceiving) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();

    // The handshake during create() already exchanges packets.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto stats = result.value().getStats();
    EXPECT_GT(stats.packets_received_from_device, 0u);
}

TEST_F(StatsTest, ResetStats) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    session.resetStats();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto stats = session.getStats();
    EXPECT_EQ(stats.packets_dropped, 0u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

class CallbackTest : public NPlayFixture {};

TEST_F(CallbackTest, ReceivePackets) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    std::atomic<int> count{0};
    session.registerPacketCallback([&count](const cbPKT_GENERIC& pkt) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count.load(), 0);
}

TEST_F(CallbackTest, ReceiveGroupData) {
    auto result = createNPlaySession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    session.setChannelSampleGroup(8, ChannelType::FRONTEND, SampleRate::SR_30kHz, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::atomic<int> count{0};
    session.registerGroupCallback(SampleRate::SR_30kHz,
        [&count](const cbPKT_GROUP& pkt) {
            count.fetch_add(1, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    EXPECT_GT(count.load(), 0) << "No group callbacks received";
}
