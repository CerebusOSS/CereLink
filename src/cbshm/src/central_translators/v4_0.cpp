///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v4_0.cpp
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Central translator implementations
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "utils.h"
#include <cbshm/central_types/translators.h>
#include <cbshm/central_types/v4_0.h>

namespace cbshm {

namespace central_v4_0 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg, const TranslationContext& ctx) {
    ::cbPKT_HEADER cur{};
    cur.time = leg.time;
    cur.chid = leg.chid;
    cur.type = static_cast<uint16_t>(leg.type);
    cur.dlen = leg.dlen;
    cur.instrument = leg.instrument;
    cur.reserved = leg.reserved;
    return cur;
}

::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_SYSINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.sysfreq = leg.sysfreq;
    cur.spikelen = leg.spikelen;
    cur.spikepre = leg.spikepre;
    cur.resetque = leg.resetque;
    cur.runlevel = leg.runlevel;
    cur.runflags = leg.runflags;
    return cur;
}

::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_PROCINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.proc = leg.proc;
    cur.idcode = leg.idcode;
    cur.idcode = leg.idcode;
    copyArr(cur.ident, leg.ident);
    cur.chanbase = leg.chanbase;
    cur.chancount = leg.chancount;
    cur.bankcount = leg.bankcount;
    cur.groupcount = leg.groupcount;
    cur.filtcount = leg.filtcount;
    cur.sortcount = leg.sortcount;
    cur.unitcount = leg.unitcount;
    cur.hoopcount = leg.hoopcount;
    cur.reserved = leg.reserved;
    cur.version = leg.version;
    return cur;
}

::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_BANKINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.proc = leg.proc;
    cur.bank = leg.bank;
    cur.idcode = leg.idcode;
    copyArr(cur.ident, leg.ident);
    copyArr(cur.label, leg.label);
    cur.chanbase = leg.chanbase;
    cur.chancount = leg.chancount;
    return cur;
}

::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_GROUPINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.proc = leg.proc;
    cur.group = leg.group;
    copyArr(cur.label, leg.label);
    cur.period = leg.period;
    cur.length = leg.length;
    copyArr(cur.list, leg.list);
    return cur;
}

::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_FILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.proc = leg.proc;
    cur.filt = leg.filt;
    copyArr(cur.label, leg.label);
    cur.hpfreq = leg.hpfreq;
    cur.hporder = leg.hporder;
    cur.hptype = leg.hptype;
    cur.lpfreq = leg.lpfreq;
    cur.lporder = leg.lporder;
    cur.lptype = leg.lptype;
    cur.gain = leg.gain;
    cur.sos1a1 = leg.sos1a1;
    cur.sos1a2 = leg.sos1a2;
    cur.sos1b1 = leg.sos1b1;
    cur.sos1b2 = leg.sos1b2;
    cur.sos2a1 = leg.sos2a1;
    cur.sos2a2 = leg.sos2a2;
    cur.sos2b1 = leg.sos2b1;
    cur.sos2b2 = leg.sos2b2;
    return cur;
}

::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_ADAPTFILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.dLearningRate = leg.dLearningRate;
    cur.nRefChan1 = leg.nRefChan1;
    cur.nRefChan2 = leg.nRefChan2;
    return cur;
}

::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_REFELECFILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.nRefChan = leg.nRefChan;
    return cur;
}

::cbSCALING fromLegacy(const cbSCALING& leg, const TranslationContext& ctx) {
    ::cbSCALING cur{};
    cur.digmin = leg.digmin;
    cur.digmax = leg.digmax;
    cur.anamin = leg.anamin;
    cur.anamax = leg.anamax;
    cur.anagain = leg.anagain;
    copyArr(cur.anaunit, leg.anaunit);
    return cur;
}

::cbFILTDESC fromLegacy(const cbFILTDESC& leg, const TranslationContext& ctx) {
    ::cbFILTDESC cur{};
    cur.hpfreq = leg.hpfreq;
    cur.hporder = leg.hporder;
    cur.hptype = leg.hptype;
    cur.lpfreq = leg.lpfreq;
    cur.lporder = leg.lporder;
    cur.lptype = leg.lptype;
    return cur;
}

::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg, const TranslationContext& ctx) {
    ::cbMANUALUNITMAPPING cur{};
    cur.nOverride = leg.nOverride;
    copyArr(cur.afOrigin, leg.afOrigin);
    copyArr2D(cur.afShape, leg.afShape);
    cur.aPhi = leg.aPhi;
    cur.bValid = leg.bValid;
    return cur;
}

::cbHOOP fromLegacy(const cbHOOP& leg, const TranslationContext& ctx) {
    ::cbHOOP cur{};
    cur.valid = leg.valid;
    cur.time = leg.time;
    cur.min = leg.min;
    cur.max = leg.max;
    return cur;
}

