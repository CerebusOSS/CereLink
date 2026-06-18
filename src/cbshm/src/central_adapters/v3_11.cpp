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

void Adapter::fromLegacy(::cbPKT_HEADER& cur, const cbPKT_HEADER& leg) const {
    cur.time = leg.time; // TODO: explicit or implicit conversion here (?) (and for all PROCTIME)
    cur.chid = leg.chid;
    cur.type = static_cast<uint16_t>(leg.type);
    cur.dlen = static_cast<uint16_t>(leg.dlen);
    cur.instrument = 0;
    cur.reserved = 0;
}

void Adapter::fromLegacy(::cbPKT_SYSINFO& cur, const cbPKT_SYSINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.sysfreq = leg.sysfreq;
    cur.spikelen = leg.spikelen;
    cur.spikepre = leg.spikepre;
    cur.resetque = leg.resetque;
    cur.runlevel = leg.runlevel;
    cur.runflags = leg.runflags;
}

void Adapter::fromLegacy(::cbPKT_PROCINFO& cur, const cbPKT_PROCINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
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
}

void Adapter::fromLegacy(::cbPKT_BANKINFO& cur, const cbPKT_BANKINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.proc = leg.proc;
    cur.bank = leg.bank;
    cur.idcode = leg.idcode;
    copyArr(cur.ident, leg.ident);
    copyArr(cur.label, leg.label);
    cur.chanbase = leg.chanbase;
    cur.chancount = leg.chancount;
}

void Adapter::fromLegacy(::cbPKT_GROUPINFO& cur, const cbPKT_GROUPINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.proc = leg.proc;
    cur.group = leg.group;
    copyArr(cur.label, leg.label);
    cur.period = leg.period;
    cur.length = leg.length;
    copyArr(cur.list, leg.list);
}

void Adapter::fromLegacy(::cbPKT_FILTINFO& cur, const cbPKT_FILTINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
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
}

void Adapter::fromLegacy(::cbPKT_ADAPTFILTINFO& cur, const cbPKT_ADAPTFILTINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.dLearningRate = leg.dLearningRate;
    cur.nRefChan1 = leg.nRefChan1;
    cur.nRefChan2 = leg.nRefChan2;
}

void Adapter::fromLegacy(::cbPKT_REFELECFILTINFO& cur, const cbPKT_REFELECFILTINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.nMode = leg.nMode;
    cur.nRefChan = leg.nRefChan;
}

void Adapter::fromLegacy(::cbSCALING& cur, const cbSCALING& leg) const {
    cur.digmin = leg.digmin;
    cur.digmax = leg.digmax;
    cur.anamin = leg.anamin;
    cur.anamax = leg.anamax;
    cur.anagain = leg.anagain;
    copyArr(cur.anaunit, leg.anaunit);
}

void Adapter::fromLegacy(::cbFILTDESC& cur, const cbFILTDESC& leg) const {
    cur.hpfreq = leg.hpfreq;
    cur.hporder = leg.hporder;
    cur.hptype = leg.hptype;
    cur.lpfreq = leg.lpfreq;
    cur.lporder = leg.lporder;
    cur.lptype = leg.lptype;
}

void Adapter::fromLegacy(::cbMANUALUNITMAPPING& cur, const cbMANUALUNITMAPPING& leg) const {
    cur.nOverride = leg.nOverride;
    copyArr(cur.afOrigin, leg.afOrigin);
    copyArr2D(cur.afShape, leg.afShape);
    cur.aPhi = leg.aPhi;
    cur.bValid = leg.bValid;
}

void Adapter::fromLegacy(::cbHOOP& cur, const cbHOOP& leg) const {
    cur.valid = leg.valid;
    cur.time = leg.time;
    cur.min = leg.min;
    cur.max = leg.max;
}

void Adapter::fromLegacy(::cbPKT_CHANINFO& cur, const cbPKT_CHANINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
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
    fromLegacy(cur.physcalin, leg.physcalin);
    fromLegacy(cur.phyfiltin, leg.phyfiltin);
    fromLegacy(cur.physcalout, leg.physcalout);
    fromLegacy(cur.phyfiltout, leg.phyfiltout);
    copyArr(cur.label, leg.label);
    cur.userflags = leg.userflags;
    copyArr(cur.position, leg.position);
    fromLegacy(cur.scalin, leg.scalin);
    fromLegacy(cur.scalout, leg.scalout);
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
}

