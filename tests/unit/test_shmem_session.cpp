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
#include <cbshm/shmem_session.h>
#include <cbproto/cbproto.h>       // For packet types and cbNSP1-4 constants
#include <cbproto/instrument_id.h>
#include <cbproto/connection.h>    // For cbproto_protocol_version_t
#include <cbproto/packet_translator.h>
#include <cstring>
#include <cstdint>
#include <vector>
#ifdef _WIN32
#include <windows.h>  // GetCurrentProcessId()
#else
#include <unistd.h>   // getpid()
#endif

using namespace cbshm;
using namespace cbproto;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for ShmemSession tests
///
class ShmemSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use unique names for each test to avoid conflicts
        // s = shmem
        test_name = "s" + std::to_string(test_counter++);
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
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk()) << "Failed to create standalone session: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
    EXPECT_EQ(session.getMode(), Mode::STANDALONE);
}

TEST_F(ShmemSessionTest, CreateAndDestroy) {
    {
        auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
        ASSERT_TRUE(result.isOk());
        EXPECT_TRUE(result.value().isOpen());
    }
    // Session should be closed after scope exit
}

TEST_F(ShmemSessionTest, MoveConstruction) {
    auto result1 = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result1.isOk());

    // Move construction
    ShmemSession session2(std::move(result1.value()));
    EXPECT_TRUE(session2.isOpen());
}

TEST_F(ShmemSessionTest, MoveAssignment) {
    auto result1 = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name + "_1", cbproto::InstrumentId::fromOneBased(cbNSP1));
    auto result2 = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name + "_2", cbproto::InstrumentId::fromOneBased(cbNSP1));
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
    // All instruments should be inactive initially
    for (uint8_t i = 1; i <= cbMAXOPEN; ++i) {
        auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, InstrumentId::fromOneBased(i));
        ASSERT_TRUE(result.isOk());
        auto& session = result.value();
        auto active_result = session.isInstrumentActive();
        ASSERT_TRUE(active_result.isOk());
        EXPECT_FALSE(active_result.value()) << "Instrument " << (int)i << " should be inactive";
    }
}

TEST_F(ShmemSessionTest, SetInstrumentActive) {
    // A ShmemSession represents a single instrument; setInstrumentActive()/
    // isInstrumentActive() operate on that one instrument's status. Verify the
    // full active/inactive round-trip (InstrumentToggle only covers activation).
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Starts inactive.
    auto initial = session.isInstrumentActive();
    ASSERT_TRUE(initial.isOk());
    EXPECT_FALSE(initial.value());

    // Activating is reflected by isInstrumentActive().
    ASSERT_TRUE(session.setInstrumentActive(true).isOk());
    auto activated = session.isInstrumentActive();
    ASSERT_TRUE(activated.isOk());
    EXPECT_TRUE(activated.value());

    // Deactivating toggles it back off.
    ASSERT_TRUE(session.setInstrumentActive(false).isOk());
    auto deactivated = session.isInstrumentActive();
    ASSERT_TRUE(deactivated.isOk());
    EXPECT_FALSE(deactivated.value());
}

// TODO: getFirstActiveInstrument() and multi-instrument-per-session tracking were
// removed with the move to per-instrument ShmemSession instances. A ShmemSession
// now represents a single instrument, so "first active instrument" discovery and
// activating multiple instruments through one session no longer apply. These tests
// should be reworked at the layer that now enumerates instruments across sessions.
//
// TEST_F(ShmemSessionTest, GetFirstActiveInstrument) { ... }
// TEST_F(ShmemSessionTest, MultipleActiveInstruments) { ... }

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Routing Tests (THE KEY FIX!)
/// @{

TEST_F(ShmemSessionTest, SetGetProcInfo_Instrument0) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Set PROCINFO for this session's instrument (cbNSP1)
    cbPKT_PROCINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.chancount = 256;

    ASSERT_TRUE(session.setProcInfo(info).isOk());
    ASSERT_TRUE(session.setInstrumentActive(true).isOk());

    auto get_result = session.getProcInfo();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().chancount, 256);

    // Instrument should be marked active
    auto active_result = session.isInstrumentActive();
    ASSERT_TRUE(active_result.isOk());
    EXPECT_TRUE(active_result.value());
}

// TODO: These tested the old single-session, multi-instrument indexing model
// ("THE KEY FIX" of routing by packet.instrument into procinfo[index]). With
// per-instrument ShmemSession instances a session owns exactly one instrument's
// config slot, so cross-instrument isolation within one session no longer exists.
// Equivalent coverage now lives in SetGetProcInfo_Instrument0 (per-instrument
// round-trip) and the CENTRAL per-instrument tests.
//
// TEST_F(ShmemSessionTest, SetGetProcInfo_Instrument2) { ... }
// TEST_F(ShmemSessionTest, SetGetProcInfo_MultipleInstruments) { ... }

TEST_F(ShmemSessionTest, SetGetBankInfo) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Set BANKINFO for bank 3
    cbPKT_BANKINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 2;
    info.bank = 3;
    info.chancount = 32;

    ASSERT_TRUE(session.setBankInfo(3, info).isOk());

    // Retrieve and verify
    auto get_result = session.getBankInfo(3);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 2);
    EXPECT_EQ(get_result.value().bank, 3);
    EXPECT_EQ(get_result.value().chancount, 32);
}

TEST_F(ShmemSessionTest, SetGetFilterInfo) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Set FILTINFO for filter 5
    cbPKT_FILTINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.filt = 5;
    info.hpfreq = 250000;  // 250 Hz in millihertz

    ASSERT_TRUE(session.setFilterInfo(5, info).isOk());

    // Retrieve and verify
    auto get_result = session.getFilterInfo(5);
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
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Try to get PROCINFO before storing anything
    auto get_result = session.getProcInfo();
    // Should succeed but return zeroed data
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 0);
}

TEST_F(ShmemSessionTest, SetAndGetProcInfo) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create and set PROCINFO
    cbPKT_PROCINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 2;
    info.chancount = 512;
    info.bankcount = 24;

    auto set_result = session.setProcInfo(info);
    ASSERT_TRUE(set_result.isOk());

    // Retrieve and verify
    auto get_result = session.getProcInfo();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 2);
    EXPECT_EQ(get_result.value().chancount, 512);
    EXPECT_EQ(get_result.value().bankcount, 24);
}

TEST_F(ShmemSessionTest, InvalidInstrumentId) {
    // The instrument ID is now fixed at session creation, so an invalid ID is
    // rejected by create() rather than by individual accessors.
    auto invalid_id = InstrumentId::fromOneBased(0);
    EXPECT_FALSE(invalid_id.isValid());

    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, invalid_id);
    EXPECT_TRUE(result.isError());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Error Handling Tests
/// @{

TEST_F(ShmemSessionTest, OperationsOnClosedSession) {
    // Create a session in a scope
    std::string name = test_name;
    {
        auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, name, cbproto::InstrumentId::fromOneBased(cbNSP1));
        ASSERT_TRUE(result.isOk());
    }
    // Session is now closed

    // Try to create a new session with same name should work
    auto result2 = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result2.isOk());
}

TEST_F(ShmemSessionTest, StorePacket_InvalidInstrument) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create config packet with invalid instrument ID
    // This should succeed (packet written to receive buffer) but skip config buffer update
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.instrument = 255;  // Way out of range
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;

    auto store_result = session.storePacket(pkt);
    EXPECT_TRUE(store_result.isOk()) << "Packet should be stored to receive buffer even with invalid instrument ID";

    // Create non-config packet with invalid instrument ID - should also succeed
    cbPKT_GENERIC lnc_pkt;
    std::memset(&lnc_pkt, 0, sizeof(lnc_pkt));
    lnc_pkt.cbpkt_header.instrument = 83;  // Invalid ID
    lnc_pkt.cbpkt_header.type = 0x28;      // cbPKTTYPE_LNCREP

    auto lnc_result = session.storePacket(lnc_pkt);
    EXPECT_TRUE(lnc_result.isOk()) << "Non-config packet should succeed regardless of instrument ID";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Advanced Packet Handler Tests
/// @{