::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_CHANINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.proc = leg.proc;
    cur.bank = leg.bank;
    cur.term = leg.term;
    cur.chancaps = leg.chancaps;
    cur.doutcaps = leg.doutcaps;
    cur.dinpcaps = leg.dinpcaps;
    cur.aoutcaps = leg.aoutcaps;
    cur.ainpcaps = leg.ainpcaps;
    cur.spkcaps = leg.spkcaps;
    cur.physcalin = fromLegacy(leg.physcalin, ctx);
    cur.phyfiltin = fromLegacy(leg.phyfiltin, ctx);
    cur.physcalout = fromLegacy(leg.physcalout, ctx);
    cur.phyfiltout = fromLegacy(leg.phyfiltout, ctx);
    copyArr(cur.label, leg.label);
    cur.userflags = leg.userflags;
    copyArr(cur.position, leg.position);
    cur.scalin = fromLegacy(leg.scalin, ctx);
    cur.scalout = fromLegacy(leg.scalout, ctx);
    cur.doutopts = leg.doutopts;
    cur.dinpopts = leg.dinpopts;
    cur.aoutopts = leg.aoutopts;
    cur.eopchar = leg.eopchar;
    cur.moninst = static_cast<uint16_t>((leg.monsource >> 16) & 0xFFFF); // aka lowsamples
    cur.monchan = static_cast<uint16_t>(leg.monsource & 0xFFFF); // aka highsamples
    cur.outvalue = leg.outvalue; // aka offset
    cur.trigtype = leg.trigtype;
    // skip reserved
    cur.triginst = 0; // TODO: VERIFY
    cur.trigchan = leg.trigchan;
    cur.trigval = leg.trigval;
    cur.ainpopts = leg.ainpopts;
    cur.lncrate = leg.lncrate;
    cur.smpfilter = leg.smpfilter;
    cur.smpgroup = leg.smpgroup;
    cur.smpdispmin = leg.smpdispmin;
    cur.smpdispmax = leg.smpdispmax;
    cur.spkfilter = leg.spkfilter;
    cur.spkdispmax = leg.spkdispmax;
    cur.lncdispmax = leg.lncdispmax;
    cur.spkopts = leg.spkopts;
    cur.spkthrlevel = leg.spkthrlevel;
    cur.spkthrlimit = leg.spkthrlimit;
    cur.spkgroup = leg.spkgroup;
    cur.amplrejpos = leg.amplrejpos;
    cur.amplrejneg = leg.amplrejneg;
    cur.refelecchan = leg.refelecchan;
    copyArr(cur.unitmapping, leg.unitmapping, fromLegacy, ctx);
    copyArr2D(cur.spkhoops, leg.spkhoops, fromLegacy, ctx);
    return cur;
}

::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg, const TranslationContext& ctx) {
    ::cbPKT_FS_BASIS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.fs = leg.fs;
    copyArr2D(cur.basis, leg.basis);
    return cur;
}

::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_MODELSET cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.unit_number = leg.unit_number;
    cur.valid = leg.valid;
    cur.inverted = leg.inverted;
    cur.num_samples = leg.num_samples;
    copyArr(cur.mu_x, leg.mu_x);
    copyArr2D(cur.Sigma_x, leg.Sigma_x);
    cur.determinant_Sigma_x = leg.determinant_Sigma_x;
    copyArr2D(cur.Sigma_x_inv, leg.Sigma_x_inv);
    cur.log_determinant_Sigma_x = leg.log_determinant_Sigma_x;
    cur.subcluster_spread_factor_numerator = leg.subcluster_spread_factor_numerator;
    cur.subcluster_spread_factor_denominator = leg.subcluster_spread_factor_denominator;
    cur.mu_e = leg.mu_e;
    cur.sigma_e_squared = leg.sigma_e_squared;
    return cur;
}

::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_DETECT cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.fThreshold = leg.fThreshold;
    cur.fMultiplier = leg.fMultiplier;
    return cur;
}

::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_ARTIF_REJECT cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.nMaxSimulChans = leg.nMaxSimulChans;
    cur.nRefractoryCount = leg.nRefractoryCount;
    return cur;
}

::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_NOISE_BOUNDARY cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    copyArr(cur.afc, leg.afc);
    copyArr2D(cur.afS, leg.afS);
    return cur;
}