void Adapter::fromLegacy(::cbPKT_FS_BASIS& cur, const cbPKT_FS_BASIS& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.fs = leg.fs;
    copyArr2D(cur.basis, leg.basis);
}

void Adapter::fromLegacy(::cbPKT_SS_MODELSET& cur, const cbPKT_SS_MODELSET& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
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
}

void Adapter::fromLegacy(::cbPKT_SS_DETECT& cur, const cbPKT_SS_DETECT& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.fThreshold = leg.fThreshold;
    cur.fMultiplier = leg.fMultiplier;
}

void Adapter::fromLegacy(::cbPKT_SS_ARTIF_REJECT& cur, const cbPKT_SS_ARTIF_REJECT& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.nMaxSimulChans = leg.nMaxSimulChans;
    cur.nRefractoryCount = leg.nRefractoryCount;
}

void Adapter::fromLegacy(::cbPKT_SS_NOISE_BOUNDARY& cur, const cbPKT_SS_NOISE_BOUNDARY& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.chan = leg.chan;
    copyArr(cur.afc, leg.afc);
    copyArr2D(cur.afS, leg.afS);
}

void Adapter::fromLegacy(::cbPKT_SS_STATISTICS& cur, const cbPKT_SS_STATISTICS& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
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
}

void Adapter::fromLegacy(::cbAdaptControl& cur, const cbAdaptControl& leg) const {
    cur.nMode = leg.nMode;
    cur.fTimeOutMinutes = leg.fTimeOutMinutes;
    cur.fElapsedMinutes = leg.fElapsedMinutes;
}

void Adapter::fromLegacy(::cbPKT_SS_STATUS& cur, const cbPKT_SS_STATUS& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    fromLegacy(cur.cntlUnitStats, leg.cntlUnitStats);
    fromLegacy(cur.cntlNumUnits, leg.cntlNumUnits);
}

void Adapter::fromLegacy(::cbproto::SpikeSorting& cur, const cbSPIKE_SORTING& leg) const {
    copyArr(cur.basis, leg.asBasis, this, &Adapter::fromLegacy);
    copyArr2D(cur.models, leg.asSortModel, this, &Adapter::fromLegacy);
    fromLegacy(cur.detect, leg.pktDetect);
    fromLegacy(cur.artifact_reject, leg.pktArtifReject);
    copyArr(cur.noise_boundary, leg.pktNoiseBoundary, this, &Adapter::fromLegacy);
    fromLegacy(cur.statistics, leg.pktStatistics);
    fromLegacy(cur.status, leg.pktStatus);
}

void Adapter::fromLegacy(::cbPKT_NTRODEINFO& cur, const cbPKT_NTRODEINFO& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.ntrode = leg.ntrode;
    copyArr(cur.label, leg.label);
    copyArr2D(cur.ellipses, leg.ellipses, this, &Adapter::fromLegacy);
    cur.nSite = leg.nSite;
    cur.fs = leg.fs;
    copyArr(cur.nChan, leg.nChan);
}

void Adapter::fromLegacy(::cbWaveformData& cur, const cbWaveformData& leg) const {
    cur.offset = leg.offset; // aka sineFrequency
    cur.seq = leg.seq; // aka sineAmplitude
    cur.seqTotal = leg.seqTotal;
    cur.phases = leg.phases;
    copyArr(cur.duration, leg.duration);
    copyArr(cur.amplitude, leg.amplitude);
}

void Adapter::fromLegacy(::cbPKT_AOUT_WAVEFORM& cur, const cbPKT_AOUT_WAVEFORM& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.chan = leg.chan;
    cur.mode = leg.mode;
    cur.repeats = leg.repeats;
    cur.trig = static_cast<uint8_t>((leg.trig >> 8) & 0xFF);
    cur.trigInst = static_cast<uint8_t>(leg.trig & 0xFF);
    cur.trigChan = leg.trigChan;
    cur.trigValue = leg.trigValue;
    cur.trigNum = leg.trigNum;
    cur.active = leg.active;
    fromLegacy(cur.wave, leg.wave);
}

void Adapter::fromLegacy(::cbPKT_LNC& cur, const cbPKT_LNC& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.lncFreq = leg.lncFreq;
    cur.lncRefChan = leg.lncRefChan;
    cur.lncGlobalMode = leg.lncGlobalMode;
}

