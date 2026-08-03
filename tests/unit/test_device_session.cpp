///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_device_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for cbdev::IDeviceSession (via createDeviceSession factory)
///
/// Tests the device transport layer including socket creation, packet send/receive,
/// and configuration access.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cbdev/device_session.h"
#include "cbdev/device_factory.h"
#include <cbproto/cbproto.h>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using fake_socket_t = SOCKET;
    static constexpr fake_socket_t kInvalidFakeSocket = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/time.h>
    using fake_socket_t = int;
    static constexpr fake_socket_t kInvalidFakeSocket = -1;
#endif

using namespace cbdev;

namespace {

/// Minimal RAII UDP "device": binds to 127.0.0.1:port and exposes recvOne().
/// Used to capture the exact bytes a DeviceSession puts on the wire so we can
/// assert they were down-translated to the legacy protocol. Create it AFTER a
/// DeviceSession exists so Winsock is already initialised on Windows.
class FakeDeviceSocket {
public:
    explicit FakeDeviceSocket(uint16_t port) {
        m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_sock == kInvalidFakeSocket) return;

        // 1 s receive timeout so a missing packet fails the test instead of hanging.
#ifdef _WIN32
        DWORD tv = 1000;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
        timeval tv{};
        tv.tv_sec = 1;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        m_bound = bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    ~FakeDeviceSocket() {
        if (m_sock != kInvalidFakeSocket) {
#ifdef _WIN32
            closesocket(m_sock);
#else
            ::close(m_sock);
#endif
        }
    }

    [[nodiscard]] bool ok() const { return m_sock != kInvalidFakeSocket && m_bound; }

    /// Block (up to the 1 s timeout) for one datagram. Returns bytes received, or <=0.
    int recvOne(uint8_t* buf, size_t buflen) {
        return static_cast<int>(recvfrom(m_sock, reinterpret_cast<char*>(buf),
                                         static_cast<int>(buflen), 0, nullptr, nullptr));
    }

    FakeDeviceSocket(const FakeDeviceSocket&) = delete;
    FakeDeviceSocket& operator=(const FakeDeviceSocket&) = delete;

private:
    fake_socket_t m_sock = kInvalidFakeSocket;
    bool m_bound = false;
};

/// Read a little-endian uint16 from a byte offset.
uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

// Legacy on-wire header sizes (bytes).
constexpr size_t kHeaderSize311 = 8;   // time(32) chid(16) type(8) dlen(8)
constexpr size_t kHeaderSize400 = 16;  // time(64) chid(16) type(8) dlen(16) instr(8) rsvd(16)

}  // namespace

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

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_LegacyNSP) {
    auto config = ConnectionParams::forDevice(DeviceType::LEGACY_NSP);

    EXPECT_EQ(config.type, DeviceType::LEGACY_NSP);
    EXPECT_EQ(config.device_address, "192.168.137.128");
    EXPECT_EQ(config.client_address, "");  // Auto-detect
    EXPECT_EQ(config.recv_port, 51002);
    EXPECT_EQ(config.send_port, 51001);
}

TEST_F(DeviceSessionTest, ConnectionParams_Predefined_Gemini) {
    auto config = ConnectionParams::forDevice(DeviceType::NSP);

    EXPECT_EQ(config.type, DeviceType::NSP);
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
    // Empty means auto-detect; createDeviceSession() resolves it to loopback
    // for NPLAY via detectClientAddress() before probing.
    EXPECT_EQ(config.client_address, "");
    EXPECT_EQ(config.recv_port, 51002);  // LEGACY_NSP_RECV_PORT (bcast)
    EXPECT_EQ(config.send_port, 51001);  // LEGACY_NSP_SEND_PORT (cnt)
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

    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session->isConnected());
}

TEST_F(DeviceSessionTest, Create_BindToAny) {
    // Bind to 0.0.0.0 (INADDR_ANY) - should always work
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51003, 51004);

    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk()) << "Error: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session->isConnected());
}

