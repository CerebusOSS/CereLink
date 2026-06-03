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

    // 8ch file is bank A, electrodes 1..8. Default start_chan=1 → bank offset 0,
    // so entries are keyed by (bank 1, term 1..8).
    for (int32_t term = 1; term <= 8; ++term) {
        auto it = entries.find(cbsdk::cmpKey(1, term));
        ASSERT_NE(it, entries.end()) << "missing (bank 1, term " << term << ")";
        EXPECT_EQ(it->second.bank, 1);
        EXPECT_EQ(it->second.term, term);
    }

    // No size column + unit-spaced index grid → scaled by the 400 µm pitch.
    // Row "0 1 A 1" → x=0, y=400; row "0 0 A 5" → x=0, y=0.
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).x, 0);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).y, 400);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 5)).x, 0);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 5)).y, 0);

    // Default index grid → size is 1 electrode-pitch (400 µm); hs_id=0 → headstage 0.
    for (const auto& [_, entry] : entries) {
        EXPECT_EQ(entry.size, 400);
        EXPECT_EQ(entry.headstage, 0);
    }

    // hs_id=0 leaves labels un-prefixed.
    for (const auto& [_, entry] : entries) {
        EXPECT_NE(entry.label.substr(0, 2), "hs");
    }
}

TEST(CmpParser, HeaderDeclaredSizeColumnTakesFaceValue) {
    // A header naming a size column → columns parsed by name; with size present
    // the col/row/size values are taken at face value (no µm scaling).
    auto path = (std::filesystem::temp_directory_path() / "cmp_size_tmp.cmp").string();
    {
        std::ofstream out(path);
        out << "// scratch file\n"
            << "size column test map\n"
            << "//col row bank elec size label\n"
            << "0 0 A 1 4 elec_a1\n"   // size=4, label=elec_a1
            << "1 0 A 2 7 elec_a2\n"   // size=7, label=elec_a2
            << "2 0 A 3 0 elec_a3\n"   // size=0, label=elec_a3
            << "3 0 A 4 9\n";          // size=9, no label
    }

    auto result = cbsdk::parseCmpFile(path, /*start_chan=*/1, /*hs_id=*/5);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    ASSERT_EQ(entries.size(), 4u);

    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).size, 4);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).label, "elec_a1");
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 2)).size, 7);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 2)).label, "elec_a2");
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 3)).size, 0);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 3)).label, "elec_a3");

    // Size present, no label → empty label.
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 4)).size, 9);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 4)).label, "");

    // Face value: x is the raw col (no ×400) because a size column was supplied.
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 4)).x, 3);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 4)).y, 0);

    // hs_id seeds the headstage field (→ position[3]).
    for (const auto& [_, entry] : entries) {
        EXPECT_EQ(entry.headstage, 5);
    }

    std::filesystem::remove(path);
}

TEST(CmpParser, HeaderDrivenColumnOrder) {
    // The header determines column order — here columns are reordered and a
    // size column is present, so values are taken at face value.
    auto path = (std::filesystem::temp_directory_path() / "cmp_reorder_tmp.cmp").string();
    {
        std::ofstream out(path);
        out << "// scratch file\n"
            << "reordered columns map\n"
            << "//label bank elec col row size\n"
            << "foo A 1 7 9 2\n";
    }

    auto result = cbsdk::parseCmpFile(path);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    ASSERT_EQ(entries.size(), 1u);
    const auto& e = entries.at(cbsdk::cmpKey(1, 1));
    EXPECT_EQ(e.label, "foo");
    EXPECT_EQ(e.bank, 1);
    EXPECT_EQ(e.term, 1);
    EXPECT_EQ(e.x, 7);
    EXPECT_EQ(e.y, 9);
    EXPECT_EQ(e.size, 2);
}

TEST(CmpParser, NonUniformGridTakesFaceValue) {
    // No size column, but the row spacing is non-uniform (delta 4, not 1), so
    // it is not a default index grid → values taken at face value, size 0.
    auto path = (std::filesystem::temp_directory_path() / "cmp_nonuniform_tmp.cmp").string();
    {
        std::ofstream out(path);
        out << "// scratch file\n"
            << "non-uniform grid map\n"
            << "//col row bank elec label\n"
            << "0 0 A 1 e1\n"
            << "0 4 A 2 e2\n";  // row delta 4 → not unit-spaced
    }

    auto result = cbsdk::parseCmpFile(path);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    ASSERT_EQ(entries.size(), 2u);
    // Face value: y is the raw row (4), not scaled; size stays unspecified (0).
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 2)).y, 4);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 2)).size, 0);
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).size, 0);
}

