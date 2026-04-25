///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_cmp_parser.cpp
/// @brief  Unit tests for CMP (channel mapping) file parser
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

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

    const auto& entries = result.value();
    EXPECT_EQ(entries.size(), 8u);

    // Defaults: start_chan=1, hs_id=0 → chans 1..8, no label prefix.
    std::set<uint32_t> chans;
    for (const auto& [chan_id, _] : entries) chans.insert(chan_id);
    EXPECT_EQ(chans, std::set<uint32_t>({1, 2, 3, 4, 5, 6, 7, 8}));

    // hs_id=0 leaves labels un-prefixed.
    for (const auto& [_, entry] : entries) {
        EXPECT_NE(entry.label.substr(0, 2), "hs");
    }
}

TEST(CmpParser, HsIdZeroLeavesLabelsUnprefixed) {
    // Same file, default hs_id=0 vs explicit hs_id=3, comparing stripped labels.
    auto bare = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"));
    auto with_hs = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"), 1, 3);
    ASSERT_TRUE(bare.isOk());
    ASSERT_TRUE(with_hs.isOk());

    for (const auto& [chan_id, prefixed] : with_hs.value()) {
        const auto it = bare.value().find(chan_id);
        ASSERT_NE(it, bare.value().end());
        EXPECT_EQ(prefixed.label, "hs3-" + it->second.label);
    }
}

TEST(CmpParser, Parse128Channel) {
    auto result = cbsdk::parseCmpFile(testFile("128ChannelDefaultMapping.cmp"));
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    EXPECT_EQ(entries.size(), 128u);

    // 128 contiguous channel IDs starting at 1.
    for (uint32_t ch = 1; ch <= 128; ++ch) {
        ASSERT_TRUE(entries.count(ch)) << "missing chan " << ch;
    }

    // Bank layout after sort: chan 1 → (bank 1, elec 1), chan 33 → (bank 2, elec 1),
    // chan 65 → (bank 3, elec 1), chan 128 → (bank 4, elec 32).
    EXPECT_EQ(entries.at(1).position[2], 1);
    EXPECT_EQ(entries.at(1).position[3], 1);
    EXPECT_EQ(entries.at(33).position[2], 2);
    EXPECT_EQ(entries.at(33).position[3], 1);
    EXPECT_EQ(entries.at(128).position[2], 4);
    EXPECT_EQ(entries.at(128).position[3], 32);
}

TEST(CmpParser, StartChanOffsetsChanIds) {
    // Second headstage: same 96-channel CMP mapped to chans 129..224.
    auto result = cbsdk::parseCmpFile(
        testFile("96ChannelDefaultMapping.cmp"), /*start_chan=*/129, /*hs_id=*/2);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    EXPECT_EQ(entries.size(), 96u);

    std::set<uint32_t> chans;
    for (const auto& [chan_id, _] : entries) chans.insert(chan_id);
    EXPECT_EQ(*chans.begin(), 129u);
    EXPECT_EQ(*chans.rbegin(), 129u + 95u);

    for (const auto& [_, entry] : entries) {
        EXPECT_EQ(entry.label.substr(0, 4), "hs2-");
    }
}

TEST(CmpParser, SortsOutOfOrderRows) {
    // Synthesize a tiny CMP whose rows are deliberately out of (bank, elec) order.
    auto path = (std::filesystem::temp_directory_path() / "cmp_unsorted_tmp.cmp").string();
    {
        std::ofstream out(path);
        out << "// scratch file\n"
            << "unsorted test map\n"
            // Rows given in reverse electrode order within bank A, then bank B first.
            << "0 0 B 1 label_b1\n"
            << "0 0 A 3 label_a3\n"
            << "0 0 A 1 label_a1\n"
            << "0 0 A 2 label_a2\n";
    }

    // Use an explicit hs_id so we exercise the prefix path here.
    auto result = cbsdk::parseCmpFile(path, /*start_chan=*/1, /*hs_id=*/1);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    ASSERT_EQ(entries.size(), 4u);

    // Sort puts (A,1), (A,2), (A,3), (B,1) at chans 1..4.
    EXPECT_EQ(entries.at(1).label, "hs1-label_a1");
    EXPECT_EQ(entries.at(2).label, "hs1-label_a2");
    EXPECT_EQ(entries.at(3).label, "hs1-label_a3");
    EXPECT_EQ(entries.at(4).label, "hs1-label_b1");

    std::filesystem::remove(path);
}

TEST(CmpParser, NonexistentFile) {
    auto result = cbsdk::parseCmpFile("/nonexistent/path.cmp");
    EXPECT_TRUE(result.isError());
}