TEST_F(ShmemSessionTest, StorePacket_ADAPTFILTINFO) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create ADAPTFILTINFO packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 1;  // cbNSP2
    pkt.cbpkt_header.type = cbPKTTYPE_ADAPTFILTREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_ADAPTFILTINFO;

    cbPKT_ADAPTFILTINFO* adapt_pkt = reinterpret_cast<cbPKT_ADAPTFILTINFO*>(&pkt);
    adapt_pkt->chan = 10;
    adapt_pkt->nMode = 1;  // Filter continuous & spikes
    adapt_pkt->dLearningRate = 0.05f;
    adapt_pkt->nRefChan1 = 5;
    adapt_pkt->nRefChan2 = 6;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // TODO: Add getter method for adaptinfo and verify
    // For now, just verify packet was stored without error
}

TEST_F(ShmemSessionTest, StorePacket_REFELECFILTINFO) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create REFELECFILTINFO packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;  // cbNSP1
    pkt.cbpkt_header.type = cbPKTTYPE_REFELECFILTREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_REFELECFILTINFO;

    cbPKT_REFELECFILTINFO* refelec_pkt = reinterpret_cast<cbPKT_REFELECFILTINFO*>(&pkt);
    refelec_pkt->chan = 15;
    refelec_pkt->nMode = 2;  // Filter spikes only
    refelec_pkt->nRefChan = 8;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_SS_STATUS) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_STATUS packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_STATUSREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_STATUS;

    cbPKT_SS_STATUS* status_pkt = reinterpret_cast<cbPKT_SS_STATUS*>(&pkt);
    status_pkt->cntlUnitStats.nMode = ADAPT_ALWAYS;  // Always adapt unit stats
    status_pkt->cntlNumUnits.nMode = ADAPT_ALWAYS;   // Always adapt unit numbers

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_SS_DETECT) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_DETECT packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_DETECTREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_DETECT;

    cbPKT_SS_DETECT* detect_pkt = reinterpret_cast<cbPKT_SS_DETECT*>(&pkt);
    detect_pkt->fThreshold = -50.0f;  // Detection threshold
    detect_pkt->fMultiplier = 4.5f;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_SS_ARTIF_REJECT) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_ARTIF_REJECT packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_ARTIF_REJECTREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_ARTIF_REJECT;

    cbPKT_SS_ARTIF_REJECT* artif_pkt = reinterpret_cast<cbPKT_SS_ARTIF_REJECT*>(&pkt);
    artif_pkt->nMaxSimulChans = 3;  // Max simultaneous channels
    artif_pkt->nRefractoryCount = 10;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_SS_NOISE_BOUNDARY) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_NOISE_BOUNDARY packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_NOISE_BOUNDARY;

    cbPKT_SS_NOISE_BOUNDARY* noise_pkt = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY*>(&pkt);
    noise_pkt->chan = 25;  // 1-based channel ID
    noise_pkt->afc[0] = -100.0f;  // Center of ellipsoid (x coordinate)
    noise_pkt->afc[1] = 0.0f;     // Center of ellipsoid (y coordinate)
    noise_pkt->afc[2] = 0.0f;     // Center of ellipsoid (z coordinate)

    // Store packet - should be stored at index chan-1 (24)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test boundary condition - channel 0 should be rejected
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYREP;
    cbPKT_SS_NOISE_BOUNDARY* noise_pkt_invalid = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY*>(&pkt_invalid);
    noise_pkt_invalid->chan = 0;  // Invalid

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Should succeed but not store to config
}

TEST_F(ShmemSessionTest, StorePacket_SS_STATISTICS) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_STATISTICS packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_STATISTICS;

    cbPKT_SS_STATISTICS* stats_pkt = reinterpret_cast<cbPKT_SS_STATISTICS*>(&pkt);
    stats_pkt->nUpdateSpikes = 1000;
    stats_pkt->nAutoalg = cbAUTOALG_PCA;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_SS_MODEL) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create SS_MODELREP packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_SS_MODELREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SS_MODELSET;

    cbPKT_SS_MODELSET* model_pkt = reinterpret_cast<cbPKT_SS_MODELSET*>(&pkt);
    model_pkt->chan = 10;  // 0-based channel
    model_pkt->unit_number = 1;  // Unit 1
    model_pkt->valid = 1;
    model_pkt->inverted = 0;
    model_pkt->num_samples = 100;
    model_pkt->mu_x[0] = 50.0f;
    model_pkt->mu_x[1] = 75.0f;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test boundary conditions
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_SS_MODELREP;
    cbPKT_SS_MODELSET* model_invalid = reinterpret_cast<cbPKT_SS_MODELSET*>(&pkt_invalid);
    model_invalid->chan = 9999;  // Out of range
    model_invalid->unit_number = 0;

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Should succeed but not store to config
}

TEST_F(ShmemSessionTest, StorePacket_FS_BASIS) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create FS_BASISREP packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_FS_BASISREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_FS_BASIS;

    cbPKT_FS_BASIS* basis_pkt = reinterpret_cast<cbPKT_FS_BASIS*>(&pkt);
    basis_pkt->chan = 20;  // 1-based channel
    basis_pkt->mode = 1;   // PCA basis
    basis_pkt->fs = cbAUTOALG_PCA;

    // Store packet - should be stored at index chan-1 (19)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test invalid channel
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_FS_BASISREP;
    cbPKT_FS_BASIS* basis_invalid = reinterpret_cast<cbPKT_FS_BASIS*>(&pkt_invalid);
    basis_invalid->chan = 0;  // Invalid (1-based, so 0 is invalid)

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Should succeed but not store to config
}

TEST_F(ShmemSessionTest, StorePacket_LNC) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create LNCREP packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 1;  // cbNSP2
    pkt.cbpkt_header.type = cbPKTTYPE_LNCREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_LNC;

    cbPKT_LNC* lnc_pkt = reinterpret_cast<cbPKT_LNC*>(&pkt);
    lnc_pkt->lncFreq = 60;  // 60 Hz line noise
    lnc_pkt->lncRefChan = 10;
    lnc_pkt->lncGlobalMode = 1;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_FILECFG) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create REPFILECFG packet with REC option
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_REPFILECFG;
    pkt.cbpkt_header.dlen = cbPKTDLEN_FILECFG;

    cbPKT_FILECFG* file_pkt = reinterpret_cast<cbPKT_FILECFG*>(&pkt);
    file_pkt->options = cbFILECFG_OPT_REC;  // Recording
    file_pkt->duration = 3600;  // 1 hour
    file_pkt->recording = 1;
    std::strncpy(file_pkt->filename, "test_recording.nev", cbLEN_STR_COMMENT);

    // Store packet - should be stored
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Create packet with non-REC/STOP/TIMEOUT option - should not be stored
    cbPKT_GENERIC pkt_other;
    std::memset(&pkt_other, 0, sizeof(pkt_other));
    pkt_other.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_other.cbpkt_header.instrument = 0;
    pkt_other.cbpkt_header.type = cbPKTTYPE_REPFILECFG;
    cbPKT_FILECFG* file_other = reinterpret_cast<cbPKT_FILECFG*>(&pkt_other);
    file_other->options = cbFILECFG_OPT_NONE;  // Other option

    ASSERT_TRUE(session.storePacket(pkt_other).isOk());  // Succeeds but not stored to config
}

TEST_F(ShmemSessionTest, StorePacket_NTRODEINFO) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create REPNTRODEINFO packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_REPNTRODEINFO;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NTRODEINFO;

    cbPKT_NTRODEINFO* ntrode_pkt = reinterpret_cast<cbPKT_NTRODEINFO*>(&pkt);
    ntrode_pkt->ntrode = 5;  // 1-based NTrode ID
    std::strncpy(ntrode_pkt->label, "Tetrode_1", cbLEN_STR_LABEL);
    ntrode_pkt->nSite = 4;  // Tetrode has 4 electrodes
    ntrode_pkt->fs = cbAUTOALG_PCA;

    // Store packet - should be stored at index ntrode-1 (4)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test invalid ntrode ID
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_REPNTRODEINFO;
    cbPKT_NTRODEINFO* ntrode_invalid = reinterpret_cast<cbPKT_NTRODEINFO*>(&pkt_invalid);
    ntrode_invalid->ntrode = 0;  // Invalid (1-based)

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Succeeds but not stored to config
}

