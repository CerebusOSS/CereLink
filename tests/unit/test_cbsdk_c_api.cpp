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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;

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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;

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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
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

TEST_F(CbsdkCApiTest, Statistics_InitiallyZero) {
    cbsdk_config_t config = cbsdk_config_default();
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
    // config.recv_port =53009;
    // config.send_port =53010;

    cbsdk_session_t session = nullptr;
    ASSERT_EQ(cbsdk_session_create(&session, &config), CBSDK_RESULT_SUCCESS);

    cbsdk_stats_t stats;
    cbsdk_session_get_stats(session, &stats);

    EXPECT_EQ(stats.packets_received_from_device, 0);
    EXPECT_EQ(stats.packets_stored_to_shmem, 0);
    EXPECT_EQ(stats.packets_queued_for_callback, 0);
    EXPECT_EQ(stats.packets_delivered_to_callback, 0);
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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
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
    config.device_type = CBPROTO_DEVICE_TYPE_NPLAY;
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