::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_STATISTICS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.nUpdateSpikes = leg.nUpdateSpikes;
    cur.nAutoalg = leg.nAutoalg;
    cur.nMode = leg.nMode;
    cur.fMinClusterPairSpreadFactor = leg.fMinClusterPairSpreadFactor;
    cur.fMaxSubclusterSpreadFactor = leg.fMaxSubclusterSpreadFactor;
    cur.fMinClusterHistCorrMajMeasure = leg.fMinClusterHistCorrMajMeasure;
    cur.fMaxClusterPairHistCorrMajMeasure = leg.fMaxClusterPairHistCorrMajMeasure;
    cur.fClusterHistValleyPercentage = leg.fClusterHistValleyPercentage;
    cur.fClusterHistClosePeakPercentage = leg.fClusterHistClosePeakPercentage;
    cur.fClusterHistMinPeakPercentage = leg.fClusterHistMinPeakPercentage;
    cur.nWaveBasisSize = leg.nWaveBasisSize;
    cur.nWaveSampleSize = leg.nWaveSampleSize;
    return cur;
}

::cbAdaptControl fromLegacy(const cbAdaptControl& leg, const TranslationContext& ctx) {
    ::cbAdaptControl cur{};
    cur.nMode = leg.nMode;
    cur.fTimeOutMinutes = leg.fTimeOutMinutes;
    cur.fElapsedMinutes = leg.fElapsedMinutes;
    return cur;
}

::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg, const TranslationContext& ctx) {
    ::cbPKT_SS_STATUS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.cntlUnitStats = fromLegacy(leg.cntlUnitStats, ctx);
    cur.cntlNumUnits = fromLegacy(leg.cntlNumUnits, ctx);
    return cur;
}

::cbproto::SpikeSorting fromLegacy(const cbSPIKE_SORTING& leg, const TranslationContext& ctx) {
    ::cbproto::SpikeSorting cur{};
    copyArr(cur.basis, leg.asBasis, fromLegacy, ctx);
    copyArr2D(cur.models, leg.asSortModel, fromLegacy, ctx);
    cur.detect = fromLegacy(leg.pktDetect, ctx);
    cur.artifact_reject = fromLegacy(leg.pktArtifReject, ctx);
    copyArr(cur.noise_boundary, leg.pktNoiseBoundary, fromLegacy, ctx);
    cur.statistics = fromLegacy(leg.pktStatistics, ctx);
    cur.status = fromLegacy(leg.pktStatus, ctx);
    return cur;
}

::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg, const TranslationContext& ctx) {
    ::cbPKT_NTRODEINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.ntrode = leg.ntrode;
    copyArr(cur.label, leg.label);
    copyArr2D(cur.ellipses, leg.ellipses, fromLegacy, ctx);
    cur.nSite = leg.nSite;
    cur.fs = leg.fs;
    copyArr(cur.nChan, leg.nChan);
    return cur;
}

::cbWaveformData fromLegacy(const cbWaveformData& leg, const TranslationContext& ctx) {
    ::cbWaveformData cur{};
    cur.offset = leg.offset; // aka sineFrequency
    cur.seq = leg.seq; // aka sineAmplitude
    cur.seqTotal = leg.seqTotal;
    cur.phases = leg.phases;
    copyArr(cur.duration, leg.duration);
    copyArr(cur.amplitude, leg.amplitude);
    return cur;
}

::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg, const TranslationContext& ctx) {
    ::cbPKT_AOUT_WAVEFORM cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.repeats = leg.repeats;
    cur.trig = static_cast<uint8_t>((leg.trig >> 8) & 0xFF);
    cur.trigInst = static_cast<uint8_t>(leg.trig & 0xFF);
    cur.trigChan = leg.trigChan;
    cur.trigValue = leg.trigValue;
    cur.trigNum = leg.trigNum;
    cur.active = leg.active;
    cur.wave = fromLegacy(leg.wave, ctx);
    return cur;
}

::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg, const TranslationContext& ctx) {
    ::cbPKT_LNC cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.lncFreq = leg.lncFreq;
    cur.lncRefChan = leg.lncRefChan;
    cur.lncGlobalMode = leg.lncGlobalMode;
    return cur;
}

::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg, const TranslationContext& ctx) {
    ::cbPKT_NPLAY cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.ftime = leg.ftime; // aka opt
    cur.stime = leg.stime;
    cur.etime = leg.etime;
    cur.val = leg.val;
    cur.mode = leg.mode;
    cur.flags = leg.flags;
    cur.speed = leg.speed;
    copyArr(cur.fname, leg.fname);
    return cur;
}

::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg, const TranslationContext& ctx) {
    ::cbVIDEOSOURCE cur{};
    copyArr(cur.name, leg.name);
    cur.fps = leg.fps;
    return cur;
}

::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg, const TranslationContext& ctx) {
    ::cbTRACKOBJ cur{};
    copyArr(cur.name, leg.name);
    cur.type = leg.type;
    cur.pointCount = leg.pointCount;
    return cur;
}