void Adapter::fromLegacy(::cbPKT_NPLAY& cur, const cbPKT_NPLAY& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.ftime = leg.ftime; // aka opt
    cur.stime = leg.stime;
    cur.etime = leg.etime;
    cur.val = leg.val;
    cur.mode = leg.mode;
    cur.flags = leg.flags;
    cur.speed = leg.speed;
    copyArr(cur.fname, leg.fname);
}

void Adapter::fromLegacy(::cbVIDEOSOURCE& cur, const cbVIDEOSOURCE& leg) const {
    copyArr(cur.name, leg.name);
    cur.fps = leg.fps;
}

void Adapter::fromLegacy(::cbTRACKOBJ& cur, const cbTRACKOBJ& leg) const {
    copyArr(cur.name, leg.name);
    cur.type = leg.type;
    cur.pointCount = leg.pointCount;
}

void Adapter::fromLegacy(::cbPKT_FILECFG& cur, const cbPKT_FILECFG& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.options = leg.options;
    cur.duration = leg.duration;
    cur.recording = leg.recording;
    cur.extctrl = leg.extctrl;
    copyArr(cur.username, leg.username);
    copyArr(cur.filename, leg.filename); // aka datetime
    copyArr(cur.comment, leg.comment);
}

void Adapter::fromLegacy(NativeConfigBuffer& cur, const cbCFGBUFF& leg) const {
    // TODO: VERIFY that each list that's assumed to be instrument-independent is in fact independent from any particular instrument.
    cur.version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR; // Central's version field contains garbage data, so replace it with the protocol version
    cur.sysflags = leg.sysflags;
    cur.instrument_status = static_cast<decltype(cur.instrument_status)>(InstrumentStatus::ACTIVE);
    fromLegacy(cur.sysinfo, leg.sysinfo);
    fromLegacy(cur.procinfo, leg.procinfo[instrument_idx]);
    copyArr(cur.bankinfo, leg.bankinfo[instrument_idx], this, &Adapter::fromLegacy);
    copyArr(cur.groupinfo, leg.groupinfo[instrument_idx], this, &Adapter::fromLegacy);
    copyArr(cur.filtinfo, leg.filtinfo[instrument_idx], this, &Adapter::fromLegacy);
    fromLegacy(cur.adaptinfo, leg.adaptinfo[instrument_idx]);
    fromLegacy(cur.refelecinfo, leg.refelecinfo[instrument_idx]);
    copyArr(cur.chaninfo, leg.chaninfo, this, &Adapter::fromLegacy);
    copyArr(cur.asBasis, leg.isSortingOptions.asBasis, this, &Adapter::fromLegacy);
    // copyArr2D(cur.asSortModel, leg.isSortingOptions.asSortModel, this, &Adapter::fromLegacy); // TODO: For some reason, this contains memory that's innaccessible
    // TODO: Move native isSortingOptions fields into a struct
    fromLegacy(cur.pktDetect, leg.isSortingOptions.pktDetect);
    fromLegacy(cur.pktArtifReject, leg.isSortingOptions.pktArtifReject);
    copyArr(cur.pktNoiseBoundary, leg.isSortingOptions.pktNoiseBoundary, this, &Adapter::fromLegacy);
    fromLegacy(cur.pktStatistics, leg.isSortingOptions.pktStatistics);
    fromLegacy(cur.pktStatus, leg.isSortingOptions.pktStatus);
    copyArr(cur.isNTrodeInfo, leg.isNTrodeInfo, this, &Adapter::fromLegacy);
    copyArr2D(cur.isWaveform, leg.isWaveform, this, &Adapter::fromLegacy);
    fromLegacy(cur.isLnc, leg.isLnc[instrument_idx]);
    fromLegacy(cur.isNPlay, leg.isNPlay);
    copyArr(cur.isVideoSource, leg.isVideoSource, this, &Adapter::fromLegacy);
    copyArr(cur.isTrackObj, leg.isTrackObj, this, &Adapter::fromLegacy);
    fromLegacy(cur.fileinfo, leg.fileinfo);

}

void Adapter::fromLegacy(NativeNSPStatus& cur, const NSPStatus& leg) const {
    switch(leg) {
        case NSPStatus::NSP_INIT:
            cur = NativeNSPStatus::NSP_INIT;
            break;
        case NSPStatus::NSP_NOIPADDR:
            cur = NativeNSPStatus::NSP_NOIPADDR;
            break;
        case NSPStatus::NSP_NOREPLY:
            cur = NativeNSPStatus::NSP_NOREPLY;
            break;
        case NSPStatus::NSP_FOUND:
            cur = NativeNSPStatus::NSP_FOUND;
            break;
        case NSPStatus::NSP_INVALID:
            /* fallthrough */
        default:
            cur = NativeNSPStatus::NSP_INVALID;
            break;
    }
}