TEST_F(ShmemSessionTest, StorePacket_WAVEFORM) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create WAVEFORMREP packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_WAVEFORMREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_WAVEFORM;

    cbPKT_AOUT_WAVEFORM* wave_pkt = reinterpret_cast<cbPKT_AOUT_WAVEFORM*>(&pkt);
    wave_pkt->chan = 2;  // 0-based channel
    wave_pkt->trigNum = 1;  // 0-based trigger number
    wave_pkt->mode = cbWAVEFORM_MODE_SINE;
    wave_pkt->repeats = 5;
    wave_pkt->wave.offset = 100;
    wave_pkt->wave.sineFrequency = 1000;  // 1 kHz
    wave_pkt->wave.sineAmplitude = 500;

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test invalid indices
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_WAVEFORMREP;
    cbPKT_AOUT_WAVEFORM* wave_invalid = reinterpret_cast<cbPKT_AOUT_WAVEFORM*>(&pkt_invalid);
    wave_invalid->chan = 999;  // Out of range
    wave_invalid->trigNum = 0;

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Succeeds but not stored to config
}

TEST_F(ShmemSessionTest, StorePacket_NPLAY) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create NPLAYREP packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_NPLAYREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NPLAY;

    cbPKT_NPLAY* nplay_pkt = reinterpret_cast<cbPKT_NPLAY*>(&pkt);
    nplay_pkt->mode = cbNPLAY_MODE_CONFIG;  // Request config
    nplay_pkt->flags = cbNPLAY_FLAG_CONF;
    nplay_pkt->val = 0;
    nplay_pkt->speed = 1.0;
    std::strncpy(nplay_pkt->fname, "playback_file.nev", sizeof(nplay_pkt->fname));

    // Store packet
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

TEST_F(ShmemSessionTest, StorePacket_NM_VideoSource) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create NMREP packet for video source
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_NMREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NM;

    cbPKT_NM* nm_pkt = reinterpret_cast<cbPKT_NM*>(&pkt);
    nm_pkt->mode = cbNM_MODE_SETVIDEOSOURCE;
    nm_pkt->flags = 2;  // 1-based video source ID
    std::strncpy(nm_pkt->name, "Camera_1", cbLEN_STR_LABEL);
    nm_pkt->value = 30000;  // 30 fps (in milli-fps)

    // Store packet - should be stored at index flags-1 (1)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Test invalid video source ID
    cbPKT_GENERIC pkt_invalid;
    std::memset(&pkt_invalid, 0, sizeof(pkt_invalid));
    pkt_invalid.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt_invalid.cbpkt_header.instrument = 0;
    pkt_invalid.cbpkt_header.type = cbPKTTYPE_NMREP;
    cbPKT_NM* nm_invalid = reinterpret_cast<cbPKT_NM*>(&pkt_invalid);
    nm_invalid->mode = cbNM_MODE_SETVIDEOSOURCE;
    nm_invalid->flags = 0;  // Invalid (1-based)

    ASSERT_TRUE(session.storePacket(pkt_invalid).isOk());  // Succeeds but not stored to config
}

TEST_F(ShmemSessionTest, StorePacket_NM_TrackableObject) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk());
    auto& session = result.value();

    // Create NMREP packet for trackable object
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_NMREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NM;

    cbPKT_NM* nm_pkt = reinterpret_cast<cbPKT_NM*>(&pkt);
    nm_pkt->mode = cbNM_MODE_SETTRACKABLE;
    nm_pkt->flags = 3;  // 1-based trackable object ID
    std::strncpy(nm_pkt->name, "LED_Marker", cbLEN_STR_LABEL);
    nm_pkt->value = (4 << 16) | 1;  // 4 points, type 1

    // Store packet - should be stored at index flags-1 (2)
    ASSERT_TRUE(session.storePacket(pkt).isOk());
}

// Note: cbNM_MODE_SETRPOS does not exist in upstream cbproto.h
// Reset test removed - if reset functionality is needed, use a different mode

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Native-Mode ShmemSession Tests
/// @{

class NativeShmemSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // n = native
        test_name = "n" + std::to_string(test_counter++);
    }

    // Helper to create a native STANDALONE session
    Result<ShmemSession> createNativeSession() {
        return ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    }

    std::string test_name;
    static int test_counter;
};

int NativeShmemSessionTest::test_counter = 0;

TEST_F(NativeShmemSessionTest, CreateNativeStandalone) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << "Failed to create native session: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
    EXPECT_EQ(session.getMode(), Mode::STANDALONE);
    EXPECT_EQ(session.getLayout(), ShmemLayout::NATIVE);
}

TEST_F(NativeShmemSessionTest, NativeConfigBufferAccessor) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Native layout should return native config buffer
    EXPECT_NE(session.getNativeConfigBuffer(), nullptr);
    // CENTRAL accessor should return an error for the native layout
    auto buf = std::make_unique<NativeConfigBuffer>();
    EXPECT_TRUE(session.getLegacyConfigBuffer(*buf).isError());
}

// The consensus offset (clock_offset_ns, for CLIENT readers) and the raw
// independent estimate (clock_raw_offset_ns, for peer voting) are separate
// fields and must not clobber each other.
TEST_F(NativeShmemSessionTest, ClockConsensusAndRawAreSeparate) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    const auto* cfg = session.getNativeConfigBuffer();
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(cfg->clock_sync_valid, 0u);
    EXPECT_EQ(cfg->clock_raw_valid, 0u);

    session.setClockSync(1'700'000'000'000'000'000LL, 500'000);  // consensus
    session.setClockRawOffset(1'700'000'000'002'500'000LL);      // own estimate, +2.5 ms

    // CLIENT-facing offset is the consensus value...
    auto consensus = session.getClockOffsetNs();
    ASSERT_TRUE(consensus.has_value());
    EXPECT_EQ(*consensus, 1'700'000'000'000'000'000LL);
    // ...while the raw field retains the independent estimate.
    EXPECT_EQ(cfg->clock_raw_valid, 1u);
    EXPECT_EQ(cfg->clock_raw_offset_ns, 1'700'000'000'002'500'000LL);
    EXPECT_EQ(cfg->clock_offset_ns, 1'700'000'000'000'000'000LL);
}

TEST_F(NativeShmemSessionTest, CreateAndDestroy) {
    {
        auto result = createNativeSession();
        ASSERT_TRUE(result.isOk());
        EXPECT_TRUE(result.value().isOpen());
    }
    // Session destroyed, shared memory released
}

// TODO: A ShmemSession is now per-instrument and carries no notion of rejecting
// "other" instruments or enumerating active ones. setInstrumentActive()/
// isInstrumentActive() operate on the session's own instrument (see InstrumentToggle
// below), and getFirstActiveInstrument() was removed. Rework instrument enumeration
// at the layer that manages the set of sessions.
//
// TEST_F(NativeShmemSessionTest, SingleInstrumentOnly) { ... }
// TEST_F(NativeShmemSessionTest, GetFirstActiveInstrument) { ... }

TEST_F(NativeShmemSessionTest, InstrumentToggle) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // The session's own instrument can be toggled active/inactive.
    auto set_result = session.setInstrumentActive(true);
    ASSERT_TRUE(set_result.isOk());

    auto active_result = session.isInstrumentActive();
    ASSERT_TRUE(active_result.isOk());
    EXPECT_TRUE(active_result.value());
}

TEST_F(NativeShmemSessionTest, SetAndGetProcInfo) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_PROCINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.chancount = 284;
    info.bankcount = 16;

    ASSERT_TRUE(session.setProcInfo(info).isOk());

    auto get_result = session.getProcInfo();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().chancount, 284);
    EXPECT_EQ(get_result.value().bankcount, 16);
}

// TODO: setProcInfo() no longer takes an instrument ID — a session writes to its
// own instrument's single procinfo slot — so there is no per-call instrument to
// reject. Multi-instrument rejection, if still desired, belongs at session creation.
//
// TEST_F(NativeShmemSessionTest, SetAndGetProcInfo_RejectMultiInstrument) { ... }

TEST_F(NativeShmemSessionTest, SetAndGetBankInfo) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_BANKINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.bank = 3;
    info.chancount = 32;

    ASSERT_TRUE(session.setBankInfo(3, info).isOk());

    auto get_result = session.getBankInfo(3);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().bank, 3);
    EXPECT_EQ(get_result.value().chancount, 32);
}

TEST_F(NativeShmemSessionTest, SetAndGetFilterInfo) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_FILTINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.filt = 5;
    info.hpfreq = 250000;

    ASSERT_TRUE(session.setFilterInfo(5, info).isOk());

    auto get_result = session.getFilterInfo(5);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().filt, 5);
    EXPECT_EQ(get_result.value().hpfreq, 250000);
}

