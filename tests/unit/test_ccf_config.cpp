///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_ccf_config.cpp
/// @brief  Tests for CCF <-> DeviceConfig conversion functions
///
/// Validates extractDeviceConfig() and buildConfigPackets() produce correct results,
/// following the same algorithms as Central's ReadCCFOfNSP() and SendCCF().
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <ccfutils/ccf_config.h>
#include <cstring>
#include <set>

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Test fixture for CCF config conversion tests
///////////////////////////////////////////////////////////////////////////////////////////////////
class CcfConfigTest : public ::testing::Test {
protected:
    cbproto::DeviceConfig device_config{};
    cbCCF ccf_data{};

    void SetUp() override
    {
        std::memset(&device_config, 0, sizeof(device_config));
        std::memset(&ccf_data, 0, sizeof(ccf_data));
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name extractDeviceConfig tests
/// @{

TEST_F(CcfConfigTest, ExtractChannels)
{
    // Populate a few channels with test data
    device_config.chaninfo[0].chan = 1;
    device_config.chaninfo[0].proc = 1;
    device_config.chaninfo[0].smpfilter = 42;
    device_config.chaninfo[0].spkfilter = 7;

    device_config.chaninfo[5].chan = 6;
    device_config.chaninfo[5].proc = 1;
    device_config.chaninfo[5].ainpopts = 0x1234;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_EQ(ccf_data.isChan[0].chan, 1u);
    EXPECT_EQ(ccf_data.isChan[0].proc, 1u);
    EXPECT_EQ(ccf_data.isChan[0].smpfilter, 42u);
    EXPECT_EQ(ccf_data.isChan[0].spkfilter, 7u);
    EXPECT_EQ(ccf_data.isChan[5].chan, 6u);
    EXPECT_EQ(ccf_data.isChan[5].ainpopts, 0x1234u);

    // Unpopulated channels should be zero
    EXPECT_EQ(ccf_data.isChan[10].chan, 0u);
}

TEST_F(CcfConfigTest, ExtractFilters)
{
    // Set custom digital filters at the expected offset in the 32-filter array
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER].filt = 1;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER].hpfreq = 300000;  // 300 Hz in milliHertz
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER + 1].filt = 2;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER + 1].lpfreq = 6000000;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER + 2].filt = 3;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER + 3].filt = 4;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_EQ(ccf_data.filtinfo[0].filt, 1u);
    EXPECT_EQ(ccf_data.filtinfo[0].hpfreq, 300000u);
    EXPECT_EQ(ccf_data.filtinfo[1].filt, 2u);
    EXPECT_EQ(ccf_data.filtinfo[1].lpfreq, 6000000u);
    EXPECT_EQ(ccf_data.filtinfo[2].filt, 3u);
    EXPECT_EQ(ccf_data.filtinfo[3].filt, 4u);
}

TEST_F(CcfConfigTest, ExtractSpikeSorting)
{
    device_config.spike_sorting.detect.fThreshold = 5.5f;
    device_config.spike_sorting.detect.cbpkt_header.type = 0xD0;
    device_config.spike_sorting.artifact_reject.nMaxSimulChans = 3;
    device_config.spike_sorting.artifact_reject.cbpkt_header.type = 0xD1;
    device_config.spike_sorting.noise_boundary[0].chan = 1;
    device_config.spike_sorting.noise_boundary[0].afc[0] = 1.0f;
    device_config.spike_sorting.statistics.nAutoalg = 5;
    device_config.spike_sorting.statistics.cbpkt_header.type = 0xD3;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_FLOAT_EQ(ccf_data.isSS_Detect.fThreshold, 5.5f);
    EXPECT_EQ(ccf_data.isSS_ArtifactReject.nMaxSimulChans, 3u);
    EXPECT_EQ(ccf_data.isSS_NoiseBoundary[0].chan, 1u);
    EXPECT_FLOAT_EQ(ccf_data.isSS_NoiseBoundary[0].afc[0], 1.0f);
    EXPECT_EQ(ccf_data.isSS_Statistics.nAutoalg, 5u);
}

TEST_F(CcfConfigTest, ExtractSysInfo)
{
    device_config.sysinfo.cbpkt_header.type = 0x10;  // original type
    device_config.sysinfo.cbpkt_header.dlen = 42;

    ccf::extractDeviceConfig(device_config, ccf_data);

    // extractDeviceConfig should override type to SYSSETSPKLEN
    EXPECT_EQ(ccf_data.isSysInfo.cbpkt_header.type, cbPKTTYPE_SYSSETSPKLEN);
    // Other fields preserved
    EXPECT_EQ(ccf_data.isSysInfo.cbpkt_header.dlen, 42u);
}