TEST_F(DeviceSessionTest, MoveConstruction) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51005, 51006);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    // Move the unique_ptr
    auto session2 = std::move(result.value());
    EXPECT_TRUE(session2->isConnected());
}

TEST_F(DeviceSessionTest, Destroy_ViaReset) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51007, 51008);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto session = std::move(result.value());
    EXPECT_TRUE(session->isConnected());

    // Destroy via unique_ptr reset (RAII cleanup)
    session.reset();
    EXPECT_EQ(session, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Send Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, SendPacket_Single) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51009, 51010);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Create test packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x01;
    pkt.cbpkt_header.dlen = 0;

    // Send packet
    auto send_result = session->sendPacket(pkt);
    EXPECT_TRUE(send_result.isOk()) << "Error: " << send_result.error();
}

TEST_F(DeviceSessionTest, SendPackets_Multiple) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51011, 51012);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    // Create test packets
    std::vector<cbPKT_GENERIC> pkts(5);
    for (int i = 0; i < 5; ++i) {
        std::memset(&pkts[i], 0, sizeof(cbPKT_GENERIC));
        pkts[i].cbpkt_header.type = 0x01 + i;
    }

    // Send packets (coalesced into minimal datagrams)
    auto send_result = session->sendPackets(pkts);
    EXPECT_TRUE(send_result.isOk()) << "Error: " << send_result.error();
}

TEST_F(DeviceSessionTest, SendPacket_AfterDestroy) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51013, 51014);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto session = std::move(result.value());
    EXPECT_TRUE(session->isConnected());

    // Destroy session via RAII
    session.reset();
    EXPECT_EQ(session, nullptr);
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

TEST_F(DeviceSessionTest, GetConnectionParams) {
    // Use loopback and 0.0.0.0 for binding (guaranteed to work)
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51035, 51036);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();
    const auto& retrieved_config = session->getConnectionParams();

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
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk());

    auto& session = result.value();

    std::vector<cbPKT_GENERIC> empty_pkts;
    auto send_result = session->sendPackets(empty_pkts);
    EXPECT_TRUE(send_result.isError());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration-packet send pacing
//
// Bursts of per-channel setChannelConfig() sends must be throttled so they don't
// overrun a device's small UDP receive buffer (which drops packets, including a
// following runlevel sync barrier). sendPacket() enforces a minimum gap between
// consecutive configuration-channel sends; streaming/data packets are exempt.
// std::this_thread::sleep_for only ever sleeps AT LEAST the requested time, so
// the lower-bound timing assertion below cannot be flaky.
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, ConfigSends_ArePaced_DataSends_AreNot) {
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51045, 51046);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    constexpr int kBurst = 40;  // 39 gaps * 200us >= ~7.8 ms guaranteed for config

    // Data-channel packets (chid without the configuration bit) are not throttled.
    cbPKT_GENERIC data{};
    data.cbpkt_header.chid = 1;  // a real acquisition channel, not configuration
    data.cbpkt_header.type = 0x0006;
    const auto data_start = std::chrono::steady_clock::now();
    for (int i = 0; i < kBurst; ++i) ASSERT_TRUE(session->sendPacket(data).isOk());
    const auto data_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - data_start).count();

    // Configuration-channel packets are paced.
    cbPKT_GENERIC cfg{};
    cfg.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    cfg.cbpkt_header.type = cbPKTTYPE_CHANSET;
    const auto cfg_start = std::chrono::steady_clock::now();
    for (int i = 0; i < kBurst; ++i) ASSERT_TRUE(session->sendPacket(cfg).isOk());
    const auto cfg_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cfg_start).count();

    // Config burst is meaningfully paced; data burst is effectively free.
    EXPECT_GE(cfg_ms, 4) << "configuration sends were not throttled";
    EXPECT_LT(data_ms, 4) << "data sends should not be throttled";
    EXPECT_GT(cfg_ms, data_ms) << "config sends should be slower than data sends";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Legacy-protocol outbound translation