::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg, const TranslationContext& ctx) {
    ::cbPKT_FILECFG cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.options = leg.options;
    cur.duration = leg.duration;
    cur.recording = leg.recording;
    cur.extctrl = leg.extctrl;
    copyArr(cur.username, leg.username);
    copyArr(cur.filename, leg.filename); // aka datetime
    copyArr(cur.comment, leg.comment);
    return cur;
}

NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg, const TranslationContext& ctx) {
    // TODO: VERIFY that each list that's assumed to be instrument-independent is in fact independent from any particular instrument.
    NativeConfigBuffer cur{};
    cur.version = leg.version;
    cur.sysflags = leg.sysflags;
    cur.instrument_status = static_cast<typeof(cur.instrument_status)>(InstrumentStatus::ACTIVE);
    cur.sysinfo = fromLegacy(leg.sysinfo, ctx);
    cur.procinfo = fromLegacy(leg.procinfo[ctx.instrument_idx], ctx);
    copyArr(cur.bankinfo, leg.bankinfo[ctx.instrument_idx], fromLegacy, ctx);
    copyArr(cur.groupinfo, leg.groupinfo[ctx.instrument_idx], fromLegacy, ctx);
    copyArr(cur.filtinfo, leg.filtinfo[ctx.instrument_idx], fromLegacy, ctx);
    cur.adaptinfo = fromLegacy(leg.adaptinfo[ctx.instrument_idx], ctx);
    cur.refelecinfo = fromLegacy(leg.refelecinfo[ctx.instrument_idx], ctx);
    copyArr(cur.chaninfo, leg.chaninfo, fromLegacy, ctx);
    copyArr(cur.asBasis, leg.isSortingOptions.asBasis, fromLegacy, ctx);
    copyArr2D(cur.asSortModel, leg.isSortingOptions.asSortModel, fromLegacy, ctx);
    // TODO: Move native isSortingOptions fields into a struct
    cur.pktDetect = fromLegacy(leg.isSortingOptions.pktDetect, ctx);
    cur.pktArtifReject = fromLegacy(leg.isSortingOptions.pktArtifReject, ctx);
    copyArr(cur.pktNoiseBoundary, leg.isSortingOptions.pktNoiseBoundary, fromLegacy, ctx);
    cur.pktStatistics = fromLegacy(leg.isSortingOptions.pktStatistics, ctx);
    cur.pktStatus = fromLegacy(leg.isSortingOptions.pktStatus, ctx);
    copyArr(cur.isNTrodeInfo, leg.isNTrodeInfo, fromLegacy, ctx);
    copyArr2D(cur.isWaveform, leg.isWaveform, fromLegacy, ctx);
    cur.isLnc = fromLegacy(leg.isLnc[ctx.instrument_idx], ctx);
    cur.isNPlay = fromLegacy(leg.isNPlay, ctx);
    copyArr(cur.isVideoSource, leg.isVideoSource, fromLegacy, ctx);
    copyArr(cur.isTrackObj, leg.isTrackObj, fromLegacy, ctx);
    cur.fileinfo = fromLegacy(leg.fileinfo, ctx);

    return cur;
}

NativeNSPStatus fromLegacy(const NSPStatus& leg, const TranslationContext& ctx) {
    switch(leg) {
        case NSPStatus::NSP_INIT:
            return NativeNSPStatus::NSP_INIT;
        case NSPStatus::NSP_NOIPADDR:
            return NativeNSPStatus::NSP_NOIPADDR;
        case NSPStatus::NSP_NOREPLY:
            return NativeNSPStatus::NSP_NOREPLY;
        case NSPStatus::NSP_FOUND:
            return NativeNSPStatus::NSP_FOUND;
        case NSPStatus::NSP_INVALID:
        default:
            return NativeNSPStatus::NSP_INVALID;
    }
}

::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg, const TranslationContext& ctx) {
    ::cbPKT_UNIT_SELECTION cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header, ctx);
    cur.lastchan = leg.lastchan;
    copyArr(cur.abyUnitSelections, leg.abyUnitSelections);
    return cur;
}

