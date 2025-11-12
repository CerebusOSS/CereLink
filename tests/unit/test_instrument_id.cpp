///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_instrument_id.cpp
/// @brief  Unit tests for cbproto::InstrumentId type
///
/// Tests the critical 0-based/1-based conversion logic that fixes the indexing bug.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbproto/cbproto.h>

using cbproto::InstrumentId;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for InstrumentId tests
///////////////////////////////////////////////////////////////////////////////////////////////////
class InstrumentIdTest : public ::testing::Test {
protected:
    // Note: cbNSP1, cbMAXOPEN, etc. are defined in cbproto/types.h
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Construction Tests
/// @{

TEST_F(InstrumentIdTest, FromOneBased_FirstInstrument) {
    InstrumentId id = InstrumentId::fromOneBased(cbNSP1);

    EXPECT_EQ(id.toOneBased(), 1);
    EXPECT_EQ(id.toIndex(), 0);
    EXPECT_EQ(id.toPacketField(), 0);
    EXPECT_TRUE(id.isValid());
}

TEST_F(InstrumentIdTest, FromOneBased_SecondInstrument) {
    InstrumentId id = InstrumentId::fromOneBased(cbNSP2);

    EXPECT_EQ(id.toOneBased(), 2);
    EXPECT_EQ(id.toIndex(), 1);
    EXPECT_EQ(id.toPacketField(), 1);
    EXPECT_TRUE(id.isValid());
}

TEST_F(InstrumentIdTest, FromIndex_ZeroIndex) {
    InstrumentId id = InstrumentId::fromIndex(0);

    EXPECT_EQ(id.toIndex(), 0);
    EXPECT_EQ(id.toOneBased(), 1);
    EXPECT_EQ(id.toPacketField(), 0);
    EXPECT_TRUE(id.isValid());
}

TEST_F(InstrumentIdTest, FromIndex_MaxIndex) {
    InstrumentId id = InstrumentId::fromIndex(cbMAXOPEN - 1);  // Index 3 for cbMAXOPEN=4

    EXPECT_EQ(id.toIndex(), 3);
    EXPECT_EQ(id.toOneBased(), 4);
    EXPECT_EQ(id.toPacketField(), 3);
    EXPECT_TRUE(id.isValid());
}

TEST_F(InstrumentIdTest, FromPacketField_ZeroValue) {
    // Packet header instrument field is 0-based
    InstrumentId id = InstrumentId::fromPacketField(0);

    EXPECT_EQ(id.toPacketField(), 0);
    EXPECT_EQ(id.toOneBased(), 1);
    EXPECT_EQ(id.toIndex(), 0);
    EXPECT_TRUE(id.isValid());
}

TEST_F(InstrumentIdTest, FromPacketField_MaxValue) {
    // Packet header instrument field is 0-based
    InstrumentId id = InstrumentId::fromPacketField(cbMAXOPEN - 1);  // 3 for cbMAXOPEN=4

    EXPECT_EQ(id.toPacketField(), 3);
    EXPECT_EQ(id.toOneBased(), 4);
    EXPECT_EQ(id.toIndex(), 3);
    EXPECT_TRUE(id.isValid());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Validation Tests
/// @{

TEST_F(InstrumentIdTest, Validation_ValidRange) {
    for (uint8_t i = 1; i <= cbMAXOPEN; ++i) {
        InstrumentId id = InstrumentId::fromOneBased(i);
        EXPECT_TRUE(id.isValid()) << "ID " << (int)i << " should be valid";
    }
}

TEST_F(InstrumentIdTest, Validation_InvalidZero) {
    InstrumentId id = InstrumentId::fromOneBased(0);
    EXPECT_FALSE(id.isValid());
}

TEST_F(InstrumentIdTest, Validation_InvalidTooHigh) {
    InstrumentId id = InstrumentId::fromOneBased(cbMAXOPEN + 1);
    EXPECT_FALSE(id.isValid());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Round-Trip Conversion Tests
///
/// These tests ensure that conversions are symmetric and don't lose information
/// @{

TEST_F(InstrumentIdTest, RoundTrip_OneBasedToIndexToOneBased) {
    for (uint8_t oneBased = 1; oneBased <= cbMAXOPEN; ++oneBased) {
        InstrumentId id1 = InstrumentId::fromOneBased(oneBased);
        uint8_t idx = id1.toIndex();
        InstrumentId id2 = InstrumentId::fromIndex(idx);

        EXPECT_EQ(id1.toOneBased(), id2.toOneBased())
            << "Round-trip failed for 1-based ID " << (int)oneBased;
    }
}

TEST_F(InstrumentIdTest, RoundTrip_PacketFieldToOneBasedToPacketField) {
    for (uint8_t pktField = 0; pktField < cbMAXOPEN; ++pktField) {
        InstrumentId id1 = InstrumentId::fromPacketField(pktField);
        uint8_t oneBased = id1.toOneBased();
        InstrumentId id2 = InstrumentId::fromOneBased(oneBased);

        EXPECT_EQ(id1.toPacketField(), id2.toPacketField())
            << "Round-trip failed for packet field " << (int)pktField;
    }
}

TEST_F(InstrumentIdTest, RoundTrip_IndexToPacketFieldToIndex) {
    for (uint8_t idx = 0; idx < cbMAXOPEN; ++idx) {
        InstrumentId id1 = InstrumentId::fromIndex(idx);
        uint8_t pktField = id1.toPacketField();
        InstrumentId id2 = InstrumentId::fromPacketField(pktField);

        EXPECT_EQ(id1.toIndex(), id2.toIndex())
            << "Round-trip failed for index " << (int)idx;
    }
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Comparison Operator Tests
/// @{

TEST_F(InstrumentIdTest, Equality_SameValue) {
    InstrumentId id1 = InstrumentId::fromOneBased(cbNSP1);
    InstrumentId id2 = InstrumentId::fromOneBased(cbNSP1);

    EXPECT_EQ(id1, id2);
    EXPECT_FALSE(id1 != id2);
}

TEST_F(InstrumentIdTest, Equality_DifferentValue) {
    InstrumentId id1 = InstrumentId::fromOneBased(cbNSP1);
    InstrumentId id2 = InstrumentId::fromOneBased(cbNSP2);

    EXPECT_NE(id1, id2);
    EXPECT_FALSE(id1 == id2);
}

TEST_F(InstrumentIdTest, Comparison_LessThan) {
    InstrumentId id1 = InstrumentId::fromOneBased(1);
    InstrumentId id2 = InstrumentId::fromOneBased(2);

    EXPECT_LT(id1, id2);
    EXPECT_LE(id1, id2);
    EXPECT_FALSE(id1 > id2);
    EXPECT_FALSE(id1 >= id2);
}

TEST_F(InstrumentIdTest, Comparison_GreaterThan) {
    InstrumentId id1 = InstrumentId::fromOneBased(2);
    InstrumentId id2 = InstrumentId::fromOneBased(1);

    EXPECT_GT(id1, id2);
    EXPECT_GE(id1, id2);
    EXPECT_FALSE(id1 < id2);
    EXPECT_FALSE(id1 <= id2);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Bug Fix Verification Tests
///
/// These tests specifically verify that the InstrumentId type fixes the indexing bug
/// described in upstream/scratch_1.md lines 504-551
/// @{

TEST_F(InstrumentIdTest, BugFix_StandaloneModeMapping) {
    // In standalone mode, the first instrument should map to index 0
    // This is what InstNetwork.cpp does: stores at [0]

    InstrumentId id = InstrumentId::fromOneBased(cbNSP1);
    EXPECT_EQ(id.toIndex(), 0) << "Standalone mode: first instrument should map to index 0";
}

TEST_F(InstrumentIdTest, BugFix_ClientModeMapping) {
    // In client mode, Central uses the packet instrument field directly as index
    // Packet field 0 -> index 0, packet field 1 -> index 1, etc.

    for (uint8_t pktField = 0; pktField < cbMAXOPEN; ++pktField) {
        InstrumentId id = InstrumentId::fromPacketField(pktField);
        EXPECT_EQ(id.toIndex(), pktField)
            << "Client mode: packet field " << (int)pktField
            << " should map to index " << (int)pktField;
    }
}

TEST_F(InstrumentIdTest, BugFix_APIToArrayIndexConversion) {
    // The bug: cbGetProcInfo(cbNSP1, ...) expects data at procinfo[0] in standalone
    // but Central stores at procinfo[pkt.instrument] where pkt.instrument can be 0, 1, 2, 3

    InstrumentId id = InstrumentId::fromOneBased(cbNSP1);  // API uses 1-based
    uint8_t arrayIndex = id.toIndex();  // Convert to array index

    EXPECT_EQ(arrayIndex, 0)
        << "cbNSP1 (1-based) must convert to array index 0 for standalone mode";
}

TEST_F(InstrumentIdTest, BugFix_PacketToArrayIndexConversion) {
    // Central receives packet with instrument field = 0 (0-based)
    // It should store at index 0

    cbPKT_HEADER pkt;
    pkt.instrument = 0;  // Packet field is 0-based

    InstrumentId id = InstrumentId::fromPacketField(pkt.instrument);
    uint8_t arrayIndex = id.toIndex();

    EXPECT_EQ(arrayIndex, 0)
        << "Packet instrument=0 should map to array index 0";
}

/// @}
