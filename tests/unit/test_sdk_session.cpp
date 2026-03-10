///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for cbsdk::SdkSession
///
/// Tests the SDK orchestration of cbdev + cbshm with the two-stage pipeline.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>  // for memset/memcpy

// Include protocol types and session headers
#include <cbproto/cbproto.h>            // Protocol types
#include "cbshm/shmem_session.h"      // For transmit callback test
#include "cbdev/device_session.h"       // For loopback test
#include "cbdev/device_factory.h"       // For createDeviceSession
#include "cbsdk/sdk_session.h"          // SDK orchestration

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
    EXPECT_TRUE(config.autorun);  // Default is to auto-run (full handshake)
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Creation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, Create_Standalone_Loopback) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;  // Don't auto-run (test mode)

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto session = std::move(result.value());  // Move session out of Result
    EXPECT_TRUE(session.isRunning());  // Session is started, just not in full auto-run mode
}

TEST_F(SdkSessionTest, Create_MoveConstruction) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    // Move construct
    SdkSession session2(std::move(result.value()));
    EXPECT_TRUE(session2.isRunning());  // Session is started
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Lifecycle Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, StartStop) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Session is already started by create()
    EXPECT_TRUE(session.isRunning());

    // Give threads time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop session
    session.stop();
    EXPECT_FALSE(session.isRunning());
}

TEST_F(SdkSessionTest, StartTwice_Error) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Session is already started by create()
    EXPECT_TRUE(session.isRunning());

    // Try to start again - should fail
    auto start_result = session.start();
    EXPECT_TRUE(start_result.isError());

    session.stop();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, SetCallbacks) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    bool packet_callback_invoked = false;
    bool error_callback_invoked = false;

    session.registerPacketCallback([&packet_callback_invoked](const cbPKT_GENERIC& pkt) {
        packet_callback_invoked = true;
    });

    session.setErrorCallback([&error_callback_invoked](const std::string& error) {
        error_callback_invoked = true;
    });

    // Callbacks set successfully
    EXPECT_FALSE(packet_callback_invoked);
    EXPECT_FALSE(error_callback_invoked);
}

TEST_F(SdkSessionTest, ReceivePackets_FromDevice) {
    // Create SDK session (receiver) - receives real packets from connected device
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Set up callback to count packets
    std::atomic<int> packets_received{0};
    session.registerPacketCallback([&packets_received](const cbPKT_GENERIC& pkt) {
        packets_received.fetch_add(1);
    });

    // Session is already started by create()
    EXPECT_TRUE(session.isRunning());

    // Wait for packets from the device
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Stop session
    session.stop();

    // Verify packets were received from the device
    EXPECT_GT(packets_received.load(), 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkSessionTest, Statistics_Valid) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // With a real device, packets may already be flowing; just verify stats are consistent
    auto stats = session.getStats();
    EXPECT_GE(stats.packets_received_from_device, stats.packets_stored_to_shmem);
    EXPECT_EQ(stats.packets_dropped, 0);
}