void Adapter::fromLegacy(::cbPKT_UNIT_SELECTION& cur, const cbPKT_UNIT_SELECTION& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    cur.lastchan = leg.lastchan;
    copyArr(cur.abyUnitSelections, leg.abyUnitSelections);
}

void Adapter::fromLegacy(NativePCStatus& cur, const cbPcStatus& leg) const {
    fromLegacy(cur.isSelection, leg.isSelection[instrument_idx]);
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
}

void Adapter::fromLegacy(NativeReceiveBuffer& cur, const cbRECBUFF& leg) const {
    cur.received = leg.received;
    cur.lasttime = leg.lasttime;
    cur.headwrap = leg.headwrap;
    cur.headindex = leg.headindex;
    copyArr(cur.buffer, leg.buffer);
}

void Adapter::fromLegacy(NativeTransmitBuffer& cur, const cbXMTBUFF& leg) const {
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
}

void Adapter::fromLegacy(NativeTransmitBufferLocal& cur, const cbXMTBUFFLOCAL& leg) const {
    cur.transmitted = leg.transmitted;
    cur.headindex = leg.headindex;
    cur.tailindex = leg.tailindex;
    cur.last_valid_index = leg.last_valid_index;
    cur.bufferlen = leg.bufferlen;
    copyArr(cur.buffer, leg.buffer);
}

void Adapter::fromLegacy(::cbPKT_SPK& cur, const cbPKT_SPK& leg) const {
    fromLegacy(cur.cbpkt_header, leg.cbpkt_header);
    copyArr(cur.fPattern, leg.fPattern);
    cur.nPeak = leg.nPeak;
    cur.nValley = leg.nValley;
    copyArr(cur.wave, leg.wave);
}

void Adapter::fromLegacy(NativeSpikeCache& cur, const cbSPKCACHE& leg) const {
    cur.chid = leg.chid;
    cur.pktcnt = leg.pktcnt;
    cur.pktsize = leg.pktsize;
    cur.head = leg.head;
    cur.valid = leg.valid;
    copyArr(cur.spkpkt, leg.spkpkt, this, &Adapter::fromLegacy);
}

void Adapter::fromLegacy(NativeSpikeBuffer& cur, const cbSPKBUFF& leg) const {
    cur.flags = leg.flags;
    cur.chidmax = leg.chidmax;
    cur.linesize = leg.linesize;
    cur.spkcount = leg.spkcount;
    copyArr(cur.cache, leg.cache, this, &Adapter::fromLegacy);
}

void Adapter::toLegacy(cbPKT_HEADER& leg, const ::cbPKT_HEADER& cur) const {
    leg.time = cur.time;
    leg.chid = cur.chid;
    leg.type = static_cast<uint8_t>(cur.type);
    leg.dlen = static_cast<uint8_t>(cur.dlen);
}

void Adapter::toLegacy(cbPKT_SYSINFO& leg, const ::cbPKT_SYSINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.sysfreq = cur.sysfreq;
    leg.spikelen = cur.spikelen;
    leg.spikepre = cur.spikepre;
    leg.resetque = cur.resetque;
    leg.runlevel = cur.runlevel;
    leg.runflags = cur.runflags;
}

void Adapter::toLegacy(cbPKT_PROCINFO& leg, const ::cbPKT_PROCINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
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
}

void Adapter::toLegacy(cbPKT_BANKINFO& leg, const ::cbPKT_BANKINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.proc = cur.proc;
    leg.bank = cur.bank;
    leg.idcode = cur.idcode;
    copyArr(leg.ident, cur.ident);
    copyArr(leg.label, cur.label);
    leg.chanbase = cur.chanbase;
    leg.chancount = cur.chancount;
}

void Adapter::toLegacy(cbPKT_GROUPINFO& leg, const ::cbPKT_GROUPINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.proc = cur.proc;
    leg.group = cur.group;
    copyArr(leg.label, cur.label);
    leg.period = cur.period;
    leg.length = cur.length;
    copyArr(leg.list, cur.list);
}

void Adapter::toLegacy(cbPKT_FILTINFO& leg, const ::cbPKT_FILTINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
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
}