TEST_F(CcfConfigTest, ExtractWaveforms)
{
    device_config.waveform[0][0].chan = cbNUM_ANALOG_CHANS + 1;
    device_config.waveform[0][0].trigNum = 0;
    device_config.waveform[0][0].active = 1;
    device_config.waveform[0][0].mode = 2;

    device_config.waveform[1][2].chan = cbNUM_ANALOG_CHANS + 2;
    device_config.waveform[1][2].trigNum = 2;
    device_config.waveform[1][2].active = 1;

    ccf::extractDeviceConfig(device_config, ccf_data);

    // Waveform data should be copied
    EXPECT_EQ(ccf_data.isWaveform[0][0].chan, static_cast<uint16_t>(cbNUM_ANALOG_CHANS + 1));
    EXPECT_EQ(ccf_data.isWaveform[0][0].mode, 2u);
    EXPECT_EQ(ccf_data.isWaveform[1][2].chan, static_cast<uint16_t>(cbNUM_ANALOG_CHANS + 2));

    // Active flag must be cleared
    EXPECT_EQ(ccf_data.isWaveform[0][0].active, 0u);
    EXPECT_EQ(ccf_data.isWaveform[1][2].active, 0u);
}

TEST_F(CcfConfigTest, ExtractSSStatus)
{
    device_config.spike_sorting.status.cntlNumUnits.nMode = ADAPT_ALWAYS;
    device_config.spike_sorting.status.cntlNumUnits.fElapsedMinutes = 5.0f;
    device_config.spike_sorting.status.cntlUnitStats.nMode = ADAPT_TIMED;
    device_config.spike_sorting.status.cntlUnitStats.fElapsedMinutes = 10.0f;
    device_config.spike_sorting.status.cbpkt_header.type = 0xD5;

    ccf::extractDeviceConfig(device_config, ccf_data);

    // Mode preserved
    EXPECT_EQ(ccf_data.isSS_Status.cntlNumUnits.nMode, static_cast<uint32_t>(ADAPT_ALWAYS));
    EXPECT_EQ(ccf_data.isSS_Status.cntlUnitStats.nMode, static_cast<uint32_t>(ADAPT_TIMED));

    // Elapsed minutes must be set to 99
    EXPECT_FLOAT_EQ(ccf_data.isSS_Status.cntlNumUnits.fElapsedMinutes, 99.0f);
    EXPECT_FLOAT_EQ(ccf_data.isSS_Status.cntlUnitStats.fElapsedMinutes, 99.0f);
}

TEST_F(CcfConfigTest, ExtractNTrodes)
{
    device_config.ntrodeinfo[0].ntrode = 1;
    std::strncpy(device_config.ntrodeinfo[0].label, "Stereo1", cbLEN_STR_LABEL - 1);
    device_config.ntrodeinfo[0].nSite = 2;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_EQ(ccf_data.isNTrodeInfo[0].ntrode, 1u);
    EXPECT_STREQ(ccf_data.isNTrodeInfo[0].label, "Stereo1");
    EXPECT_EQ(ccf_data.isNTrodeInfo[0].nSite, 2u);
}

TEST_F(CcfConfigTest, ExtractAdaptInfo)
{
    device_config.adaptinfo.nMode = 1;
    device_config.adaptinfo.dLearningRate = 5e-12f;
    device_config.adaptinfo.nRefChan1 = 3;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_EQ(ccf_data.isAdaptInfo.nMode, 1u);
    EXPECT_FLOAT_EQ(ccf_data.isAdaptInfo.dLearningRate, 5e-12f);
    EXPECT_EQ(ccf_data.isAdaptInfo.nRefChan1, 3u);
}

TEST_F(CcfConfigTest, ExtractLnc)
{
    device_config.lnc.lncFreq = 60;
    device_config.lnc.lncRefChan = 5;
    device_config.lnc.cbpkt_header.type = 0xA6;

    ccf::extractDeviceConfig(device_config, ccf_data);

    EXPECT_EQ(ccf_data.isLnc.lncFreq, 60u);
    EXPECT_EQ(ccf_data.isLnc.lncRefChan, 5u);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name buildConfigPackets tests
/// @{

TEST_F(CcfConfigTest, ChannelPackets)
{
    ccf_data.isChan[0].chan = 1;
    ccf_data.isChan[0].proc = 1;
    ccf_data.isChan[0].cbpkt_header.dlen =
        (sizeof(cbPKT_CHANINFO) - sizeof(cbPKT_HEADER)) / 4;

    auto packets = ccf::buildConfigPackets(ccf_data);

    // Should produce at least one packet
    ASSERT_GE(packets.size(), 1u);

    // Find the CHANSET packet
    bool found = false;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_CHANSET)
        {
            found = true;
            // instrument should be proc - 1 (0-based)
            EXPECT_EQ(pkt.cbpkt_header.instrument, 0u);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected a CHANSET packet";
}

TEST_F(CcfConfigTest, FilterPackets)
{
    ccf_data.filtinfo[0].filt = 1;
    ccf_data.filtinfo[0].hpfreq = 300000;
    ccf_data.filtinfo[0].cbpkt_header.dlen =
        (sizeof(cbPKT_FILTINFO) - sizeof(cbPKT_HEADER)) / 4;

    auto packets = ccf::buildConfigPackets(ccf_data);

    ASSERT_GE(packets.size(), 1u);

    bool found = false;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_FILTSET)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected a FILTSET packet";
}

