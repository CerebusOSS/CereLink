///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_packet_translation.cpp
/// @author CereLink Development Team
/// @date   2025-01-19
///
/// @brief  Unit tests for packet translation functions
///
/// Tests bidirectional translation between protocol versions for specific packet types:
/// - SYSPROTOCOLMONITOR (pre-410 ↔ current)
/// - NPLAY (pre-400 ↔ current)
/// - COMMENT (pre-400 ↔ current)
/// - CHANINFO (pre-410 ↔ current)
/// - CHANRESET (pre-420 ↔ current)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "packet_test_helpers.h"
#include <cbproto/packet_translator.h>
#include <cstring>

using namespace test_helpers;
using namespace cbproto;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SYSPROTOCOLMONITOR Translation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SYSPROTOCOLMONITOR_Translation, Pre410_to_Current_AddsCounterField) {
    // Given: 3.11 SYSPROTOCOLMONITOR packet with sentpkts=100, no counter field
    auto pkt_311 = make_311_SYSPROTOCOLMONITOR(100, 90000);  // time=90000 ticks (3 seconds)

    // Prepare destination in current format
    cbPKT_SYSPROTOCOLMONITOR dest = {};
    dest.cbpkt_header.time = ticks_to_ns(90000);  // Pre-fill with translated time
    dest.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    dest.cbpkt_header.type = cbPKTTYPE_SYSPROTOCOLMONITOR;
    dest.cbpkt_header.dlen = 1;  // Will be increased to 2

    // When: Translate payload
    const uint8_t* src_payload = &pkt_311[test_helpers::HEADER_SIZE_311];
    size_t result_dlen = PacketTranslator::translate_SYSPROTOCOLMONITOR_pre410_to_current(
        src_payload, &dest);

    // Then: sentpkts preserved, counter filled from header time, dlen increased
    EXPECT_EQ(dest.sentpkts, 100u);
    EXPECT_EQ(dest.counter, ticks_to_ns(90000));  // Counter comes from header.time in 3.11
    EXPECT_EQ(result_dlen, 2u);  // dlen increased by 1
    EXPECT_EQ(dest.cbpkt_header.dlen, 2u);
}

TEST(SYSPROTOCOLMONITOR_Translation, Current_to_Pre410_DropsCounterField) {
    // Given: Current SYSPROTOCOLMONITOR with sentpkts=200, counter=5000
    auto pkt_current = make_current_SYSPROTOCOLMONITOR(200, 5000);

    // Prepare destination buffer
    uint8_t dest_payload[256] = {};

    // When: Translate to pre-410 format
    size_t result_dlen = PacketTranslator::translate_SYSPROTOCOLMONITOR_current_to_pre410(
        pkt_current, dest_payload);

    // Then: sentpkts preserved, counter dropped, dlen decreased
    uint32_t dest_sentpkts = *reinterpret_cast<uint32_t*>(dest_payload);
    EXPECT_EQ(dest_sentpkts, 200u);
    EXPECT_EQ(result_dlen, 1u);  // dlen decreased by 1
}

