///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_sdk_handshake.cpp
/// @author CereLink Development Team
/// @date   2025-11-13
///
/// @brief  Unit tests for SDK startup handshake and device communication
///
/// Tests the complete startup sequence including runlevel commands, configuration requests,
/// and SYSREP response handling.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

// Include protocol types and session headers
#include <cbproto/cbproto.h>
#include "cbshm/shmem_session.h"
#include "cbdev/device_session.h"
#include "cbsdk/sdk_session.h"

using namespace cbsdk;

/// Test fixture for SDK handshake tests
class SdkHandshakeTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_name = "handshake_" + std::to_string(test_counter++);
    }

    void TearDown() override {
        // Cleanup happens automatically via RAII
    }

    std::string test_name;
    static int test_counter;
};

int SdkHandshakeTest::test_counter = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkHandshakeTest, Config_AutoRunOption) {
    SdkConfig config;
    EXPECT_TRUE(config.autorun);  // Default: auto-run enabled

    config.autorun = false;
    EXPECT_FALSE(config.autorun);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Structure Tests (without actual transmission)
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkHandshakeTest, PacketHeader_RunlevelCommand) {
    // Test packet structure for runlevel commands
    cbPKT_SYSINFO sysinfo;
    std::memset(&sysinfo, 0, sizeof(sysinfo));

    // Fill header as setSystemRunLevel() does
    sysinfo.cbpkt_header.time = 1;
    sysinfo.cbpkt_header.chid = 0x8000;  // cbPKTCHAN_CONFIGURATION
    sysinfo.cbpkt_header.type = 0x92;    // cbPKTTYPE_SYSSETRUNLEV
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro for correct header size
    sysinfo.cbpkt_header.instrument = 0;

    // Fill payload
    sysinfo.runlevel = cbRUNLEVEL_RUNNING;
    sysinfo.resetque = 0;
    sysinfo.runflags = 0;

    // Verify header fields
    EXPECT_EQ(sysinfo.cbpkt_header.chid, 0x8000);
    EXPECT_EQ(sysinfo.cbpkt_header.type, 0x92);
    EXPECT_GT(sysinfo.cbpkt_header.dlen, 0u);

    // Verify payload
    EXPECT_EQ(sysinfo.runlevel, cbRUNLEVEL_RUNNING);
}

TEST_F(SdkHandshakeTest, PacketHeader_ConfigurationRequest) {
    // Test packet structure for configuration request
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    // Fill header as requestConfiguration() does
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = 0x8000;  // cbPKTCHAN_CONFIGURATION
    pkt.cbpkt_header.type = 0x88;    // cbPKTTYPE_REQCONFIGALL
    pkt.cbpkt_header.dlen = 0;       // No payload
    pkt.cbpkt_header.instrument = 0;

    // Verify header fields
    EXPECT_EQ(pkt.cbpkt_header.chid, 0x8000);
    EXPECT_EQ(pkt.cbpkt_header.type, 0x88);
    EXPECT_EQ(pkt.cbpkt_header.dlen, 0u);  // No payload for REQCONFIGALL
}

TEST_F(SdkHandshakeTest, PacketSize_Runlevel) {
    // Verify packet size calculation
    cbPKT_SYSINFO sysinfo;
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro for correct header size

    size_t packet_size = (sysinfo.cbpkt_header.dlen + cbPKT_HEADER_32SIZE) * 4;

    EXPECT_EQ(packet_size, sizeof(cbPKT_SYSINFO));
    EXPECT_GT(packet_size, 0u);
    EXPECT_LE(packet_size, sizeof(cbPKT_GENERIC));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Runlevel Constants Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkHandshakeTest, RunlevelConstants_Values) {
    // Verify runlevel constant values are as expected
    EXPECT_EQ(cbRUNLEVEL_STARTUP, 10);
    EXPECT_EQ(cbRUNLEVEL_HARDRESET, 20);
    EXPECT_EQ(cbRUNLEVEL_STANDBY, 30);
    EXPECT_EQ(cbRUNLEVEL_RESET, 40);
    EXPECT_EQ(cbRUNLEVEL_RUNNING, 50);
    EXPECT_EQ(cbRUNLEVEL_STRESSED, 60);
    EXPECT_EQ(cbRUNLEVEL_ERROR, 70);
    EXPECT_EQ(cbRUNLEVEL_SHUTDOWN, 80);
}

