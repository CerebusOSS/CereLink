///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_cbsdk_c_api.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for C API (cbsdk.h)
///
/// Tests the C interface for language bindings
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cbsdk/cbsdk.h"
#include <cstring>

/// Test fixture for C API tests
class CbsdkCApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_name = "test_capi_" + std::to_string(test_counter++);
    }

    void TearDown() override {
        // Cleanup happens via cbsdk_session_destroy()
    }

    std::string test_name;
    static int test_counter;
};

int CbsdkCApiTest::test_counter = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, Config_Default) {
    cbsdk_config_t config = cbsdk_config_default();

    EXPECT_EQ(config.device_type, CBPROTO_DEVICE_TYPE_LEGACY_NSP);
    EXPECT_EQ(config.callback_queue_depth, 16384);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Creation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, Create_NullSessionPointer) {
    cbsdk_config_t config = cbsdk_config_default();
    cbsdk_result_t result = cbsdk_session_create(nullptr, &config);
    EXPECT_EQ(result, CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, Create_NullConfig) {
    cbsdk_session_t session = nullptr;
    cbsdk_result_t result = cbsdk_session_create(&session, nullptr);
    EXPECT_EQ(result, CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, Create_Success) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;

    cbsdk_session_t session = nullptr;
    cbsdk_result_t result = cbsdk_session_create(&session, &config);

    EXPECT_EQ(result, CBSDK_RESULT_SUCCESS);
    EXPECT_NE(session, nullptr);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, Destroy_NullSession) {
    // Should not crash
    cbsdk_session_destroy(nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, StartStop) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    // Start session
    EXPECT_EQ(cbsdk_session_start(session), CBSDK_RESULT_SUCCESS);
    EXPECT_TRUE(cbsdk_session_is_running(session));

    // Stop session
    cbsdk_session_stop(session);
    EXPECT_FALSE(cbsdk_session_is_running(session));

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, StartTwice_Error) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    // config.recv_port =53005;
    // config.send_port =53006;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    // start() is idempotent - calling it when already running returns SUCCESS
    EXPECT_EQ(cbsdk_session_start(session), CBSDK_RESULT_SUCCESS);
    EXPECT_EQ(cbsdk_session_start(session), CBSDK_RESULT_SUCCESS);

    cbsdk_session_stop(session);
    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, IsRunning_NullSession) {
    EXPECT_FALSE(cbsdk_session_is_running(nullptr));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

static int g_packet_callback_count = 0;
static int g_error_callback_count = 0;

static void packet_callback(const cbPKT_GENERIC* pkts, size_t count, void* user_data) {
    g_packet_callback_count++;
    int* counter = static_cast<int*>(user_data);
    if (counter) {
        (*counter)++;
    }
}

static void error_callback(const char* error_message, void* user_data) {
    g_error_callback_count++;
    int* counter = static_cast<int*>(user_data);
    if (counter) {
        (*counter)++;
    }
}

TEST_F(CbsdkCApiTest, SetCallbacks) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    // config.recv_port =53007;
    // config.send_port =53008;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    int packet_user_data = 0;
    int error_user_data = 0;

    cbsdk_session_set_packet_callback(session, packet_callback, &packet_user_data);
    cbsdk_session_set_error_callback(session, error_callback, &error_user_data);

    // Callbacks set successfully (no crash)
    EXPECT_EQ(packet_user_data, 0);
    EXPECT_EQ(error_user_data, 0);

    cbsdk_session_destroy(session);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, Statistics_Valid) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    cbsdk_stats_t stats;
    cbsdk_session_get_stats(session, &stats);

    // With a real device, packets may already be flowing; just verify no drops
    EXPECT_GE(stats.packets_received_from_device, stats.packets_stored_to_shmem);
    EXPECT_EQ(stats.packets_dropped, 0);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, Statistics_GetStats_NullSession) {
    cbsdk_stats_t stats;
    cbsdk_session_get_stats(nullptr, &stats);
    // Should not crash
}