TEST_F(NativeShmemSessionTest, SetAndGetChanInfo) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_CHANINFO info;
    std::memset(&info, 0, sizeof(info));
    info.chan = 10;
    info.proc = 1;
    info.bank = 1;
    std::strncpy(info.label, "chan_10", cbLEN_STR_LABEL);

    ASSERT_TRUE(session.setChanInfo(10, info).isOk());

    auto get_result = session.getChanInfo(10);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().chan, 10);
    EXPECT_STREQ(get_result.value().label, "chan_10");
}

TEST_F(NativeShmemSessionTest, ChanInfo_RejectOutOfRange) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Channel 284 is valid (0-based: 0..283), 284 is out of range
    cbPKT_CHANINFO info;
    std::memset(&info, 0, sizeof(info));

    auto set_result = session.setChanInfo(cbshm::NATIVE_MAXCHANS, info);
    EXPECT_TRUE(set_result.isError()) << "Channel " << cbshm::NATIVE_MAXCHANS << " should be out of range for native mode";
}

TEST_F(NativeShmemSessionTest, StorePacket_PROCINFO) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_PROCINFO;

    cbPKT_PROCINFO* proc_pkt = reinterpret_cast<cbPKT_PROCINFO*>(&pkt);
    proc_pkt->proc = 1;
    proc_pkt->chancount = 284;

    // storePacket writes to receive buffer only (config parsing done at device layer)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    // Verify packet was stored to receive buffer
    auto received = session.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 1u);
}

TEST_F(NativeShmemSessionTest, StorePacket_AnyInstrument) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // storePacket writes to receive buffer regardless of instrument field
    // (config parsing is done at device layer, not in storePacket)
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 1;
    pkt.cbpkt_header.type = cbPKTTYPE_PROCREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_PROCINFO;

    // Store should succeed (packet goes to receive buffer)
    auto store_result = session.storePacket(pkt);
    EXPECT_TRUE(store_result.isOk());

    // Verify it went to receive buffer
    auto received = session.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 1u);
}

TEST_F(NativeShmemSessionTest, StorePacket_CHANINFO) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.instrument = 0;
    pkt.cbpkt_header.type = cbPKTTYPE_CHANREP;
    pkt.cbpkt_header.dlen = cbPKTDLEN_CHANINFO;

    cbPKT_CHANINFO* chan_pkt = reinterpret_cast<cbPKT_CHANINFO*>(&pkt);
    chan_pkt->chan = 50;
    chan_pkt->proc = 1;
    chan_pkt->bank = 2;
    std::strncpy(chan_pkt->label, "elec050", cbLEN_STR_LABEL);

    // storePacket writes to receive buffer (config parsing at device layer)
    ASSERT_TRUE(session.storePacket(pkt).isOk());

    auto received = session.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 1u);
}

TEST_F(NativeShmemSessionTest, NspStatus) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Set NSP status for this session's instrument
    ASSERT_TRUE(session.setNspStatus(NativeNSPStatus::NSP_FOUND).isOk());

    auto get_result = session.getNspStatus();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value(), NativeNSPStatus::NSP_FOUND);
}

TEST_F(NativeShmemSessionTest, GeminiSystem) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Default should be false
    auto get_result = session.isGeminiSystem();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_FALSE(get_result.value());

    // Set to true
    ASSERT_TRUE(session.setGeminiSystem(true).isOk());

    get_result = session.isGeminiSystem();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_TRUE(get_result.value());
}

TEST_F(NativeShmemSessionTest, TransmitQueueRoundTrip) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Enqueue a packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x01;
    pkt.cbpkt_header.dlen = 4;
    pkt.data_u32[0] = 0xDEADBEEF;

    ASSERT_FALSE(session.hasTransmitPackets());
    ASSERT_TRUE(session.enqueuePacket(pkt).isOk());
    EXPECT_TRUE(session.hasTransmitPackets());

    // Dequeue and verify
    cbPKT_GENERIC out_pkt;
    auto deq_result = session.dequeuePacket(out_pkt);
    ASSERT_TRUE(deq_result.isOk());
    EXPECT_TRUE(deq_result.value());
    EXPECT_EQ(out_pkt.cbpkt_header.type, 0x01);
    EXPECT_EQ(out_pkt.data_u32[0], 0xDEADBEEF);

    EXPECT_FALSE(session.hasTransmitPackets());
}

TEST_F(NativeShmemSessionTest, LocalTransmitQueueRoundTrip) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x42;
    pkt.cbpkt_header.dlen = 2;
    pkt.data_u32[0] = 0xCAFEBABE;

    ASSERT_FALSE(session.hasLocalTransmitPackets());
    ASSERT_TRUE(session.enqueueLocalPacket(pkt).isOk());
    EXPECT_TRUE(session.hasLocalTransmitPackets());

    cbPKT_GENERIC out_pkt;
    auto deq_result = session.dequeueLocalPacket(out_pkt);
    ASSERT_TRUE(deq_result.isOk());
    EXPECT_TRUE(deq_result.value());
    EXPECT_EQ(out_pkt.cbpkt_header.type, 0x42);
    EXPECT_EQ(out_pkt.data_u32[0], 0xCAFEBABE);

    EXPECT_FALSE(session.hasLocalTransmitPackets());
}

TEST_F(NativeShmemSessionTest, ReceiveBufferStoreAndStats) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Initially empty
    auto received = session.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 0u);

    // Store a few packets
    constexpr int NUM_PKTS = 5;
    for (int i = 0; i < NUM_PKTS; i++) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.type = static_cast<uint8_t>(i + 1);
        pkt.cbpkt_header.dlen = 4;
        pkt.data_u32[0] = 100 + i;

        ASSERT_TRUE(session.storePacket(pkt).isOk());
    }

    // Check stats - received count should match number of packets stored
    received = session.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), static_cast<uint32_t>(NUM_PKTS));
}

TEST_F(NativeShmemSessionTest, NumTotalChans) {
    auto result = createNativeSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    auto chans_result = session.getNumTotalChans();
    ASSERT_TRUE(chans_result.isOk());
    // Native mode init sets total channels to NATIVE_MAXCHANS (284)
    EXPECT_EQ(chans_result.value(), cbshm::NATIVE_MAXCHANS);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name CENTRAL ShmemSession Tests
/// @{

class CentralCompatShmemSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // c = compat
        test_name = "c" + std::to_string(test_counter++);
    }

    // Helper to create a CENTRAL STANDALONE session (for testing)
    Result<ShmemSession> createCompatSession() {
        return ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    }

    std::string test_name;
    static int test_counter;
};

int CentralCompatShmemSessionTest::test_counter = 0;

TEST_F(CentralCompatShmemSessionTest, CreateCompatStandalone) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << "Failed to create compat session: " << result.error();

    auto& session = result.value();
    EXPECT_TRUE(session.isOpen());
    EXPECT_EQ(session.getMode(), Mode::STANDALONE);
    EXPECT_EQ(session.getLayout(), ShmemLayout::CENTRAL);
}

TEST_F(CentralCompatShmemSessionTest, LegacyConfigBufferAccessor) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // CENTRAL should provide a translated copy of the legacy config buffer
    auto buf = std::make_unique<NativeConfigBuffer>();
    EXPECT_TRUE(session.getLegacyConfigBuffer(*buf).isOk());
    // Native accessor should return nullptr for the CENTRAL layout
    EXPECT_EQ(session.getNativeConfigBuffer(), nullptr);
}

TEST_F(CentralCompatShmemSessionTest, IsInstrumentActive_AlwaysTrue) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // In CENTRAL mode the session's instrument always reports as active
    // (Central's layout has no instrument_status field).
    auto active_result = session.isInstrumentActive();
    ASSERT_TRUE(active_result.isOk());
    EXPECT_TRUE(active_result.value()) << "Instrument should be active in compat mode";
}

TEST_F(CentralCompatShmemSessionTest, SetInstrumentActive_ReturnsError) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // Setting instrument status should fail (read-only in compat mode)
    auto set_result = session.setInstrumentActive(true);
    EXPECT_TRUE(set_result.isError());
}

// TODO: getFirstActiveInstrument() was removed; a CENTRAL session is bound to a
// single instrument at creation. Instrument enumeration now happens above the
// ShmemSession layer.
//
// TEST_F(CentralCompatShmemSessionTest, GetFirstActiveInstrument) { ... }