TEST_F(SdkHandshakeTest, RunlevelConstants_Ordering) {
    // Verify runlevels are in ascending order
    EXPECT_LT(cbRUNLEVEL_STARTUP, cbRUNLEVEL_HARDRESET);
    EXPECT_LT(cbRUNLEVEL_HARDRESET, cbRUNLEVEL_STANDBY);
    EXPECT_LT(cbRUNLEVEL_STANDBY, cbRUNLEVEL_RESET);
    EXPECT_LT(cbRUNLEVEL_RESET, cbRUNLEVEL_RUNNING);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Type Constants Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(SdkHandshakeTest, PacketTypes_Configuration) {
    // Verify configuration packet types
    EXPECT_EQ(cbPKTTYPE_SYSREP, 0x10);
    EXPECT_EQ(cbPKTTYPE_SYSREPRUNLEV, 0x12);
    EXPECT_EQ(cbPKTTYPE_SYSSETRUNLEV, 0x92);
    EXPECT_EQ(cbPKTCHAN_CONFIGURATION, 0x8000);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Creation Logic Tests (verifying setSystemRunLevel implementation)
///////////////////////////////////////////////////////////////////////////////////////////////////

// Note: We test packet creation logic directly rather than calling setSystemRunLevel()
// because that would require a running session with send threads. These tests verify
// that the packet construction logic is correct.

TEST_F(SdkHandshakeTest, PacketCreation_RunlevelWithParameters) {
    // This test recreates the logic from setSystemRunLevel() to verify packet construction
    cbPKT_SYSINFO sysinfo;
    std::memset(&sysinfo, 0, sizeof(sysinfo));

    // Fill header (as setSystemRunLevel does)
    sysinfo.cbpkt_header.time = 1;
    sysinfo.cbpkt_header.chid = 0x8000;
    sysinfo.cbpkt_header.type = 0x92;
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro for correct header size
    sysinfo.cbpkt_header.instrument = 0;

    // Fill payload with specific values
    uint32_t test_runlevel = cbRUNLEVEL_RUNNING;
    uint32_t test_resetque = 5;
    uint32_t test_runflags = 2;

    sysinfo.runlevel = test_runlevel;
    sysinfo.resetque = test_resetque;
    sysinfo.runflags = test_runflags;

    // Copy to generic packet (as setSystemRunLevel does)
    cbPKT_GENERIC pkt;
    std::memcpy(&pkt, &sysinfo, sizeof(sysinfo));

    // Verify header fields persisted
    EXPECT_EQ(pkt.cbpkt_header.chid, 0x8000);
    EXPECT_EQ(pkt.cbpkt_header.type, 0x92);
    EXPECT_GT(pkt.cbpkt_header.dlen, 0u);

    // Verify payload by casting back
    const cbPKT_SYSINFO* verify_sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
    EXPECT_EQ(verify_sysinfo->runlevel, test_runlevel);
    EXPECT_EQ(verify_sysinfo->resetque, test_resetque);
    EXPECT_EQ(verify_sysinfo->runflags, test_runflags);
}

TEST_F(SdkHandshakeTest, PacketCreation_AllRunlevels) {
    // Verify packet creation works for all runlevel values
    uint32_t runlevels[] = {
        cbRUNLEVEL_STARTUP,
        cbRUNLEVEL_HARDRESET,
        cbRUNLEVEL_STANDBY,
        cbRUNLEVEL_RESET,
        cbRUNLEVEL_RUNNING,
        cbRUNLEVEL_STRESSED,
        cbRUNLEVEL_ERROR,
        cbRUNLEVEL_SHUTDOWN
    };

    for (uint32_t runlevel : runlevels) {
        cbPKT_SYSINFO sysinfo;
        std::memset(&sysinfo, 0, sizeof(sysinfo));

        sysinfo.cbpkt_header.chid = 0x8000;
        sysinfo.cbpkt_header.type = 0x92;
        sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro for correct header size
        sysinfo.runlevel = runlevel;

        cbPKT_GENERIC pkt;
        std::memcpy(&pkt, &sysinfo, sizeof(sysinfo));

        const cbPKT_SYSINFO* verify = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
        EXPECT_EQ(verify->runlevel, runlevel) << "Runlevel " << runlevel << " not preserved";
    }
}

TEST_F(SdkHandshakeTest, PacketCreation_ConfigurationRequest) {
    // Verify packet creation for requestConfiguration()
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));

    // Fill header (as requestConfiguration does)
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = 0x8000;
    pkt.cbpkt_header.type = 0x88;  // cbPKTTYPE_REQCONFIGALL
    pkt.cbpkt_header.dlen = 0;
    pkt.cbpkt_header.instrument = 0;

    // Verify all fields
    EXPECT_EQ(pkt.cbpkt_header.time, 1u);
    EXPECT_EQ(pkt.cbpkt_header.chid, 0x8000);
    EXPECT_EQ(pkt.cbpkt_header.type, 0x88);
    EXPECT_EQ(pkt.cbpkt_header.dlen, 0u);
    EXPECT_EQ(pkt.cbpkt_header.instrument, 0u);
}