TEST_F(SdkSessionTest, Statistics_ResetStats) {
    SdkConfig config;
    config.device_type = DeviceType::HUB1;
    config.autorun = false;

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
    config.device_type = DeviceType::HUB1;
    config.callback_queue_depth = 8192;
    config.autorun = false;

    auto result = SdkSession::create(config);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    const auto& retrieved_config = session.getConfig();

    EXPECT_EQ(retrieved_config.device_type, DeviceType::HUB1);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Transmission Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

// Helper to print packet header for debugging
void print_packet_header(const char* label, const cbPKT_GENERIC& pkt) {
    printf("%s:\n", label);
    printf("  time: 0x%08llu (%llu)\n", pkt.cbpkt_header.time, pkt.cbpkt_header.time);
    printf("  chid: 0x%04X\n", pkt.cbpkt_header.chid);
    printf("  type: 0x%02X\n", pkt.cbpkt_header.type);
    printf("  dlen: %u\n", pkt.cbpkt_header.dlen);
    printf("  instrument: %u\n", pkt.cbpkt_header.instrument);

    // Print first few dwords after header for debugging
    const uint32_t* data = reinterpret_cast<const uint32_t*>(&pkt);
    printf("  First 8 dwords: ");
    for (int i = 0; i < 8 && i < pkt.cbpkt_header.dlen + 2; i++) {
        printf("0x%08X ", data[i]);
    }
    printf("\n");
}

// Test packet header construction
TEST_F(SdkSessionTest, PacketHeader_BasicFields) {
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    // Set header fields
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = 0x8000;
    pkt.cbpkt_header.type = 0x92;
    pkt.cbpkt_header.dlen = 10;
    pkt.cbpkt_header.instrument = 0;

    print_packet_header("Basic header test", pkt);

    // Verify fields
    EXPECT_EQ(pkt.cbpkt_header.time, 1u);
    EXPECT_EQ(pkt.cbpkt_header.chid, 0x8000);
    EXPECT_EQ(pkt.cbpkt_header.type, 0x92);
    EXPECT_EQ(pkt.cbpkt_header.dlen, 10u);
}

// Test packet size calculation
TEST_F(SdkSessionTest, PacketSize_Calculation) {
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.dlen = 10;

    // Calculate packet size (as device_session send thread does)
    size_t packet_size = (pkt.cbpkt_header.dlen + 2) * 4;

    printf("dlen = %u\n", pkt.cbpkt_header.dlen);
    printf("Calculated packet_size = %zu\n", packet_size);

    // Verify packet size makes sense
    EXPECT_EQ(packet_size, 48u);  // (10 + 2) * 4 = 48 bytes
    EXPECT_GT(packet_size, 0u);
    EXPECT_LT(packet_size, sizeof(cbPKT_GENERIC));
}

// Test transmit callback - enqueue and dequeue packet through shared memory
TEST_F(SdkSessionTest, TransmitCallback_RoundTrip) {
    // Create shared memory session for testing (use short name to avoid length limits)
    std::string name = "xmt_rt";
    auto shmem_result = cbshm::ShmemSession::create(
        name, name + "_r", name + "_x", name + "_xl",
        name + "_s", name + "_p", name + "_g",
        cbshm::Mode::STANDALONE);
    ASSERT_TRUE(shmem_result.isOk()) << "Failed to create shmem: " << shmem_result.error();
    auto shmem = std::move(shmem_result.value());

    // Create a test packet with known values
    cbPKT_GENERIC test_pkt;
    std::memset(&test_pkt, 0, sizeof(test_pkt));
    test_pkt.cbpkt_header.time = 1;
    test_pkt.cbpkt_header.chid = 0x8000;
    test_pkt.cbpkt_header.type = 0x92;  // cbPKTTYPE_SYSSETRUNLEV
    test_pkt.cbpkt_header.dlen = 10;
    test_pkt.cbpkt_header.instrument = 0;

    // Set some payload data (first few dwords after header)
    uint32_t* payload = reinterpret_cast<uint32_t*>(&test_pkt);
    payload[5] = 0xDEADBEEF;  // Test pattern
    payload[6] = 20;          // Simulated runlevel value

    printf("Original packet before enqueue:\n");
    print_packet_header("  ", test_pkt);

    // Enqueue the packet
    auto enqueue_result = shmem.enqueuePacket(test_pkt);
    ASSERT_TRUE(enqueue_result.isOk()) << "Failed to enqueue: " << enqueue_result.error();

    // Now dequeue using the callback
    cbPKT_GENERIC retrieved_pkt;
    std::memset(&retrieved_pkt, 0, sizeof(retrieved_pkt));

    auto dequeue_result = shmem.dequeuePacket(retrieved_pkt);
    ASSERT_TRUE(dequeue_result.isOk()) << "Failed to dequeue: " << dequeue_result.error();
    ASSERT_TRUE(dequeue_result.value()) << "dequeuePacket returned false (no packet available)";

    printf("\nRetrieved packet after dequeue:\n");
    print_packet_header("  ", retrieved_pkt);

    // Verify all header fields match
    EXPECT_EQ(retrieved_pkt.cbpkt_header.time, test_pkt.cbpkt_header.time)
        << "Time field mismatch";
    EXPECT_EQ(retrieved_pkt.cbpkt_header.chid, test_pkt.cbpkt_header.chid)
        << "Chid field mismatch";
    EXPECT_EQ(retrieved_pkt.cbpkt_header.type, test_pkt.cbpkt_header.type)
        << "Type field mismatch";
    EXPECT_EQ(retrieved_pkt.cbpkt_header.dlen, test_pkt.cbpkt_header.dlen)
        << "Dlen field mismatch";
    EXPECT_EQ(retrieved_pkt.cbpkt_header.instrument, test_pkt.cbpkt_header.instrument)
        << "Instrument field mismatch";

    // Verify payload data
    const uint32_t* original_payload = reinterpret_cast<const uint32_t*>(&test_pkt);
    const uint32_t* retrieved_payload = reinterpret_cast<const uint32_t*>(&retrieved_pkt);
    EXPECT_EQ(retrieved_payload[5], original_payload[5])
        << "Payload[5] mismatch (test pattern)";
    EXPECT_EQ(retrieved_payload[6], original_payload[6])
        << "Payload[6] mismatch (simulated runlevel)";

    printf("\nPayload comparison:\n");
    printf("  Original payload[5]: 0x%08X\n", original_payload[5]);
    printf("  Retrieved payload[5]: 0x%08X\n", retrieved_payload[5]);
    printf("  Original payload[6]: 0x%08X (%u)\n", original_payload[6], original_payload[6]);
    printf("  Retrieved payload[6]: 0x%08X (%u)\n", retrieved_payload[6], retrieved_payload[6]);
}
