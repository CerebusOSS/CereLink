///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_capi_configuration.cpp
/// @brief  Integration tests for C API (cbsdk.h) device configuration
///
/// Mirrors the C++ tests but exercises the C FFI interface used by pycbsdk
/// and other language bindings.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "nplay_fixture.h"

#include "cbsdk/cbsdk.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

static std::string makeTempCcfPath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    std::string path = std::string(buf) + "cerelink_capi_XXXXXX.ccf";
    _mktemp_s(&path[0], path.size() + 1);
    return path;
#else
    char tmpl[] = "/tmp/cerelink_capi_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) close(fd);
    std::string path = std::string(tmpl) + ".ccf";
    std::rename(tmpl, path.c_str());
    return path;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper: RAII wrapper for cbsdk_session_t
///////////////////////////////////////////////////////////////////////////////////////////////////

struct SessionGuard {
    cbsdk_session_t session = nullptr;

    SessionGuard() = default;
    ~SessionGuard() {
        if (session) {
            cbsdk_session_stop(session);
            cbsdk_session_destroy(session);
        }
    }
    SessionGuard(const SessionGuard&) = delete;
    SessionGuard& operator=(const SessionGuard&) = delete;

    /// Create and start a session connected to nPlayServer
    bool create() {
        cbsdk_config_t config = cbsdk_config_default();
        config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
        cbsdk_result_t result = cbsdk_session_create(&session, &config);
        return result == CBSDK_RESULT_SUCCESS;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiLifecycleTest : public NPlayFixture {};

TEST_F(CApiLifecycleTest, ConnectsToNPlay) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());
    EXPECT_TRUE(cbsdk_session_is_running(sg.session));
}

TEST_F(CApiLifecycleTest, IsStandalone) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());
    EXPECT_EQ(cbsdk_session_is_standalone(sg.session), 1);
}

TEST_F(CApiLifecycleTest, ProtocolVersion) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());
    EXPECT_NE(cbsdk_session_get_protocol_version(sg.session), 0u);
}

TEST_F(CApiLifecycleTest, ProcIdent) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    char buf[64] = {};
    uint32_t len = cbsdk_session_get_proc_ident(sg.session, buf, sizeof(buf));
    EXPECT_GT(len, 0u);
    EXPECT_GT(strlen(buf), 0u);
}

TEST_F(CApiLifecycleTest, SysFreq) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());
    EXPECT_EQ(cbsdk_session_get_sysfreq(sg.session), 30000u);
}

TEST_F(CApiLifecycleTest, Version) {
    const char* ver = cbsdk_get_version();
    EXPECT_NE(ver, nullptr);
    EXPECT_GT(strlen(ver), 0u);
}

TEST_F(CApiLifecycleTest, Constants) {
    EXPECT_GT(cbsdk_get_max_chans(), 0u);
    EXPECT_GT(cbsdk_get_num_fe_chans(), 0u);
    EXPECT_GT(cbsdk_get_num_analog_chans(), 0u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Sample Group Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiSampleGroupTest : public NPlayFixture {};

TEST_F(CApiSampleGroupTest, SetFrontend30kHz) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_sample_group(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND,
        CBPROTO_GROUP_RATE_30000Hz, true), CBSDK_RESULT_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify via group list
    uint16_t list[256];
    uint32_t count = 256;
    EXPECT_EQ(cbsdk_session_get_group_list(
        sg.session, CBPROTO_GROUP_RATE_30000Hz, list, &count), CBSDK_RESULT_SUCCESS);
    EXPECT_GE(count, 4u);
}

TEST_F(CApiSampleGroupTest, SetAndVerifyField) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_sample_group(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND,
        CBPROTO_GROUP_RATE_10000Hz, true), CBSDK_RESULT_SUCCESS);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Query via bulk field getter
    int64_t values[512];
    uint32_t count = 512;
    EXPECT_EQ(cbsdk_session_get_channels_field(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND,
        CBSDK_CHANINFO_FIELD_SMPGROUP, values, &count), CBSDK_RESULT_SUCCESS);

    EXPECT_EQ(count, 4u);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_EQ(values[i], CBPROTO_GROUP_RATE_10000Hz)
            << "Channel " << i << " has wrong sample group";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Info Accessors
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiChannelInfoTest : public NPlayFixture {};

