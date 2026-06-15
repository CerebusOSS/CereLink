///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v3_11.cpp
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Central adapter implementation
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbshm/central_types/adapters.h>
#include <cbshm/central_types/v3_11.h>

namespace cbshm {

namespace central_v3_11 {

size_t BootstrapAdapter::getConfigBufferSize() const {
    return sizeof(cbCFGBUFF);
}

size_t BootstrapAdapter::getReceiveBufferSize() const {
    return sizeof(cbRECBUFF);
}

size_t BootstrapAdapter::getTransmitBufferSize() const {
    return sizeof(cbXMTBUFF);
}

size_t BootstrapAdapter::getTransmitBufferLocalSize() const {
    return sizeof(cbXMTBUFFLOCAL);
}

size_t BootstrapAdapter::getStatusBufferSize() const {
    return sizeof(cbPcStatus);
}

size_t BootstrapAdapter::getSpikeBufferSize() const {
    return sizeof(cbSPKBUFF);
}

size_t BootstrapAdapter::getReceiveBufferLen() const {
    return sizeof(cbRECBUFFLEN);
}

::cbPKT_HEADER Adapter::fromLegacy(const cbPKT_HEADER& leg) const {
    ::cbPKT_HEADER cur{};
    cur.time = leg.time; // TODO: explicit or implicit conversion here (?) (and for all PROCTIME)
    cur.chid = leg.chid;
    cur.type = static_cast<uint16_t>(leg.type);
    cur.dlen = static_cast<uint16_t>(leg.dlen);
    cur.instrument = 0;
    cur.reserved = 0;
    return cur;
}

::cbPKT_SYSINFO Adapter::fromLegacy(const cbPKT_SYSINFO& leg) const {
    ::cbPKT_SYSINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.sysfreq = leg.sysfreq;
    cur.spikelen = leg.spikelen;
    cur.spikepre = leg.spikepre;
    cur.resetque = leg.resetque;
    cur.runlevel = leg.runlevel;
    cur.runflags = leg.runflags;
    return cur;
}

::cbPKT_PROCINFO Adapter::fromLegacy(const cbPKT_PROCINFO& leg) const {
    ::cbPKT_PROCINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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
    cur.reserved = leg.sortmethod;
    cur.version = leg.version;
    return cur;
}

::cbPKT_BANKINFO Adapter::fromLegacy(const cbPKT_BANKINFO& leg) const {
    ::cbPKT_BANKINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.proc = leg.proc;
    cur.bank = leg.bank;
    cur.idcode = leg.idcode;
    copyArr(cur.ident, leg.ident);
    copyArr(cur.label, leg.label);
    cur.chanbase = leg.chanbase;
    cur.chancount = leg.chancount;
    return cur;
}

::cbPKT_GROUPINFO Adapter::fromLegacy(const cbPKT_GROUPINFO& leg) const {
    ::cbPKT_GROUPINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.proc = leg.proc;
    cur.group = leg.group;
    copyArr(cur.label, leg.label);
    cur.period = leg.period;
    cur.length = leg.length;
    copyArr(cur.list, leg.list);
    return cur;
}

::cbPKT_FILTINFO Adapter::fromLegacy(const cbPKT_FILTINFO& leg) const {
    ::cbPKT_FILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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

::cbPKT_ADAPTFILTINFO Adapter::fromLegacy(const cbPKT_ADAPTFILTINFO& leg) const {
    ::cbPKT_ADAPTFILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.dLearningRate = leg.dLearningRate;
    cur.nRefChan1 = leg.nRefChan1;
    cur.nRefChan2 = leg.nRefChan2;
    return cur;
}

::cbPKT_REFELECFILTINFO Adapter::fromLegacy(const cbPKT_REFELECFILTINFO& leg) const {
    ::cbPKT_REFELECFILTINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.nRefChan = leg.nRefChan;
    return cur;
}

::cbSCALING Adapter::fromLegacy(const cbSCALING& leg) const {
    ::cbSCALING cur{};
    cur.digmin = leg.digmin;
    cur.digmax = leg.digmax;
    cur.anamin = leg.anamin;
    cur.anamax = leg.anamax;
    cur.anagain = leg.anagain;
    copyArr(cur.anaunit, leg.anaunit);
    return cur;
}

::cbFILTDESC Adapter::fromLegacy(const cbFILTDESC& leg) const {
    ::cbFILTDESC cur{};
    cur.hpfreq = leg.hpfreq;
    cur.hporder = leg.hporder;
    cur.hptype = leg.hptype;
    cur.lpfreq = leg.lpfreq;
    cur.lporder = leg.lporder;
    cur.lptype = leg.lptype;
    return cur;
}

::cbMANUALUNITMAPPING Adapter::fromLegacy(const cbMANUALUNITMAPPING& leg) const {
    ::cbMANUALUNITMAPPING cur{};
    cur.nOverride = leg.nOverride;
    copyArr(cur.afOrigin, leg.afOrigin);
    copyArr2D(cur.afShape, leg.afShape);
    cur.aPhi = leg.aPhi;
    cur.bValid = leg.bValid;
    return cur;
}

::cbHOOP Adapter::fromLegacy(const cbHOOP& leg) const {
    ::cbHOOP cur{};
    cur.valid = leg.valid;
    cur.time = leg.time;
    cur.min = leg.min;
    cur.max = leg.max;
    return cur;
}

::cbPKT_CHANINFO Adapter::fromLegacy(const cbPKT_CHANINFO& leg) const {
    ::cbPKT_CHANINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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
    cur.physcalin = fromLegacy(leg.physcalin);
    cur.phyfiltin = fromLegacy(leg.phyfiltin);
    cur.physcalout = fromLegacy(leg.physcalout);
    cur.phyfiltout = fromLegacy(leg.phyfiltout);
    copyArr(cur.label, leg.label);
    cur.userflags = leg.userflags;
    copyArr(cur.position, leg.position);
    cur.scalin = fromLegacy(leg.scalin);
    cur.scalout = fromLegacy(leg.scalout);
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
    copyArr(cur.unitmapping, leg.unitmapping, this, &Adapter::fromLegacy);
    copyArr2D(cur.spkhoops, leg.spkhoops, this, &Adapter::fromLegacy);
    return cur;
}

::cbPKT_FS_BASIS Adapter::fromLegacy(const cbPKT_FS_BASIS& leg) const {
    ::cbPKT_FS_BASIS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.fs = leg.fs;
    copyArr2D(cur.basis, leg.basis);
    return cur;
}

::cbPKT_SS_MODELSET Adapter::fromLegacy(const cbPKT_SS_MODELSET& leg) const {
    ::cbPKT_SS_MODELSET cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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

::cbPKT_SS_DETECT Adapter::fromLegacy(const cbPKT_SS_DETECT& leg) const {
    ::cbPKT_SS_DETECT cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.fThreshold = leg.fThreshold;
    cur.fMultiplier = leg.fMultiplier;
    return cur;
}

::cbPKT_SS_ARTIF_REJECT Adapter::fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg) const {
    ::cbPKT_SS_ARTIF_REJECT cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.nMaxSimulChans = leg.nMaxSimulChans;
    cur.nRefractoryCount = leg.nRefractoryCount;
    return cur;
}

::cbPKT_SS_NOISE_BOUNDARY Adapter::fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg) const {
    ::cbPKT_SS_NOISE_BOUNDARY cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.chan = leg.chan;
    copyArr(cur.afc, leg.afc);
    copyArr2D(cur.afS, leg.afS);
    return cur;
}

::cbPKT_SS_STATISTICS Adapter::fromLegacy(const cbPKT_SS_STATISTICS& leg) const {
    ::cbPKT_SS_STATISTICS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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

::cbAdaptControl Adapter::fromLegacy(const cbAdaptControl& leg) const {
    ::cbAdaptControl cur{};
    cur.nMode = leg.nMode;
    cur.fTimeOutMinutes = leg.fTimeOutMinutes;
    cur.fElapsedMinutes = leg.fElapsedMinutes;
    return cur;
}

::cbPKT_SS_STATUS Adapter::fromLegacy(const cbPKT_SS_STATUS& leg) const {
    ::cbPKT_SS_STATUS cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.cntlUnitStats = fromLegacy(leg.cntlUnitStats);
    cur.cntlNumUnits = fromLegacy(leg.cntlNumUnits);
    return cur;
}

::cbproto::SpikeSorting Adapter::fromLegacy(const cbSPIKE_SORTING& leg) const {
    ::cbproto::SpikeSorting cur{};
    copyArr(cur.basis, leg.asBasis, this, &Adapter::fromLegacy);
    copyArr2D(cur.models, leg.asSortModel, this, &Adapter::fromLegacy);
    cur.detect = fromLegacy(leg.pktDetect);
    cur.artifact_reject = fromLegacy(leg.pktArtifReject);
    copyArr(cur.noise_boundary, leg.pktNoiseBoundary, this, &Adapter::fromLegacy);
    cur.statistics = fromLegacy(leg.pktStatistics);
    cur.status = fromLegacy(leg.pktStatus);
    return cur;
}

::cbPKT_NTRODEINFO Adapter::fromLegacy(const cbPKT_NTRODEINFO& leg) const {
    ::cbPKT_NTRODEINFO cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.ntrode = leg.ntrode;
    copyArr(cur.label, leg.label);
    copyArr2D(cur.ellipses, leg.ellipses, this, &Adapter::fromLegacy);
    cur.nSite = leg.nSite;
    cur.fs = leg.fs;
    copyArr(cur.nChan, leg.nChan);
    return cur;
}

::cbWaveformData Adapter::fromLegacy(const cbWaveformData& leg) const {
    ::cbWaveformData cur{};
    cur.offset = leg.offset; // aka sineFrequency
    cur.seq = leg.seq; // aka sineAmplitude
    cur.seqTotal = leg.seqTotal;
    cur.phases = leg.phases;
    copyArr(cur.duration, leg.duration);
    copyArr(cur.amplitude, leg.amplitude);
    return cur;
}

::cbPKT_AOUT_WAVEFORM Adapter::fromLegacy(const cbPKT_AOUT_WAVEFORM& leg) const {
    ::cbPKT_AOUT_WAVEFORM cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.repeats = leg.repeats;
    cur.trig = static_cast<uint8_t>((leg.trig >> 8) & 0xFF);
    cur.trigInst = static_cast<uint8_t>(leg.trig & 0xFF);
    cur.trigChan = leg.trigChan;
    cur.trigValue = leg.trigValue;
    cur.trigNum = leg.trigNum;
    cur.active = leg.active;
    cur.wave = fromLegacy(leg.wave);
    return cur;
}

::cbPKT_LNC Adapter::fromLegacy(const cbPKT_LNC& leg) const {
    ::cbPKT_LNC cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.lncFreq = leg.lncFreq;
    cur.lncRefChan = leg.lncRefChan;
    cur.lncGlobalMode = leg.lncGlobalMode;
    return cur;
}

::cbPKT_NPLAY Adapter::fromLegacy(const cbPKT_NPLAY& leg) const {
    ::cbPKT_NPLAY cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
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

::cbVIDEOSOURCE Adapter::fromLegacy(const cbVIDEOSOURCE& leg) const {
    ::cbVIDEOSOURCE cur{};
    copyArr(cur.name, leg.name);
    cur.fps = leg.fps;
    return cur;
}

::cbTRACKOBJ Adapter::fromLegacy(const cbTRACKOBJ& leg) const {
    ::cbTRACKOBJ cur{};
    copyArr(cur.name, leg.name);
    cur.type = leg.type;
    cur.pointCount = leg.pointCount;
    return cur;
}

::cbPKT_FILECFG Adapter::fromLegacy(const cbPKT_FILECFG& leg) const {
    ::cbPKT_FILECFG cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.options = leg.options;
    cur.duration = leg.duration;
    cur.recording = leg.recording;
    cur.extctrl = leg.extctrl;
    copyArr(cur.username, leg.username);
    copyArr(cur.filename, leg.filename); // aka datetime
    copyArr(cur.comment, leg.comment);
    return cur;
}

NativeConfigBuffer Adapter::fromLegacy(const cbCFGBUFF& leg) const {
    // TODO: VERIFY that each list that's assumed to be instrument-independent is in fact independent from any particular instrument.
    NativeConfigBuffer cur{};
    cur.version = leg.version;
    cur.sysflags = leg.sysflags;
    cur.instrument_status = static_cast<typeof(cur.instrument_status)>(InstrumentStatus::ACTIVE);
    cur.sysinfo = fromLegacy(leg.sysinfo);
    cur.procinfo = fromLegacy(leg.procinfo[instrument_idx]);
    copyArr(cur.bankinfo, leg.bankinfo[instrument_idx], this, &Adapter::fromLegacy);
    copyArr(cur.groupinfo, leg.groupinfo[instrument_idx], this, &Adapter::fromLegacy);
    copyArr(cur.filtinfo, leg.filtinfo[instrument_idx], this, &Adapter::fromLegacy);
    cur.adaptinfo = fromLegacy(leg.adaptinfo[instrument_idx]);
    cur.refelecinfo = fromLegacy(leg.refelecinfo[instrument_idx]);
    copyArr(cur.chaninfo, leg.chaninfo, this, &Adapter::fromLegacy);
    copyArr(cur.asBasis, leg.isSortingOptions.asBasis, this, &Adapter::fromLegacy);
    copyArr2D(cur.asSortModel, leg.isSortingOptions.asSortModel, this, &Adapter::fromLegacy);
    // TODO: Move native isSortingOptions fields into a struct
    cur.pktDetect = fromLegacy(leg.isSortingOptions.pktDetect);
    cur.pktArtifReject = fromLegacy(leg.isSortingOptions.pktArtifReject);
    copyArr(cur.pktNoiseBoundary, leg.isSortingOptions.pktNoiseBoundary, this, &Adapter::fromLegacy);
    cur.pktStatistics = fromLegacy(leg.isSortingOptions.pktStatistics);
    cur.pktStatus = fromLegacy(leg.isSortingOptions.pktStatus);
    copyArr(cur.isNTrodeInfo, leg.isNTrodeInfo, this, &Adapter::fromLegacy);
    copyArr2D(cur.isWaveform, leg.isWaveform, this, &Adapter::fromLegacy);
    cur.isLnc = fromLegacy(leg.isLnc[instrument_idx]);
    cur.isNPlay = fromLegacy(leg.isNPlay);
    copyArr(cur.isVideoSource, leg.isVideoSource, this, &Adapter::fromLegacy);
    copyArr(cur.isTrackObj, leg.isTrackObj, this, &Adapter::fromLegacy);
    cur.fileinfo = fromLegacy(leg.fileinfo);

    return cur;
}

NativeNSPStatus Adapter::fromLegacy(const NSPStatus& leg) const {
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

::cbPKT_UNIT_SELECTION Adapter::fromLegacy(const cbPKT_UNIT_SELECTION& leg) const {
    ::cbPKT_UNIT_SELECTION cur{};
    cur.cbpkt_header = fromLegacy(leg.cbpkt_header);
    cur.lastchan = leg.lastchan;
    copyArr(cur.abyUnitSelections, leg.abyUnitSelections);
    return cur;
}

NativePCStatus Adapter::fromLegacy(const cbPcStatus& leg) const {
    NativePCStatus cur{};
    cur.isSelection = fromLegacy(leg.isSelection[instrument_idx]);
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
    cur.m_nNspStatus = NativeNSPStatus::NSP_FOUND; // TODO: VERIFY
    cur.m_nNumNTrodesPerInstrument = cbMAXNTRODES; // TODO: VERIFY
    cur.m_nGeminiSystem = 1; // TODO: VERIFY
    // ignore APP_WORKSPACE
    return cur;
}

NativeReceiveBuffer Adapter::fromLegacy(const cbRECBUFF& leg) const {
    NativeReceiveBuffer cur{};
    cur.received = leg.received;
    cur.lasttime = leg.lasttime;
    cur.headwrap = leg.headwrap;
    cur.headindex = leg.headindex;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

NativeTransmitBuffer Adapter::fromLegacy(const cbXMTBUFF& leg) const {
    NativeTransmitBuffer cur{};
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

NativeTransmitBufferLocal Adapter::fromLegacy(const cbXMTBUFFLOCAL& leg) const {
    NativeTransmitBufferLocal cur{};
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
    return cur;
}

cbPKT_HEADER Adapter::toLegacy(const ::cbPKT_HEADER& cur) const {
    cbPKT_HEADER leg{};
    leg.time = cur.time;
    leg.chid = cur.chid;
    leg.type = static_cast<uint8_t>(cur.type);
    leg.dlen = static_cast<uint8_t>(cur.dlen);
    return leg;
}

cbPKT_SYSINFO Adapter::toLegacy(const ::cbPKT_SYSINFO& cur) const {
    cbPKT_SYSINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.sysfreq = cur.sysfreq;
    leg.spikelen = cur.spikelen;
    leg.spikepre = cur.spikepre;
    leg.resetque = cur.resetque;
    leg.runlevel = cur.runlevel;
    leg.runflags = cur.runflags;
    return leg;
}

cbPKT_PROCINFO Adapter::toLegacy(const ::cbPKT_PROCINFO& cur) const {
    cbPKT_PROCINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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
    leg.sortmethod = cur.reserved;
    leg.version = cur.version;
    return leg;
}

cbPKT_BANKINFO Adapter::toLegacy(const ::cbPKT_BANKINFO& cur) const {
    cbPKT_BANKINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.proc = cur.proc;
    leg.bank = cur.bank;
    leg.idcode = cur.idcode;
    copyArr(leg.ident, cur.ident);
    copyArr(leg.label, cur.label);
    leg.chanbase = cur.chanbase;
    leg.chancount = cur.chancount;
    return leg;
}

cbPKT_GROUPINFO Adapter::toLegacy(const ::cbPKT_GROUPINFO& cur) const {
    cbPKT_GROUPINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.proc = cur.proc;
    leg.group = cur.group;
    copyArr(leg.label, cur.label);
    leg.period = cur.period;
    leg.length = cur.length;
    copyArr(leg.list, cur.list);
    return leg;
}

cbPKT_FILTINFO Adapter::toLegacy(const ::cbPKT_FILTINFO& cur) const {
    cbPKT_FILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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

cbPKT_ADAPTFILTINFO Adapter::toLegacy(const ::cbPKT_ADAPTFILTINFO& cur) const {
    cbPKT_ADAPTFILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.dLearningRate = cur.dLearningRate;
    leg.nRefChan1 = cur.nRefChan1;
    leg.nRefChan2 = cur.nRefChan2;
    return leg;
}

cbPKT_REFELECFILTINFO Adapter::toLegacy(const ::cbPKT_REFELECFILTINFO& cur) const {
    cbPKT_REFELECFILTINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.nRefChan = cur.nRefChan;
    return leg;
}

cbSCALING Adapter::toLegacy(const ::cbSCALING& cur) const {
    cbSCALING leg{};
    leg.digmin = cur.digmin;
    leg.digmax = cur.digmax;
    leg.anamin = cur.anamin;
    leg.anamax = cur.anamax;
    leg.anagain = cur.anagain;
    copyArr(leg.anaunit, cur.anaunit);
    return leg;
}

cbFILTDESC Adapter::toLegacy(const ::cbFILTDESC& cur) const {
    cbFILTDESC leg{};
    leg.hpfreq = cur.hpfreq;
    leg.hporder = cur.hporder;
    leg.hptype = cur.hptype;
    leg.lpfreq = cur.lpfreq;
    leg.lporder = cur.lporder;
    leg.lptype = cur.lptype;
    return leg;
}

cbMANUALUNITMAPPING Adapter::toLegacy(const ::cbMANUALUNITMAPPING& cur) const {
    cbMANUALUNITMAPPING leg{};
    leg.nOverride = cur.nOverride;
    copyArr(leg.afOrigin, cur.afOrigin);
    copyArr2D(leg.afShape, cur.afShape);
    leg.aPhi = cur.aPhi;
    leg.bValid = cur.bValid;
    return leg;
}

cbHOOP Adapter::toLegacy(const ::cbHOOP& cur) const {
    cbHOOP leg{};
    leg.valid = cur.valid;
    leg.time = cur.time;
    leg.min = cur.min;
    leg.max = cur.max;
    return leg;
}

cbPKT_CHANINFO Adapter::toLegacy(const ::cbPKT_CHANINFO& cur) const {
    cbPKT_CHANINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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
    leg.physcalin = toLegacy(cur.physcalin);
    leg.phyfiltin = toLegacy(cur.phyfiltin);
    leg.physcalout = toLegacy(cur.physcalout);
    leg.phyfiltout = toLegacy(cur.phyfiltout);
    copyArr(leg.label, cur.label);
    leg.userflags = cur.userflags;
    copyArr(leg.position, cur.position);
    leg.scalin = toLegacy(cur.scalin);
    leg.scalout = toLegacy(cur.scalout);
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
    copyArr(leg.unitmapping, cur.unitmapping, this, &Adapter::toLegacy);
    copyArr2D(leg.spkhoops, cur.spkhoops, this, &Adapter::toLegacy);
    return leg;
}

cbPKT_FS_BASIS Adapter::toLegacy(const ::cbPKT_FS_BASIS& cur) const {
    cbPKT_FS_BASIS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.fs = cur.fs;
    copyArr2D(leg.basis, cur.basis);
    return leg;
}

cbPKT_SS_MODELSET Adapter::toLegacy(const ::cbPKT_SS_MODELSET& cur) const {
    cbPKT_SS_MODELSET leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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

cbPKT_SS_DETECT Adapter::toLegacy(const ::cbPKT_SS_DETECT& cur) const {
    cbPKT_SS_DETECT leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.fThreshold = cur.fThreshold;
    leg.fMultiplier = cur.fMultiplier;
    return leg;
}

cbPKT_SS_ARTIF_REJECT Adapter::toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur) const {
    cbPKT_SS_ARTIF_REJECT leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.nMaxSimulChans = cur.nMaxSimulChans;
    leg.nRefractoryCount = cur.nRefractoryCount;
    return leg;
}

cbPKT_SS_NOISE_BOUNDARY Adapter::toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur) const {
    cbPKT_SS_NOISE_BOUNDARY leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.chan = cur.chan;
    copyArr(leg.afc, cur.afc);
    copyArr2D(leg.afS, cur.afS);
    return leg;
}

cbPKT_SS_STATISTICS Adapter::toLegacy(const ::cbPKT_SS_STATISTICS& cur) const {
    cbPKT_SS_STATISTICS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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

cbAdaptControl Adapter::toLegacy(const ::cbAdaptControl& cur) const {
    cbAdaptControl leg{};
    leg.nMode = cur.nMode;
    leg.fTimeOutMinutes = cur.fTimeOutMinutes;
    leg.fElapsedMinutes = cur.fElapsedMinutes;
    return leg;
}

cbPKT_SS_STATUS Adapter::toLegacy(const ::cbPKT_SS_STATUS& cur) const {
    cbPKT_SS_STATUS leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.cntlUnitStats = toLegacy(cur.cntlUnitStats);
    leg.cntlNumUnits = toLegacy(cur.cntlNumUnits);
    return leg;
}

cbSPIKE_SORTING Adapter::toLegacy(const ::cbproto::SpikeSorting& cur) const {
    cbSPIKE_SORTING leg{};
    copyArr(leg.asBasis, cur.basis, this, &Adapter::toLegacy);
    copyArr2D(leg.asSortModel, cur.models, this, &Adapter::toLegacy);
    leg.pktDetect = toLegacy(cur.detect);
    leg.pktArtifReject = toLegacy(cur.artifact_reject);
    copyArr(leg.pktNoiseBoundary, cur.noise_boundary, this, &Adapter::toLegacy);
    leg.pktStatistics = toLegacy(cur.statistics);
    leg.pktStatus = toLegacy(cur.status);
    return leg;
}

cbPKT_NTRODEINFO Adapter::toLegacy(const ::cbPKT_NTRODEINFO& cur) const {
    cbPKT_NTRODEINFO leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.ntrode = cur.ntrode;
    copyArr(leg.label, cur.label);
    copyArr2D(leg.ellipses, cur.ellipses, this, &Adapter::toLegacy);
    leg.nSite = cur.nSite;
    leg.fs = cur.fs;
    copyArr(leg.nChan, cur.nChan);
    return leg;
}

cbWaveformData Adapter::toLegacy(const ::cbWaveformData& cur) const {
    cbWaveformData leg{};
    leg.offset = cur.offset; // aka sineFrequency
    leg.seq = cur.seq; // aka sineAmplitude
    leg.seqTotal = cur.seqTotal;
    leg.phases = cur.phases;
    copyArr(leg.duration, cur.duration);
    copyArr(leg.amplitude, cur.amplitude);
    return leg;
}

cbPKT_AOUT_WAVEFORM Adapter::toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur) const {
    cbPKT_AOUT_WAVEFORM leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.repeats = cur.repeats;
    leg.trig = (static_cast<uint16_t>(cur.trig) << 8) | static_cast<uint16_t>(cur.trigInst);
    leg.trigChan = cur.trigChan;
    leg.trigValue = cur.trigValue;
    leg.trigNum = cur.trigNum;
    leg.active = cur.active;
    leg.wave = toLegacy(cur.wave);
    return leg;
}

cbPKT_LNC Adapter::toLegacy(const ::cbPKT_LNC& cur) const {
    cbPKT_LNC leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.lncFreq = cur.lncFreq;
    leg.lncRefChan = cur.lncRefChan;
    leg.lncGlobalMode = cur.lncGlobalMode;
    return leg;
}

cbPKT_NPLAY Adapter::toLegacy(const ::cbPKT_NPLAY& cur) const {
    cbPKT_NPLAY leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
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

cbVIDEOSOURCE Adapter::toLegacy(const ::cbVIDEOSOURCE& cur) const {
    cbVIDEOSOURCE leg{};
    copyArr(leg.name, cur.name);
    leg.fps = cur.fps;
    return leg;
}

cbTRACKOBJ Adapter::toLegacy(const ::cbTRACKOBJ& cur) const {
    cbTRACKOBJ leg{};
    copyArr(leg.name, cur.name);
    leg.type = cur.type;
    leg.pointCount = cur.pointCount;
    return leg;
}

cbPKT_FILECFG Adapter::toLegacy(const ::cbPKT_FILECFG& cur) const {
    cbPKT_FILECFG leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.options = cur.options;
    leg.duration = cur.duration;
    leg.recording = cur.recording;
    leg.extctrl = cur.extctrl;
    copyArr(leg.username, cur.username);
    copyArr(leg.filename, cur.filename); // aka datetime
    copyArr(leg.comment, cur.comment);
    return leg;
}

NSPStatus Adapter::toLegacy(const NativeNSPStatus& cur) const {
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

cbPKT_UNIT_SELECTION Adapter::toLegacy(const ::cbPKT_UNIT_SELECTION& cur) const {
    cbPKT_UNIT_SELECTION leg{};
    leg.cbpkt_header = toLegacy(cur.cbpkt_header);
    leg.lastchan = cur.lastchan;
    copyArr(leg.abyUnitSelections, cur.abyUnitSelections);
    return leg;
}

cbPcStatus Adapter::toLegacy(const NativePCStatus& cur) const {
    cbPcStatus leg{};
    leg.isSelection[instrument_idx] = toLegacy(cur.isSelection);
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
    // ignore APP_WORKSPACE
    return leg;
}

cbRECBUFF Adapter::toLegacy(const NativeReceiveBuffer& cur) const {
    cbRECBUFF leg{};
    leg.received = cur.received;
    leg.lasttime = cur.lasttime;
    leg.headwrap = cur.headwrap;
    leg.headindex = cur.headindex;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

cbXMTBUFF Adapter::toLegacy(const NativeTransmitBuffer& cur) const {
    cbXMTBUFF leg{};
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

cbXMTBUFFLOCAL Adapter::toLegacy(const NativeTransmitBufferLocal& cur) const {
    cbXMTBUFFLOCAL leg{};
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
    return leg;
}

Adapter::Adapter(uint8_t instrument_idx, void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr)
    : instrument_idx(instrument_idx)
    , cfg(static_cast<cbCFGBUFF*>(cfg_ptr))
    , rec(static_cast<cbRECBUFF*>(rec_ptr))
    , xmt(static_cast<cbXMTBUFF*>(xmt_ptr))
    , xmt_local(static_cast<cbXMTBUFFLOCAL*>(xmt_local_ptr))
    , status(static_cast<cbPcStatus*>(status_ptr))
    , spike(static_cast<cbSPKBUFF*>(spike_ptr))
{}

uint32_t Adapter::getMaxProcs() const {
    return cbMAXPROCS;
}

uint32_t& Adapter::getRecReceived() {
    return rec->received;
}

uint64_t Adapter::getRecLasttime() {
    return rec->lasttime;
}

void Adapter::setRecLasttime(uint64_t lasttime) {
    rec->lasttime = lasttime;
}

uint32_t& Adapter::getRecHeadwrapPtr() {
    return rec->headwrap;
}

uint32_t& Adapter::getRecHeadindexPtr() {
    return rec->headindex;
}

uint32_t* Adapter::getRecBufferPtr() {
    return rec->buffer;
}

uint32_t& Adapter::getXmtTransmittedPtr() {
    return xmt->transmitted;
}

uint32_t& Adapter::getXmtHeadindexPtr() {
    return xmt->headindex;
}

uint32_t& Adapter::getXmtTailindexPtr() {
    return xmt->tailindex;
}

uint32_t& Adapter::getXmtLastValidIndexPtr() {
    return xmt->last_valid_index;
}

uint32_t& Adapter::getXmtBufferlenPtr() {
    return xmt->bufferlen;
}

uint32_t* Adapter::getXmtBufferPtr() {
    return xmt->buffer;
}

uint32_t& Adapter::getLocalXmtTransmittedPtr() {
    return xmt->transmitted;
}

uint32_t& Adapter::getLocalXmtHeadindexPtr() {
    return xmt->headindex;
}

uint32_t& Adapter::getLocalXmtTailindexPtr() {
    return xmt->tailindex;
}

uint32_t& Adapter::getLocalXmtLastValidIndexPtr() {
    return xmt->last_valid_index;
}

uint32_t& Adapter::getLocalXmtBufferlenPtr() {
    return xmt->bufferlen;
}

uint32_t* Adapter::getLocalXmtBufferPtr() {
    return xmt->buffer;
}

Result<::cbPKT_PROCINFO> Adapter::getProcInfo() const {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<::cbPKT_PROCINFO>::error("Instrument index out of range");
    }
    return Result<::cbPKT_PROCINFO>::ok(fromLegacy(cfg->procinfo[instrument_idx]));
}

Result<::cbPKT_BANKINFO> Adapter::getBankInfo(uint32_t bank_num) const {
    if (instrument_idx >= std::size(cfg->bankinfo)) {
        return Result<::cbPKT_BANKINFO>::error("Instrument index out of range");
    }
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<::cbPKT_BANKINFO>::error("Bank number out of range");
    }
    return Result<::cbPKT_BANKINFO>::ok(fromLegacy(cfg->bankinfo[instrument_idx][bank_idx]));
}

Result<::cbPKT_FILTINFO> Adapter::getFilterInfo(uint32_t filter_num) const {
    if (instrument_idx >= std::size(cfg->filtinfo)) {
        return Result<::cbPKT_FILTINFO>::error("Instrument index out of range");
    }
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<::cbPKT_FILTINFO>::error("Filter number out of range");
    }
    return Result<::cbPKT_FILTINFO>::ok(fromLegacy(cfg->filtinfo[instrument_idx][filter_idx]));
}

Result<::cbPKT_CHANINFO> Adapter::getChanInfo(uint32_t channel_idx) const {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<::cbPKT_CHANINFO>::error("Channel number out of range");
    }
    return Result<::cbPKT_CHANINFO>::ok(fromLegacy(cfg->chaninfo[channel_idx]));
}

Result<::cbPKT_SYSINFO> Adapter::getSysInfo() const {
    return Result<::cbPKT_SYSINFO>::ok(fromLegacy(cfg->sysinfo));
}

Result<::cbPKT_GROUPINFO> Adapter::getGroupInfo(uint32_t group_idx) const {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<::cbPKT_GROUPINFO>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<::cbPKT_GROUPINFO>::error("Group index out of range");
    }
    return Result<::cbPKT_GROUPINFO>::ok(fromLegacy(cfg->groupinfo[instrument_idx][group_idx]));
}

Result<NativeConfigBuffer> Adapter::getConfigBuffer() const {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<NativeConfigBuffer>::error("Instrument index out of range");
    }
    return Result<NativeConfigBuffer>::ok(fromLegacy(*cfg));
}

Result<NativePCStatus> Adapter::getPcStatus() const {
    if (instrument_idx >= std::size(status->isSelection)) {
        return Result<NativePCStatus>::error("Instrument index out of range");
    }
    return Result<NativePCStatus>::ok(fromLegacy(*status));
}

Result<void> Adapter::setProcInfo(const ::cbPKT_PROCINFO& info) {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    cfg->procinfo[instrument_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) {
    if (instrument_idx >= std::size(cfg->bankinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<void>::error("Bank number out of range");
    }
    cfg->bankinfo[instrument_idx][bank_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) {
    if (instrument_idx >= std::size(cfg->filtinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<void>::error("Filter number out of range");
    }
    cfg->filtinfo[instrument_idx][filter_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<void>::error("Channel number out of range");
    }
    cfg->chaninfo[channel_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setSysInfo(const ::cbPKT_SYSINFO& info) {
    cfg->sysinfo = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<void>::error("Group index out of range");
    }
    cfg->groupinfo[instrument_idx][group_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setPcStatus(const NativePCStatus& status) const {
    if (instrument_idx >= std::size(this->status->isSelection)) {
        return Result<void>::error("Instrument index out of range");
    }
    *(this->status) = toLegacy(status);
    return Result<void>::ok();
}

} // namespace central_v3_11

} // namespace cbshm