TEST_F(CcfConfigTest, EmptyFieldsSkipped)
{
    // Zero-initialized cbCCF should produce no packets
    auto packets = ccf::buildConfigPackets(ccf_data);
    EXPECT_EQ(packets.size(), 0u) << "Zero-initialized cbCCF should produce no packets";
}

TEST_F(CcfConfigTest, PacketOrder)
{
    // Populate multiple field types
    ccf_data.filtinfo[0].filt = 1;
    ccf_data.filtinfo[0].cbpkt_header.type = 0x01;

    ccf_data.isChan[0].chan = 1;
    ccf_data.isChan[0].proc = 1;

    ccf_data.isSS_Statistics.cbpkt_header.type = 0x01;
    ccf_data.isSS_Statistics.nAutoalg = 5;

    ccf_data.isSysInfo.cbpkt_header.type = 0x01;

    ccf_data.isLnc.cbpkt_header.type = 0x01;

    ccf_data.isAdaptInfo.cbpkt_header.type = 0x01;

    ccf_data.isSS_Status.cbpkt_header.type = 0x01;

    auto packets = ccf::buildConfigPackets(ccf_data);

    // Verify ordering: filters, channels, SS stats, sysinfo, LNC, adapt, SS status
    ASSERT_GE(packets.size(), 7u);

    // Collect the types in order
    std::vector<uint16_t> types;
    for (const auto& pkt : packets)
    {
        types.push_back(pkt.cbpkt_header.type);
    }

    // Find positions
    auto pos = [&types](uint16_t t) -> int {
        for (int i = 0; i < static_cast<int>(types.size()); ++i)
            if (types[i] == t) return i;
        return -1;
    };

    int filtPos    = pos(cbPKTTYPE_FILTSET);
    int chanPos    = pos(cbPKTTYPE_CHANSET);
    int ssStatPos  = pos(cbPKTTYPE_SS_STATISTICSSET);
    int sysPos     = pos(cbPKTTYPE_SYSSETSPKLEN);
    int lncPos     = pos(cbPKTTYPE_LNCSET);
    int adaptPos   = pos(cbPKTTYPE_ADAPTFILTSET);
    int statusPos  = pos(cbPKTTYPE_SS_STATUSSET);

    ASSERT_NE(filtPos, -1);
    ASSERT_NE(chanPos, -1);
    ASSERT_NE(ssStatPos, -1);
    ASSERT_NE(sysPos, -1);
    ASSERT_NE(lncPos, -1);
    ASSERT_NE(adaptPos, -1);
    ASSERT_NE(statusPos, -1);

    EXPECT_LT(filtPos, chanPos) << "Filters must come before channels";
    EXPECT_LT(chanPos, ssStatPos) << "Channels must come before SS statistics";
    EXPECT_LT(ssStatPos, sysPos) << "SS statistics must come before sysinfo";
    EXPECT_LT(sysPos, lncPos) << "Sysinfo must come before LNC";
    EXPECT_LT(lncPos, adaptPos) << "LNC must come before adaptive filter";
    EXPECT_LT(adaptPos, statusPos) << "Adaptive filter must come before SS status";
}

TEST_F(CcfConfigTest, SSStatusSetsAdaptNever)
{
    ccf_data.isSS_Status.cbpkt_header.type = 0x01;
    ccf_data.isSS_Status.cntlNumUnits.nMode = ADAPT_ALWAYS;

    auto packets = ccf::buildConfigPackets(ccf_data);

    bool found = false;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_SS_STATUSSET)
        {
            found = true;
            // Interpret as cbPKT_SS_STATUS
            const auto* status = reinterpret_cast<const cbPKT_SS_STATUS*>(&pkt);
            EXPECT_EQ(status->cntlNumUnits.nMode, static_cast<uint32_t>(ADAPT_NEVER));
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected an SS_STATUSSET packet";
}