TEST_F(CApiChannelInfoTest, GetChannelLabel) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    const char* label = cbsdk_session_get_channel_label(sg.session, 1);
    EXPECT_NE(label, nullptr);
    EXPECT_GT(strlen(label), 0u);
}

TEST_F(CApiChannelInfoTest, GetChannelType) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    cbproto_channel_type_t ct = cbsdk_session_get_channel_type(sg.session, 1);
    EXPECT_EQ(ct, CBPROTO_CHANNEL_TYPE_FRONTEND);
}

TEST_F(CApiChannelInfoTest, GetChannelChancaps) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    uint32_t caps = cbsdk_session_get_channel_chancaps(sg.session, 1);
    EXPECT_GT(caps, 0u);
}

TEST_F(CApiChannelInfoTest, GetChannelScaling) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    cbsdk_channel_scaling_t scaling;
    EXPECT_EQ(cbsdk_session_get_channel_scaling(sg.session, 1, &scaling),
              CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiChannelInfoTest, GetMatchingChannels) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    uint32_t ids[512];
    uint32_t count = 512;
    EXPECT_EQ(cbsdk_session_get_matching_channels(
        sg.session, cbsdk_get_max_chans(), CBPROTO_CHANNEL_TYPE_FRONTEND,
        ids, &count), CBSDK_RESULT_SUCCESS);
    EXPECT_GT(count, 0u);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_GE(ids[i], 1u);
    }
}

TEST_F(CApiChannelInfoTest, GetChannelsLabels) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    const uint32_t n = 4;
    char buf[n * 16];
    uint32_t count = n;
    EXPECT_EQ(cbsdk_session_get_channels_labels(
        sg.session, n, CBPROTO_CHANNEL_TYPE_FRONTEND,
        buf, 16, &count), CBSDK_RESULT_SUCCESS);
    EXPECT_EQ(count, n);
    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_GT(strlen(buf + i * 16), 0u);
    }
}

TEST_F(CApiChannelInfoTest, GetGroupLabel) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    const char* label = cbsdk_session_get_group_label(sg.session, CBPROTO_GROUP_RATE_30000Hz);
    EXPECT_NE(label, nullptr);
}

TEST_F(CApiChannelInfoTest, GetChannelField) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    int64_t value = cbsdk_session_get_channel_field(
        sg.session, 1, CBSDK_CHANINFO_FIELD_CHANCAPS);
    EXPECT_GT(value, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// AC Input Coupling
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiACCouplingTest : public NPlayFixture {};

TEST_F(CApiACCouplingTest, SetACCoupling) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_ac_input_coupling(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND, true), CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiACCouplingTest, SetDCCoupling) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_ac_input_coupling(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND, false), CBSDK_RESULT_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Sorting
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiSpikeSortingTest : public NPlayFixture {};

TEST_F(CApiSpikeSortingTest, SetSpikeSorting) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_spike_sorting(
        sg.session, 4, CBPROTO_CHANNEL_TYPE_FRONTEND, 0), CBSDK_RESULT_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Length
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiSpikeLengthTest : public NPlayFixture {};

TEST_F(CApiSpikeLengthTest, GetSpikeLength) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_GT(cbsdk_session_get_spike_length(sg.session), 0u);
}

TEST_F(CApiSpikeLengthTest, SetSpikeLength) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_spike_length(sg.session, 48, 12), CBSDK_RESULT_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(cbsdk_session_get_spike_length(sg.session), 48u);
    EXPECT_EQ(cbsdk_session_get_spike_pretrigger(sg.session), 12u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Per-Channel Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiPerChannelTest : public NPlayFixture {};

TEST_F(CApiPerChannelTest, SetChannelLabel) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_label(sg.session, 1, "TestCh"),
              CBSDK_RESULT_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const char* label = cbsdk_session_get_channel_label(sg.session, 1);
    ASSERT_NE(label, nullptr);
    EXPECT_STREQ(label, "TestCh");
}

TEST_F(CApiPerChannelTest, SetChannelSmpfilter) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_smpfilter(sg.session, 1, 2),
              CBSDK_RESULT_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(cbsdk_session_get_channel_smpfilter(sg.session, 1), 2u);
}