TEST_F(CentralCompatShmemSessionTest, SetAndGetProcInfo) {
    // A CENTRAL session targets a single instrument; create one for instrument 2.
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, test_name, InstrumentId::fromOneBased(2));
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_PROCINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 2;
    info.chancount = 512;
    info.bankcount = 24;

    auto set_result = session.setProcInfo(info);
    ASSERT_TRUE(set_result.isOk());

    // Retrieve and verify
    auto get_result = session.getProcInfo();
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 2);
    EXPECT_EQ(get_result.value().chancount, 512);
    EXPECT_EQ(get_result.value().bankcount, 24);
}

TEST_F(CentralCompatShmemSessionTest, SetAndGetBankInfo) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_BANKINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.bank = 5;
    info.chancount = 32;

    ASSERT_TRUE(session.setBankInfo(5, info).isOk());

    auto get_result = session.getBankInfo(5);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().proc, 1);
    EXPECT_EQ(get_result.value().bank, 5);
    EXPECT_EQ(get_result.value().chancount, 32);
}

TEST_F(CentralCompatShmemSessionTest, SetAndGetFilterInfo) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_FILTINFO info;
    std::memset(&info, 0, sizeof(info));
    info.proc = 1;
    info.filt = 10;
    info.hpfreq = 300000;

    ASSERT_TRUE(session.setFilterInfo(10, info).isOk());

    auto get_result = session.getFilterInfo(10);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().filt, 10);
    EXPECT_EQ(get_result.value().hpfreq, 300000);
}

TEST_F(CentralCompatShmemSessionTest, SetAndGetChanInfo) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_CHANINFO info;
    std::memset(&info, 0, sizeof(info));
    info.chan = 100;
    info.proc = 1;
    info.bank = 4;
    std::strncpy(info.label, "elec100", cbLEN_STR_LABEL);

    ASSERT_TRUE(session.setChanInfo(100, info).isOk());

    auto get_result = session.getChanInfo(100);
    ASSERT_TRUE(get_result.isOk());
    EXPECT_EQ(get_result.value().chan, 100);
    EXPECT_STREQ(get_result.value().label, "elec100");
}

// TODO: The explicit setInstrumentFilter()/getInstrumentFilter() API was removed.
// readReceiveBuffer() now implicitly returns only packets for the session's own
// instrument (the instrument fixed at create()), so there is no separate filter to
// get/set and no "no filter" mode. The implicit filtering is covered by
// InstrumentFilter_ReturnsOwnInstrument below.
//
// TEST_F(CentralCompatShmemSessionTest, InstrumentFilter_DefaultNoFilter) { ... }
// TEST_F(CentralCompatShmemSessionTest, InstrumentFilter_SetAndGet) { ... }
// TEST_F(CentralCompatShmemSessionTest, InstrumentFilter_NoFilter_ReturnsAll) { ... }

TEST_F(CentralCompatShmemSessionTest, InstrumentFilter_ReturnsOwnInstrument) {
    // A session bound to instrument 2 should only see instrument-2 packets when
    // reading the (shared) receive buffer.
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, test_name, InstrumentId::fromIndex(2));
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // STANDALONE compat session initializes procinfo version to current,
    // so readReceiveBuffer correctly parses current-format packets written by storePacket.
    ASSERT_EQ(session.getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);

    // Store packets from different instruments with realistic timestamps
    uint32_t dlen = 4;

    for (uint8_t inst = 0; inst < 4; ++inst) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.time = 1000000000ULL + inst * 33333ULL;  // Realistic nanosecond timestamps
        pkt.cbpkt_header.chid = 1;  // Non-configuration packet
        pkt.cbpkt_header.instrument = inst;
        pkt.cbpkt_header.type = 0x01;
        pkt.cbpkt_header.dlen = dlen;
        pkt.data_u32[0] = 0xAA00 + inst;

        ASSERT_TRUE(session.storePacket(pkt).isOk());
    }

    // Read packets - should only get the one from this session's instrument (2)
    cbPKT_GENERIC read_pkts[10];
    size_t packets_read = 0;
    auto read_result = session.readReceiveBuffer(read_pkts, 10, packets_read);
    ASSERT_TRUE(read_result.isOk()) << read_result.error();
    EXPECT_EQ(packets_read, 1u);
    EXPECT_EQ(read_pkts[0].cbpkt_header.instrument, 2);
    EXPECT_EQ(read_pkts[0].data_u32[0], 0xAA02u);
}

TEST_F(CentralCompatShmemSessionTest, TransmitQueueRoundTrip) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x05;
    pkt.cbpkt_header.dlen = 4;
    pkt.data_u32[0] = 0x12345678;

    ASSERT_FALSE(session.hasTransmitPackets());
    ASSERT_TRUE(session.enqueuePacket(pkt).isOk());
    EXPECT_TRUE(session.hasTransmitPackets());

    cbPKT_GENERIC out_pkt;
    auto deq_result = session.dequeuePacket(out_pkt);
    ASSERT_TRUE(deq_result.isOk());
    EXPECT_TRUE(deq_result.value());
    EXPECT_EQ(out_pkt.cbpkt_header.type, 0x05);
    EXPECT_EQ(out_pkt.data_u32[0], 0x12345678u);

    EXPECT_FALSE(session.hasTransmitPackets());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Central Compat Protocol Translation Tests
/// @{

class CentralCompatProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // p = proto
        test_name = "p" + std::to_string(test_counter++);
    }

    Result<ShmemSession> createCompatSession() {
        return ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    }

    std::string test_name;
    static int test_counter;
};

int CentralCompatProtocolTest::test_counter = 0;

/// @brief Test protocol version detection: STANDALONE initializes to current
TEST_F(CentralCompatProtocolTest, DetectProtocol_StandaloneDefault) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    // STANDALONE compat session initializes procinfo version to current (4.2)
    EXPECT_EQ(session.getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);
}

/// @brief Test that readReceiveBuffer correctly parses current-format packets
TEST_F(CentralCompatProtocolTest, ReadCurrentFormat_WithRealisticTimestamps) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    EXPECT_EQ(session.getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);

    // Store packets (current format, written by storePacket).  The session is bound
    // to instrument 0, and readReceiveBuffer only returns that instrument's packets,
    // so store all three as instrument 0.
    for (uint8_t i = 0; i < 3; ++i) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.time = 5000000000ULL + i * 33333ULL;
        pkt.cbpkt_header.chid = 1;
        pkt.cbpkt_header.instrument = 0;
        pkt.cbpkt_header.type = 0x01;
        pkt.cbpkt_header.dlen = 4;
        pkt.data_u32[0] = 0xCC00 + i;

        ASSERT_TRUE(session.storePacket(pkt).isOk());
    }

    // Read packets (current-format parsing, no translation)
    cbPKT_GENERIC read_pkts[10];
    size_t packets_read = 0;
    auto read_result = session.readReceiveBuffer(read_pkts, 10, packets_read);
    ASSERT_TRUE(read_result.isOk()) << read_result.error();
    EXPECT_EQ(packets_read, 3u);

    for (size_t i = 0; i < packets_read; ++i) {
        EXPECT_EQ(read_pkts[i].cbpkt_header.instrument, 0);
        EXPECT_EQ(read_pkts[i].data_u32[0], 0xCC00u + i);
        EXPECT_EQ(read_pkts[i].cbpkt_header.dlen, 4u);
    }
}

// TODO: Protocol-version detection no longer reads procinfo[0].version from a
// writable legacy config buffer (that pointer accessor was removed). It is now
// inferred from Central.exe's file version, which requires Windows and a running
// Central, so these CLIENT+CENTRAL detection tests can't be exercised here. They
// should be reworked as Windows-only integration tests against a real Central.
//
// TEST_F(CentralCompatProtocolTest, DetectProtocol_311) { ... }
// TEST_F(CentralCompatProtocolTest, DetectProtocol_400) { ... }
// TEST_F(CentralCompatProtocolTest, DetectProtocol_410) { ... }
// TEST_F(CentralCompatProtocolTest, DetectProtocol_Current_42) { ... }

