///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for cbsdk::SdkSession
///
/// Tests the SDK orchestration of cbdev + cbshmem with the two-stage pipeline.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cbsdk_v2/sdk_session.h"
#include "cbdev/device_session.h"  // For loopback test
#include <thread>
#include <chrono>
#include <atomic>

using namespace cbsdk;

/// Test fixture for SdkSession tests
class SdkSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_name = "test_sdk_" + std::to_string(test_counter++);
    }

    void TearDown() override {
        // Cleanup happens automatically via RAII
    }

    std::string test_name;
    static int test_counter;
};

int SdkSessionTest::test_counter = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, Config_Default) {
    SdkConfig config;

    EXPECT_EQ(config.device_type, DeviceType::LEGACY_NSP);
    EXPECT_EQ(config.callback_queue_depth, 16384);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Creation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, Create_Standalone_Loopback) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;  // Loopback device

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_FALSE(session.isRunning());
}

TEST_F(SdkSessionTest, Create_MoveConstruction) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    // Move construct
    SdkSession session2(std::move(result.value()));
    EXPECT_FALSE(session2.isRunning());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, StartStop) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Start session
    auto start_result = session.start();
    ASSERT_TRUE(start_result.isOk()) << "Error: " << start_result.error();
    EXPECT_TRUE(session.isRunning());

    // Give threads time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop session
    session.stop();
    EXPECT_FALSE(session.isRunning());
}

TEST_F(SdkSessionTest, StartTwice_Error) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    auto start_result1 = session.start();
    ASSERT_TRUE(start_result1.isOk());

    // Try to start again
    auto start_result2 = session.start();
    EXPECT_TRUE(start_result2.isError());

    session.stop();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, SetCallbacks) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    bool packet_callback_invoked = false;
    bool error_callback_invoked = false;

    session.setPacketCallback([&packet_callback_invoked](const cbPKT_GENERIC* pkts, size_t count) {
        packet_callback_invoked = true;
    });

    session.setErrorCallback([&error_callback_invoked](const std::string& error) {
        error_callback_invoked = true;
    });

    // Callbacks set successfully
    EXPECT_FALSE(packet_callback_invoked);
    EXPECT_FALSE(error_callback_invoked);
}

TEST_F(SdkSessionTest, ReceivePackets_Loopback) {
    // Create SDK session (receiver)
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Set up callback to count packets
    std::atomic<int> packets_received{0};
    session.setPacketCallback([&packets_received](const cbPKT_GENERIC* pkts, size_t count) {
        packets_received.fetch_add(count);
    });

    // Start session
    auto start_result = session.start();
    ASSERT_TRUE(start_result.isOk());

    // Give threads time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create a separate device session to send packets
    // Sender: bind to different port (51002) but send to receiver's port (51001)
    auto dev_config = cbdev::DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51002, 51001);
    auto dev_result = cbdev::DeviceSession::create(dev_config);
    ASSERT_TRUE(dev_result.isOk()) << "Error: " << dev_result.error();
    auto& dev_session = dev_result.value();

    // Send test packets
    for (int i = 0; i < 10; ++i) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.type = 0x10 + i;
        pkt.cbpkt_header.dlen = 0;
        dev_session.sendPacket(pkt);
    }

    // Wait for packets to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Stop session
    session.stop();

    // Verify packets were received
    EXPECT_GT(packets_received.load(), 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, Statistics_InitiallyZero) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    auto stats = session.getStats();

    EXPECT_EQ(stats.packets_received_from_device, 0);
    EXPECT_EQ(stats.packets_stored_to_shmem, 0);
    EXPECT_EQ(stats.packets_queued_for_callback, 0);
    EXPECT_EQ(stats.packets_delivered_to_callback, 0);
    EXPECT_EQ(stats.packets_dropped, 0);
}

TEST_F(SdkSessionTest, Statistics_ResetStats) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Start and immediately stop (generates some internal activity)
    session.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    session.stop();

    // Reset stats
    session.resetStats();

    auto stats = session.getStats();
    EXPECT_EQ(stats.packets_received_from_device, 0);
    EXPECT_EQ(stats.packets_stored_to_shmem, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, GetConfig) {
    SdkConfig config;
    config.device_type = DeviceType::NPLAY;
    config.callback_queue_depth = 8192;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    const auto& retrieved_config = session.getConfig();

    EXPECT_EQ(retrieved_config.device_type, DeviceType::NPLAY);
    EXPECT_EQ(retrieved_config.callback_queue_depth, 8192);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SPSC Queue Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, SPSCQueue_PushPop) {
    SPSCQueue<int, 16> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    // Push items
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 3);

    // Pop items
    int val;
    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 2);

    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 3);

    EXPECT_TRUE(queue.empty());
}

TEST_F(SdkSessionTest, SPSCQueue_Overflow) {
    SPSCQueue<int, 4> queue;  // Small queue (capacity 3)

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));

    // Queue should be full now (one slot reserved)
    EXPECT_FALSE(queue.push(4));  // Should fail

    // Pop one item
    int val;
    EXPECT_TRUE(queue.pop(val));

    // Now we can push again
    EXPECT_TRUE(queue.push(4));
}
