///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_shmem_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Unit tests for ShmemSession class
///
/// Tests shared memory session management with focus on THE KEY FIX: mode-independent indexing
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbshmem/shmem_session.h>
#include <cbshmem/upstream_protocol.h>  // For packet types and cbNSP1-4 constants
#include <cbproto/instrument_id.h>
#include <cstring>

using namespace cbshmem;
using namespace cbproto;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for ShmemSession tests
///
class ShmemSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique names for each test to avoid conflicts
        test_name = "test_shmem_" + std::to_string(test_counter++);
    }

    void TearDown() override {
        // Sessions are automatically closed in destructor
    }

    std::string test_name;
    static int test_counter;
};

int ShmemSessionTest::test_counter = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Basic Lifecycle Tests
/// @{

TEST_F(ShmemSessionTest, CreateStandalone) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk()) << "Failed to create standalone session: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
    EXPECT_EQ(session.getMode(), Mode::STANDALONE);
}

TEST_F(ShmemSessionTest, CreateAndDestroy) {
    {
        auto result = ShmemSession::create(test_name, Mode::STANDALONE);
        ASSERT_TRUE(result.isOk());
        EXPECT_TRUE(result.value().isOpen());
    }
    // Session should be closed after scope exit
}

TEST_F(ShmemSessionTest, MoveConstruction) {
    auto result1 = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result1.isOk());

    // Move construction
    ShmemSession session2(std::move(result1.value()));
    EXPECT_TRUE(session2.isOpen());
}

TEST_F(ShmemSessionTest, MoveAssignment) {
    auto result1 = ShmemSession::create(test_name + "_1", Mode::STANDALONE);
    auto result2 = ShmemSession::create(test_name + "_2", Mode::STANDALONE);
    ASSERT_TRUE(result1.isOk());
    ASSERT_TRUE(result2.isOk());

    // Move assignment
    result1.value() = std::move(result2.value());
    EXPECT_TRUE(result1.value().isOpen());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Instrument Status Tests
/// @{

TEST_F(ShmemSessionTest, InstrumentStatusInitiallyInactive) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // All instruments should be inactive initially
    for (uint8_t i = 1; i <= cbMAXOPEN; ++i) {
        auto id = InstrumentId::fromOneBased(i);
        auto active_result = session.isInstrumentActive(id);
        ASSERT_TRUE(active_result.isOk());
        EXPECT_FALSE(active_result.value()) << "Instrument " << (int)i << " should be inactive";
    }
}

TEST_F(ShmemSessionTest, SetInstrumentActive) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Activate first instrument
    auto id1 = InstrumentId::fromOneBased(cbNSP1);
    auto set_result = session.setInstrumentActive(id1, true);
    ASSERT_TRUE(set_result.isOk());

    // Verify it's active
    auto active_result = session.isInstrumentActive(id1);
    ASSERT_TRUE(active_result.isOk());
    EXPECT_TRUE(active_result.value());

    // Other instruments should still be inactive
    auto id2 = InstrumentId::fromOneBased(cbNSP2);
    auto active_result2 = session.isInstrumentActive(id2);
    ASSERT_TRUE(active_result2.isOk());
    EXPECT_FALSE(active_result2.value());
}

TEST_F(ShmemSessionTest, GetFirstActiveInstrument) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // No active instruments initially
    auto first_result = session.getFirstActiveInstrument();
    EXPECT_TRUE(first_result.isError());
    EXPECT_NE(first_result.error().find("No active"), std::string::npos);

    // Activate third instrument
    auto id3 = InstrumentId::fromOneBased(cbNSP3);
    ASSERT_TRUE(session.setInstrumentActive(id3, true).isOk());

    // Should return third instrument (index 2, 1-based = 3)
    first_result = session.getFirstActiveInstrument();
    ASSERT_TRUE(first_result.isOk());
    EXPECT_EQ(first_result.value().toOneBased(), cbNSP3);
}

