///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_device_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for cbdev::DeviceSession
///
/// Tests the device transport layer including socket creation, packet send/receive,
/// callback system, and statistics tracking.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cbdev/device_session.h"
#include <cstring>
#include <thread>
#include <chrono>

using namespace cbdev;

/// Test fixture for DeviceSession tests
class DeviceSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique session name for each test
        test_name = "test_session_" + std::to_string(test_counter++);
    }

    void TearDown() override {
        // Cleanup happens automatically via RAII
    }

    std::string test_name;
    static int test_counter;
};

int DeviceSessionTest::test_counter = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, DeviceConfig_Predefined_NSP) {
    auto config = DeviceConfig::forDevice(DeviceType::NSP);

    EXPECT_EQ(config.type, DeviceType::NSP);
    EXPECT_EQ(config.device_address, "192.168.137.128");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51001);
    EXPECT_EQ(config.send_port, 51002);
}

TEST_F(DeviceSessionTest, DeviceConfig_Predefined_Gemini) {
    auto config = DeviceConfig::forDevice(DeviceType::GEMINI_NSP);

    EXPECT_EQ(config.type, DeviceType::GEMINI_NSP);
    EXPECT_EQ(config.device_address, "192.168.137.128");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51001);  // Same port for send & recv
    EXPECT_EQ(config.send_port, 51001);
}

TEST_F(DeviceSessionTest, DeviceConfig_Predefined_GeminiHub1) {
    auto config = DeviceConfig::forDevice(DeviceType::HUB1);

    EXPECT_EQ(config.type, DeviceType::HUB1);
    EXPECT_EQ(config.device_address, "192.168.137.200");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51002);  // Same port for send & recv
    EXPECT_EQ(config.send_port, 51002);
}

TEST_F(DeviceSessionTest, DeviceConfig_Predefined_NPlay) {
    auto config = DeviceConfig::forDevice(DeviceType::NPLAY);

    EXPECT_EQ(config.type, DeviceType::NPLAY);
    EXPECT_EQ(config.device_address, "127.0.0.1");
    EXPECT_EQ(config.client_address, "127.0.0.1");  // Loopback, not 0.0.0.0
    EXPECT_EQ(config.recv_port, 51001);
    EXPECT_EQ(config.send_port, 51001);
}

TEST_F(DeviceSessionTest, DeviceConfig_Custom) {
    auto config = DeviceConfig::custom("10.0.0.100", "10.0.0.1", 12345, 12346);

    EXPECT_EQ(config.type, DeviceType::CUSTOM);
    EXPECT_EQ(config.device_address, "10.0.0.100");
    EXPECT_EQ(config.client_address, "10.0.0.1");
    EXPECT_EQ(config.recv_port, 12345);
    EXPECT_EQ(config.send_port, 12346);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, Create_Loopback) {
    // Use loopback address to avoid network interface requirements
    auto config = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51001, 51002);

    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
}

TEST_F(DeviceSessionTest, Create_BindToAny) {
    // Bind to 0.0.0.0 (INADDR_ANY) - should always work
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51003, 51004);

    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
}

TEST_F(DeviceSessionTest, MoveConstruction) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51005, 51006);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    // Move construct
    DeviceSession session2(std::move(result.value()));
    EXPECT_TRUE(session2.isOpen());
}

TEST_F(DeviceSessionTest, Close) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51007, 51008);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());

    session.close();
    EXPECT_FALSE(session.isOpen());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Send Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, SendPacket_Single) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51009, 51010);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Create test packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x01;
    pkt.cbpkt_header.dlen = 0;

    // Send packet
    auto send_result = session.sendPacket(pkt);
    EXPECT_TRUE(send_result.isOk()) << "Error: " << send_result.error();

    // Check statistics
    auto stats = session.getStats();
    EXPECT_EQ(stats.packets_sent, 1);
    EXPECT_GT(stats.bytes_sent, 0);
}