/// @brief Test readReceiveBuffer's implicit per-instrument filtering with current-format packets
TEST_F(CentralCompatProtocolTest, InstrumentFilterWithCurrentProtocol) {
    // Bind the session to instrument 3; readReceiveBuffer returns only its packets.
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, test_name, InstrumentId::fromIndex(3));
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    EXPECT_EQ(session.getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);

    // Store packets from 4 different instruments
    for (uint8_t inst = 0; inst < 4; ++inst) {
        cbPKT_GENERIC pkt;
        std::memset(&pkt, 0, sizeof(pkt));
        pkt.cbpkt_header.time = 3000000000ULL + inst * 33333ULL;
        pkt.cbpkt_header.chid = 1;
        pkt.cbpkt_header.instrument = inst;
        pkt.cbpkt_header.type = 0x01;
        pkt.cbpkt_header.dlen = 4;
        pkt.data_u32[0] = 0xDD00 + inst;

        ASSERT_TRUE(session.storePacket(pkt).isOk());
    }

    cbPKT_GENERIC read_pkts[10];
    size_t packets_read = 0;
    auto read_result = session.readReceiveBuffer(read_pkts, 10, packets_read);
    ASSERT_TRUE(read_result.isOk()) << read_result.error();
    EXPECT_EQ(packets_read, 1u);
    EXPECT_EQ(read_pkts[0].cbpkt_header.instrument, 3);
    EXPECT_EQ(read_pkts[0].data_u32[0], 0xDD03u);
}

/// @brief Test transmit round-trip with current protocol (no translation)
TEST_F(CentralCompatProtocolTest, TransmitRoundTrip_CurrentProtocol) {
    auto result = createCompatSession();
    ASSERT_TRUE(result.isOk()) << result.error();
    auto& session = result.value();

    EXPECT_EQ(session.getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);

    // Enqueue a packet
    cbPKT_GENERIC pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.cbpkt_header.type = 0x10;
    pkt.cbpkt_header.dlen = 4;
    pkt.cbpkt_header.instrument = 1;
    pkt.data_u32[0] = 0xFEEDFACE;

    ASSERT_TRUE(session.enqueuePacket(pkt).isOk());
    EXPECT_TRUE(session.hasTransmitPackets());

    // Dequeue and verify (no translation, so data should match exactly)
    cbPKT_GENERIC out_pkt;
    auto deq_result = session.dequeuePacket(out_pkt);
    ASSERT_TRUE(deq_result.isOk());
    EXPECT_TRUE(deq_result.value());
    EXPECT_EQ(out_pkt.cbpkt_header.type, 0x10);
    EXPECT_EQ(out_pkt.cbpkt_header.dlen, 4u);
    EXPECT_EQ(out_pkt.cbpkt_header.instrument, 1);
    EXPECT_EQ(out_pkt.data_u32[0], 0xFEEDFACEu);

    EXPECT_FALSE(session.hasTransmitPackets());
}

/// @brief Non-compat layout always detects CURRENT protocol
TEST_F(CentralCompatProtocolTest, NativeLayout_AlwaysCurrent) {
    std::string name = test_name;
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk()) << result.error();
    EXPECT_EQ(result.value().getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);
}

/// @brief CENTRAL layout always detects CURRENT protocol
TEST_F(CentralCompatProtocolTest, CentralLayout_AlwaysCurrent) {
    std::string name = test_name;
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::CENTRAL, name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk()) << result.error();
    EXPECT_EQ(result.value().getCompatProtocolVersion(), CBPROTO_PROTOCOL_CURRENT);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Owner Liveness Tests
/// @{

class OwnerLivenessTest : public ::testing::Test {
protected:
    void SetUp() override {
        // l = liveness
        test_name = "l" + std::to_string(test_counter++);
    }

    std::string test_name;
    static int test_counter;
};

int OwnerLivenessTest::test_counter = 0;

TEST_F(OwnerLivenessTest, StandaloneWritesOwnerPid) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk()) << result.error();

    auto* cfg = result.value().getNativeConfigBuffer();
    ASSERT_NE(cfg, nullptr);
#ifdef _WIN32
    EXPECT_EQ(cfg->owner_pid, GetCurrentProcessId());
#else
    EXPECT_EQ(cfg->owner_pid, static_cast<uint32_t>(getpid()));
#endif
}

TEST_F(OwnerLivenessTest, StandaloneAlwaysReturnsTrue) {
    auto result = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(result.isOk()) << result.error();

    // isOwnerAlive() is only meaningful for CLIENT — STANDALONE always returns true
    EXPECT_TRUE(result.value().isOwnerAlive());
}

TEST_F(OwnerLivenessTest, ClientDetectsLiveOwner) {
    // Create STANDALONE (sets owner_pid to current process)
    auto standalone = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(standalone.isOk()) << standalone.error();

    // Create CLIENT on same segments
    auto client = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(client.isOk()) << client.error();

    // Owner (this process) is alive
    EXPECT_TRUE(client.value().isOwnerAlive());
}

TEST_F(OwnerLivenessTest, ClientDetectsDeadOwner) {
    // Create STANDALONE, then set a fake dead PID
    auto standalone = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(standalone.isOk()) << standalone.error();

    // Overwrite owner_pid with a PID that (almost certainly) doesn't exist
    auto* cfg = standalone.value().getNativeConfigBuffer();
    ASSERT_NE(cfg, nullptr);
    cfg->owner_pid = 4000000000u;  // Well above any real PID

    // Create CLIENT on same segments
    auto client = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(client.isOk()) << client.error();

    // Owner PID doesn't exist — should detect as dead
    EXPECT_FALSE(client.value().isOwnerAlive());
}

TEST_F(OwnerLivenessTest, StandaloneWritesSegmentUid) {
    auto result = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::STANDALONE, ShmemLayout::NATIVE);
    ASSERT_TRUE(result.isOk()) << result.error();

    auto* cfg = result.value().getNativeConfigBuffer();
    ASSERT_NE(cfg, nullptr);
    EXPECT_NE(cfg->segment_uid, 0ull);
}

TEST_F(OwnerLivenessTest, ClientDetectsUnlinkedSegment) {
    // Owner creates the segment; client attaches (records uid N1).
    auto standalone = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::STANDALONE, ShmemLayout::NATIVE);
    ASSERT_TRUE(standalone.isOk()) << standalone.error();

    auto client = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::CLIENT, ShmemLayout::NATIVE);
    ASSERT_TRUE(client.isOk()) << client.error();
    EXPECT_TRUE(client.value().isOwnerAlive());  // segment is current

    // Owner tears down the segment (unlinks the names); the client keeps its
    // mapping.  With the name gone, the client's segment is stale.
    {
        ShmemSession owner = std::move(standalone.value());
        // owner's destructor at end of scope runs close() -> shm_unlink.
    }
    EXPECT_FALSE(client.value().isOwnerAlive());
}

TEST_F(OwnerLivenessTest, ClientDetectsSupersededSegment) {
    // Owner creates the segment (uid N1); client attaches.
    auto standalone1 = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::STANDALONE, ShmemLayout::NATIVE);
    ASSERT_TRUE(standalone1.isOk()) << standalone1.error();

    auto client = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::CLIENT, ShmemLayout::NATIVE);
    ASSERT_TRUE(client.isOk()) << client.error();
    EXPECT_TRUE(client.value().isOwnerAlive());

    // Owner tears the segment down, then a NEW owner recreates it under the SAME
    // name (fresh uid N2).  This is the persistent-service case: owner_pid is
    // identical, but the segment instance is different.
    {
        ShmemSession owner1 = std::move(standalone1.value());
    }
    auto standalone2 = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::STANDALONE, ShmemLayout::NATIVE);
    ASSERT_TRUE(standalone2.isOk()) << standalone2.error();

    // A fresh client on the new segment sees it as current...
    auto client2 = ShmemSession::create(
        test_name + "_cfg", test_name + "_rec", test_name + "_xmt",
        test_name + "_xmt_local", test_name + "_status", test_name + "_spk",
        test_name + "_signal", Mode::CLIENT, ShmemLayout::NATIVE);
    ASSERT_TRUE(client2.isOk()) << client2.error();
    EXPECT_TRUE(client2.value().isOwnerAlive());

    // ...but the old client, still mapped to the superseded segment, is stale —
    // even though a live owner now holds the name (owner_pid alone can't tell).
    EXPECT_FALSE(client.value().isOwnerAlive());
}

TEST_F(OwnerLivenessTest, ClientTreatsZeroPidAsAlive) {
    // Create STANDALONE, then clear owner_pid to simulate pre-liveness segments
    auto standalone = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(standalone.isOk()) << standalone.error();

    auto* cfg = standalone.value().getNativeConfigBuffer();
    ASSERT_NE(cfg, nullptr);
    cfg->owner_pid = 0;

    // Create CLIENT on same segments
    auto client = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, cbproto::InstrumentId::fromOneBased(cbNSP1));
    ASSERT_TRUE(client.isOk()) << client.error();

    // PID 0 = unknown — assume alive (backward compat)
    EXPECT_TRUE(client.value().isOwnerAlive());
}