TEST_F(ShmemSessionTest, MultipleActiveInstruments) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Activate instruments 1 and 3
    ASSERT_TRUE(session.setInstrumentActive(InstrumentId::fromOneBased(cbNSP1), true).isOk());
    ASSERT_TRUE(session.setInstrumentActive(InstrumentId::fromOneBased(cbNSP3), true).isOk());

    // First active should be cbNSP1
    auto first_result = session.getFirstActiveInstrument();
    ASSERT_TRUE(first_result.isOk());
    EXPECT_EQ(first_result.value().toOneBased(), cbNSP1);

    // Verify both are active
    EXPECT_TRUE(session.isInstrumentActive(InstrumentId::fromOneBased(cbNSP1)).value());
    EXPECT_TRUE(session.isInstrumentActive(InstrumentId::fromOneBased(cbNSP3)).value());
    EXPECT_FALSE(session.isInstrumentActive(InstrumentId::fromOneBased(cbNSP2)).value());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Routing Tests (THE KEY FIX!)
/// @{

TEST_F(ShmemSessionTest, StorePacket_PROCINFO_Instrument0) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create PROCINFO packet for instrument 0 (cbNSP1 = 1)
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 0;  // 0-based in packet (represents cbNSP1)
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_PROCINFO;

    cbPKT_PROCINFO* proc_pkt = reinterpret_cast<cbPKT_PROCINFO*>(&pkt);
    proc_pkt->proc = 1;
    proc_pkt->chancount = 256;

    // Store packet
    auto store_result = session.storePacket(pkt);
    ASSERT_TRUE(store_result.isOk()) << "Failed to store packet: " << store_result.error();

    // THE KEY FIX: Should be stored at index 0 (packet.instrument)
    auto id = InstrumentId::fromPacketField(0);
    auto get_result = session.getProcInfo(id);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().chancount, 256);

    // Instrument should be marked active
    auto active_result = session.isInstrumentActive(id);
    ASSERT_TRUE(active_result.isOk());
    EXPECT_TRUE(active_result.value());
}

TEST_F(ShmemSessionTest, StorePacket_PROCINFO_Instrument2) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create PROCINFO packet for instrument 2 (cbNSP3 = 3)
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 2;  // 0-based in packet (represents cbNSP3)
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_PROCINFO;

    cbPKT_PROCINFO* proc_pkt = reinterpret_cast<cbPKT_PROCINFO*>(&pkt);
    proc_pkt->proc = 3;
    proc_pkt->chancount = 128;

    // Store packet
    auto store_result = session.storePacket(pkt);
    ASSERT_TRUE(store_result.isOk());

    // THE KEY FIX: Should be stored at index 2 (packet.instrument), NOT index 0!
    auto id = InstrumentId::fromPacketField(2);
    auto get_result = session.getProcInfo(id);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 3);
    EXPECT_EQ(get_result.value().chancount, 128);

    // Verify it's NOT stored at index 0
    auto id0 = InstrumentId::fromPacketField(0);
    auto get_result0 = session.getProcInfo(id0);
    ASSERT_TRUE(get_result0.isOk());
    EXPECT_NE(get_result0.value().proc, 3);  // Should not have this data
}

TEST_F(ShmemSessionTest, StorePacket_MultipleInstruments) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Store PROCINFO for instruments 0, 1, and 3
    for (uint8_t inst : {0, 1, 3}) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.instrument = inst;
        pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;
        pkt.cbpkt_header.dlen = cbPKTDLEN_PROCINFO;

        cbPKT_PROCINFO* proc_pkt = reinterpret_cast<cbPKT_PROCINFO*>(&pkt);
        proc_pkt->proc = inst + 1;  // Unique value for verification
        proc_pkt->chancount = 100 + inst;

        ASSERT_TRUE(session.storePacket(pkt).isOk());
    }

    // Verify each instrument has correct data at correct index
    for (uint8_t inst : {0, 1, 3}) {
        auto id = InstrumentId::fromPacketField(inst);
        auto get_result = session.getProcInfo(id);
        ASSERT_TRUE(get_result.isOk());
        EXPECT_EQ(get_result.value().proc, inst + 1);
        EXPECT_EQ(get_result.value().chancount, 100 + inst);
    }
}