TEST_F(DeviceSessionTest, SendPackets_Multiple) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51011, 51012);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Create test packets
    cbPKT_GENERIC pkts[5];
    for (int i = 0; i < 5; ++i) {
        std::memset(&pkts[i], 0, sizeof(cbPKT_GENERIC));
        pkts[i].cbpkt_header.type = 0x01 + i;
    }

    // Send packets
    auto send_result = session.sendPackets(pkts, 5);
    EXPECT_TRUE(send_result.isOk()) << "Error: " << send_result.error();

    // Check statistics
    auto stats = session.getStats();
    EXPECT_EQ(stats.packets_sent, 5);
}

TEST_F(DeviceSessionTest, SendPacket_AfterClose) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51013, 51014);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    session.close();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    auto send_result = session.sendPacket(pkt);
    EXPECT_TRUE(send_result.isError());
    EXPECT_NE(send_result.error().find("not open"), std::string::npos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Receive Tests (Loopback)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, PollPacket_Timeout) {
    // Use same port for send/recv to enable loopback
    auto config = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51015, 51015);
    config.non_blocking = true;
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    cbPKT_GENERIC pkt;
    auto poll_result = session.pollPacket(pkt, 10);  // 10ms timeout
    ASSERT_TRUE(poll_result.isOk());
    EXPECT_FALSE(poll_result.value());  // Should timeout (no data)
}

TEST_F(DeviceSessionTest, SendAndReceive_Loopback) {
    // Create two sessions on loopback for send/receive test
    // Session 1: sends on 51016, receives on 51017
    auto config1 = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51016, 51017);
    config1.non_blocking = true;
    auto result1 = DeviceSession::create(config1);
    ASSERT_TRUE(result1.isOk());
    auto& session1 = result1.value();

    // Session 2: sends on 51017, receives on 51016
    auto config2 = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51017, 51016);
    config2.non_blocking = true;
    auto result2 = DeviceSession::create(config2);
    ASSERT_TRUE(result2.isOk());
    auto& session2 = result2.value();

    // Send packet from session1
    cbPKT_GENERIC send_pkt;
    std::memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.cbpkt_header.type = 0x42;
    send_pkt.cbpkt_header.dlen = 0;

    auto send_result = session1.sendPacket(send_pkt);
    ASSERT_TRUE(send_result.isOk());

    // Give time for packet to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receive packet on session2
    cbPKT_GENERIC recv_pkt;
    auto poll_result = session2.pollPacket(recv_pkt, 100);
    ASSERT_TRUE(poll_result.isOk());
    ASSERT_TRUE(poll_result.value());  // Packet should be available

    // Verify packet contents
    EXPECT_EQ(recv_pkt.cbpkt_header.type, 0x42);

    // Check statistics
    auto stats1 = session1.getStats();
    EXPECT_EQ(stats1.packets_sent, 1);

    auto stats2 = session2.getStats();
    EXPECT_EQ(stats2.packets_received, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, SetPacketCallback) {
    auto config = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51018, 51019);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    bool callback_invoked = false;
    session.setPacketCallback([&callback_invoked](const cbPKT_GENERIC* pkts, size_t count) {
        callback_invoked = true;
    });

    // Callback set successfully (no error)
    EXPECT_FALSE(callback_invoked);  // Not invoked yet
}

TEST_F(DeviceSessionTest, ReceiveThread_StartStop) {
    auto config = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51020, 51021);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Set callback
    session.setPacketCallback([](const cbPKT_GENERIC* pkts, size_t count) {
        // Do nothing
    });

    // Start receive thread
    auto start_result = session.startReceiveThread();
    ASSERT_TRUE(start_result.isOk());
    EXPECT_TRUE(session.isReceiveThreadRunning());

    // Give thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop receive thread
    session.stopReceiveThread();
    EXPECT_FALSE(session.isReceiveThreadRunning());
}