void Adapter::toLegacy(cbPKT_ADAPTFILTINFO& leg, const ::cbPKT_ADAPTFILTINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.dLearningRate = cur.dLearningRate;
    leg.nRefChan1 = cur.nRefChan1;
    leg.nRefChan2 = cur.nRefChan2;
}

void Adapter::toLegacy(cbPKT_REFELECFILTINFO& leg, const ::cbPKT_REFELECFILTINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.nMode = cur.nMode;
    leg.nRefChan = cur.nRefChan;
}

void Adapter::toLegacy(cbSCALING& leg, const ::cbSCALING& cur) const {
    leg.digmin = cur.digmin;
    leg.digmax = cur.digmax;
    leg.anamin = cur.anamin;
    leg.anamax = cur.anamax;
    leg.anagain = cur.anagain;
    copyArr(leg.anaunit, cur.anaunit);
}

void Adapter::toLegacy(cbFILTDESC& leg, const ::cbFILTDESC& cur) const {
    leg.hpfreq = cur.hpfreq;
    leg.hporder = cur.hporder;
    leg.hptype = cur.hptype;
    leg.lpfreq = cur.lpfreq;
    leg.lporder = cur.lporder;
    leg.lptype = cur.lptype;
}

void Adapter::toLegacy(cbMANUALUNITMAPPING& leg, const ::cbMANUALUNITMAPPING& cur) const {
    leg.nOverride = cur.nOverride;
    copyArr(leg.afOrigin, cur.afOrigin);
    copyArr2D(leg.afShape, cur.afShape);
    leg.aPhi = cur.aPhi;
    leg.bValid = cur.bValid;
}

void Adapter::toLegacy(cbHOOP& leg, const ::cbHOOP& cur) const {
    leg.valid = cur.valid;
    leg.time = cur.time;
    leg.min = cur.min;
    leg.max = cur.max;
}

void Adapter::toLegacy(cbPKT_CHANINFO& leg, const ::cbPKT_CHANINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
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
    toLegacy(leg.physcalin, cur.physcalin);
    toLegacy(leg.phyfiltin, cur.phyfiltin);
    toLegacy(leg.physcalout, cur.physcalout);
    toLegacy(leg.phyfiltout, cur.phyfiltout);
    copyArr(leg.label, cur.label);
    leg.userflags = cur.userflags;
    copyArr(leg.position, cur.position);
    toLegacy(leg.scalin, cur.scalin);
    toLegacy(leg.scalout, cur.scalout);
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
}

void Adapter::toLegacy(cbPKT_FS_BASIS& leg, const ::cbPKT_FS_BASIS& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.fs = cur.fs;
    copyArr2D(leg.basis, cur.basis);
}

void Adapter::toLegacy(cbPKT_SS_MODELSET& leg, const ::cbPKT_SS_MODELSET& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
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
}

void Adapter::toLegacy(cbPKT_SS_DETECT& leg, const ::cbPKT_SS_DETECT& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.fThreshold = cur.fThreshold;
    leg.fMultiplier = cur.fMultiplier;
}

void Adapter::toLegacy(cbPKT_SS_ARTIF_REJECT& leg, const ::cbPKT_SS_ARTIF_REJECT& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.nMaxSimulChans = cur.nMaxSimulChans;
    leg.nRefractoryCount = cur.nRefractoryCount;
}

void Adapter::toLegacy(cbPKT_SS_NOISE_BOUNDARY& leg, const ::cbPKT_SS_NOISE_BOUNDARY& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.chan = cur.chan;
    copyArr(leg.afc, cur.afc);
    copyArr2D(leg.afS, cur.afS);
}

void Adapter::toLegacy(cbPKT_SS_STATISTICS& leg, const ::cbPKT_SS_STATISTICS& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
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
}

void Adapter::toLegacy(cbAdaptControl& leg, const ::cbAdaptControl& cur) const {
    leg.nMode = cur.nMode;
    leg.fTimeOutMinutes = cur.fTimeOutMinutes;
    leg.fElapsedMinutes = cur.fElapsedMinutes;
}

void Adapter::toLegacy(cbPKT_SS_STATUS& leg, const ::cbPKT_SS_STATUS& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    toLegacy(leg.cntlUnitStats, cur.cntlUnitStats);
    toLegacy(leg.cntlNumUnits, cur.cntlNumUnits);
}

