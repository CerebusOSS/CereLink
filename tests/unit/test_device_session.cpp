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

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_NSP) {
    auto config = ConnectionParams::forDevice(DeviceType::NSP);

    EXPECT_EQ(config.type, DeviceType::NSP);
    EXPECT_EQ(config.device_address, "192.168.137.128");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51001);
    EXPECT_EQ(config.send_port, 51002);
}

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_Gemini) {
    auto config = ConnectionParams::forDevice(DeviceType::GEMINI_NSP);

    EXPECT_EQ(config.type, DeviceType::GEMINI_NSP);
    EXPECT_EQ(config.device_address, "192.168.137.128");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51001);  // Same port for send & recv
    EXPECT_EQ(config.send_port, 51001);
}

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_GeminiHub1) {
    auto config = ConnectionParams::forDevice(DeviceType::HUB1);

    EXPECT_EQ(config.type, DeviceType::HUB1);
    EXPECT_EQ(config.device_address, "192.168.137.200");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51002);  // Same port for send & recv
    EXPECT_EQ(config.send_port, 51002);
}

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_NPlay) {
    auto config = ConnectionParams::forDevice(DeviceType::NPLAY);

    EXPECT_EQ(config.type, DeviceType::NPLAY);
    EXPECT_EQ(config.device_address, "127.0.0.1");
    EXPECT_EQ(config.client_address, "127.0.0.1");  // Loopback, not 0.0.0.0
    EXPECT_EQ(config.recv_port, 51001);
    EXPECT_EQ(config.send_port, 51001);
}

TEST_F(DeviceSessionTest, ConnectionParams_Custom) {
    auto config = ConnectionParams::custom("10.0.0.100", "10.0.0.1", 12345, 12346);

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
    auto config = ConnectionParams::custom("127.0.0.1", "127.0.0.1", 51001, 51002);

    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isConnected());
}

TEST_F(DeviceSessionTest, Create_BindToAny) {
    // Bind to 0.0.0.0 (INADDR_ANY) - should always work
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51003, 51004);

    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isConnected());
}

TEST_F(DeviceSessionTest, MoveConstruction) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51005, 51006);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    // Move construct
    DeviceSession session2(std::move(result.value()));
    EXPECT_TRUE(session2.isConnected());
}

TEST_F(DeviceSessionTest, Close) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51007, 51008);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    EXPECT_TRUE(session.isConnected());

    session.close();
    EXPECT_FALSE(session.isConnected());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Send Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, SendPacket_Single) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51009, 51010);
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
}

TEST_F(DeviceSessionTest, SendPackets_Multiple) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51011, 51012);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Create test packets
    std::vector<cbPKT_GENERIC> pkts(5);
    for (int i = 0; i < 5; ++i) {
        std::memset(&pkts[i], 0, sizeof(cbPKT_GENERIC));
        pkts[i].cbpkt_header.type = 0x01 + i;
    }

    // Send packets (coalesced into minimal datagrams)
    auto send_result = session.sendPackets(pkts);
    EXPECT_TRUE(send_result.isOk()) << "Error: " << send_result.error();
}

TEST_F(DeviceSessionTest, SendPacket_AfterClose) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51013, 51014);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    session.close();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    auto send_result = session.sendPacket(pkt);
    EXPECT_TRUE(send_result.isError());
    EXPECT_NE(send_result.error().find("not connected"), std::string::npos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Receive Tests (Loopback)
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: Callback and statistics tests removed - those features moved to SdkSession
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, GetConfig) {
    // Use loopback and 0.0.0.0 for binding (guaranteed to work)
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51035, 51036);
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

TEST_F(DeviceSessionTest, Error_SendPacketsEmpty) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51029, 51030);
    auto result = DeviceSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    std::vector<cbPKT_GENERIC> empty_pkts;
    auto send_result = session.sendPackets(empty_pkts);
    EXPECT_TRUE(send_result.isError());
}
