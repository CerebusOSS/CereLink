///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_cmp_parser.cpp
/// @brief  Unit tests for CMP (channel mapping) file parser
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "cmp_parser.h"

#ifndef CMP_TEST_DATA_DIR
#define CMP_TEST_DATA_DIR "."
#endif

static std::string testFile(const char* name) {
    return std::string(CMP_TEST_DATA_DIR) + "/" + name;
}

TEST(CmpParser, Parse8Channel) {
    auto result = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"));
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& positions = result.value();
    // 8 channels: bank A, electrodes 1-8
    EXPECT_EQ(positions.size(), 8u);

    // First entry: 0 1 A 1 chan1 → bank=1(A), term=0(electrode 1-1)
    auto it = positions.find(cbsdk::cmpKey(1, 0));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 0);  // col
    EXPECT_EQ(it->second[1], 1);  // row
    EXPECT_EQ(it->second[2], 1);  // bank_letter (A=1)
    EXPECT_EQ(it->second[3], 1);  // electrode

    // Last entry: 3 0 A 8 chan8 → bank=1(A), term=7(electrode 8-1)
    it = positions.find(cbsdk::cmpKey(1, 7));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 3);  // col
    EXPECT_EQ(it->second[1], 0);  // row
    EXPECT_EQ(it->second[2], 1);  // bank_letter (A=1)
    EXPECT_EQ(it->second[3], 8);  // electrode
}

TEST(CmpParser, Parse128Channel) {
    auto result = cbsdk::parseCmpFile(testFile("128ChannelDefaultMapping.cmp"));
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& positions = result.value();
    EXPECT_EQ(positions.size(), 128u);

    // First entry: 0 7 A 1 → bank=1, term=0
    auto it = positions.find(cbsdk::cmpKey(1, 0));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 0);   // col
    EXPECT_EQ(it->second[1], 7);   // row

    // Bank B entry: 0 5 B 1 chan33 → bank=2, term=0
    it = positions.find(cbsdk::cmpKey(2, 0));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 0);   // col
    EXPECT_EQ(it->second[1], 5);   // row
    EXPECT_EQ(it->second[2], 2);   // bank_letter (B=2)

    // Bank D, last electrode: 15 0 D 32 chan128 → bank=4, term=31
    it = positions.find(cbsdk::cmpKey(4, 31));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 15);  // col
    EXPECT_EQ(it->second[1], 0);   // row
    EXPECT_EQ(it->second[2], 4);   // bank_letter (D=4)
    EXPECT_EQ(it->second[3], 32);  // electrode
}

TEST(CmpParser, Parse96Channel) {
    auto result = cbsdk::parseCmpFile(testFile("96ChannelDefaultMapping.cmp"));
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& positions = result.value();
    EXPECT_EQ(positions.size(), 96u);

    // 96-channel has banks A, B, C (3 banks × 32 electrodes)
    // Verify bank C exists
    auto it = positions.find(cbsdk::cmpKey(3, 0));
    ASSERT_NE(it, positions.end());

    // Verify bank D does NOT exist (only 96 channels)
    it = positions.find(cbsdk::cmpKey(4, 0));
    EXPECT_EQ(it, positions.end());
}

TEST(CmpParser, BankOffset) {
    // Load 8-channel CMP with bank_offset=4 (simulating port 2)
    auto result = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"), 4);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& positions = result.value();
    EXPECT_EQ(positions.size(), 8u);

    // Bank A with offset 4 → absolute bank 5
    auto it = positions.find(cbsdk::cmpKey(5, 0));
    ASSERT_NE(it, positions.end());
    EXPECT_EQ(it->second[0], 0);  // col
    EXPECT_EQ(it->second[1], 1);  // row
    // position[2] stores the original bank letter index (A=1), not the offset bank
    EXPECT_EQ(it->second[2], 1);

    // Original bank 1 should NOT have entries
    it = positions.find(cbsdk::cmpKey(1, 0));
    EXPECT_EQ(it, positions.end());
}

TEST(CmpParser, NonexistentFile) {
    auto result = cbsdk::parseCmpFile("/nonexistent/path.cmp");
    EXPECT_TRUE(result.isError());
}

TEST(CmpParser, CmpKeyUniqueness) {
    // Verify different (bank, term) pairs produce different keys
    EXPECT_NE(cbsdk::cmpKey(1, 0), cbsdk::cmpKey(2, 0));
    EXPECT_NE(cbsdk::cmpKey(1, 0), cbsdk::cmpKey(1, 1));
    EXPECT_EQ(cbsdk::cmpKey(3, 15), cbsdk::cmpKey(3, 15));
}