TEST_F(CApiPerChannelTest, SetChannelSpkfilter) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_set_channel_spkfilter(sg.session, 1, 3),
              CBSDK_RESULT_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(cbsdk_session_get_channel_spkfilter(sg.session, 1), 3u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Filter Info
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiFilterTest : public NPlayFixture {};

TEST_F(CApiFilterTest, NumFilters) {
    EXPECT_GT(cbsdk_get_num_filters(), 0u);
}

TEST_F(CApiFilterTest, GetFilterLabel) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    // Filter 0 — may or may not have a label, but should not crash
    cbsdk_session_get_filter_label(sg.session, 0);
}

TEST_F(CApiFilterTest, GetFilterLabelInvalid) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_get_filter_label(sg.session, 9999), nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CCF Files
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiCCFTest : public NPlayFixture {};

TEST_F(CApiCCFTest, SaveCCF) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::string tmp = makeTempCcfPath();
    EXPECT_EQ(cbsdk_session_save_ccf(sg.session, tmp.c_str()), CBSDK_RESULT_SUCCESS);

    std::ifstream f(tmp, std::ios::ate);
    EXPECT_TRUE(f.is_open());
    EXPECT_GT(f.tellg(), 0);
    f.close();
    std::remove(tmp.c_str());
}

TEST_F(CApiCCFTest, LoadCCF) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::string ccf;
#ifdef NPLAY_CCF_PATH
    ccf = NPLAY_CCF_PATH;
#endif
    if (ccf.empty()) GTEST_SKIP() << "No CCF test file available";

    EXPECT_EQ(cbsdk_session_load_ccf(sg.session, ccf.c_str()), CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiCCFTest, LoadCCFSync) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::string ccf;
#ifdef NPLAY_CCF_PATH
    ccf = NPLAY_CCF_PATH;
#endif
    if (ccf.empty()) GTEST_SKIP() << "No CCF test file available";

    EXPECT_EQ(cbsdk_session_load_ccf_sync(sg.session, ccf.c_str(), 5000),
              CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiCCFTest, LoadCCFSyncInvalidFile) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_NE(cbsdk_session_load_ccf_sync(sg.session, "/nonexistent.ccf", 1000),
              CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiCCFTest, LoadCCFSyncNullArgs) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    EXPECT_EQ(cbsdk_session_load_ccf_sync(nullptr, "file.ccf", 1000),
              CBSDK_RESULT_INVALID_PARAMETER);
    EXPECT_EQ(cbsdk_session_load_ccf_sync(sg.session, nullptr, 1000),
              CBSDK_RESULT_INVALID_PARAMETER);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CMP Files
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiCMPTest : public NPlayFixture {};

TEST_F(CApiCMPTest, LoadChannelMap) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::string cmp;
#ifdef NPLAY_CMP_PATH
    cmp = NPLAY_CMP_PATH;
#endif
    if (cmp.empty()) GTEST_SKIP() << "No CMP test file available";

    EXPECT_EQ(cbsdk_session_load_channel_map(sg.session, cmp.c_str(), 0),
              CBSDK_RESULT_SUCCESS);
}

TEST_F(CApiCMPTest, LoadChannelMapWithBankOffset) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::string cmp;
#ifdef NPLAY_CMP_PATH
    cmp = NPLAY_CMP_PATH;
#endif
    if (cmp.empty()) GTEST_SKIP() << "No CMP test file available";

    EXPECT_EQ(cbsdk_session_load_channel_map(sg.session, cmp.c_str(), 4),
              CBSDK_RESULT_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics
///////////////////////////////////////////////////////////////////////////////////////////////////

class CApiStatsTest : public NPlayFixture {};

TEST_F(CApiStatsTest, NonZeroAfterReceiving) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::this_thread::sleep_for(std::chrono::seconds(2));

    cbsdk_stats_t stats;
    cbsdk_session_get_stats(sg.session, &stats);
    EXPECT_GT(stats.packets_received_from_device, 0u);
}

TEST_F(CApiStatsTest, ResetStats) {
    SessionGuard sg;
    ASSERT_TRUE(sg.create());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cbsdk_session_reset_stats(sg.session);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    cbsdk_stats_t stats;
    cbsdk_session_get_stats(sg.session, &stats);
    EXPECT_EQ(stats.packets_dropped, 0u);
}