//
// Regression tests for the backward-compat bug where high-level config helpers
// (setSystemRunLevel, setChannelConfig, sync, …) reached legacy firmware in the
// CURRENT wire format. Those helpers are delegated to the wrapped DeviceSession
// and call sendPacket() internally; the fix routes that send through the
// wrapped session's protocol-aware sendPacket(). These tests drive a *delegated*
// helper (setSystemRunLevel) — not sendPacket() directly — and assert the bytes
// on the wire use the legacy header layout. Before the fix they arrived as
// current-format packets and these assertions fail.
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DeviceSessionTest, LegacyConfigHelper_TranslatesOutbound_311) {
    // Client sends to device_address:send_port — bind our fake device there.
    const uint16_t send_port = 51041;
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51042, send_port);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_311);
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    FakeDeviceSocket device(send_port);
    ASSERT_TRUE(device.ok()) << "Failed to bind fake device socket";

    // Delegated config helper — NOT sendPacket() directly.
    ASSERT_TRUE(session->setSystemRunLevel(cbRUNLEVEL_RUNNING, 0, 0).isOk());

    uint8_t buf[cbPKT_MAX_SIZE] = {};
    const int n = device.recvOne(buf, sizeof(buf));
    ASSERT_GT(n, 0) << "No datagram received from config helper";

    // 3.11 header (8 bytes): time(0-3) chid(4-5) type(6) dlen(7).
    // In CURRENT format byte[6] is part of the 64-bit timestamp (==0 here), so
    // a non-zero SYSSETRUNLEV type at offset 6 proves 3.11 translation happened.
    EXPECT_EQ(buf[6], static_cast<uint8_t>(cbPKTTYPE_SYSSETRUNLEV));
    EXPECT_EQ(buf[7], static_cast<uint8_t>(cbPKTDLEN_SYSINFO));
    EXPECT_EQ(readU16LE(&buf[4]), static_cast<uint16_t>(cbPKTCHAN_CONFIGURATION));
    EXPECT_EQ(static_cast<size_t>(n), kHeaderSize311 + cbPKTDLEN_SYSINFO * 4);
}

TEST_F(DeviceSessionTest, LegacyConfigHelper_TranslatesOutbound_400) {
    const uint16_t send_port = 51043;
    auto config = ConnectionParams::custom("127.0.0.1", "0.0.0.0", 51044, send_port);
    auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_400);
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    FakeDeviceSocket device(send_port);
    ASSERT_TRUE(device.ok()) << "Failed to bind fake device socket";

    ASSERT_TRUE(session->setSystemRunLevel(cbRUNLEVEL_RUNNING, 0, 0).isOk());

    uint8_t buf[cbPKT_MAX_SIZE] = {};
    const int n = device.recvOne(buf, sizeof(buf));
    ASSERT_GT(n, 0) << "No datagram received from config helper";

    // 4.0 header (16 bytes): time(0-7) chid(8-9) type(8-bit @10) dlen(16-bit @11-12)
    //                        instrument(13) reserved(14-15).
    // The discriminator vs CURRENT is the dlen position: CURRENT puts a 16-bit
    // type at 10-11 (so byte[11]==0) and dlen at 12-13. Reading a non-zero
    // SYSINFO dlen at offset 11 proves the 4.0 layout — i.e. the helper's send
    // was down-translated to 4.0.
    EXPECT_EQ(buf[10], static_cast<uint8_t>(cbPKTTYPE_SYSSETRUNLEV));
    EXPECT_EQ(readU16LE(&buf[11]), static_cast<uint16_t>(cbPKTDLEN_SYSINFO));
    EXPECT_EQ(readU16LE(&buf[8]), static_cast<uint16_t>(cbPKTCHAN_CONFIGURATION));
    EXPECT_EQ(static_cast<size_t>(n), kHeaderSize400 + cbPKTDLEN_SYSINFO * 4);
}
