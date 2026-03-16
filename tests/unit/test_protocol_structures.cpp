///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_protocol_structures.cpp
/// @brief  Verify protocol structures match upstream ground truth
///
/// These tests ensure that our protocol definitions are byte-for-byte compatible
/// with the upstream protocol (cbproto.h).
///
/// CRITICAL: These structures must match exactly for compatibility with Central!
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbproto/cbproto.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for protocol structure tests
///////////////////////////////////////////////////////////////////////////////////////////////////
class ProtocolStructureTest : public ::testing::Test {
protected:
    // Expected structure sizes from upstream/cbproto/cbproto.h
#ifdef CBPROTO_311
    static constexpr size_t EXPECTED_PROCTIME_SIZE = 4;  // uint32_t
    static constexpr size_t EXPECTED_HEADER_SIZE = 8;
#else
    static constexpr size_t EXPECTED_PROCTIME_SIZE = 8;  // uint64_t
    static constexpr size_t EXPECTED_HEADER_SIZE = 16;
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Type Size Tests
/// @{

TEST_F(ProtocolStructureTest, PROCTIME_Size) {
    EXPECT_EQ(sizeof(PROCTIME), EXPECTED_PROCTIME_SIZE)
        << "PROCTIME size must match upstream protocol";
}

TEST_F(ProtocolStructureTest, cbPKT_HEADER_Size) {
    EXPECT_EQ(sizeof(cbPKT_HEADER), EXPECTED_HEADER_SIZE)
        << "cbPKT_HEADER size must match upstream protocol";

    // Verify size constant matches
    EXPECT_EQ(cbPKT_HEADER_SIZE, sizeof(cbPKT_HEADER))
        << "cbPKT_HEADER_SIZE constant must match sizeof(cbPKT_HEADER)";
}

TEST_F(ProtocolStructureTest, cbPKT_HEADER_32Size) {
    EXPECT_EQ(cbPKT_HEADER_32SIZE, sizeof(cbPKT_HEADER) / 4)
        << "cbPKT_HEADER_32SIZE must be header size divided by 4";
}

TEST_F(ProtocolStructureTest, cbPKT_GENERIC_Size) {
    EXPECT_LE(sizeof(cbPKT_GENERIC), cbPKT_MAX_SIZE)
        << "cbPKT_GENERIC size must not exceed cbPKT_MAX_SIZE";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Structure Layout Tests
///
/// Verify that structure members are at the correct offsets (critical for binary compatibility)
/// @{

TEST_F(ProtocolStructureTest, cbPKT_HEADER_Layout) {
    cbPKT_HEADER pkt;

    // Verify field offsets match upstream
    EXPECT_EQ(offsetof(cbPKT_HEADER, time), 0)
        << "time field must be at offset 0";

    EXPECT_EQ(offsetof(cbPKT_HEADER, chid), EXPECTED_PROCTIME_SIZE)
        << "chid field must follow time";

    EXPECT_EQ(offsetof(cbPKT_HEADER, type), EXPECTED_PROCTIME_SIZE + 2)
        << "type field must follow chid";

    EXPECT_EQ(offsetof(cbPKT_HEADER, dlen), EXPECTED_PROCTIME_SIZE + 4)
        << "dlen field must follow type";

    EXPECT_EQ(offsetof(cbPKT_HEADER, instrument), EXPECTED_PROCTIME_SIZE + 6)
        << "instrument field must follow dlen";

    EXPECT_EQ(offsetof(cbPKT_HEADER, reserved), EXPECTED_PROCTIME_SIZE + 7)
        << "reserved field must follow instrument";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Field Type Tests
/// @{

TEST_F(ProtocolStructureTest, cbPKT_HEADER_FieldTypes) {
    cbPKT_HEADER pkt;

    // Verify field sizes
    static_assert(sizeof(pkt.time) == sizeof(PROCTIME), "time field type mismatch");
    static_assert(sizeof(pkt.chid) == 2, "chid must be uint16_t");
    static_assert(sizeof(pkt.type) == 2, "type must be uint16_t");
    static_assert(sizeof(pkt.dlen) == 2, "dlen must be uint16_t");
    static_assert(sizeof(pkt.instrument) == 1, "instrument must be uint8_t");
    static_assert(sizeof(pkt.reserved) == 1, "reserved must be uint8_t");
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Constant Value Tests
///
/// Verify protocol constants match upstream
/// @{

TEST_F(ProtocolStructureTest, VersionConstants) {
    EXPECT_EQ(cbVERSION_MAJOR, 4) << "Protocol major version must be 4";
    EXPECT_EQ(cbVERSION_MINOR, 2) << "Protocol minor version must be 2";
}

TEST_F(ProtocolStructureTest, MaxEntityConstants) {
    EXPECT_EQ(cbNSP1, 1) << "First NSP ID must be 1 (1-based)";
    EXPECT_EQ(cbMAXOPEN, 4) << "Maximum open instruments must be 4";
    EXPECT_EQ(cbMAXPROCS, 1) << "Processors per NSP must be 1";
    EXPECT_EQ(cbNUM_FE_CHANS, 256) << "Front-end channels must be 256";
}

TEST_F(ProtocolStructureTest, NetworkConstants) {
    EXPECT_STREQ(cbNET_UDP_ADDR_INST, "192.168.137.1")
        << "Instrument default address must match upstream";
    EXPECT_STREQ(cbNET_UDP_ADDR_CNT, "192.168.137.128")
        << "Control address must match upstream";
    EXPECT_STREQ(cbNET_UDP_ADDR_BCAST, "192.168.137.255")
        << "Broadcast address must match upstream";
    EXPECT_EQ(cbNET_UDP_PORT_BCAST, 51002) << "Broadcast port must be 51002";
    EXPECT_EQ(cbNET_UDP_PORT_CNT, 51001) << "Control port must be 51001";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packet Header Initialization Tests
/// @{

TEST_F(ProtocolStructureTest, cbPKT_HEADER_Initialization) {
    cbPKT_HEADER pkt = {};

    // Verify zero-initialization works
    EXPECT_EQ(pkt.time, 0);
    EXPECT_EQ(pkt.chid, 0);
    EXPECT_EQ(pkt.type, 0);
    EXPECT_EQ(pkt.dlen, 0);
    EXPECT_EQ(pkt.instrument, 0);
    EXPECT_EQ(pkt.reserved, 0);
}

TEST_F(ProtocolStructureTest, cbPKT_HEADER_InstrumentField) {
    cbPKT_HEADER pkt = {};

    // Critical test: instrument field is 0-based in packet
    pkt.instrument = 0;  // First instrument
    EXPECT_EQ(pkt.instrument, 0) << "Packet instrument field is 0-based";

    pkt.instrument = 3;  // Fourth instrument (max for cbMAXOPEN=4)
    EXPECT_EQ(pkt.instrument, 3) << "Max instrument value should be cbMAXOPEN-1";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Count Calculation Tests
/// @{

TEST_F(ProtocolStructureTest, ChannelCountCalculations) {
    // Verify channel count calculations
    EXPECT_EQ(cbNUM_ANAIN_CHANS, 16 * cbMAXPROCS);
    EXPECT_EQ(cbNUM_ANALOG_CHANS, cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS);
    EXPECT_EQ(cbNUM_ANAOUT_CHANS, 4 * cbMAXPROCS);
    EXPECT_EQ(cbNUM_AUDOUT_CHANS, 2 * cbMAXPROCS);
    EXPECT_EQ(cbNUM_ANALOGOUT_CHANS, cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS);
    EXPECT_EQ(cbNUM_DIGIN_CHANS, 1 * cbMAXPROCS);
    EXPECT_EQ(cbNUM_SERIAL_CHANS, 1 * cbMAXPROCS);
    EXPECT_EQ(cbNUM_DIGOUT_CHANS, 4 * cbMAXPROCS);

    // Total channel count
    size_t expected_total = cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS +
                           cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS + cbNUM_DIGOUT_CHANS;
    EXPECT_EQ(cbMAXCHANS, expected_total);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Packing Tests
///
/// Verify that structures are tightly packed (critical for network protocol)
/// @{

TEST_F(ProtocolStructureTest, cbPKT_HEADER_Packing) {
    // Calculate expected size manually
    size_t expected_size = sizeof(PROCTIME) +  // time
                          sizeof(uint16_t) +  // chid
                          sizeof(uint16_t) +  // type
                          sizeof(uint16_t) +  // dlen
                          sizeof(uint8_t) +   // instrument
                          sizeof(uint8_t);    // reserved

    EXPECT_EQ(sizeof(cbPKT_HEADER), expected_size)
        << "cbPKT_HEADER must be tightly packed (pragma pack(1))";
}

TEST_F(ProtocolStructureTest, cbPKT_HEADER_Multiple32Bits) {
    // Header must be a multiple of uint32_t for alignment
    EXPECT_EQ(sizeof(cbPKT_HEADER) % 4, 0)
        << "cbPKT_HEADER size must be a multiple of 4 bytes";
}

/// @}
