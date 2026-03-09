///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   ccf_config.cpp
/// @brief  Bridge between cbCCF (file data) and DeviceConfig (device state)
///
/// Implementation follows Central-Suite's CCFUtils.cpp algorithms:
///   - extractDeviceConfig() mirrors ReadCCFOfNSP() (lines 444-486)
///   - buildConfigPackets() mirrors SendCCF() (lines 82-279)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <ccfutils/ccf_config.h>
#include <cstring>

namespace ccf {

void extractDeviceConfig(const cbproto::DeviceConfig& config, cbCCF& ccf_data)
{
    // Digital filters: copy the 4 custom filters from the 32-filter array
    // Central indexes with cbFIRST_DIGITAL_FILTER + i - 1 (1-based), but CereLink's
    // filtinfo array is 0-based, so we use cbFIRST_DIGITAL_FILTER + i directly for i in [0..3]
    for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
    {
        ccf_data.filtinfo[i] = config.filtinfo[cbFIRST_DIGITAL_FILTER + i];
    }

    // Channel info: direct copy
    for (int i = 0; i < cbMAXCHANS; ++i)
    {
        ccf_data.isChan[i] = config.chaninfo[i];
    }

    // Adaptive filter info
    ccf_data.isAdaptInfo = config.adaptinfo;

    // Spike sorting configuration
    ccf_data.isSS_Detect = config.spike_sorting.detect;
    ccf_data.isSS_ArtifactReject = config.spike_sorting.artifact_reject;
    for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
    {
        ccf_data.isSS_NoiseBoundary[i] = config.spike_sorting.noise_boundary[i];
    }
    ccf_data.isSS_Statistics = config.spike_sorting.statistics;

    // Spike sorting status: set elapsed minutes to 99 (Central convention)
    ccf_data.isSS_Status = config.spike_sorting.status;
    ccf_data.isSS_Status.cntlNumUnits.fElapsedMinutes = 99;
    ccf_data.isSS_Status.cntlUnitStats.fElapsedMinutes = 99;

    // System info: set type to SYSSETSPKLEN
    ccf_data.isSysInfo = config.sysinfo;
    ccf_data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;

    // N-trode info
    for (int i = 0; i < cbMAXNTRODES; ++i)
    {
        ccf_data.isNTrodeInfo[i] = config.ntrodeinfo[i];
    }

    // LNC
    ccf_data.isLnc = config.lnc;

    // Waveforms: copy and clear active flag to prevent auto-generation on load
    for (int i = 0; i < AOUT_NUM_GAIN_CHANS; ++i)
    {
        for (int j = 0; j < cbMAX_AOUT_TRIGGER; ++j)
        {
            ccf_data.isWaveform[i][j] = config.waveform[i][j];
            ccf_data.isWaveform[i][j].active = 0;
        }
    }
}

// Helper: reinterpret any packet struct as cbPKT_GENERIC and push to vector
template <typename T>
static void pushPacket(std::vector<cbPKT_GENERIC>& packets, const T& pkt)
{
    static_assert(sizeof(T) <= sizeof(cbPKT_GENERIC), "Packet exceeds cbPKT_GENERIC size");
    cbPKT_GENERIC generic;
    std::memset(&generic, 0, sizeof(generic));
    std::memcpy(&generic, &pkt, sizeof(T));
    packets.push_back(generic);
}

std::vector<cbPKT_GENERIC> buildConfigPackets(const cbCCF& ccf_data)
{
    std::vector<cbPKT_GENERIC> packets;

    // 1. Filters (cbPKTTYPE_FILTSET)
    for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
    {
        if (ccf_data.filtinfo[i].filt != 0)
        {
            cbPKT_FILTINFO pkt = ccf_data.filtinfo[i];
            pkt.cbpkt_header.type = cbPKTTYPE_FILTSET;
            pushPacket(packets, pkt);
        }
    }

    // 2. Channels (cbPKTTYPE_CHANSET)
    for (int i = 0; i < cbMAXCHANS; ++i)
    {
        if (ccf_data.isChan[i].chan != 0)
        {
            cbPKT_CHANINFO pkt = ccf_data.isChan[i];
            pkt.cbpkt_header.type = cbPKTTYPE_CHANSET;
            pkt.cbpkt_header.instrument = static_cast<uint8_t>(pkt.proc - 1);
            pushPacket(packets, pkt);
        }
    }

    // 3. SS Statistics (cbPKTTYPE_SS_STATISTICSSET)
    if (ccf_data.isSS_Statistics.cbpkt_header.type != 0)
    {
        cbPKT_SS_STATISTICS pkt = ccf_data.isSS_Statistics;
        pkt.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSSET;
        pushPacket(packets, pkt);
    }

    // 4. SS Noise Boundary (cbPKTTYPE_SS_NOISE_BOUNDARYSET)
    for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
    {
        if (ccf_data.isSS_NoiseBoundary[i].chan != 0)
        {
            cbPKT_SS_NOISE_BOUNDARY pkt = ccf_data.isSS_NoiseBoundary[i];
            pkt.cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYSET;
            pushPacket(packets, pkt);
        }
    }

    // 5. SS Detect (cbPKTTYPE_SS_DETECTSET)
    if (ccf_data.isSS_Detect.cbpkt_header.type != 0)
    {
        cbPKT_SS_DETECT pkt = ccf_data.isSS_Detect;
        pkt.cbpkt_header.type = cbPKTTYPE_SS_DETECTSET;
        pushPacket(packets, pkt);
    }

    // 6. SS Artifact Reject (cbPKTTYPE_SS_ARTIF_REJECTSET)
    if (ccf_data.isSS_ArtifactReject.cbpkt_header.type != 0)
    {
        cbPKT_SS_ARTIF_REJECT pkt = ccf_data.isSS_ArtifactReject;
        pkt.cbpkt_header.type = cbPKTTYPE_SS_ARTIF_REJECTSET;
        pushPacket(packets, pkt);
    }

    // 7. SysInfo (cbPKTTYPE_SYSSETSPKLEN)
    if (ccf_data.isSysInfo.cbpkt_header.type != 0)
    {
        cbPKT_SYSINFO pkt = ccf_data.isSysInfo;
        pkt.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
        pushPacket(packets, pkt);
    }

    // 8. LNC (cbPKTTYPE_LNCSET)
    if (ccf_data.isLnc.cbpkt_header.type != 0)
    {
        cbPKT_LNC pkt = ccf_data.isLnc;
        pkt.cbpkt_header.type = cbPKTTYPE_LNCSET;
        pushPacket(packets, pkt);
    }

    // 9. Waveforms (cbPKTTYPE_WAVEFORMSET)
    for (int i = 0; i < AOUT_NUM_GAIN_CHANS; ++i)
    {
        for (int j = 0; j < cbMAX_AOUT_TRIGGER; ++j)
        {
            if (ccf_data.isWaveform[i][j].chan != 0)
            {
                cbPKT_AOUT_WAVEFORM pkt = ccf_data.isWaveform[i][j];
                pkt.cbpkt_header.type = cbPKTTYPE_WAVEFORMSET;
                pkt.active = 0;
                pushPacket(packets, pkt);
            }
        }
    }

    // 10. NTrodes (cbPKTTYPE_SETNTRODEINFO)
    for (int i = 0; i < cbMAXNTRODES; ++i)
    {
        if (ccf_data.isNTrodeInfo[i].ntrode != 0)
        {
            cbPKT_NTRODEINFO pkt = ccf_data.isNTrodeInfo[i];
            pkt.cbpkt_header.type = cbPKTTYPE_SETNTRODEINFO;
            pushPacket(packets, pkt);
        }
    }

    // 11. Adaptive filter (cbPKTTYPE_ADAPTFILTSET)
    if (ccf_data.isAdaptInfo.cbpkt_header.type != 0)
    {
        cbPKT_ADAPTFILTINFO pkt = ccf_data.isAdaptInfo;
        pkt.cbpkt_header.type = cbPKTTYPE_ADAPTFILTSET;
        pushPacket(packets, pkt);
    }

    // 12. SS Status (cbPKTTYPE_SS_STATUSSET) — set ADAPT_NEVER to prevent rebuild
    if (ccf_data.isSS_Status.cbpkt_header.type != 0)
    {
        cbPKT_SS_STATUS pkt = ccf_data.isSS_Status;
        pkt.cbpkt_header.type = cbPKTTYPE_SS_STATUSSET;
        pkt.cntlNumUnits.nMode = ADAPT_NEVER;
        pushPacket(packets, pkt);
    }

    return packets;
}

} // namespace ccf