TEST_F(CbsdkCApiTest, Statistics_GetStats_NullStats) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    // config.recv_port =53011;
    // config.send_port =53012;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    cbsdk_session_get_stats(session, nullptr);
    // Should not crash

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, Statistics_ResetStats) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    // config.recv_port =53013;
    // config.send_port =53014;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    cbsdk_session_reset_stats(session);

    cbsdk_stats_t stats;
    cbsdk_session_get_stats(session, &stats);
    EXPECT_EQ(stats.packets_received_from_device, 0);

    cbsdk_session_destroy(session);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Typed Callback Registration Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

static void event_callback(const cbPKT_GENERIC* pkt, void* user_data) {
    int* counter = static_cast<int*>(user_data);
    if (counter) (*counter)++;
}

static void group_callback(const cbPKT_GROUP* pkt, void* user_data) {
    int* counter = static_cast<int*>(user_data);
    if (counter) (*counter)++;
}

static void config_callback(const cbPKT_GENERIC* pkt, void* user_data) {
    int* counter = static_cast<int*>(user_data);
    if (counter) (*counter)++;
}

TEST_F(CbsdkCApiTest, RegisterPacketCallback_NullSession) {
    int counter = 0;
    EXPECT_EQ(cbsdk_session_register_packet_callback(nullptr, packet_callback, &counter), 0);
}

TEST_F(CbsdkCApiTest, RegisterPacketCallback_NullCallback) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    EXPECT_EQ(cbsdk_session_register_packet_callback(session, nullptr, nullptr), 0);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, RegisterTypedCallbacks) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    int counter = 0;

    // Register each typed callback and verify we get a valid handle
    cbsdk_callback_handle_t h1 = cbsdk_session_register_packet_callback(
        session, packet_callback, &counter);
    EXPECT_NE(h1, 0);

    cbsdk_callback_handle_t h2 = cbsdk_session_register_event_callback(
        session, CBPROTO_CHANNEL_TYPE_FRONTEND, event_callback, &counter);
    EXPECT_NE(h2, 0);

    cbsdk_callback_handle_t h3 = cbsdk_session_register_group_callback(
        session, CBPROTO_GROUP_RATE_30000Hz, group_callback, &counter);
    EXPECT_NE(h3, 0);

    cbsdk_callback_handle_t h4 = cbsdk_session_register_config_callback(
        session, 0x01, config_callback, &counter);
    EXPECT_NE(h4, 0);

    // All handles should be unique
    EXPECT_NE(h1, h2);
    EXPECT_NE(h2, h3);
    EXPECT_NE(h3, h4);

    // Unregister all (should not crash)
    cbsdk_session_unregister_callback(session, h1);
    cbsdk_session_unregister_callback(session, h2);
    cbsdk_session_unregister_callback(session, h3);
    cbsdk_session_unregister_callback(session, h4);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, RegisterEventCallback_AnyChannel) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    int counter = 0;
    // -1 cast to cbproto_channel_type_t = ANY
    cbsdk_callback_handle_t h = cbsdk_session_register_event_callback(
        session, (cbproto_channel_type_t)(-1), event_callback, &counter);
    EXPECT_NE(h, 0);

    cbsdk_session_unregister_callback(session, h);
    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, UnregisterCallback_NullSession) {
    // Should not crash
    cbsdk_session_unregister_callback(nullptr, 1);
}

TEST_F(CbsdkCApiTest, UnregisterCallback_ZeroHandle) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    // Should not crash
    cbsdk_session_unregister_callback(session, 0);

    cbsdk_session_destroy(session);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, GetRunlevel_NullSession) {
    EXPECT_EQ(cbsdk_session_get_runlevel(nullptr), 0);
}

TEST_F(CbsdkCApiTest, GetChannelLabel_NullSession) {
    EXPECT_EQ(cbsdk_session_get_channel_label(nullptr, 1), nullptr);
}

TEST_F(CbsdkCApiTest, GetChannelSmpgroup_NullSession) {
    EXPECT_EQ(cbsdk_session_get_channel_smpgroup(nullptr, 1), 0);
}