TEST_F(DeviceSessionTest, ReceiveThread_ReceivePackets) {
    // Session 1: receives on 51022
    auto config1 = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51022, 51023);
    auto result1 = DeviceSession::create(config1);
    ASSERT_TRUE(result1.isOk());
    auto& session1 = result1.value();

    // Session 2: sends to 51022
    auto config2 = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51024, 51022);
    auto result2 = DeviceSession::create(config2);
    ASSERT_TRUE(result2.isOk());
    auto& session2 = result2.value();

    // Set callback on session1
    std::atomic<int> packets_received{0};
    session1.setPacketCallback([&packets_received](const cbPKT_GENERIC* pkts, size_t count) {
        packets_received.fetch_add(count);
    });

    // Start receive thread
    auto start_result = session1.startReceiveThread();
    ASSERT_TRUE(start_result.isOk());

    // Send 5 packets from session2
    for (int i = 0; i < 5; ++i) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.type = 0x10 + i;
        session2.sendPacket(pkt);
    }

    // Wait for packets to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop receive thread
    session1.stopReceiveThread();

    // Verify packets were received
    EXPECT_EQ(packets_received.load(), 5);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, Statistics_InitiallyZero) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51025, 51026);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    auto stats = session.getStats();

    EXPECT_EQ(stats.packets_sent, 0);
    EXPECT_EQ(stats.packets_received, 0);
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.send_errors, 0);
    EXPECT_EQ(stats.recv_errors, 0);
}

TEST_F(DeviceSessionTest, Statistics_ResetStats) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51027, 51028);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Send some packets
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    session.sendPacket(pkt);
    session.sendPacket(pkt);

    // Verify stats updated
    auto stats1 = session.getStats();
    EXPECT_EQ(stats1.packets_sent, 2);

    // Reset stats
    session.resetStats();

    // Verify stats cleared
    auto stats2 = session.getStats();
    EXPECT_EQ(stats2.packets_sent, 0);
    EXPECT_EQ(stats2.bytes_sent, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, GetConfig) {
    // Use loopback and 0.0.0.0 for binding (guaranteed to work)
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51035, 51036);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    const auto& retrieved_config = session.getConfig();

    EXPECT_EQ(retrieved_config.device_address, "127.0.0.1");
    EXPECT_EQ(retrieved_config.client_address, "0.0.0.0");
    EXPECT_EQ(retrieved_config.recv_port, 51035);
    EXPECT_EQ(retrieved_config.send_port, 51036);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Function Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, DetectLocalIP) {
    std::string ip = detectLocalIP();

    // Should return valid IP string
    EXPECT_FALSE(ip.empty());

    // On macOS, should return "0.0.0.0" (recommended for multi-interface systems)
#ifdef __APPLE__
    EXPECT_EQ(ip, "0.0.0.0");
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Handling Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, Error_SendPacketsNullPointer) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51029, 51030);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    auto send_result = session.sendPackets(nullptr, 5);
    EXPECT_TRUE(send_result.isError());
}

TEST_F(DeviceSessionTest, Error_SendPacketsZeroCount) {
    auto config = DeviceConfig::custom("127.0.0.1", "0.0.0.0", 51031, 51032);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    cbPKT_GENERIC pkts[5];
    auto send_result = session.sendPackets(pkts, 0);
    EXPECT_TRUE(send_result.isError());
}

TEST_F(DeviceSessionTest, Error_StartReceiveThreadTwice) {
    auto config = DeviceConfig::custom("127.0.0.1", "127.0.0.1", 51033, 51034);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    session.setPacketCallback([](const cbPKT_GENERIC* pkts, size_t count) {});

    auto start_result1 = session.startReceiveThread();
    ASSERT_TRUE(start_result1.isOk());

    // Try to start again while running
    auto start_result2 = session.startReceiveThread();
    EXPECT_TRUE(start_result2.isError());

    session.stopReceiveThread();
}