// TODO: Opening a CLIENT + CENTRAL session triggers Central protocol-version
// detection, which inspects Central.exe's file version and therefore requires
// Windows with a running Central. This can't be exercised on a non-Windows build,
// so this liveness test should be reworked as a Windows-only integration test.
//
// TEST_F(OwnerLivenessTest, CentralCompatAlwaysReturnsTrue) { ... }

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Receive-ring wrap regression
///
/// Regression for the receive-ring wrap desynchronization.  The producer and
/// consumer both wrap their index to 0 once it passes rec_buffer_len minus a
/// reserved tail zone (Central's convention); if the reader used a different
/// threshold it would drift off the packet boundary at the wrap and deliver
/// misframed/duplicated/lost packets.  This forces the NATIVE ring to wrap
/// multiple times and asserts every packet survives intact and in order.
/// @{

TEST_F(ShmemSessionTest, ReceiveRingForcedWrapIntegrity) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    // Producer (writes the ring) and consumer (reads it) share one NATIVE
    // segment set, exactly like a STANDALONE owner and a CLIENT reader.
    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();

    // A CLIENT syncs its tail to the current head (0/0) on open, so it observes
    // only packets written after this point.
    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    // Max-size packets (256 words) wrap the ~268 MB ring in the fewest
    // iterations.  dlen counts 32-bit data words after the 16-byte header.
    constexpr uint16_t kDlen = (cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 4;   // 252
    constexpr uint32_t kPktWords = cbPKT_HEADER_32SIZE + kDlen;            // 256

    // Force just over two full wraps so we exercise steady-state wrapping, not
    // only the first crossing.
    const uint64_t target_words =
        2ull * NATIVE_cbRECBUFFLEN + NATIVE_cbRECBUFFLEN / 16;

    auto make_pkt = [&](uint64_t seq) {
        cbPKT_GENERIC pkt{};
        pkt.cbpkt_header.time = static_cast<PROCTIME>(seq);
        pkt.cbpkt_header.chid = 5;              // a real channel (<= 0x0FFF)
        pkt.cbpkt_header.type = 0;              // continuous sample (high byte clear)
        pkt.cbpkt_header.dlen = kDlen;
        pkt.cbpkt_header.instrument = inst_index;
        pkt.cbpkt_header.reserved = 0;
        pkt.data_u32[0] = static_cast<uint32_t>(seq);         // low 32 bits of seq
        pkt.data_u32[1] = static_cast<uint32_t>(seq >> 32);   // high 32 bits of seq
        return pkt;
    };

    // Batch is far smaller than one lap (~262 K packets), and we drain fully
    // after every burst, so the producer never laps the consumer.
    constexpr size_t kBatch = 4096;
    std::vector<cbPKT_GENERIC> read_buf(kBatch);

    uint64_t written = 0;        // next sequence number to write
    uint64_t verified = 0;       // next sequence number we expect to read
    uint64_t written_words = 0;

    while (written_words < target_words) {
        for (size_t i = 0; i < kBatch; ++i) {
            auto w = producer.storePacket(make_pkt(written));
            ASSERT_TRUE(w.isOk()) << w.error() << " at seq " << written;
            ++written;
            written_words += kPktWords;
        }

        // Drain everything the consumer can currently see.
        for (;;) {
            size_t got = 0;
            auto r = consumer.readReceiveBuffer(read_buf.data(), read_buf.size(), got);
            ASSERT_TRUE(r.isOk()) << r.error() << " after " << verified
                << " packets read, " << written_words << " words written";
            if (got == 0) break;
            for (size_t i = 0; i < got; ++i) {
                const auto& p = read_buf[i];
                // Sequence must be contiguous: any gap = loss, repeat = dup,
                // mismatch = misframed bytes from a botched wrap.
                ASSERT_EQ(static_cast<uint64_t>(p.cbpkt_header.time), verified)
                    << "packet out of sequence at the wrap boundary";
                ASSERT_EQ(p.data_u32[0], static_cast<uint32_t>(verified));
                ASSERT_EQ(p.data_u32[1], static_cast<uint32_t>(verified >> 32));
                ASSERT_EQ(p.cbpkt_header.chid, 5);
                ASSERT_EQ(p.cbpkt_header.type, 0);
                ASSERT_EQ(p.cbpkt_header.dlen, kDlen);
                ASSERT_EQ(p.cbpkt_header.instrument, inst_index);
                ++verified;
            }
        }
    }

    // Every packet written was read back exactly once, in order.
    EXPECT_EQ(verified, written);

    // Confirm we genuinely forced multiple wraps (wrote more than two rings).
    EXPECT_GT(written_words, 2ull * NATIVE_cbRECBUFFLEN);

    // The producer's received counter matches what we wrote.
    auto received = producer.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), static_cast<uint32_t>(written));
}

// The forced-wrap test above uses uniform max-size packets, so the write
// position never lands exactly ON the wrap threshold (buflen - reserve).
// That landing is the strict-greater-than edge of the wrap rule: no wrap
// happens, and the NEXT packet legally starts at the threshold and extends
// into the reserve zone before wrapping.  Arrange packet sizes to hit the
// threshold exactly and verify the consumer stays in lockstep through both
// edges.
TEST_F(ShmemSessionTest, ReceiveRingExactThresholdBoundary) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();
    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    constexpr uint32_t kReserve = cbCER_UDP_SIZE_MAX / 4;
    constexpr uint32_t kThreshold = NATIVE_cbRECBUFFLEN - kReserve;
    constexpr uint32_t kMaxPkt = sizeof(cbPKT_GENERIC) / sizeof(uint32_t);
    // Smallest useful packet: header plus one data word for the sequence stamp.
    constexpr uint32_t kMinPkt = cbPKT_HEADER_32SIZE + 1;

    auto make_pkt = [&](uint32_t total_dwords, uint64_t seq) {
        cbPKT_GENERIC pkt{};
        pkt.cbpkt_header.time = static_cast<PROCTIME>(seq + 1);
        pkt.cbpkt_header.chid = 5;              // a real channel (<= 0x0FFF)
        pkt.cbpkt_header.type = 0;              // continuous sample (high byte clear)
        pkt.cbpkt_header.dlen = static_cast<uint16_t>(total_dwords - cbPKT_HEADER_32SIZE);
        pkt.cbpkt_header.instrument = inst_index;
        pkt.data_u32[0] = static_cast<uint32_t>(seq);
        return pkt;
    };

    uint64_t seq_w = 0, seq_r = 0;
    std::vector<cbPKT_GENERIC> rb(4096);
    auto drain = [&]() {
        for (;;) {
            size_t got = 0;
            auto r = consumer.readReceiveBuffer(rb.data(), rb.size(), got);
            ASSERT_TRUE(r.isOk()) << r.error() << " at seq " << seq_r;
            if (got == 0) break;
            for (size_t i = 0; i < got; ++i) {
                ASSERT_EQ(rb[i].data_u32[0], static_cast<uint32_t>(seq_r))
                    << "packet out of sequence at the threshold boundary";
                ++seq_r;
            }
        }
    };

    // Fill with max-size packets up to the threshold, finishing with two
    // remainder packets sized so the last one ends exactly ON the threshold.
    uint64_t full = kThreshold / kMaxPkt;
    uint32_t rem = kThreshold % kMaxPkt;
    if (rem != 0 && rem < 2 * kMinPkt) {
        // Keep the remainder packets well-formed regardless of how the
        // constants evolve.
        full -= 1;
        rem += kMaxPkt;
    }
    for (uint64_t i = 0; i < full; ++i, ++seq_w) {
        ASSERT_TRUE(producer.storePacket(make_pkt(kMaxPkt, seq_w)).isOk());
        if ((i & 0xFFF) == 0) {
            drain();
            if (::testing::Test::HasFatalFailure()) return;
        }
    }
    if (rem != 0) {
        uint32_t first = rem / 2;
        if (first < kMinPkt) first = kMinPkt;
        uint32_t second = rem - first;
        ASSERT_GE(second, kMinPkt);
        ASSERT_TRUE(producer.storePacket(make_pkt(first, seq_w)).isOk());
        ++seq_w;
        ASSERT_TRUE(producer.storePacket(make_pkt(second, seq_w)).isOk());
        ++seq_w;
    }
    drain();
    if (::testing::Test::HasFatalFailure()) return;
    // Reader is parked exactly on the threshold (no wrap yet: rule is strict >).
    ASSERT_EQ(seq_r, seq_w) << "reader not parked exactly on the threshold";

    // The next packet starts AT the threshold, extends into the reserve zone,
    // and wraps the write position; the one after it lands at offset 0.
    ASSERT_TRUE(producer.storePacket(make_pkt(kMaxPkt, seq_w)).isOk());
    ++seq_w;
    ASSERT_TRUE(producer.storePacket(make_pkt(kMaxPkt, seq_w)).isOk());
    ++seq_w;
    drain();
    if (::testing::Test::HasFatalFailure()) return;
    EXPECT_EQ(seq_r, seq_w) << "reader desynced across the wrap";
}