NativePCStatus fromLegacy(const cbPcStatus& leg, const TranslationContext& ctx) {
    NativePCStatus cur{};
    cur.isSelection = fromLegacy(leg.isSelection[ctx.instrument_idx], ctx);
    cur.m_iBlockRecording = leg.m_iBlockRecording;
    cur.m_nPCStatusFlags = leg.m_nPCStatusFlags;
    cur.m_nNumFEChans = leg.m_nNumFEChans;
    cur.m_nNumAnainChans = leg.m_nNumAnainChans;
    cur.m_nNumAnalogChans = leg.m_nNumAnalogChans;
    cur.m_nNumAoutChans = leg.m_nNumAoutChans;
    cur.m_nNumAudioChans = leg.m_nNumAudioChans;
    cur.m_nNumAnalogoutChans = leg.m_nNumAnalogoutChans;
    cur.m_nNumDiginChans = leg.m_nNumDiginChans;
    cur.m_nNumSerialChans = leg.m_nNumSerialChans;
    cur.m_nNumDigoutChans = leg.m_nNumDigoutChans;
    cur.m_nNumTotalChans = leg.m_nNumTotalChans;
    cur.m_nNspStatus = fromLegacy(leg.m_nNspStatus[ctx.instrument_idx], ctx);
    cur.m_nNumNTrodesPerInstrument = leg.m_nNumNTrodesPerInstrument[ctx.instrument_idx];
    cur.m_nGeminiSystem = leg.m_nGeminiSystem;
    // ignore APP_WORKSPACE
    return cur;
}

NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg, const TranslationContext& ctx) {
    NativeReceiveBuffer cur{};
    cur.received = leg.received;
    cur.lasttime = leg.lasttime;
    cur.headwrap = leg.headwrap;
    cur.headindex = leg.headindex;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg, const TranslationContext& ctx) {
    NativeTransmitBuffer cur{};
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg, const TranslationContext& ctx) {
    NativeTransmitBufferLocal cur{};
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur, const TranslationContext& ctx) {
    cbPKT_HEADER leg{};
    leg.time = cur.time;
    leg.chid = cur.chid;
    leg.type = static_cast<uint8_t>(cur.type);
    leg.dlen = cur.dlen;
    leg.instrument = cur.instrument;
    leg.reserved = cur.reserved;
    return leg;
}

cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur, const TranslationContext& ctx) {
    cbPKT_SYSINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.sysfreq = cur.sysfreq;
    leg.spikelen = cur.spikelen;
    leg.spikepre = cur.spikepre;
    leg.resetque = cur.resetque;
    leg.runlevel = cur.runlevel;
    leg.runflags = cur.runflags;
    return leg;
}

cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur, const TranslationContext& ctx) {
    cbPKT_PROCINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.proc = cur.proc;
    leg.idcode = cur.idcode;
    leg.idcode = cur.idcode;
    copyArr(leg.ident, cur.ident);
    leg.chanbase = cur.chanbase;
    leg.chancount = cur.chancount;
    leg.bankcount = cur.bankcount;
    leg.groupcount = cur.groupcount;
    leg.filtcount = cur.filtcount;
    leg.sortcount = cur.sortcount;
    leg.unitcount = cur.unitcount;
    leg.hoopcount = cur.hoopcount;
    leg.reserved = cur.reserved;
    leg.version = cur.version;
    return leg;
}

cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur, const TranslationContext& ctx) {
    cbPKT_BANKINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.proc = cur.proc;
    leg.bank = cur.bank;
    leg.idcode = cur.idcode;
    copyArr(leg.ident, cur.ident);
    copyArr(leg.label, cur.label);
    leg.chanbase = cur.chanbase;
    leg.chancount = cur.chancount;
    return leg;
}

cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur, const TranslationContext& ctx) {
    cbPKT_GROUPINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.proc = cur.proc;
    leg.group = cur.group;
    copyArr(leg.label, cur.label);
    leg.period = cur.period;
    leg.length = cur.length;
    copyArr(leg.list, cur.list);
    return leg;
}

cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur, const TranslationContext& ctx) {
    cbPKT_FILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.proc = cur.proc;
    leg.filt = cur.filt;
    copyArr(leg.label, cur.label);
    leg.hpfreq = cur.hpfreq;
    leg.hporder = cur.hporder;
    leg.hptype = cur.hptype;
    leg.lpfreq = cur.lpfreq;
    leg.lporder = cur.lporder;
    leg.lptype = cur.lptype;
    leg.gain = cur.gain;
    leg.sos1a1 = cur.sos1a1;
    leg.sos1a2 = cur.sos1a2;
    leg.sos1b1 = cur.sos1b1;
    leg.sos1b2 = cur.sos1b2;
    leg.sos2a1 = cur.sos2a1;
    leg.sos2a2 = cur.sos2a2;
    leg.sos2b1 = cur.sos2b1;
    leg.sos2b2 = cur.sos2b2;
    return leg;
}

cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur, const TranslationContext& ctx) {
    cbPKT_ADAPTFILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.dLearningRate = cur.dLearningRate;
    leg.nRefChan1 = cur.nRefChan1;
    leg.nRefChan2 = cur.nRefChan2;
    return leg;
}

cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur, const TranslationContext& ctx) {
    cbPKT_REFELECFILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.nRefChan = cur.nRefChan;
    return leg;
}