void Adapter::toLegacy(cbSPIKE_SORTING& leg, const ::cbproto::SpikeSorting& cur) const {
    copyArr(leg.asBasis, cur.basis, this, &Adapter::toLegacy);
    copyArr2D(leg.asSortModel, cur.models, this, &Adapter::toLegacy);
    toLegacy(leg.pktDetect, cur.detect);
    toLegacy(leg.pktArtifReject, cur.artifact_reject);
    copyArr(leg.pktNoiseBoundary, cur.noise_boundary, this, &Adapter::toLegacy);
    toLegacy(leg.pktStatistics, cur.statistics);
    toLegacy(leg.pktStatus, cur.status);
}

void Adapter::toLegacy(cbPKT_NTRODEINFO& leg, const ::cbPKT_NTRODEINFO& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.ntrode = cur.ntrode;
    copyArr(leg.label, cur.label);
    copyArr2D(leg.ellipses, cur.ellipses, this, &Adapter::toLegacy);
    leg.nSite = cur.nSite;
    leg.fs = cur.fs;
    copyArr(leg.nChan, cur.nChan);
}

void Adapter::toLegacy(cbWaveformData& leg, const ::cbWaveformData& cur) const {
    leg.offset = cur.offset; // aka sineFrequency
    leg.seq = cur.seq; // aka sineAmplitude
    leg.seqTotal = cur.seqTotal;
    leg.phases = cur.phases;
    copyArr(leg.duration, cur.duration);
    copyArr(leg.amplitude, cur.amplitude);
}

void Adapter::toLegacy(cbPKT_AOUT_WAVEFORM& leg, const ::cbPKT_AOUT_WAVEFORM& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.chan = cur.chan;
    leg.mode = cur.mode;
    leg.repeats = cur.repeats;
    leg.trig = (static_cast<uint16_t>(cur.trig) << 8) | static_cast<uint16_t>(cur.trigInst);
    leg.trigChan = cur.trigChan;
    leg.trigValue = cur.trigValue;
    leg.trigNum = cur.trigNum;
    leg.active = cur.active;
    toLegacy(leg.wave, cur.wave);
}

void Adapter::toLegacy(cbPKT_LNC& leg, const ::cbPKT_LNC& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.lncFreq = cur.lncFreq;
    leg.lncRefChan = cur.lncRefChan;
    leg.lncGlobalMode = cur.lncGlobalMode;
}

void Adapter::toLegacy(cbPKT_NPLAY& leg, const ::cbPKT_NPLAY& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.ftime = cur.ftime; // aka opt
    leg.stime = cur.stime;
    leg.etime = cur.etime;
    leg.val = cur.val;
    leg.mode = cur.mode;
    leg.flags = cur.flags;
    leg.speed = cur.speed;
    copyArr(leg.fname, cur.fname);
}

void Adapter::toLegacy(cbVIDEOSOURCE& leg, const ::cbVIDEOSOURCE& cur) const {
    copyArr(leg.name, cur.name);
    leg.fps = cur.fps;
}

void Adapter::toLegacy(cbTRACKOBJ& leg, const ::cbTRACKOBJ& cur) const {
    copyArr(leg.name, cur.name);
    leg.type = cur.type;
    leg.pointCount = cur.pointCount;
}

void Adapter::toLegacy(cbPKT_FILECFG& leg, const ::cbPKT_FILECFG& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.options = cur.options;
    leg.duration = cur.duration;
    leg.recording = cur.recording;
    leg.extctrl = cur.extctrl;
    copyArr(leg.username, cur.username);
    copyArr(leg.filename, cur.filename); // aka datetime
    copyArr(leg.comment, cur.comment);
}

void Adapter::toLegacy(NSPStatus& leg, const NativeNSPStatus& cur) const {
    switch(cur) {
        case NativeNSPStatus::NSP_INIT:
            leg = NSPStatus::NSP_INIT;
            break;
        case NativeNSPStatus::NSP_NOIPADDR:
            leg = NSPStatus::NSP_NOIPADDR;
            break;
        case NativeNSPStatus::NSP_NOREPLY:
            leg = NSPStatus::NSP_NOREPLY;
            break;
        case NativeNSPStatus::NSP_FOUND:
            leg = NSPStatus::NSP_FOUND;
            break;
        case NativeNSPStatus::NSP_INVALID:
            /* fallthrough */
        default:
            leg = NSPStatus::NSP_INVALID;
            break;
    }
}