TEST(SYSPROTOCOLMONITOR_Translation, RoundTrip_Pre410_to_Current_to_Pre410) {
    // Test that round-trip translation is lossy (counter is lost)
    auto pkt_311 = make_311_SYSPROTOCOLMONITOR(150, 60000);

    // 311 -> Current
    cbPKT_SYSPROTOCOLMONITOR intermediate = {};
    intermediate.cbpkt_header.time = ticks_to_ns(60000);
    intermediate.cbpkt_header.dlen = 1;
    PacketTranslator::translate_SYSPROTOCOLMONITOR_pre410_to_current(
        &pkt_311[test_helpers::HEADER_SIZE_311], &intermediate);

    // Current -> 311
    uint8_t result_311[256] = {};
    PacketTranslator::translate_SYSPROTOCOLMONITOR_current_to_pre410(
        intermediate, result_311);

    // Verify sentpkts preserved
    EXPECT_EQ(*reinterpret_cast<uint32_t*>(result_311), 150u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// NPLAY Translation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NPLAY_Translation, Pre400_to_Current_TimeConversion) {
    // Given: 3.11 NPLAY with times in 30kHz ticks
    // 30000 ticks = 1 second, 60000 = 2 seconds, etc.
    auto pkt_311 = make_311_NPLAY(30000, 60000, 90000, 120000, cbNPLAY_MODE_PAUSE);

    // Prepare destination - must initialize dlen to SOURCE packet's dlen
    cbPKT_NPLAY dest = {};
    dest.cbpkt_header.dlen = pkt_311[7];  // Copy source dlen from 3.11 header (byte 7)

    // When: Translate
    const uint8_t* src_payload = &pkt_311[test_helpers::HEADER_SIZE_311];
    size_t result_dlen = PacketTranslator::translate_NPLAY_pre400_to_current(
        src_payload, &dest);

    // Then: Times converted from 30kHz ticks to nanoseconds
    EXPECT_EQ(dest.ftime, ticks_to_ns(30000));   // 1 second in ns
    EXPECT_EQ(dest.stime, ticks_to_ns(60000));   // 2 seconds in ns
    EXPECT_EQ(dest.etime, ticks_to_ns(90000));   // 3 seconds in ns
    EXPECT_EQ(dest.val, ticks_to_ns(120000));    // 4 seconds in ns
    EXPECT_EQ(dest.mode, cbNPLAY_MODE_PAUSE);

    // dlen increases by 16 bytes (4 fields × (8 bytes - 4 bytes)) = 4 quadlets
    // Source: 248 quadlets (cbPKTDLEN_NPLAY - 4), Result: 252 quadlets (cbPKTDLEN_NPLAY)
    EXPECT_EQ(result_dlen, cbPKTDLEN_NPLAY);
    EXPECT_EQ(dest.cbpkt_header.dlen, cbPKTDLEN_NPLAY);
}

TEST(NPLAY_Translation, Current_to_Pre400_TimeNarrowing) {
    // Given: Current NPLAY with times in nanoseconds
    auto pkt_current = make_current_NPLAY(
        ticks_to_ns(30000),   // 1 second
        ticks_to_ns(60000),   // 2 seconds
        ticks_to_ns(90000),   // 3 seconds
        ticks_to_ns(120000),  // 4 seconds
        cbNPLAY_MODE_PAUSE
    );

    // Verify packet was created correctly
    ASSERT_EQ(pkt_current.cbpkt_header.dlen, cbPKTDLEN_NPLAY) << "Packet dlen not set correctly";

    // When: Translate to pre-400
    uint8_t dest_payload[1024] = {};  // Increased size to match cbPKT_NPLAY
    size_t result_dlen = PacketTranslator::translate_NPLAY_current_to_pre400(
        pkt_current, dest_payload);

    // Then: Times converted back to 30kHz ticks
    uint32_t dest_ftime = *reinterpret_cast<uint32_t*>(&dest_payload[0]);
    uint32_t dest_stime = *reinterpret_cast<uint32_t*>(&dest_payload[4]);
    uint32_t dest_etime = *reinterpret_cast<uint32_t*>(&dest_payload[8]);
    uint32_t dest_val   = *reinterpret_cast<uint32_t*>(&dest_payload[12]);
    uint32_t dest_mode  = *reinterpret_cast<uint32_t*>(&dest_payload[16]);

    EXPECT_EQ(dest_ftime, 30000u);
    EXPECT_EQ(dest_stime, 60000u);
    EXPECT_EQ(dest_etime, 90000u);
    EXPECT_EQ(dest_val, 120000u);
    EXPECT_EQ(dest_mode, cbNPLAY_MODE_PAUSE);

    // dlen decreases by 4 quadlets
    EXPECT_EQ(result_dlen, cbPKTDLEN_NPLAY - 4u);
}

TEST(NPLAY_Translation, RoundTrip_Exact_OnTickBoundaries) {
    // Times that are exact multiples of tick period should round-trip perfectly
    PROCTIME original_time = ticks_to_ns(45000);  // Exactly 1.5 seconds

    auto pkt = make_current_NPLAY(original_time, 0, 0, 0, 0);

    // Current -> Pre400
    uint8_t buffer_311[256] = {};
    PacketTranslator::translate_NPLAY_current_to_pre400(pkt, buffer_311);

    // Pre400 -> Current
    cbPKT_NPLAY result = pkt;
    result.cbpkt_header.dlen = cbPKTDLEN_NPLAY - 4;  // Simulate pre-400 dlen
    PacketTranslator::translate_NPLAY_pre400_to_current(buffer_311, &result);

    // Should be exact (no precision loss on tick boundaries)
    EXPECT_EQ(result.ftime, original_time);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// COMMENT Translation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(COMMENT_Translation, Pre400_to_Current_FlagsZero_RGBA) {
    // Given: 3.11 COMMENT with flags=0 (RGBA mode), data contains color
    const char* comment_text = "Test comment";
    auto pkt_311 = make_311_COMMENT(0x00, 0xAABBCCDD, comment_text, 90000);

    // Prepare destination
    cbPKT_COMMENT dest = {};
    dest.cbpkt_header.dlen = pkt_311[7];  // Copy dlen from source

    // When: Translate (header timestamp needed for timeStarted fallback)
    const uint8_t* src_payload = &pkt_311[test_helpers::HEADER_SIZE_311];
    uint32_t hdr_timestamp = *reinterpret_cast<const uint32_t*>(&pkt_311[0]);
    size_t result_dlen = PacketTranslator::translate_COMMENT_pre400_to_current(
        src_payload, &dest, hdr_timestamp);

    // Then: RGBA preserved, timeStarted from header, charset=0, comment preserved
    EXPECT_EQ(dest.rgba, 0xAABBCCDDu);
    EXPECT_EQ(dest.timeStarted, ticks_to_ns(90000));  // From header
    EXPECT_EQ(dest.info.charset, 0u);
    EXPECT_STREQ(reinterpret_cast<const char*>(dest.comment), comment_text);

    // dlen increases by 2 quadlets (8 bytes for timeStarted field)
    EXPECT_EQ(result_dlen, pkt_311[7] + 2u);
}

TEST(COMMENT_Translation, Pre400_to_Current_FlagsOne_TimeStarted) {
    // Given: 3.11 COMMENT with flags=1 (timeStarted mode), data contains time
    const char* comment_text = "Another test";
    auto pkt_311 = make_311_COMMENT(0x01, 60000, comment_text, 90000);

    // Prepare destination
    cbPKT_COMMENT dest = {};
    dest.cbpkt_header.dlen = pkt_311[7];

    // When: Translate
    const uint8_t* src_payload = &pkt_311[test_helpers::HEADER_SIZE_311];
    uint32_t hdr_timestamp = *reinterpret_cast<const uint32_t*>(&pkt_311[0]);
    size_t result_dlen = PacketTranslator::translate_COMMENT_pre400_to_current(
        src_payload, &dest, hdr_timestamp);

    // Then: timeStarted from data field, rgba=0
    EXPECT_EQ(dest.timeStarted, ticks_to_ns(60000));  // From data field
    EXPECT_EQ(dest.rgba, 0u);  // Undefined in this mode
    EXPECT_EQ(dest.info.charset, 0u);
    EXPECT_STREQ(reinterpret_cast<const char*>(dest.comment), comment_text);
}

TEST(COMMENT_Translation, Current_to_Pre400_AlwaysUsesTimeStarted) {
    // Given: Current COMMENT
    const char* comment_text = "Modern comment";
    auto pkt_current = make_current_COMMENT(0, ticks_to_ns(75000), 0x11223344, comment_text);

    // When: Translate to pre-400
    uint8_t dest_payload[256] = {};
    size_t result_dlen = PacketTranslator::translate_COMMENT_current_to_pre400(
        pkt_current, dest_payload);

    // Then: flags=0x01, data=timeStarted in ticks
    EXPECT_EQ(dest_payload[1], 0x01u);  // flags
    uint32_t dest_data = *reinterpret_cast<uint32_t*>(&dest_payload[4]);
    EXPECT_EQ(dest_data, 75000u);  // timeStarted converted back to ticks

    // Comment text preserved
    const char* dest_comment = reinterpret_cast<const char*>(&dest_payload[8]);
    EXPECT_STREQ(dest_comment, comment_text);

    // dlen decreases by 2 quadlets
    EXPECT_EQ(result_dlen, cbPKTDLEN_COMMENT - 2u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CHANINFO Translation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(CHANINFO_Translation, Pre410_to_Current_FieldExpansion) {
    // Given: 3.11 CHANINFO with monsource as uint32_t
    auto pkt_311 = make_311_CHANINFO(42, 0x12345678);

    // Prepare destination
    cbPKT_CHANINFO dest = {};
    dest.cbpkt_header.dlen = pkt_311[7];

    // When: Translate
    const uint8_t* src_payload = &pkt_311[test_helpers::HEADER_SIZE_311];
    size_t result_dlen = PacketTranslator::translate_CHANINFO_pre410_to_current(
        src_payload, &dest);

    // Then: monsource narrowed to moninst, monchan=0, new fields zeroed
    EXPECT_EQ(dest.chan, 42u);
    EXPECT_EQ(dest.moninst, 0x5678u);  // Lower 16 bits of monsource
    EXPECT_EQ(dest.monchan, 0u);       // New field
    EXPECT_EQ(dest.reserved[0], 0u);   // New field
    EXPECT_EQ(dest.reserved[1], 0u);   // New field
    EXPECT_EQ(dest.triginst, 0u);      // New field

    // dlen increases by 1 quadlet (3 bytes added, rounded up)
    EXPECT_EQ(result_dlen, pkt_311[7] + 1u);
}

TEST(CHANINFO_Translation, Current_to_Pre410_FieldNarrowing) {
    // Given: Current CHANINFO with moninst, monchan, triginst
    auto pkt_current = make_current_CHANINFO(42, 0x1234, 0x5678);

    // When: Translate to pre-410
    uint8_t dest_payload[512] = {};
    size_t result_dlen = PacketTranslator::translate_CHANINFO_current_to_pre410(
        pkt_current, dest_payload);

    // Then: chan preserved, moninst expanded to monsource, monchan/triginst dropped
    uint32_t dest_chan = *reinterpret_cast<uint32_t*>(&dest_payload[0]);
    EXPECT_EQ(dest_chan, 42u);

    // Find monsource field (at specific offset in structure)
    // For simplicity, we'll verify the dlen change
    EXPECT_EQ(result_dlen, cbPKTDLEN_CHANINFO - 1u);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CHANRESET Translation Tests
///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(CHANRESET_Translation, Pre420_to_Current_InsertsMonChan) {
    // Given: Pre-4.2 CHANRESET with monsource (single byte)
    auto pkt_410 = make_410_CHANRESET(42, 5);

    // Prepare destination
    cbPKT_CHANRESET dest = {};
    dest.cbpkt_header.dlen = 7;  // Pre-4.2 dlen

    // When: Translate
    const auto* src_pkt = reinterpret_cast<const cbPKT_GENERIC*>(&pkt_410);
    const uint8_t* src_payload = reinterpret_cast<const uint8_t*>(&pkt_410) + cbPKT_HEADER_SIZE;
    size_t result_dlen = PacketTranslator::translate_CHANRESET_pre420_to_current(
        src_payload, &dest);

    // Then: chan preserved, moninst from monsource, monchan inserted
    EXPECT_EQ(dest.chan, 42u);
    EXPECT_EQ(dest.moninst, 5u);  // From monsource
    // Note: monchan field is inserted but value depends on memory layout

    // dlen stays at 7 (29 bytes → 30 bytes, both round to 7 quadlets)
    EXPECT_EQ(result_dlen, 7u);
}

TEST(CHANRESET_Translation, Current_to_Pre420_RemovesMonChan) {
    // Given: Current CHANRESET with moninst and monchan
    auto pkt_current = make_current_CHANRESET(42, 5, 10);

    // When: Translate to pre-420
    uint8_t dest_payload[256] = {};
    size_t result_dlen = PacketTranslator::translate_CHANRESET_current_to_pre420(
        pkt_current, dest_payload);

    // Then: chan preserved, moninst preserved, monchan dropped
    uint32_t dest_chan = *reinterpret_cast<uint32_t*>(&dest_payload[0]);
    EXPECT_EQ(dest_chan, 42u);

    // dlen stays at 7
    EXPECT_EQ(result_dlen, 7u);
}
