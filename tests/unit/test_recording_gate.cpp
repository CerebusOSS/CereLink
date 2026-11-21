///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_recording_gate.cpp
/// @brief  Unit tests for the settings a session locks while the device is recording
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "recording_gate.h"

#include <vector>

using cbsdk::recordingBlockReason;

namespace {

struct Config {
    std::vector<cbPKT_CHANINFO> chans = std::vector<cbPKT_CHANINFO>(cbMAXCHANS);
    cbPKT_SS_STATUS status{};

    Config() {
        chans[0].chan = 1;
        chans[0].smpgroup = 5;
        chans[0].ainpopts = cbAINP_RAWSTREAM_ENABLED;
    }
    const char* reason(const cbPKT_GENERIC& pkt) const {
        return recordingBlockReason(pkt, chans.data(), chans.size(), status);
    }
};

template <class T>
const cbPKT_GENERIC& generic(const T& pkt) { return reinterpret_cast<const cbPKT_GENERIC&>(pkt); }

cbPKT_GENERIC ofType(uint16_t type) {
    cbPKT_GENERIC pkt{};
    pkt.cbpkt_header.type = type;
    return pkt;
}

}  // namespace

TEST(RecordingGate, RecordingMeansOptRec) {
    cbPKT_FILECFG f{};
    f.options = cbFILECFG_OPT_REC;
    EXPECT_TRUE(cbsdk::isRecording(f));
    f.options = cbFILECFG_OPT_STOP;
    EXPECT_FALSE(cbsdk::isRecording(f));
}

TEST(RecordingGate, CentralsLockedPacketsAreBlocked) {
    Config c;
    for (uint16_t t : {cbPKTTYPE_SYSSETSPKLEN, cbPKTTYPE_SS_DETECTSET, cbPKTTYPE_SS_NOISE_BOUNDARYSET,
                       cbPKTTYPE_CHANSETNTRODEGROUP, cbPKTTYPE_SETNTRODEINFO, cbPKTTYPE_SS_RESETSET})
        EXPECT_NE(c.reason(ofType(t)), nullptr) << "type 0x" << std::hex << t;
}

TEST(RecordingGate, OtherPacketsPass) {
    Config c;
    for (uint16_t t : {cbPKTTYPE_CHANSETLABEL, cbPKTTYPE_CHANSETSPKTHR, cbPKTTYPE_CHANSETSPK,
                       cbPKTTYPE_COMMENTSET, cbPKTTYPE_SETFILECFG, cbPKTTYPE_SYSSETRUNLEV})
        EXPECT_EQ(c.reason(ofType(t)), nullptr) << "type 0x" << std::hex << t;
}

TEST(RecordingGate, SampleGroupChangeIsBlockedInSmpAndFullChanset) {
    Config c;
    cbPKT_CHANINFO ci = c.chans[0];
    ci.cbpkt_header.type = cbPKTTYPE_CHANSETSMP;
    EXPECT_EQ(c.reason(generic(ci)), nullptr);  // unchanged resend is fine
    ci.smpgroup = 2;
    EXPECT_NE(c.reason(generic(ci)), nullptr);
    ci.cbpkt_header.type = cbPKTTYPE_CHANSET;
    EXPECT_NE(c.reason(generic(ci)), nullptr);
    ci.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;  // AINP does not carry the group
    EXPECT_EQ(c.reason(generic(ci)), nullptr);
}

TEST(RecordingGate, OnlyTheRawBitIsLockedInAinp) {
    Config c;
    cbPKT_CHANINFO ci = c.chans[0];
    ci.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
    ci.ainpopts |= cbAINP_OFFSET_CORRECT;
    EXPECT_EQ(c.reason(generic(ci)), nullptr);
    ci.ainpopts &= ~cbAINP_RAWSTREAM_ENABLED;
    EXPECT_NE(c.reason(generic(ci)), nullptr);
}

TEST(RecordingGate, ChannelOutOfRangeIsNotBlocked) {
    Config c;
    cbPKT_CHANINFO ci{};
    ci.cbpkt_header.type = cbPKTTYPE_CHANSET;
    ci.chan = 0;
    ci.smpgroup = 3;
    EXPECT_EQ(c.reason(generic(ci)), nullptr);
}

TEST(RecordingGate, SwitchingContinuousSortingOnIsBlockedFreezingIsNot) {
    Config c;
    c.status.cntlNumUnits.nMode = ADAPT_NEVER;
    cbPKT_SS_STATUS s{};
    s.cbpkt_header.type = cbPKTTYPE_SS_STATUSSET;
    s.cntlNumUnits.nMode = ADAPT_ALWAYS;
    EXPECT_NE(c.reason(generic(s)), nullptr);
    c.status.cntlNumUnits.nMode = ADAPT_ALWAYS;
    s.cntlNumUnits.nMode = ADAPT_NEVER;  // freeze
    EXPECT_EQ(c.reason(generic(s)), nullptr);
}