void Adapter::toLegacy(cbPKT_UNIT_SELECTION& leg, const ::cbPKT_UNIT_SELECTION& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    leg.lastchan = cur.lastchan;
    copyArr(leg.abyUnitSelections, cur.abyUnitSelections);
}

void Adapter::toLegacy(cbRECBUFF& leg, const NativeReceiveBuffer& cur) const {
    leg.received = cur.received;
    leg.lasttime = cur.lasttime;
    leg.headwrap = cur.headwrap;
    leg.headindex = cur.headindex;
    copyArr(leg.buffer, cur.buffer);
}

void Adapter::toLegacy(cbXMTBUFF& leg, const NativeTransmitBuffer& cur) const {
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
}

void Adapter::toLegacy(cbXMTBUFFLOCAL& leg, const NativeTransmitBufferLocal& cur) const {
    leg.transmitted = cur.transmitted;
    leg.headindex = cur.headindex;
    leg.tailindex = cur.tailindex;
    leg.last_valid_index = cur.last_valid_index;
    leg.bufferlen = cur.bufferlen;
    copyArr(leg.buffer, cur.buffer);
}

void Adapter::toLegacy(cbPKT_SPK& leg, const ::cbPKT_SPK& cur) const {
    toLegacy(leg.cbpkt_header, cur.cbpkt_header);
    copyArr(leg.fPattern, leg.fPattern);
    leg.nPeak = cur.nPeak;
    leg.nValley = cur.nValley;
    copyArr(leg.wave, cur.wave);
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

Result<void> Adapter::getProcInfo(::cbPKT_PROCINFO& buf) const {
    fromLegacy(buf, cfg->procinfo[instrument_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const {
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<void>::error("Bank number out of range");
    }
    fromLegacy(buf, cfg->bankinfo[instrument_idx][bank_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const {
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<void>::error("Filter number out of range");
    }
    fromLegacy(buf, cfg->filtinfo[instrument_idx][filter_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<void>::error("Channel number out of range");
    }
    fromLegacy(buf, cfg->chaninfo[channel_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::getSysInfo(::cbPKT_SYSINFO& buf) const {
    fromLegacy(buf, cfg->sysinfo);
    return Result<void>::ok();
}

Result<void> Adapter::getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const {
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<void>::error("Group index out of range");
    }
    fromLegacy(buf, cfg->groupinfo[instrument_idx][group_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::getConfigBuffer(NativeConfigBuffer& buf) const {
    fromLegacy(buf, *cfg);
    return Result<void>::ok();
}

Result<void> Adapter::getPcStatus(NativePCStatus& buf) const {
    fromLegacy(buf, *status);
    return Result<void>::ok();
}

Result<void> Adapter::getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const {
    if (channel_idx >= std::size(spike->cache)) {
        return Result<void>::error("Channel index out of range");
    }
    fromLegacy(buf, spike->cache[channel_idx]);
    return Result<void>::ok();
}

Result<void> Adapter::setProcInfo(const ::cbPKT_PROCINFO& info) {
    toLegacy(cfg->procinfo[instrument_idx], info);
    return Result<void>::ok();
}

Result<void> Adapter::setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) {
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<void>::error("Bank number out of range");
    }
    toLegacy(cfg->bankinfo[instrument_idx][bank_idx], info);
    return Result<void>::ok();
}

Result<void> Adapter::setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) {
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<void>::error("Filter number out of range");
    }
    toLegacy(cfg->filtinfo[instrument_idx][filter_idx], info);
    return Result<void>::ok();
}

Result<void> Adapter::setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<void>::error("Channel number out of range");
    }
    toLegacy(cfg->chaninfo[channel_idx], info);
    return Result<void>::ok();
}

Result<void> Adapter::setSysInfo(const ::cbPKT_SYSINFO& info) {
    toLegacy(cfg->sysinfo, info);
    return Result<void>::ok();
}

Result<void> Adapter::setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) {
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<void>::error("Group index out of range");
    }
    toLegacy(cfg->groupinfo[instrument_idx][group_idx], info);
    return Result<void>::ok();
}

Result<void> Adapter::setNspStatus(const NativeNSPStatus& status) const {
    return Result<void>::error("Central v3.11 does not have fields for NSP status");
}

Result<void> Adapter::setGeminiSystem(bool is_gemini) const {
    return Result<void>::error("Central v3.11 does not recognize Gemini systems");
}

} // namespace central_v3_11

} // namespace cbshm