TEST_F(ShmemSessionTest, StorePacket_BANKINFO) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create BANKINFO packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 1;  // cbNSP2
    pkt.cbpkt_header.type = cbPKTTYPE_BANKREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_BANKINFO;

    cbPKT_BANKINFO* bank_pkt = reinterpret_cast<cbPKT_BANKINFO*>(&pkt);
    bank_pkt->proc = 2;
    bank_pkt->bank = 3;
    bank_pkt->chancount = 32;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Retrieve and verify
    auto id = InstrumentId::fromPacketField(1);
    auto get_result = session.getBankInfo(id, 3);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 2);
    EXPECT_EQ(get_result.value().bank, 3);
    EXPECT_EQ(get_result.value().chancount, 32);
}

TEST_F(ShmemSessionTest, StorePacket_FILTINFO) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create FILTINFO packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 0;  // cbNSP1
    pkt.cbpkt_header.type = cbPKTTYPE_FILTREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_FILTINFO;

    cbPKT_FILTINFO* filt_pkt = reinterpret_cast<cbPKT_FILTINFO*>(&pkt);
    filt_pkt->proc = 1;
    filt_pkt->filt = 5;
    filt_pkt->hpfreq = 250000;  // 250 Hz in millihertz

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Retrieve and verify
    auto id = InstrumentId::fromPacketField(0);
    auto get_result = session.getFilterInfo(id, 5);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().filt, 5);
    EXPECT_EQ(get_result.value().hpfreq, 250000);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Configuration Read/Write Tests
/// @{

TEST_F(ShmemSessionTest, GetProcInfo_NotFound) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Try to get PROCINFO before storing anything
    auto id = InstrumentId::fromOneBased(cbNSP1);
    auto get_result = session.getProcInfo(id);
    // Should succeed but return zeroed data
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 0);
}

TEST_F(ShmemSessionTest, SetAndGetProcInfo) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create and set PROCINFO
    cbPKT_PROCINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 2;
    info.chancount = 512;
    info.bankcount = 24;

    auto id = InstrumentId::fromOneBased(cbNSP2);
    auto set_result = session.setProcInfo(id, info);
    ASSERT_TRUE(set_result.isOk());

    // Retrieve and verify
    auto get_result = session.getProcInfo(id);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 2);
    EXPECT_EQ(get_result.value().chancount, 512);
    EXPECT_EQ(get_result.value().bankcount, 24);
}

TEST_F(ShmemSessionTest, InvalidInstrumentId) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Try to use invalid instrument ID (0 is invalid, 1-4 are valid)
    auto invalid_id = InstrumentId::fromOneBased(0);
    EXPECT_FALSE(invalid_id.isValid());

    auto get_result = session.getProcInfo(invalid_id);
    EXPECT_TRUE(get_result.isError());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Error Handling Tests
/// @{

TEST_F(ShmemSessionTest, OperationsOnClosedSession) {
    // Create a session in a scope
    std::string name = test_name;
    {
        auto result = ShmemSession::create(name, Mode::STANDALONE);
        ASSERT_TRUE(result.isOk());
    }
    // Session is now closed

    // Try to create a new session with same name should work
    auto result2 = ShmemSession::create(name, Mode::STANDALONE);
    ASSERT_TRUE(result2.isOk());
}

TEST_F(ShmemSessionTest, StorePacket_InvalidInstrument) {
    auto result = ShmemSession::create(test_name, Mode::STANDALONE);
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create packet with invalid instrument ID
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 255;  // Way out of range
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;

    auto store_result = session.storePacket(pkt);
    EXPECT_TRUE(store_result.isError());
    EXPECT_NE(store_result.error().find("Invalid"), std::string::npos);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Run all tests
///
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