// A CLIENT adopts the producer's current head on open, so it must observe
// nothing that was written before it attached and everything written after.
TEST_F(ShmemSessionTest, ReceiveRingClientAttachMidStreamSeesOnlyNewPackets) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();

    auto make_pkt = [&](uint64_t seq) {
        cbPKT_GENERIC pkt{};
        pkt.cbpkt_header.time = static_cast<PROCTIME>(seq + 1);
        pkt.cbpkt_header.chid = 5;
        pkt.cbpkt_header.type = 0;
        pkt.cbpkt_header.dlen = 4;
        pkt.cbpkt_header.instrument = inst_index;
        pkt.data_u32[0] = static_cast<uint32_t>(seq);
        return pkt;
    };

    // Write some packets before the client attaches.
    for (uint64_t i = 0; i < 100; ++i) {
        ASSERT_TRUE(producer.storePacket(make_pkt(i)).isOk());
    }

    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    // The client adopted the current head: nothing to read yet.
    cbPKT_GENERIC read_pkts[16];
    size_t got = 123;
    ASSERT_TRUE(consumer.readReceiveBuffer(read_pkts, 16, got).isOk());
    EXPECT_EQ(got, 0u);

    // Packets written after the attach are visible, in order, and none of the
    // pre-attach packets leak through.
    for (uint64_t i = 100; i < 110; ++i) {
        ASSERT_TRUE(producer.storePacket(make_pkt(i)).isOk());
    }
    uint64_t expect_seq = 100;
    for (;;) {
        ASSERT_TRUE(consumer.readReceiveBuffer(read_pkts, 16, got).isOk());
        if (got == 0) break;
        for (size_t i = 0; i < got; ++i) {
            ASSERT_EQ(read_pkts[i].data_u32[0], static_cast<uint32_t>(expect_seq));
            ++expect_seq;
        }
    }
    EXPECT_EQ(expect_seq, 110u);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Receive-ring fail-safe regression
///
/// The receive ring has three defensive paths that a happy-path stream never
/// exercises: the writer rejects a packet too large for the reserved tail zone
/// (before its memcpy runs off the ring / off the source packet), and the
/// reader resyncs to head on an overrun (producer lapped the consumer) or a
/// desync (a header that cannot belong to a real packet).  These drive each
/// path directly.
/// @{

// The writer must reject a packet whose size exceeds the reserved tail zone.
// Otherwise the memcpy reads past the end of the source cbPKT_GENERIC and, when
// head is near the wrap threshold, writes past the end of the ring.  storePacket
// swallows the ring-write error by design, so the rejection is observed through
// its side effects: the packet is neither counted nor delivered, and the ring
// stays usable for the next valid packet.
TEST_F(ShmemSessionTest, WriterRejectsPacketLargerThanReserve) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();
    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    // dlen = 20000 words is larger than the reserve on every layout
    // (cbCER_UDP_SIZE_MAX / 4 <= 14520) but still representable in the 16-bit
    // dlen field — exactly the malformed-oversized case the guard exists for.
    cbPKT_GENERIC bad{};
    bad.cbpkt_header.chid = 5;
    bad.cbpkt_header.type = 0;
    bad.cbpkt_header.dlen = 20000;
    bad.cbpkt_header.instrument = inst_index;
    ASSERT_TRUE(producer.storePacket(bad).isOk());

    // Rejected before the received++ in writeToReceiveBuffer, so nothing landed.
    auto received = producer.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 0u) << "oversized packet must be rejected, not written";

    // The ring is still usable: a valid packet stored next round-trips intact.
    cbPKT_GENERIC good{};
    good.cbpkt_header.time = 42;
    good.cbpkt_header.chid = 5;
    good.cbpkt_header.type = 0;
    good.cbpkt_header.dlen = 4;
    good.cbpkt_header.instrument = inst_index;
    good.data_u32[0] = 0xABCD1234u;
    ASSERT_TRUE(producer.storePacket(good).isOk());

    std::vector<cbPKT_GENERIC> out(4);
    size_t got = 0;
    ASSERT_TRUE(consumer.readReceiveBuffer(out.data(), out.size(), got).isOk());
    ASSERT_EQ(got, 1u);
    EXPECT_EQ(static_cast<uint64_t>(out[0].cbpkt_header.time), 42u);
    EXPECT_EQ(out[0].data_u32[0], 0xABCD1234u);

    received = producer.getReceivedPacketCount();
    ASSERT_TRUE(received.isOk());
    EXPECT_EQ(received.value(), 1u);
}

// When the producer laps a consumer that has not drained, the consumer's tail
// has been overwritten.  The next read must fail safe with an overrun error
// rather than deliver clobbered bytes.
TEST_F(ShmemSessionTest, ReceiveRingOverrunReportsDataLost) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();
    // Opens with tail synced to head (0/0) and never reads until the end, so the
    // producer's wrap counter runs ahead of the still-zero tail.
    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    constexpr uint16_t kDlen = (cbPKT_MAX_SIZE - cbPKT_HEADER_SIZE) / 4;
    constexpr uint32_t kPktWords = cbPKT_HEADER_32SIZE + kDlen;

    cbPKT_GENERIC pkt{};
    pkt.cbpkt_header.chid = 5;
    pkt.cbpkt_header.type = 0;
    pkt.cbpkt_header.dlen = kDlen;
    pkt.cbpkt_header.instrument = inst_index;

    // Fill more than one full ring so the producer's head wraps past the tail.
    const uint64_t words_to_write = NATIVE_cbRECBUFFLEN + NATIVE_cbRECBUFFLEN / 8;
    uint64_t w = 0;
    while (w < words_to_write) {
        ASSERT_TRUE(producer.storePacket(pkt).isOk());
        w += kPktWords;
    }

    std::vector<cbPKT_GENERIC> out(16);
    size_t got = 0;
    auto r = consumer.readReceiveBuffer(out.data(), out.size(), got);
    EXPECT_FALSE(r.isOk());
    EXPECT_NE(r.error().find("overrun"), std::string::npos) << r.error();
}

// A packet the writer accepts (its size is fine; the writer does not validate
// chid) but whose channel id cannot belong to a real packet must be caught by
// the reader's fail-safe header validation, which resyncs instead of delivering
// garbage to user callbacks.
TEST_F(ShmemSessionTest, ReceiveRingDesyncReportsDataLost) {
    const auto inst = InstrumentId::fromOneBased(cbNSP1);
    const uint8_t inst_index = static_cast<uint8_t>(inst.toIndex());

    auto producer_r = ShmemSession::create(Mode::STANDALONE, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(producer_r.isOk()) << producer_r.error();
    auto& producer = producer_r.value();
    auto consumer_r = ShmemSession::create(Mode::CLIENT, ShmemLayout::NATIVE, test_name, inst);
    ASSERT_TRUE(consumer_r.isOk()) << consumer_r.error();
    auto& consumer = consumer_r.value();

    // chid 0x2000 is > 0x0FFF and is not cbPKTCHAN_CONFIGURATION (0x8000), so it
    // fails the reader's validity check.  Size stays plausible so the failure is
    // the header check, not the implausible-size check.
    cbPKT_GENERIC pkt{};
    pkt.cbpkt_header.time = 7;
    pkt.cbpkt_header.chid = 0x2000;
    pkt.cbpkt_header.type = 0;
    pkt.cbpkt_header.dlen = 4;
    pkt.cbpkt_header.instrument = inst_index;
    ASSERT_TRUE(producer.storePacket(pkt).isOk());

    std::vector<cbPKT_GENERIC> out(4);
    size_t got = 0;
    auto r = consumer.readReceiveBuffer(out.data(), out.size(), got);
    EXPECT_FALSE(r.isOk());
    EXPECT_NE(r.error().find("desync"), std::string::npos) << r.error();
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Run all tests
///
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