cbSCALING toLegacy(const ::cbSCALING& cur, const TranslationContext& ctx) {
    cbSCALING leg{};
    leg.digmin = cur.digmin;
    leg.digmax = cur.digmax;
    leg.anamin = cur.anamin;
    leg.anamax = cur.anamax;
    leg.anagain = cur.anagain;
    copyArr(leg.anaunit, cur.anaunit);
    return leg;
}

cbFILTDESC toLegacy(const ::cbFILTDESC& cur, const TranslationContext& ctx) {
    cbFILTDESC leg{};
    leg.hpfreq = cur.hpfreq;
    leg.hporder = cur.hporder;
    leg.hptype = cur.hptype;
    leg.lpfreq = cur.lpfreq;
    leg.lporder = cur.lporder;
    leg.lptype = cur.lptype;
    return leg;
}

cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur, const TranslationContext& ctx) {
    cbMANUALUNITMAPPING leg{};
    leg.nOverride = cur.nOverride;
    copyArr(leg.afOrigin, cur.afOrigin);
    copyArr2D(leg.afShape, cur.afShape);
    leg.aPhi = cur.aPhi;
    leg.bValid = cur.bValid;
    return leg;
}

cbHOOP toLegacy(const ::cbHOOP& cur, const TranslationContext& ctx) {
    cbHOOP leg{};
    leg.valid = cur.valid;
    leg.time = cur.time;
    leg.min = cur.min;
    leg.max = cur.max;
    return leg;
}

cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur, const TranslationContext& ctx) {
    cbPKT_CHANINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.proc = cur.proc;
    leg.bank = cur.bank;
    leg.term = cur.term;
    leg.chancaps = cur.chancaps;
    leg.doutcaps = cur.doutcaps;
    leg.dinpcaps = cur.dinpcaps;
    leg.aoutcaps = cur.aoutcaps;
    leg.ainpcaps = cur.ainpcaps;
    leg.spkcaps = cur.spkcaps;
    leg.physcalin = toLegacy(cur.physcalin, ctx);
    leg.phyfiltin = toLegacy(cur.phyfiltin, ctx);
    leg.physcalout = toLegacy(cur.physcalout, ctx);
    leg.phyfiltout = toLegacy(cur.phyfiltout, ctx);
    copyArr(leg.label, cur.label);
    leg.userflags = cur.userflags;
    copyArr(leg.position, cur.position);
    leg.scalin = toLegacy(cur.scalin, ctx);
    leg.scalout = toLegacy(cur.scalout, ctx);
    leg.doutopts = cur.doutopts;
    leg.dinpopts = cur.dinpopts;
    leg.aoutopts = cur.aoutopts;
    leg.eopchar = cur.eopchar;
    leg.monsource = (static_cast<uint32_t>(cur.moninst) << 16) | static_cast<uint32_t>(cur.monchan); // aka highsamples and lowsamples
    leg.outvalue = cur.outvalue; // aka offset
    leg.trigtype = cur.trigtype;
    leg.trigchan = cur.trigchan;
    leg.trigval = cur.trigval;
    leg.ainpopts = cur.ainpopts;
    leg.lncrate = cur.lncrate;
    leg.smpfilter = cur.smpfilter;
    leg.smpgroup = cur.smpgroup;
    leg.smpdispmin = cur.smpdispmin;
    leg.smpdispmax = cur.smpdispmax;
    leg.spkfilter = cur.spkfilter;
    leg.spkdispmax = cur.spkdispmax;
    leg.lncdispmax = cur.lncdispmax;
    leg.spkopts = cur.spkopts;
    leg.spkthrlevel = cur.spkthrlevel;
    leg.spkthrlimit = cur.spkthrlimit;
    leg.spkgroup = cur.spkgroup;
    leg.amplrejpos = cur.amplrejpos;
    leg.amplrejneg = cur.amplrejneg;
    leg.refelecchan = cur.refelecchan;
    copyArr(leg.unitmapping, cur.unitmapping, toLegacy, ctx);
    copyArr2D(leg.spkhoops, cur.spkhoops, toLegacy, ctx);
    return leg;
}

cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur, const TranslationContext& ctx) {
    cbPKT_FS_BASIS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.fs = cur.fs;
    copyArr2D(leg.basis, cur.basis);
    return leg;
}

cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur, const TranslationContext& ctx) {
    cbPKT_SS_MODELSET leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.unit_number = cur.unit_number;
    leg.valid = cur.valid;
    leg.inverted = cur.inverted;
    leg.num_samples = cur.num_samples;
    copyArr(leg.mu_x, cur.mu_x);
    copyArr2D(leg.Sigma_x, cur.Sigma_x);
    leg.determinant_Sigma_x = cur.determinant_Sigma_x;
    copyArr2D(leg.Sigma_x_inv, cur.Sigma_x_inv);
    leg.log_determinant_Sigma_x = cur.log_determinant_Sigma_x;
    leg.subcluster_spread_factor_numerator = cur.subcluster_spread_factor_numerator;
    leg.subcluster_spread_factor_denominator = cur.subcluster_spread_factor_denominator;
    leg.mu_e = cur.mu_e;
    leg.sigma_e_squared = cur.sigma_e_squared;
    return leg;
}

cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur, const TranslationContext& ctx) {
    cbPKT_SS_DETECT leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.fThreshold = cur.fThreshold;
    leg.fMultiplier = cur.fMultiplier;
    return leg;
}

cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur, const TranslationContext& ctx) {
    cbPKT_SS_ARTIF_REJECT leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.nMaxSimulChans = cur.nMaxSimulChans;
    leg.nRefractoryCount = cur.nRefractoryCount;
    return leg;
}

cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur, const TranslationContext& ctx) {
    cbPKT_SS_NOISE_BOUNDARY leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    copyArr(leg.afc, cur.afc);
    copyArr2D(leg.afS, cur.afS);
    return leg;
}

cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur, const TranslationContext& ctx) {
    cbPKT_SS_STATISTICS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.nUpdateSpikes = cur.nUpdateSpikes;
    leg.nAutoalg = cur.nAutoalg;
    leg.nMode = cur.nMode;
    leg.fMinClusterPairSpreadFactor = cur.fMinClusterPairSpreadFactor;
    leg.fMaxSubclusterSpreadFactor = cur.fMaxSubclusterSpreadFactor;
    leg.fMinClusterHistCorrMajMeasure = cur.fMinClusterHistCorrMajMeasure;
    leg.fMaxClusterPairHistCorrMajMeasure = cur.fMaxClusterPairHistCorrMajMeasure;
    leg.fClusterHistValleyPercentage = cur.fClusterHistValleyPercentage;
    leg.fClusterHistClosePeakPercentage = cur.fClusterHistClosePeakPercentage;
    leg.fClusterHistMinPeakPercentage = cur.fClusterHistMinPeakPercentage;
    leg.nWaveBasisSize = cur.nWaveBasisSize;
    leg.nWaveSampleSize = cur.nWaveSampleSize;
    return leg;
}

cbAdaptControl toLegacy(const ::cbAdaptControl& cur, const TranslationContext& ctx) {
    cbAdaptControl leg{};
    leg.nMode = cur.nMode;
    leg.fTimeOutMinutes = cur.fTimeOutMinutes;
    leg.fElapsedMinutes = cur.fElapsedMinutes;
    return leg;
}

cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur, const TranslationContext& ctx) {
    cbPKT_SS_STATUS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.cntlUnitStats = toLegacy(cur.cntlUnitStats, ctx);
    leg.cntlNumUnits = toLegacy(cur.cntlNumUnits, ctx);
    return leg;
}

cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur, const TranslationContext& ctx) {
    cbSPIKE_SORTING leg{};
    copyArr(leg.asBasis, cur.basis, toLegacy, ctx);
    copyArr2D(leg.asSortModel, cur.models, toLegacy, ctx);
    leg.pktDetect = toLegacy(cur.detect, ctx);
    leg.pktArtifReject = toLegacy(cur.artifact_reject, ctx);
    copyArr(leg.pktNoiseBoundary, cur.noise_boundary, toLegacy, ctx);
    leg.pktStatistics = toLegacy(cur.statistics, ctx);
    leg.pktStatus = toLegacy(cur.status, ctx);
    return leg;
}

cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur, const TranslationContext& ctx) {
    cbPKT_NTRODEINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.ntrode = cur.ntrode;
    copyArr(leg.label, cur.label);
    copyArr2D(leg.ellipses, cur.ellipses, toLegacy, ctx);
    leg.nSite = cur.nSite;
    leg.fs = cur.fs;
    copyArr(leg.nChan, cur.nChan);
    return leg;
}

cbWaveformData toLegacy(const ::cbWaveformData& cur, const TranslationContext& ctx) {
    cbWaveformData leg{};
    leg.offset = cur.offset; // aka sineFrequency
    leg.seq = cur.seq; // aka sineAmplitude
    leg.seqTotal = cur.seqTotal;
    leg.phases = cur.phases;
    copyArr(leg.duration, cur.duration);
    copyArr(leg.amplitude, cur.amplitude);
    return leg;
}

cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur, const TranslationContext& ctx) {
    cbPKT_AOUT_WAVEFORM leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.repeats = cur.repeats;
    leg.trig = (static_cast<uint16_t>(cur.trig) << 8) | static_cast<uint16_t>(cur.trigInst);
    leg.trigChan = cur.trigChan;
    leg.trigValue = cur.trigValue;
    leg.trigNum = cur.trigNum;
    leg.active = cur.active;
    leg.wave = toLegacy(cur.wave, ctx);
    return leg;
}

cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur, const TranslationContext& ctx) {
    cbPKT_LNC leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.lncFreq = cur.lncFreq;
    leg.lncRefChan = cur.lncRefChan;
    leg.lncGlobalMode = cur.lncGlobalMode;
    return leg;
}

cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur, const TranslationContext& ctx) {
    cbPKT_NPLAY leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.ftime = cur.ftime; // aka opt
    leg.stime = cur.stime;
    leg.etime = cur.etime;
    leg.val = cur.val;
    leg.mode = cur.mode;
    leg.flags = cur.flags;
    leg.speed = cur.speed;
    copyArr(leg.fname, cur.fname);
    return leg;
}

cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur, const TranslationContext& ctx) {
    cbVIDEOSOURCE leg{};
    copyArr(leg.name, cur.name);
    leg.fps = cur.fps;
    return leg;
}

cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur, const TranslationContext& ctx) {
    cbTRACKOBJ leg{};
    copyArr(leg.name, cur.name);
    leg.type = cur.type;
    leg.pointCount = cur.pointCount;
    return leg;
}

cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur, const TranslationContext& ctx) {
    cbPKT_FILECFG leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.options = cur.options;
    leg.duration = cur.duration;
    leg.recording = cur.recording;
    leg.extctrl = cur.extctrl;
    copyArr(leg.username, cur.username);
    copyArr(leg.filename, cur.filename); // aka datetime
    copyArr(leg.comment, cur.comment);
    return leg;
}

NSPStatus toLegacy(const NativeNSPStatus& cur, const TranslationContext& ctx) {
    switch(cur) {
        case NativeNSPStatus::NSP_INIT:
            return NSPStatus::NSP_INIT;
        case NativeNSPStatus::NSP_NOIPADDR:
            return NSPStatus::NSP_NOIPADDR;
        case NativeNSPStatus::NSP_NOREPLY:
            return NSPStatus::NSP_NOREPLY;
        case NativeNSPStatus::NSP_FOUND:
            return NSPStatus::NSP_FOUND;
        case NativeNSPStatus::NSP_INVALID:
        default:
            return NSPStatus::NSP_INVALID;
    }
}

cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur, const TranslationContext& ctx) {
    cbPKT_UNIT_SELECTION leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header, ctx);
    leg.lastchan = cur.lastchan;
    copyArr(leg.abyUnitSelections, cur.abyUnitSelections);
    return leg;
}

cbPcStatus toLegacy(const NativePCStatus& cur, const TranslationContext& ctx) {
    cbPcStatus leg{};
    leg.isSelection[ctx.instrument_idx] = toLegacy(cur.isSelection, ctx);
    leg.m_iBlockRecording = cur.m_iBlockRecording;
    leg.m_nPCStatusFlags = cur.m_nPCStatusFlags;
    leg.m_nNumFEChans = cur.m_nNumFEChans;
    leg.m_nNumAnainChans = cur.m_nNumAnainChans;
    leg.m_nNumAnalogChans = cur.m_nNumAnalogChans;
    leg.m_nNumAoutChans = cur.m_nNumAoutChans;
    leg.m_nNumAudioChans = cur.m_nNumAudioChans;
    leg.m_nNumAnalogoutChans = cur.m_nNumAnalogoutChans;
    leg.m_nNumDiginChans = cur.m_nNumDiginChans;
    leg.m_nNumSerialChans = cur.m_nNumSerialChans;
    leg.m_nNumDigoutChans = cur.m_nNumDigoutChans;
    leg.m_nNumTotalChans = cur.m_nNumTotalChans;
    leg.m_nNspStatus[ctx.instrument_idx] = toLegacy(cur.m_nNspStatus, ctx);
    leg.m_nNumNTrodesPerInstrument[ctx.instrument_idx] = cur.m_nNumNTrodesPerInstrument;
    leg.m_nGeminiSystem = cur.m_nGeminiSystem;
    // ignore APP_WORKSPACE
    return leg;
}

cbRECBUFF toLegacy(const NativeReceiveBuffer& cur, const TranslationContext& ctx) {
    cbRECBUFF leg{};
    leg.received = cur.received;
    leg.lasttime = cur.lasttime;
    leg.headwrap = cur.headwrap;
    leg.headindex = cur.headindex;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur, const TranslationContext& ctx) {
    cbXMTBUFF leg{};
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur, const TranslationContext& ctx) {
    cbXMTBUFFLOCAL leg{};
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

} // namespace central_v4_0

} // namespace cbshm
