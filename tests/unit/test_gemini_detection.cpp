///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_gemini_detection.cpp
/// @brief  Verify the shared Gemini-classification helper (cbproto/gemini.h)
///
/// This helper is the single source of truth used by both the direct-to-device
/// path (cbdev) and the shared-memory config mirror (cbsdk) to decide whether a
/// device is a Gemini system (nanosecond PTP timestamps) or a legacy
/// sample-counter device. Keeping the two consumers in agreement is the whole
/// point, so the matching rules are pinned here.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbproto/gemini.h>

#include <cstring>

namespace {

cbPKT_PROCINFO makeProcInfo(const char* ident) {
    cbPKT_PROCINFO p{};
    // Copy without overrunning; leave the remainder zeroed as a real PROCREP would.
    std::strncpy(p.ident, ident, sizeof(p.ident) - 1);
    p.ident[sizeof(p.ident) - 1] = '\0';
    return p;
}

}  // namespace

TEST(GeminiDetection, MatchesGeminiIdentsCaseInsensitively) {
    EXPECT_TRUE(cbproto::procInfoIsGemini(makeProcInfo("Gemini NSP")));
    EXPECT_TRUE(cbproto::procInfoIsGemini(makeProcInfo("Gemini Hub 1")));
    EXPECT_TRUE(cbproto::procInfoIsGemini(makeProcInfo("gemini")));
    EXPECT_TRUE(cbproto::procInfoIsGemini(makeProcInfo("GEMINI")));
    // Substring anywhere in the ident still counts.
    EXPECT_TRUE(cbproto::procInfoIsGemini(makeProcInfo("256-Channel Gemini Hub with Experiment I/O")));
}

TEST(GeminiDetection, RejectsNonGeminiIdents) {
    EXPECT_FALSE(cbproto::procInfoIsGemini(makeProcInfo("256-Channel player")));
    EXPECT_FALSE(cbproto::procInfoIsGemini(makeProcInfo("Neuroport NSP")));
    EXPECT_FALSE(cbproto::procInfoIsGemini(makeProcInfo("")));
    // A near-miss that shares a prefix but is not the whole needle.
    EXPECT_FALSE(cbproto::procInfoIsGemini(makeProcInfo("Gemin")));
}

TEST(GeminiDetection, HandlesNonNullTerminatedIdent) {
    // Fill the whole buffer with 'x' (no null terminator) — must not read past it.
    cbPKT_PROCINFO p{};
    std::memset(p.ident, 'x', sizeof(p.ident));
    EXPECT_FALSE(cbproto::procInfoIsGemini(p));

    // Same, but with "gemini" embedded and the buffer fully packed.
    std::memset(p.ident, 'x', sizeof(p.ident));
    std::memcpy(p.ident, "gemini", 6);
    EXPECT_TRUE(cbproto::procInfoIsGemini(p));
}

TEST(GeminiDetection, IdentOverloadGuardsNullAndEmpty) {
    EXPECT_FALSE(cbproto::identIsGemini(nullptr, 64));
    EXPECT_FALSE(cbproto::identIsGemini("gemini", 0));
}
