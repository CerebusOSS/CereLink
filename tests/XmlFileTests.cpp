#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "CCFUtils/XmlFile.h"

namespace fs = std::filesystem;

static std::string ResourcePath(const std::string & rel) {
#ifdef PROJECT_SOURCE_DIR
    return std::string(PROJECT_SOURCE_DIR) + "/" + rel;
#else
    return rel;
#endif
}

TEST(XmlFileConstructor, LoadValidFile_NoError) {
    std::string path = ResourcePath("resources/BrSDK_default_v7.ccf");
    ASSERT_TRUE(fs::exists(path)) << "Resource file missing at " << path;
    XmlFile xf(path, true); // read mode
    EXPECT_FALSE(xf.HasError()) << "Expected successful parse";
    // Navigate to CCF root
    xf.beginGroup("CCF");
    std::string version = xf.attribute("Version");
    xf.endGroup(false);
    EXPECT_FALSE(version.empty()) << "Version attribute should be present";
}

TEST(XmlFileConstructor, LoadMissingFile_ErrorFlag) {
    std::string path = ResourcePath("resources/does_not_exist_12345.ccf");
    ASSERT_FALSE(fs::exists(path));
    XmlFile xf(path, true); // attempt to read
    EXPECT_TRUE(xf.HasError()) << "Missing file should set error flag";
}

TEST(XmlFileConstructor, WriteMode_BeginsEmpty) {
    // Create temp file path
    auto tmp = fs::temp_directory_path() / "xmlfile_write_mode_test.ccf";
    if (fs::exists(tmp)) fs::remove(tmp);
    XmlFile xf(tmp.string(), false); // write mode - do not attempt read
    EXPECT_FALSE(xf.HasError());
    // Should have no children yet
    auto kids = xf.childKeys();
    EXPECT_TRUE(kids.empty()) << "Should start with empty document";
    xf.beginGroup("CCF", "Version", 1, std::string());
    bool err = xf.endGroup(true); // save
    EXPECT_FALSE(err);
    EXPECT_TRUE(fs::exists(tmp)) << "File should be created on save";
    // Read it back quickly to ensure it wrote
    XmlFile xf2(tmp.string(), true);
    EXPECT_FALSE(xf2.HasError());
}