TEST_F(CbsdkCApiTest, GetChannelChancaps_NullSession) {
    EXPECT_EQ(cbsdk_session_get_channel_chancaps(nullptr, 1), 0);
}

TEST_F(CbsdkCApiTest, GetGroupLabel_NullSession) {
    EXPECT_EQ(cbsdk_session_get_group_label(nullptr, 1), nullptr);
}

TEST_F(CbsdkCApiTest, GetConstants) {
    EXPECT_GT(cbsdk_get_max_chans(), 0);
    EXPECT_GT(cbsdk_get_num_fe_chans(), 0);
    EXPECT_GT(cbsdk_get_num_analog_chans(), 0);
    EXPECT_GE(cbsdk_get_max_chans(), cbsdk_get_num_analog_chans());
}

TEST_F(CbsdkCApiTest, ConfigAccess_WithSession) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    // These may return NULL/0 without a device, but must not crash
    cbsdk_session_get_channel_label(session, 1);
    cbsdk_session_get_channel_label(session, 0);       // Invalid channel
    cbsdk_session_get_channel_label(session, 99999);    // Out of range
    cbsdk_session_get_channel_smpgroup(session, 1);
    cbsdk_session_get_channel_chancaps(session, 1);
    cbsdk_session_get_group_label(session, 1);
    cbsdk_session_get_group_label(session, 0);          // Invalid group
    cbsdk_session_get_runlevel(session);

    cbsdk_session_destroy(session);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Configuration Tests (NULL safety)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, SetChannelSampleGroup_NullSession) {
    EXPECT_EQ(cbsdk_session_set_channel_sample_group(nullptr, 256,
        CBPROTO_CHANNEL_TYPE_FRONTEND, CBPROTO_GROUP_RATE_30000Hz, false), CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, SetChannelConfig_NullSession) {
    cbPKT_CHANINFO info = {};
    EXPECT_EQ(cbsdk_session_set_channel_config(nullptr, &info), CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, SetChannelConfig_NullChaninfo) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    EXPECT_EQ(cbsdk_session_set_channel_config(session, nullptr), CBSDK_RESULT_INVALID_PARAMETER);

    cbsdk_session_destroy(session);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Command Tests (NULL safety)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, SendComment_NullSession) {
    EXPECT_EQ(cbsdk_session_send_comment(nullptr, "test", 0, 0), CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, SendComment_NullComment) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    EXPECT_EQ(cbsdk_session_send_comment(session, nullptr, 0, 0), CBSDK_RESULT_INVALID_PARAMETER);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, SendPacket_NullSession) {
    cbPKT_GENERIC pkt = {};
    EXPECT_EQ(cbsdk_session_send_packet(nullptr, &pkt), CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, SendPacket_NullPacket) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    EXPECT_EQ(cbsdk_session_send_packet(session, nullptr), CBSDK_RESULT_INVALID_PARAMETER);

    cbsdk_session_destroy(session);
}

TEST_F(CbsdkCApiTest, SetDigitalOutput_NullSession) {
    EXPECT_EQ(cbsdk_session_set_digital_output(nullptr, 1, 0), CBSDK_RESULT_INVALID_PARAMETER);
}

TEST_F(CbsdkCApiTest, SetRunlevel_NullSession) {
    EXPECT_EQ(cbsdk_session_set_runlevel(nullptr, 0), CBSDK_RESULT_INVALID_PARAMETER);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Handling Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, ErrorMessage_AllCodes) {
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_SUCCESS), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_INVALID_PARAMETER), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_ALREADY_RUNNING), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_NOT_RUNNING), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_SHMEM_ERROR), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_DEVICE_ERROR), "");
    EXPECT_STRNE(cbsdk_get_error_message(CBSDK_RESULT_INTERNAL_ERROR), "");
}

TEST_F(CbsdkCApiTest, ErrorMessage_InvalidCode) {
    const char* msg = cbsdk_get_error_message((cbsdk_result_t)9999);
    EXPECT_STRNE(msg, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Version Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CbsdkCApiTest, Version) {
    const char* version = cbsdk_get_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0);
}
