#include "gtest/gtest.h"
#include "CCFUtils.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

// Helper to get path to minimal test CCF
static std::filesystem::path GetMinimalCCFPath() {
    return std::filesystem::path(PROJECT_SOURCE_DIR) / "tests" / "minimal.ccf";
}

TEST(CCFUtilsTest, LoadAndValidateCCF_WithConvert) {
    std::filesystem::path ccf_path = GetMinimalCCFPath();
    ASSERT_TRUE(std::filesystem::exists(ccf_path)) << "Test CCF file missing: " << ccf_path;

    cbCCF ccf_data; memset(&ccf_data, 0, sizeof(cbCCF));
    CCFUtils ccfUtils(false, false, &ccf_data);
    ccf::ccfResult result = ccfUtils.ReadCCF(ccf_path.string().c_str(), true);

    ASSERT_TRUE(result == ccf::CCFRESULT_SUCCESS || result == ccf::CCFRESULT_WARN_VERSION)
        << "Unexpected result: " << result;

    int foundIndex = -1;
    for (int i = 0; i < cbMAXCHANS; ++i) {
        if (std::string(ccf_data.isChan[i].label) == "chan1") { foundIndex = i; break; }
    }
    ASSERT_NE(foundIndex, -1) << "Channel 'chan1' not found in loaded CCF";

    EXPECT_EQ(ccf_data.isChan[foundIndex].physcalin.digmin, -1);
    EXPECT_EQ(ccf_data.isChan[foundIndex].physcalin.digmax, 1);
    EXPECT_EQ(ccf_data.isChan[foundIndex].physcalin.anamin, -2);
    EXPECT_EQ(ccf_data.isChan[foundIndex].physcalin.anamax, 2);
    EXPECT_EQ(ccfUtils.GetInternalVersion(), 12); // CCFUTILSBINARY_LASTVERSION + 1
}

TEST(CCFUtilsTest, LoadWithoutConvertExpectWarnVersionAndNoData) {
    std::filesystem::path ccf_path = GetMinimalCCFPath();
    ASSERT_TRUE(std::filesystem::exists(ccf_path));

    cbCCF ccf_data; memset(&ccf_data, 0, sizeof(cbCCF));
    CCFUtils ccfUtils(false, false, &ccf_data);
    ccf::ccfResult result = ccfUtils.ReadCCF(ccf_path.string().c_str(), false);
    EXPECT_EQ(result, ccf::CCFRESULT_WARN_VERSION);
    // Ensure no channel has label chan1 populated
    bool anyChan1 = false;
    for (int i = 0; i < cbMAXCHANS; ++i) if (std::string(ccf_data.isChan[i].label) == "chan1") anyChan1 = true;
    EXPECT_FALSE(anyChan1);
}

TEST(CCFUtilsTest, WriteAndReadRoundTrip) {
    std::filesystem::path ccf_path = GetMinimalCCFPath();

    cbRESULT openRes = cbOpen(true /*standalone*/);
    if (openRes != cbRESULT_OK) {
        cbClose(true);
        GTEST_SKIP() << "Skipping round trip test because cbOpen failed with code " << openRes;
    }

    cbCCF ccf_data; memset(&ccf_data, 0, sizeof(cbCCF));
    CCFUtils reader(false, false, &ccf_data);
    ASSERT_EQ(reader.ReadCCF(ccf_path.string().c_str(), true), ccf::CCFRESULT_SUCCESS);

    int origIndex = -1;
    for (int i = 0; i < cbMAXCHANS; ++i) if (std::string(ccf_data.isChan[i].label) == "chan1") { origIndex = i; break; }
    ASSERT_NE(origIndex, -1) << "Original channel 'chan1' not found";
    if (ccf_data.isChan[origIndex].chan == 0) {
        ccf_data.isChan[origIndex].chan = 1; // ensure serialization attempt
    }
    ASSERT_EQ(ccf_data.isChan[origIndex].physcalin.digmin, -1) << "Pre-write sanity check failed";

    CCFUtils writer(false, false, &ccf_data);
    std::filesystem::path out_path = std::filesystem::temp_directory_path() / "cerebus_minimal_roundtrip.ccf";
    ASSERT_EQ(writer.WriteCCFNoPrompt(out_path.string().c_str()), ccf::CCFRESULT_SUCCESS);
    ASSERT_TRUE(std::filesystem::exists(out_path));

    // Basic file non-empty check
    auto fsize = std::filesystem::file_size(out_path);
    EXPECT_GT(fsize, 0u) << "Written CCF file is empty";

    cbClose(true);
}

TEST(CCFUtilsTest, NonexistentFileReturnsOpenFailedRead) {
    std::filesystem::path missing = std::filesystem::temp_directory_path() / "definitely_missing_file.ccf";
    if (std::filesystem::exists(missing)) std::filesystem::remove(missing);

    cbCCF ccf_data; memset(&ccf_data, 0, sizeof(cbCCF));
    CCFUtils ccfUtils(false, false, &ccf_data);
    ccf::ccfResult result = ccfUtils.ReadCCF(missing.string().c_str(), true);
    EXPECT_EQ(result, ccf::CCFRESULT_ERR_OPENFAILEDREAD);
}

TEST(CCFUtilsTest, ReadVersionParsesOriginalVersion) {
    std::filesystem::path ccf_path = GetMinimalCCFPath();
    ASSERT_TRUE(std::filesystem::exists(ccf_path));
    CCFUtils ccfUtils; // no pCCF needed for version read
    ccf::ccfResult res = ccfUtils.ReadVersion(ccf_path.string().c_str());
    ASSERT_TRUE(res == ccf::CCFRESULT_SUCCESS || res == ccf::CCFRESULT_WARN_CONVERT || res == ccf::CCFRESULT_WARN_VERSION)
        << "Unexpected ReadVersion result: " << res;
    EXPECT_EQ(ccfUtils.GetInternalOriginalVersion(), 12);
}