TEST(CmpParser, HsIdSetsHeadstageNotLabel) {
    // hs_id is stored in the headstage field, never mixed into the label.
    // The same file parsed with hs_id=0 vs hs_id=3 yields identical labels,
    // differing only in the headstage value.
    auto bare = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"));
    auto with_hs = cbsdk::parseCmpFile(testFile("8ChannelDefaultMapping.cmp"), 1, 3);
    ASSERT_TRUE(bare.isOk());
    ASSERT_TRUE(with_hs.isOk());

    for (const auto& [key, entry] : with_hs.value()) {
        const auto it = bare.value().find(key);
        ASSERT_NE(it, bare.value().end());
        EXPECT_EQ(entry.label, it->second.label);  // label unchanged by hs_id
        EXPECT_EQ(entry.headstage, 3);
        EXPECT_EQ(it->second.headstage, 0);
        EXPECT_NE(entry.label.substr(0, 2), "hs");  // no "hs3-" prefix
    }
}

TEST(CmpParser, Parse128Channel) {
    auto result = cbsdk::parseCmpFile(testFile("128ChannelDefaultMapping.cmp"));
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    EXPECT_EQ(entries.size(), 128u);

    // 128ch = banks A..D (1..4), each electrodes 1..32. start_chan=1 → offset 0.
    for (int32_t bank = 1; bank <= 4; ++bank) {
        for (int32_t term = 1; term <= 32; ++term) {
            auto it = entries.find(cbsdk::cmpKey(bank, term));
            ASSERT_NE(it, entries.end())
                << "missing (bank " << bank << ", term " << term << ")";
            EXPECT_EQ(it->second.bank, bank);
            EXPECT_EQ(it->second.term, term);
        }
    }
}

TEST(CmpParser, StartChanOffsetsBanks) {
    // Second headstage: same 96-channel CMP (banks A,B,C) mapped onto the
    // device's second set of banks. start_chan=129 → offset 129/32 = 4, so
    // CMP bank A→E (5), B→F (6), C→G (7).
    auto result = cbsdk::parseCmpFile(
        testFile("96ChannelDefaultMapping.cmp"), /*start_chan=*/129, /*hs_id=*/2);
    ASSERT_TRUE(result.isOk()) << result.error();

    const auto& entries = result.value();
    EXPECT_EQ(entries.size(), 96u);

    // Every entry sits in device banks 5, 6 or 7.
    std::set<int32_t> banks;
    for (const auto& [_, entry] : entries) banks.insert(entry.bank);
    EXPECT_EQ(banks, std::set<int32_t>({5, 6, 7}));

    // chan1 row "0 5 A 1" → device (bank 5, term 1). Default index grid → ×400,
    // so x=0, y=2000, size=400.
    auto first = entries.find(cbsdk::cmpKey(5, 1));
    ASSERT_NE(first, entries.end());
    EXPECT_EQ(first->second.x, 0);
    EXPECT_EQ(first->second.y, 2000);
    EXPECT_EQ(first->second.size, 400);

    // chan96 row "15 0 C 32" → device (bank 7, term 32).
    ASSERT_TRUE(entries.count(cbsdk::cmpKey(7, 32)));

    // Labels are verbatim from the file (no "hs2-" prefix); hs_id lands in
    // the headstage field instead.
    EXPECT_EQ(entries.at(cbsdk::cmpKey(5, 1)).label, "chan1");
    for (const auto& [_, entry] : entries) {
        EXPECT_NE(entry.label.substr(0, 2), "hs");
        EXPECT_EQ(entry.headstage, 2);  // hs_id → headstage (position[3])
    }
}

TEST(CmpParser, MatchesRowsByBankAndTerm) {
    // Rows given out of (bank, elec) order — order no longer matters since
    // entries are keyed by (bank, term), not an ordinal channel id.
    auto path = (std::filesystem::temp_directory_path() / "cmp_unsorted_tmp.cmp").string();
    {
        std::ofstream out(path);
        out << "// scratch file\n"
            << "unsorted test map\n"
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

    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 1)).label, "label_a1");
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 2)).label, "label_a2");
    EXPECT_EQ(entries.at(cbsdk::cmpKey(1, 3)).label, "label_a3");
    EXPECT_EQ(entries.at(cbsdk::cmpKey(2, 1)).label, "label_b1");

    std::filesystem::remove(path);
}

TEST(CmpParser, NonexistentFile) {
    auto result = cbsdk::parseCmpFile("/nonexistent/path.cmp");
    EXPECT_TRUE(result.isError());
}
