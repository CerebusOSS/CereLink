///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_ccf_xml.cpp
/// @brief  Tests for CCF XML read/write using minimal fixture files
///
/// Validates that CCFUtils can read XML CCF files and produce correct cbCCF data,
/// and that write → read round-trips preserve all key fields.
/// These tests do NOT require a device connection.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <CCFUtils.h>
#include <ccfutils/ccf_config.h>
#include <cstring>
#include <cstdio>
#include <filesystem>

#ifndef CCF_TEST_DATA_DIR
#error "CCF_TEST_DATA_DIR must be defined to locate test fixtures"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for CCF XML tests
///////////////////////////////////////////////////////////////////////////////////////////////////
class CcfXmlTest : public ::testing::Test {
protected:
    cbCCF ccf_data{};

    void SetUp() override
    {
        std::memset(&ccf_data, 0, sizeof(ccf_data));
    }

    std::string fixturePath(const char* filename) {
        return std::string(CCF_TEST_DATA_DIR) + "/" + filename;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Read tests
/// @{

TEST_F(CcfXmlTest, ReadRaw)
{
    std::string path = fixturePath("ccf_raw.ccf");

    CCFUtils reader(false, &ccf_data);
    auto res = reader.ReadCCF(path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS) << "ReadCCF failed with code " << res;

    // Channel 1 (index 0)
    EXPECT_EQ(ccf_data.isChan[0].chan, 1u);
    EXPECT_EQ(ccf_data.isChan[0].proc, 1u);
    EXPECT_EQ(ccf_data.isChan[0].bank, 1u);
    EXPECT_EQ(ccf_data.isChan[0].term, 1u);

    // ainpopts=64 (0x40 = cbAINP_RAWSTREAM)
    EXPECT_EQ(ccf_data.isChan[0].ainpopts, 64u);
    // spkopts=65792 (0x10100 = spike detection off)
    EXPECT_EQ(ccf_data.isChan[0].spkopts, 65792u);
    // smpfilter=0, smpgroup=0 (no continuous sampling)
    EXPECT_EQ(ccf_data.isChan[0].smpfilter, 0u);
    EXPECT_EQ(ccf_data.isChan[0].smpgroup, 0u);
    // lncrate=10000
    EXPECT_EQ(ccf_data.isChan[0].lncrate, 10000u);

    // Channel 2 (index 1) — same config pattern
    EXPECT_EQ(ccf_data.isChan[1].chan, 2u);
    EXPECT_EQ(ccf_data.isChan[1].ainpopts, 64u);
    EXPECT_EQ(ccf_data.isChan[1].spkopts, 65792u);
    EXPECT_EQ(ccf_data.isChan[1].smpfilter, 0u);
    EXPECT_EQ(ccf_data.isChan[1].smpgroup, 0u);
    EXPECT_EQ(ccf_data.isChan[1].lncrate, 10000u);

    // Channel 3+ should be zero (not present in fixture)
    EXPECT_EQ(ccf_data.isChan[2].chan, 0u);
}

TEST_F(CcfXmlTest, ReadSpk1k)
{
    std::string path = fixturePath("ccf_spk_1k.ccf");

    CCFUtils reader(false, &ccf_data);
    auto res = reader.ReadCCF(path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS) << "ReadCCF failed with code " << res;

    // Channel 1 (index 0)
    EXPECT_EQ(ccf_data.isChan[0].chan, 1u);
    EXPECT_EQ(ccf_data.isChan[0].proc, 1u);

    // ainpopts=258 (0x102 = cbAINP_LNC | cbAINP_SPKSTREAM)
    EXPECT_EQ(ccf_data.isChan[0].ainpopts, 258u);
    // spkopts=65793 (0x10101 = spike extraction on)
    EXPECT_EQ(ccf_data.isChan[0].spkopts, 65793u);
    // smpfilter=6 (digital filter index 6)
    EXPECT_EQ(ccf_data.isChan[0].smpfilter, 6u);
    // smpgroup=2 (1 kHz continuous group)
    EXPECT_EQ(ccf_data.isChan[0].smpgroup, 2u);
    // lncrate=10000
    EXPECT_EQ(ccf_data.isChan[0].lncrate, 10000u);

    // Channel 2 (index 1) — same config
    EXPECT_EQ(ccf_data.isChan[1].chan, 2u);
    EXPECT_EQ(ccf_data.isChan[1].ainpopts, 258u);
    EXPECT_EQ(ccf_data.isChan[1].spkopts, 65793u);
    EXPECT_EQ(ccf_data.isChan[1].smpfilter, 6u);
    EXPECT_EQ(ccf_data.isChan[1].smpgroup, 2u);
}

TEST_F(CcfXmlTest, ReadDifferentConfigs)
{
    // Read both CCFs and verify they have DIFFERENT values for key fields
    cbCCF raw{}, filtered{};
    std::memset(&raw, 0, sizeof(raw));
    std::memset(&filtered, 0, sizeof(filtered));

    CCFUtils reader_raw(false, &raw);
    auto r1 = reader_raw.ReadCCF(fixturePath("ccf_raw.ccf").c_str(), true);
    ASSERT_GE(r1, ccf::CCFRESULT_SUCCESS);

    CCFUtils reader_filt(false, &filtered);
    auto r2 = reader_filt.ReadCCF(fixturePath("ccf_spk_1k.ccf").c_str(), true);
    ASSERT_GE(r2, ccf::CCFRESULT_SUCCESS);

    // Key differences between the two configs
    EXPECT_NE(raw.isChan[0].ainpopts, filtered.isChan[0].ainpopts)
        << "ainpopts should differ (raw=0x40 vs LNC+spk=0x102)";
    EXPECT_NE(raw.isChan[0].spkopts, filtered.isChan[0].spkopts)
        << "spkopts should differ (spike detect off vs on)";
    EXPECT_NE(raw.isChan[0].smpfilter, filtered.isChan[0].smpfilter)
        << "smpfilter should differ (0 vs 6)";
    EXPECT_NE(raw.isChan[0].smpgroup, filtered.isChan[0].smpgroup)
        << "smpgroup should differ (0 vs 2)";
    // lncrate is the same in both
    EXPECT_EQ(raw.isChan[0].lncrate, filtered.isChan[0].lncrate)
        << "lncrate should be the same (10000)";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Write/Read round-trip tests
/// @{

TEST_F(CcfXmlTest, WriteReadRoundTrip)
{
    // Read the 64ch_1k fixture
    std::string input_path = fixturePath("ccf_spk_1k.ccf");
    CCFUtils reader(false, &ccf_data);
    auto res = reader.ReadCCF(input_path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS);

    // Write it out to a temp file
    std::string temp_path = fixturePath("_test_roundtrip.ccf");
    CCFUtils writer(false, &ccf_data);
    res = writer.WriteCCFNoPrompt(temp_path.c_str());
    ASSERT_EQ(res, ccf::CCFRESULT_SUCCESS) << "WriteCCFNoPrompt failed with code " << res;

    // Read the written file back
    cbCCF ccf_data2{};
    std::memset(&ccf_data2, 0, sizeof(ccf_data2));
    CCFUtils reader2(false, &ccf_data2);
    res = reader2.ReadCCF(temp_path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS) << "Round-trip ReadCCF failed with code " << res;

    // Compare key fields for first 2 channels
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(ccf_data.isChan[i].chan, ccf_data2.isChan[i].chan)
            << "chan mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].ainpopts, ccf_data2.isChan[i].ainpopts)
            << "ainpopts mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].spkopts, ccf_data2.isChan[i].spkopts)
            << "spkopts mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].smpfilter, ccf_data2.isChan[i].smpfilter)
            << "smpfilter mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].smpgroup, ccf_data2.isChan[i].smpgroup)
            << "smpgroup mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].lncrate, ccf_data2.isChan[i].lncrate)
            << "lncrate mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].proc, ccf_data2.isChan[i].proc)
            << "proc mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].bank, ccf_data2.isChan[i].bank)
            << "bank mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].term, ccf_data2.isChan[i].term)
            << "term mismatch at index " << i;
    }

    // Cleanup temp file
    std::remove(temp_path.c_str());
}

TEST_F(CcfXmlTest, WriteReadRoundTripRaw)
{
    // Same round-trip test with the raw128 fixture
    std::string input_path = fixturePath("ccf_raw.ccf");
    CCFUtils reader(false, &ccf_data);
    auto res = reader.ReadCCF(input_path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS);

    std::string temp_path = fixturePath("_test_roundtrip_raw.ccf");
    CCFUtils writer(false, &ccf_data);
    res = writer.WriteCCFNoPrompt(temp_path.c_str());
    ASSERT_EQ(res, ccf::CCFRESULT_SUCCESS);

    cbCCF ccf_data2{};
    std::memset(&ccf_data2, 0, sizeof(ccf_data2));
    CCFUtils reader2(false, &ccf_data2);
    res = reader2.ReadCCF(temp_path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS);

    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(ccf_data.isChan[i].chan, ccf_data2.isChan[i].chan)
            << "chan mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].ainpopts, ccf_data2.isChan[i].ainpopts)
            << "ainpopts mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].spkopts, ccf_data2.isChan[i].spkopts)
            << "spkopts mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].smpfilter, ccf_data2.isChan[i].smpfilter)
            << "smpfilter mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].smpgroup, ccf_data2.isChan[i].smpgroup)
            << "smpgroup mismatch at index " << i;
        EXPECT_EQ(ccf_data.isChan[i].lncrate, ccf_data2.isChan[i].lncrate)
            << "lncrate mismatch at index " << i;
    }

    std::remove(temp_path.c_str());
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name buildConfigPackets integration
/// @{

TEST_F(CcfXmlTest, ReadThenBuildPackets)
{
    // Verify the full pipeline: read CCF → buildConfigPackets → packets have correct values
    std::string path = fixturePath("ccf_spk_1k.ccf");
    CCFUtils reader(false, &ccf_data);
    auto res = reader.ReadCCF(path.c_str(), true);
    ASSERT_GE(res, ccf::CCFRESULT_SUCCESS);

    auto packets = ccf::buildConfigPackets(ccf_data);

    // Should have at least 2 CHANSET packets (one per channel in fixture)
    int chan_count = 0;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_CHANSET)
        {
            const auto* ch = reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
            if (ch->chan == 1)
            {
                EXPECT_EQ(ch->ainpopts, 258u);
                EXPECT_EQ(ch->spkopts, 65793u);
                EXPECT_EQ(ch->smpfilter, 6u);
                EXPECT_EQ(ch->smpgroup, 2u);
                EXPECT_EQ(ch->lncrate, 10000u);
            }
            else if (ch->chan == 2)
            {
                EXPECT_EQ(ch->ainpopts, 258u);
                EXPECT_EQ(ch->spkopts, 65793u);
            }
            ++chan_count;
        }
    }
    EXPECT_GE(chan_count, 2) << "Expected at least 2 CHANSET packets";
}

/// @}