TEST_F(CcfConfigTest, WaveformsClearedActive)
{
    ccf_data.isWaveform[0][0].chan = cbNUM_ANALOG_CHANS + 1;
    ccf_data.isWaveform[0][0].active = 1;
    ccf_data.isWaveform[0][0].mode = 2;

    auto packets = ccf::buildConfigPackets(ccf_data);

    bool found = false;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_WAVEFORMSET)
        {
            found = true;
            const auto* wf = reinterpret_cast<const cbPKT_AOUT_WAVEFORM*>(&pkt);
            EXPECT_EQ(wf->active, 0u) << "Waveform active flag must be cleared";
            EXPECT_EQ(wf->mode, 2u) << "Other waveform fields should be preserved";
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected a WAVEFORMSET packet";
}

TEST_F(CcfConfigTest, NTrodePackets)
{
    ccf_data.isNTrodeInfo[0].ntrode = 1;
    std::strncpy(ccf_data.isNTrodeInfo[0].label, "NT1", cbLEN_STR_LABEL - 1);

    auto packets = ccf::buildConfigPackets(ccf_data);

    bool found = false;
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_SETNTRODEINFO)
        {
            found = true;
            const auto* nt = reinterpret_cast<const cbPKT_NTRODEINFO*>(&pkt);
            EXPECT_EQ(nt->ntrode, 1u);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected a SETNTRODEINFO packet";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Round-trip test
/// @{

TEST_F(CcfConfigTest, RoundTrip)
{
    // Populate DeviceConfig with representative data
    device_config.chaninfo[0].chan = 1;
    device_config.chaninfo[0].proc = 1;
    device_config.chaninfo[0].smpfilter = 13;
    device_config.chaninfo[0].cbpkt_header.type = 0x40;

    device_config.filtinfo[cbFIRST_DIGITAL_FILTER].filt = 1;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER].hpfreq = 250000;
    device_config.filtinfo[cbFIRST_DIGITAL_FILTER].cbpkt_header.type = 0xA2;

    device_config.spike_sorting.detect.fThreshold = 4.5f;
    device_config.spike_sorting.detect.cbpkt_header.type = 0xD0;

    device_config.spike_sorting.statistics.nAutoalg = 5;
    device_config.spike_sorting.statistics.cbpkt_header.type = 0xD3;

    device_config.sysinfo.cbpkt_header.type = 0x10;

    device_config.lnc.lncFreq = 50;
    device_config.lnc.cbpkt_header.type = 0xA6;

    device_config.adaptinfo.nMode = 1;
    device_config.adaptinfo.cbpkt_header.type = 0xA4;

    device_config.spike_sorting.status.cbpkt_header.type = 0xD5;
    device_config.spike_sorting.status.cntlNumUnits.nMode = ADAPT_ALWAYS;

    // Extract to CCF
    ccf::extractDeviceConfig(device_config, ccf_data);

    // Build packets from CCF
    auto packets = ccf::buildConfigPackets(ccf_data);

    // Verify expected packet types are present
    std::set<uint16_t> expected_types = {
        cbPKTTYPE_FILTSET,
        cbPKTTYPE_CHANSET,
        cbPKTTYPE_SS_DETECTSET,
        cbPKTTYPE_SS_STATISTICSSET,
        cbPKTTYPE_SYSSETSPKLEN,
        cbPKTTYPE_LNCSET,
        cbPKTTYPE_ADAPTFILTSET,
        cbPKTTYPE_SS_STATUSSET,
    };

    std::set<uint16_t> actual_types;
    for (const auto& pkt : packets)
    {
        actual_types.insert(pkt.cbpkt_header.type);
    }

    for (uint16_t t : expected_types)
    {
        EXPECT_TRUE(actual_types.count(t))
            << "Missing packet type 0x" << std::hex << t;
    }

    // Verify specific packet contents survived the round trip
    for (const auto& pkt : packets)
    {
        if (pkt.cbpkt_header.type == cbPKTTYPE_CHANSET)
        {
            const auto* ch = reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
            if (ch->chan == 1)
            {
                EXPECT_EQ(ch->smpfilter, 13u);
                EXPECT_EQ(ch->cbpkt_header.instrument, 0u);  // proc=1 -> instrument=0
            }
        }
        else if (pkt.cbpkt_header.type == cbPKTTYPE_FILTSET)
        {
            const auto* f = reinterpret_cast<const cbPKT_FILTINFO*>(&pkt);
            if (f->filt == 1)
            {
                EXPECT_EQ(f->hpfreq, 250000u);
            }
        }
        else if (pkt.cbpkt_header.type == cbPKTTYPE_SS_STATUSSET)
        {
            const auto* s = reinterpret_cast<const cbPKT_SS_STATUS*>(&pkt);
            EXPECT_EQ(s->cntlNumUnits.nMode, static_cast<uint32_t>(ADAPT_NEVER));
        }
    }
}

/// @}
