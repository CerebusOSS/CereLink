// =STS=> CCFUtilsXmlItems.cpp[4878].aa03   open     SMID:3 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXmlItems.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXmlItems.cpp $
// $Revision: 1 $
// $Date: 4/13/12 3:47p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Implements CCFUtilsXmlItemsGenerate and CCFUtilsXmlItemsParse interfaces
//
// Note:
//  Do not validate the packets in this unit, here is not the right place
//
// Note on selecting tag names:
//  1- We do not yet have a standard for this,
//      these rules help in keeping the field format clean.
//  2- Do not include the type qualifiers in the name,
//      For example use "Threshold" for pkt.fThreshold (instead of "fThreshold")
//      Type attribute is automatically added for clarity
//  3- It is encouraged to use prefix tags for domain clarity,
//      For example, "monitor/lowsamples" separates the monitor union
//  4- It is encouraged to predict future additions and use prefix tags.
//      For example even though we currently have only 2 reference channels for adaptive filter
//      "RefChan/RefChan_item" is used to make future possible addition more seamless.
//  5- Use English tag names, unless it conveys something different
//      For example use "spike/filter" instead of "spkfilter",
//       but keep "spkcaps" because it better conveys a bitfiled.
//  6- If the packet has positional field in an array (such as chan in chaninfo) keep it
//  7- If a filed name is used in one packet (e.g. "mode") keep the capital/lower case the same
//      in future packets, for example another packet with "nMode" should also use "mode" instead of "Mode"
//      this means before adding new packets consistency in naming is important and older name should be kept
//
//     --------- Last rule after this line ---------
//  8- Giving above rules higher precedence, use filed-name keeping its captial/lower case the same
//

#include "StdAfx.h"
#include "CCFUtilsXmlItems.h"
#include "CCFUtilsXmlItemsGenerate.h"
#include "CCFUtilsXmlItemsParse.h"
#include "debugmacs.h"

// Keep this after all headers
#include "compat.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif


// Author & Date: Ehsan Azar       17 April 2012
// Purpose: CCF XML item copy constructor
CCFXmlItem::operator const QVariant() const
{
    return QVariant::fromValue(*this);
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_CHANINFO & pkt, QString strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert("label", QString(pkt.label));
    m_xmlAttribs.insert("Type", "cbPKT_CHANINFO");
    if (pkt.chan == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.chan, "chan");
    lst += ccf::GetCCFXmlItem(pkt.proc, "proc");
    lst += ccf::GetCCFXmlItem(pkt.bank, "bank");
    lst += ccf::GetCCFXmlItem(pkt.term, "term");
    lst += ccf::GetCCFXmlItem(pkt.chancaps, "caps/chancaps");
    lst += ccf::GetCCFXmlItem(pkt.doutcaps, "caps/doutcaps");
    lst += ccf::GetCCFXmlItem(pkt.dinpcaps, "caps/dinpcaps");
    lst += ccf::GetCCFXmlItem(pkt.aoutcaps, "caps/aoutcaps");
    lst += ccf::GetCCFXmlItem(pkt.ainpcaps, "caps/ainpcaps");
    lst += ccf::GetCCFXmlItem(pkt.spkcaps, "caps/spkcaps");
    lst += ccf::GetCCFXmlItem(pkt.physcalin, "scale/physcalin");
    lst += ccf::GetCCFXmlItem(pkt.phyfiltin, "filterdesc/phyfiltin");
    lst += ccf::GetCCFXmlItem(pkt.physcalout, "scale/physcalout");
    lst += ccf::GetCCFXmlItem(pkt.phyfiltout, "filterdesc/phyfiltout");
    lst += ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label");
    lst += ccf::GetCCFXmlItem(pkt.userflags, "userflags");
    lst += ccf::GetCCFXmlItem(pkt.position, 4, "position");
    lst += ccf::GetCCFXmlItem(pkt.scalin, "scale/scalin");
    lst += ccf::GetCCFXmlItem(pkt.scalout, "scale/scalout");
    lst += ccf::GetCCFXmlItem(pkt.doutopts, "options/doutopts");
    lst += ccf::GetCCFXmlItem(pkt.dinpopts, "options/dinpopts");
    lst += ccf::GetCCFXmlItem(pkt.aoutopts, "options/aoutopts");
    lst += ccf::GetCCFXmlItem(pkt.eopchar, "eopchar");
    { // For union we recoord the most informative one
        lst += ccf::GetCCFXmlItem(pkt.lowsamples, "monitor/lowsamples");
        lst += ccf::GetCCFXmlItem(pkt.highsamples, "monitor/highsamples");
        lst += ccf::GetCCFXmlItem(pkt.offset, "monitor/offset");
    }
    lst += ccf::GetCCFXmlItem(pkt.ainpopts, "options/ainpopts");
    lst += ccf::GetCCFXmlItem(pkt.lncrate, "lnc/rate");
    lst += ccf::GetCCFXmlItem(pkt.smpfilter, "sample/filter");
    lst += ccf::GetCCFXmlItem(pkt.smpgroup, "sample/group");
    lst += ccf::GetCCFXmlItem(pkt.smpdispmin, "sample/dispmin");
    lst += ccf::GetCCFXmlItem(pkt.smpdispmax, "sample/dispmax");
    lst += ccf::GetCCFXmlItem(pkt.spkfilter, "spike/filter");
    lst += ccf::GetCCFXmlItem(pkt.spkdispmax, "spike/dispmax");
    lst += ccf::GetCCFXmlItem(pkt.lncdispmax, "lnc/dispmax");
    lst += ccf::GetCCFXmlItem(pkt.spkopts, "options/spkopts");
    lst += ccf::GetCCFXmlItem(pkt.spkthrlevel, "spike/threshold/level");
    lst += ccf::GetCCFXmlItem(pkt.spkthrlimit, "spike/threshold/limit");
    lst += ccf::GetCCFXmlItem(pkt.spkgroup, "spike/group");
    lst += ccf::GetCCFXmlItem(pkt.amplrejpos, "spike/amplituderejecet/positive");
    lst += ccf::GetCCFXmlItem(pkt.amplrejneg, "spike/amplituderejecet/negative");
    lst += ccf::GetCCFXmlItem(pkt.refelecchan, "refelecchan");
    lst += ccf::GetCCFXmlItem(pkt.unitmapping, cbMAXUNITS, "unitmapping");
    lst += ccf::GetCCFXmlItem(pkt.spkhoops, cbMAXUNITS, cbMAXHOOPS, "spike/hoops", "hoop");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_ADAPTFILTINFO & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_ADAPTFILTINFO");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.chan, "chan");
    lst += ccf::GetCCFXmlItem(pkt.nMode, "mode");
    lst += ccf::GetCCFXmlItem(pkt.dLearningRate, "LearningRate");
    lst += ccf::GetCCFXmlItem(pkt.nRefChan1, "RefChan/RefChan_item");
    lst += ccf::GetCCFXmlItem(pkt.nRefChan2, "RefChan/RefChan_item<1>");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_DETECT & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SS_DETECT");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.fThreshold, "Threshold");
    lst += ccf::GetCCFXmlItem(pkt.fMultiplier, "Multiplier");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_ARTIF_REJECT & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SS_ARTIF_REJECT");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.nMaxSimulChans, "MaxSimulChans");
    lst += ccf::GetCCFXmlItem(pkt.nRefractoryCount, "RefractoryCount");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_NOISE_BOUNDARY & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SS_NOISE_BOUNDARY");
    if (pkt.chan == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.chan, "chan");
    lst += ccf::GetCCFXmlItem(pkt.afc, 3, "center");
    lst += ccf::GetCCFXmlItem(pkt.afS, 3, 3, "axes", "axis");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_STATISTICS & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SS_STATISTICS");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.nUpdateSpikes, "UpdateSpikes");
    lst += ccf::GetCCFXmlItem(pkt.nAutoalg, "Autoalg");
    lst += ccf::GetCCFXmlItem(pkt.nMode, "mode");
    lst += ccf::GetCCFXmlItem(pkt.fMinClusterPairSpreadFactor, "Cluster/MinClusterPairSpreadFactor");
    lst += ccf::GetCCFXmlItem(pkt.fMaxSubclusterSpreadFactor, "Cluster/MaxSubclusterSpreadFactor");
    lst += ccf::GetCCFXmlItem(pkt.fMinClusterHistCorrMajMeasure, "Cluster/MinClusterHistCorrMajMeasure");
    lst += ccf::GetCCFXmlItem(pkt.fMaxClusterPairHistCorrMajMeasure, "Cluster/MaxClusterPairHistCorrMajMeasure");
    lst += ccf::GetCCFXmlItem(pkt.fClusterHistValleyPercentage, "Cluster/ClusterHistValleyPercentage");
    lst += ccf::GetCCFXmlItem(pkt.fClusterHistClosePeakPercentage, "Cluster/ClusterHistClosePeakPercentage");
    lst += ccf::GetCCFXmlItem(pkt.fClusterHistMinPeakPercentage, "Cluster/ClusterHistMinPeakPercentage");
    lst += ccf::GetCCFXmlItem(pkt.nWaveBasisSize, "WaveBasisSize");
    lst += ccf::GetCCFXmlItem(pkt.nWaveSampleSize, "WaveSampleSize");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_STATUS & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SS_STATUS");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.cntlUnitStats, "cntlUnitStats");
    lst += ccf::GetCCFXmlItem(pkt.cntlNumUnits, "cntlNumUnits");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SYSINFO & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_SYSINFO");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.sysfreq, "sysfreq");
    lst += ccf::GetCCFXmlItem(pkt.spikelen, "spike/length");
    lst += ccf::GetCCFXmlItem(pkt.spikepre, "spike/pretrigger");
    lst += ccf::GetCCFXmlItem(pkt.resetque, "resetque");
    lst += ccf::GetCCFXmlItem(pkt.runlevel, "runlevel");
    lst += ccf::GetCCFXmlItem(pkt.runflags, "runflags");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_NTRODEINFO & pkt, QString strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert("label", QString(pkt.label));
    m_xmlAttribs.insert("Type", "cbPKT_NTRODEINFO");
    if (pkt.ntrode == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.ntrode, "ntrode");
    lst += ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label");
    lst += ccf::GetCCFXmlItem(pkt.ellipses, cbMAXSITEPLOTS, cbMAXUNITS, "ellipses", "ellipse");
    lst += ccf::GetCCFXmlItem(pkt.nSite, "site");
    lst += ccf::GetCCFXmlItem(pkt.fs, "featurespace");
    lst += ccf::GetCCFXmlItem(pkt.nChan, cbMAXSITES, "chan");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_LNC & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_LNC");
    if (pkt.lncFreq == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.lncFreq, "frequency");
    lst += ccf::GetCCFXmlItem(pkt.lncRefChan, "RefChan/RefChan_item");
    lst += ccf::GetCCFXmlItem(pkt.lncGlobalMode, "GlobalMode");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_FILTINFO & pkt, QString strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert("label", QString(pkt.label));
    m_xmlAttribs.insert("Type", "cbPKT_FILTINFO");
    if (pkt.type == 0)
        return;

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.chid, "chid");
    lst += ccf::GetCCFXmlItem(pkt.type, "type");
    lst += ccf::GetCCFXmlItem(pkt.dlen, "dlen");
    lst += ccf::GetCCFXmlItem(pkt.proc, "proc");
    lst += ccf::GetCCFXmlItem(pkt.filt, "filter");
    lst += ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label");
    lst += ccf::GetCCFXmlItem(pkt.hpfreq, "filterdesc/digfilt/hpfreq");
    lst += ccf::GetCCFXmlItem(pkt.hporder, "filterdesc/digfilt/hporder");
    lst += ccf::GetCCFXmlItem(pkt.hptype, "filterdesc/digfilt/hptype");
    lst += ccf::GetCCFXmlItem(pkt.lpfreq, "filterdesc/digfilt/lpfreq");
    lst += ccf::GetCCFXmlItem(pkt.lporder, "filterdesc/digfilt/lporder");
    lst += ccf::GetCCFXmlItem(pkt.lptype, "filterdesc/digfilt/lptype");
    // Although we can save as an array such as sos[2][2], I do not think that is good idea
    //  because we never plan to go higher than second-order (already IIR is problematic)
    lst += ccf::GetCCFXmlItem(pkt.sos1a1, "filterdesc/digfilt/sos1/a1");
    lst += ccf::GetCCFXmlItem(pkt.sos1a2, "filterdesc/digfilt/sos1/a2");
    lst += ccf::GetCCFXmlItem(pkt.sos1b1, "filterdesc/digfilt/sos1/b1");
    lst += ccf::GetCCFXmlItem(pkt.sos1b2, "filterdesc/digfilt/sos1/b2");
    lst += ccf::GetCCFXmlItem(pkt.sos2a1, "filterdesc/digfilt/sos2/a1");
    lst += ccf::GetCCFXmlItem(pkt.sos2a2, "filterdesc/digfilt/sos2/a2");
    lst += ccf::GetCCFXmlItem(pkt.sos2b1, "filterdesc/digfilt/sos2/b1");
    lst += ccf::GetCCFXmlItem(pkt.sos2b2, "filterdesc/digfilt/sos2/b2");
    lst += ccf::GetCCFXmlItem(pkt.gain, "filterdesc/digfilt/gain");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Griffin Milsap       12 Dec 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_AOUT_WAVEFORM & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbPKT_AOUT_WAVEFORM");

    // FIXME: Implement this.
    QVariantList lst;

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbSCALING & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbSCALING");

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.digmin, "digmin");
    lst += ccf::GetCCFXmlItem(pkt.digmax, "digmax");
    lst += ccf::GetCCFXmlItem(pkt.anamin, "anamin");
    lst += ccf::GetCCFXmlItem(pkt.anamax, "anamax");
    lst += ccf::GetCCFXmlItem(pkt.anagain, "anagain");
    lst += ccf::GetCCFXmlItem(pkt.anaunit, cbLEN_STR_UNIT, "anaunit");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbFILTDESC & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbFILTDESC");

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_FILT_LABEL, "label");
    lst += ccf::GetCCFXmlItem(pkt.hpfreq, "hpfreq");
    lst += ccf::GetCCFXmlItem(pkt.hporder, "hporder");
    lst += ccf::GetCCFXmlItem(pkt.hptype, "hptype");
    lst += ccf::GetCCFXmlItem(pkt.lpfreq, "lpfreq");
    lst += ccf::GetCCFXmlItem(pkt.lporder, "lporder");
    lst += ccf::GetCCFXmlItem(pkt.lptype, "lptype");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbMANUALUNITMAPPING & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbMANUALUNITMAPPING");

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.nOverride, "Override");
    lst += ccf::GetCCFXmlItem(pkt.afOrigin, 3, "center");
    lst += ccf::GetCCFXmlItem(pkt.afShape, 3, 3, "axes", "axis");
    lst += ccf::GetCCFXmlItem(pkt.aPhi, "Phi");
    lst += ccf::GetCCFXmlItem(pkt.bValid, "Valid");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbHOOP & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbHOOP");

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.valid, "valid");
    lst += ccf::GetCCFXmlItem(pkt.time, "time");
    lst += ccf::GetCCFXmlItem(pkt.min, "min");
    lst += ccf::GetCCFXmlItem(pkt.max, "max");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       17 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbAdaptControl & pkt, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cbAdaptControl");

    QVariantList lst;
    lst += ccf::GetCCFXmlItem(pkt.nMode, "mode");
    lst += ccf::GetCCFXmlItem(pkt.fTimeOutMinutes, "TimeOutMinutes");
    lst += ccf::GetCCFXmlItem(pkt.fElapsedMinutes, "ElapsedMinutes");

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(UINT32 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", QString("UINT32"));

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(INT32 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "INT32");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(UINT16 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "UINT16");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(INT16 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "INT16");

    m_xmlValue = number;
}

    // Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(UINT8 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "UINT8");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(INT8 & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "INT8");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(float & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "single");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(double & number, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "double");

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: CCF XML basic array constructor
CCFXmlItem::CCFXmlItem(char cstr[], int count, QString strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert("Type", "cstring");

    cstr[count - 1] = 0;
    m_xmlValue = QString(cstr);
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(QVariantList lst, QString strName)
{
    m_xmlTag = strName;
    m_xmlValue = lst;
}

//------------------------------------------------------------------------------------
//                    Abstract interface for CCFUtilsXmlItems
//------------------------------------------------------------------------------------

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct basic Xml item aray
template <typename T>
QVariant ccf::GetCCFXmlItem(T & pkt, QString strName)
{
    QVariant var = CCFXmlItem(pkt, strName);
    return var;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct Xml item aray
// Inputs:
//   pkt     - array
//   count   - number of elements
//   strName - array name
template <typename T>
QVariant ccf::GetCCFXmlItem(T pkt[], int count, QString strName)
{
    QVariantList lst;
    for (int i = 0; i < count; ++i)
    {
        CCFXmlItem item(pkt[i]);
        if (item.IsValid())
            lst += item;
    }
    QVariant var = CCFXmlItem(lst, strName);
    return var;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct Xml item 2D aray
// Inputs:
//   ppkt     - pointer to array
//   count1   - number of first level elements
//   count2   - number of second level elements
//   strName1 - first level name
//   strName2 - second level name
template <typename T>
QVariant ccf::GetCCFXmlItem(T ppkt[], int count1, int count2, QString strName1, QString strName2)
{
    QVariantList sublst;
    for (int i = 0; i < count1; ++i)
        sublst += ccf::GetCCFXmlItem(ppkt[i], count2, strName2);
    QVariant var = CCFXmlItem(sublst, strName1);
    return var;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct Xml item character string (specialize array for char type)
template <>
QVariant ccf::GetCCFXmlItem<char>(char pkt[], int count, QString strName)
{
    QVariant var = CCFXmlItem(pkt, count, strName);
    return var;
}

//------------------------------------------------------------------------------------
//                    Abstract interface for reading CCFUtilsXmlItems
//------------------------------------------------------------------------------------

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read generic version of item
//          (item should be something QVariant can parse)
// Inputs:
//   xml      - the xml file in the item group
//   item     - item to read
//   strName  - item name
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T & item)
{
    QVariant var = xml->value();
    item = var.value<T>();
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
// Outputs:
//  Returns the item number (1-based)
template<>
int ccf::ItemNumber(XmlFile * const xml, cbPKT_CHANINFO & item)
{
    ccf::ReadItem(xml, item.chan, "chan");
    return item.chan;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_CHANINFO & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.chan, "chan");
    ccf::ReadItem(xml, item.proc, "proc");
    ccf::ReadItem(xml, item.bank, "bank");
    ccf::ReadItem(xml, item.term, "term");
    ccf::ReadItem(xml, item.chancaps, "caps/chancaps");
    ccf::ReadItem(xml, item.doutcaps, "caps/doutcaps");
    ccf::ReadItem(xml, item.dinpcaps, "caps/dinpcaps");
    ccf::ReadItem(xml, item.aoutcaps, "caps/aoutcaps");
    ccf::ReadItem(xml, item.ainpcaps, "caps/ainpcaps");
    ccf::ReadItem(xml, item.spkcaps, "caps/spkcaps");
    ccf::ReadItem(xml, item.physcalin, "scale/physcalin");
    ccf::ReadItem(xml, item.phyfiltin, "filterdesc/phyfiltin");
    ccf::ReadItem(xml, item.physcalout, "scale/physcalout");
    ccf::ReadItem(xml, item.phyfiltout, "filterdesc/phyfiltout");
    ccf::ReadItem(xml, item.label, cbLEN_STR_LABEL, "label");
    ccf::ReadItem(xml, item.userflags, "userflags");
    ccf::ReadItem(xml, item.position, 4, "position");
    ccf::ReadItem(xml, item.scalin, "scale/scalin");
    ccf::ReadItem(xml, item.scalout, "scale/scalout");
    ccf::ReadItem(xml, item.doutopts, "options/doutopts");
    ccf::ReadItem(xml, item.dinpopts, "options/dinpopts");
    ccf::ReadItem(xml, item.aoutopts, "options/aoutopts");
    ccf::ReadItem(xml, item.eopchar, "eopchar");
    { // For union we recoord the most informative one
        ccf::ReadItem(xml, item.lowsamples, "monitor/lowsamples");
        ccf::ReadItem(xml, item.highsamples, "monitor/highsamples");
        ccf::ReadItem(xml, item.offset, "monitor/offset");
    }
    ccf::ReadItem(xml, item.ainpopts, "options/ainpopts");
    ccf::ReadItem(xml, item.lncrate, "lnc/rate");
    ccf::ReadItem(xml, item.smpfilter, "sample/filter");
    ccf::ReadItem(xml, item.smpgroup, "sample/group");
    ccf::ReadItem(xml, item.smpdispmin, "sample/dispmin");
    ccf::ReadItem(xml, item.smpdispmax, "sample/dispmax");
    ccf::ReadItem(xml, item.spkfilter, "spike/filter");
    ccf::ReadItem(xml, item.spkdispmax, "spike/dispmax");
    ccf::ReadItem(xml, item.lncdispmax, "lnc/dispmax");
    ccf::ReadItem(xml, item.spkopts, "options/spkopts");
    ccf::ReadItem(xml, item.spkthrlevel, "spike/threshold/level");
    ccf::ReadItem(xml, item.spkthrlimit, "spike/threshold/limit");
    ccf::ReadItem(xml, item.spkgroup, "spike/group");
    ccf::ReadItem(xml, item.amplrejpos, "spike/amplituderejecet/positive");
    ccf::ReadItem(xml, item.amplrejneg, "spike/amplituderejecet/negative");
    ccf::ReadItem(xml, item.refelecchan, "refelecchan");
    ccf::ReadItem(xml, item.unitmapping, cbMAXUNITS, "unitmapping");
    ccf::ReadItem(xml, item.spkhoops, cbMAXUNITS, cbMAXHOOPS, "spike/hoops");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_ADAPTFILTINFO & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.chan, "chan");
    ccf::ReadItem(xml, item.nMode, "mode");
    ccf::ReadItem(xml, item.dLearningRate, "LearningRate");
    ccf::ReadItem(xml, item.nRefChan1, "RefChan/RefChan_item");
    ccf::ReadItem(xml, item.nRefChan2, "RefChan/RefChan_item<1>");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SS_DETECT & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.fThreshold, "Threshold");
    ccf::ReadItem(xml, item.fMultiplier, "Multiplier");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SS_ARTIF_REJECT & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.nMaxSimulChans, "MaxSimulChans");
    ccf::ReadItem(xml, item.nRefractoryCount, "RefractoryCount");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
// Outputs:
//  Returns the item number (1-based)
template<>
int ccf::ItemNumber(XmlFile * const xml, cbPKT_SS_NOISE_BOUNDARY & item)
{
    ccf::ReadItem(xml, item.chan, "chan");
    return item.chan;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SS_NOISE_BOUNDARY & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.chan, "chan");
    ccf::ReadItem(xml, item.afc, 3, "center");
    ccf::ReadItem(xml, item.afS, 3, 3, "axes");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SS_STATISTICS & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.nUpdateSpikes, "UpdateSpikes");
    ccf::ReadItem(xml, item.nAutoalg, "Autoalg");
    ccf::ReadItem(xml, item.nMode, "mode");
    ccf::ReadItem(xml, item.fMinClusterPairSpreadFactor, "Cluster/MinClusterPairSpreadFactor");
    ccf::ReadItem(xml, item.fMaxSubclusterSpreadFactor, "Cluster/MaxSubclusterSpreadFactor");
    ccf::ReadItem(xml, item.fMinClusterHistCorrMajMeasure, "Cluster/MinClusterHistCorrMajMeasure");
    ccf::ReadItem(xml, item.fMaxClusterPairHistCorrMajMeasure, "Cluster/MaxClusterPairHistCorrMajMeasure");
    ccf::ReadItem(xml, item.fClusterHistValleyPercentage, "Cluster/ClusterHistValleyPercentage");
    ccf::ReadItem(xml, item.fClusterHistClosePeakPercentage, "Cluster/ClusterHistClosePeakPercentage");
    ccf::ReadItem(xml, item.fClusterHistMinPeakPercentage, "Cluster/ClusterHistMinPeakPercentage");
    ccf::ReadItem(xml, item.nWaveBasisSize, "WaveBasisSize");
    ccf::ReadItem(xml, item.nWaveSampleSize, "WaveSampleSize");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SS_STATUS & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.cntlUnitStats, "cntlUnitStats");
    ccf::ReadItem(xml, item.cntlNumUnits, "cntlNumUnits");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_SYSINFO & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.sysfreq, "sysfreq");
    ccf::ReadItem(xml, item.spikelen, "spike/length");
    ccf::ReadItem(xml, item.spikepre, "spike/pretrigger");
    ccf::ReadItem(xml, item.resetque, "resetque");
    ccf::ReadItem(xml, item.runlevel, "runlevel");
    ccf::ReadItem(xml, item.runflags, "runflags");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
// Outputs:
//  Returns the item number (1-based)
template<>
int ccf::ItemNumber(XmlFile * const xml, cbPKT_NTRODEINFO & item)
{
    ccf::ReadItem(xml, item.ntrode, "ntrode");
    return item.ntrode;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_NTRODEINFO & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.ntrode, "ntrode");
    ccf::ReadItem(xml, item.label, cbLEN_STR_LABEL, "label");
    ccf::ReadItem(xml, item.ellipses, cbMAXSITEPLOTS, cbMAXUNITS, "ellipses");
    ccf::ReadItem(xml, item.nSite, "site");
    ccf::ReadItem(xml, item.fs, "featurespace");
    ccf::ReadItem(xml, item.nChan, cbMAXSITES, "chan");
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_LNC & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.lncFreq, "frequency");
    ccf::ReadItem(xml, item.lncRefChan, "RefChan/RefChan_item");
    ccf::ReadItem(xml, item.lncGlobalMode, "GlobalMode");
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_FILTINFO & item)
{
    ccf::ReadItem(xml, item.chid, "chid");
    ccf::ReadItem(xml, item.type, "type");
    ccf::ReadItem(xml, item.dlen, "dlen");
    ccf::ReadItem(xml, item.proc, "proc");
    ccf::ReadItem(xml, item.filt, "filter");
    ccf::ReadItem(xml, item.label, cbLEN_STR_LABEL, "label");
    ccf::ReadItem(xml, item.hpfreq, "filterdesc/digfilt/hpfreq");
    ccf::ReadItem(xml, item.hporder, "filterdesc/digfilt/hporder");
    ccf::ReadItem(xml, item.hptype, "filterdesc/digfilt/hptype");
    ccf::ReadItem(xml, item.lpfreq, "filterdesc/digfilt/lpfreq");
    ccf::ReadItem(xml, item.lporder, "filterdesc/digfilt/lporder");
    ccf::ReadItem(xml, item.lptype, "filterdesc/digfilt/lptype");
    ccf::ReadItem(xml, item.sos1a1, "filterdesc/digfilt/sos1/a1");
    ccf::ReadItem(xml, item.sos1a2, "filterdesc/digfilt/sos1/a2");
    ccf::ReadItem(xml, item.sos1b1, "filterdesc/digfilt/sos1/b1");
    ccf::ReadItem(xml, item.sos1b2, "filterdesc/digfilt/sos1/b2");
    ccf::ReadItem(xml, item.sos2a1, "filterdesc/digfilt/sos2/a1");
    ccf::ReadItem(xml, item.sos2a2, "filterdesc/digfilt/sos2/a2");
    ccf::ReadItem(xml, item.sos2b1, "filterdesc/digfilt/sos2/b1");
    ccf::ReadItem(xml, item.sos2b2, "filterdesc/digfilt/sos2/b2");
    ccf::ReadItem(xml, item.gain, "filterdesc/digfilt/gain");
}

// Author & Date: Griffin Milsap       10 Dec 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_AOUT_WAVEFORM & item)
{
    // FIXME: Implement this.
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbSCALING & item)
{
    ccf::ReadItem(xml, item.digmin, "digmin");
    ccf::ReadItem(xml, item.digmax, "digmax");
    ccf::ReadItem(xml, item.anamin, "anamin");
    ccf::ReadItem(xml, item.anamax, "anamax");
    ccf::ReadItem(xml, item.anagain, "anagain");
    ccf::ReadItem(xml, item.anaunit, cbLEN_STR_UNIT, "anaunit");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbFILTDESC & item)
{
    ccf::ReadItem(xml, item.label, cbLEN_STR_FILT_LABEL, "label");
    ccf::ReadItem(xml, item.hpfreq, "hpfreq");
    ccf::ReadItem(xml, item.hporder, "hporder");
    ccf::ReadItem(xml, item.hptype, "hptype");
    ccf::ReadItem(xml, item.lpfreq, "lpfreq");
    ccf::ReadItem(xml, item.lporder, "lporder");
    ccf::ReadItem(xml, item.lptype, "lptype");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbMANUALUNITMAPPING & item)
{
    ccf::ReadItem(xml, item.nOverride, "Override");
    ccf::ReadItem(xml, item.afOrigin, 3, "center");
    ccf::ReadItem(xml, item.afShape, 3, 3, "axes");
    ccf::ReadItem(xml, item.aPhi, "Phi");
    ccf::ReadItem(xml, item.bValid, "Valid");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbHOOP & item)
{
    ccf::ReadItem(xml, item.valid, "valid");
    ccf::ReadItem(xml, item.time, "time");
    ccf::ReadItem(xml, item.min, "min");
    ccf::ReadItem(xml, item.max, "max");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbAdaptControl & item)
{
    ccf::ReadItem(xml, item.nMode, "mode");
    ccf::ReadItem(xml, item.fTimeOutMinutes, "TimeOutMinutes");
    ccf::ReadItem(xml, item.fElapsedMinutes, "ElapsedMinutes");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read array of items
// Inputs:
//   xml      - the xml file in the item group
//   item     - array of items
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T item[], int count)
{
    QMap<QString, int> mapItemCount;
    int subcount = 0;
    QStringList lst = xml->childKeys();
    for (int i = 0; i < lst.count(); ++i)
    {
        QString strSubKey = lst.at(i);
        subcount = mapItemCount[strSubKey];
        mapItemCount[strSubKey] = subcount + 1;
        if (subcount)
            strSubKey = strSubKey + QString("<%1>").arg(subcount);
        T tmp;
        // Initialize with possible item
        int num = ItemNumber(xml, tmp, strSubKey);
        if (num == 0 && i < count)
            num = i;
        else
            num--; // Make it 0-baed
        if (num < count)
        {
            tmp = item[num];
            ReadItem(xml, tmp, strSubKey);
            item[num] = tmp;
        }
    }
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read generic version of cstring item
// Inputs:
//   xml      - the xml file in the item group
//   item     - cstring item to load
//   count    - cstring length
template<>
void ccf::ReadItem(XmlFile * const xml, char item[], int count)
{
    QString var = xml->value().toString();
    strncpy(item, var.toAscii().constData(), count);
    item[count - 1] = 0;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read array of items
// Inputs:
//   xml      - the xml file in the item group
//   item     - array of items
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T item[], int count, QString strName)
{
    if (!xml->beginGroup(strName))
        ReadItem(xml, item, count);
    xml->endGroup();
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read 2D array of items
// Inputs:
//   xml      - the xml file in the item group
//   pItem    - array of items
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T pItem[], int count1, int count2, QString strName)
{
    if (!xml->beginGroup(strName))
    {
        QMap<QString, int> mapItemCount;
        int subcount = 0;
        QStringList lst = xml->childKeys();
        count1 = min(lst.count(), count1);
        for (int i = 0; i < count1; ++i)
        {
            QString strSubKey = lst.at(i);
            subcount = mapItemCount[strSubKey];
            mapItemCount[strSubKey] = subcount + 1;
            if (subcount)
                strSubKey = strSubKey + QString("<%1>").arg(subcount);
            ReadItem(xml, pItem[i], count2, strSubKey);
        }
    }
    xml->endGroup();
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read generic version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item to read
//   strName  - item name
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T & item, QString strName)
{
    if (!xml->beginGroup(strName))
        ReadItem(xml, item);
    xml->endGroup();
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
//   strName  - item name to navigaet to
// Outputs:
//  Returns the item number (1-based)
template<typename T>
int ccf::ItemNumber(XmlFile * const xml, T & item, QString strName)
{
    int res = 0;
    if (!xml->beginGroup(strName))
        res = ItemNumber(xml, item);
    xml->endGroup();
    return res;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
// Outputs:
//  Returns the item number (1-based)
template<typename T>
int ccf::ItemNumber(XmlFile * const xml, T & item)
{
    // Any item that is not specialized cannot be enumerated
    return 0;
}

// For link to proceed here we have to mention possible template types if not used in this unit
template QVariant ccf::GetCCFXmlItem<cbPKT_ADAPTFILTINFO>(cbPKT_ADAPTFILTINFO&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SYSINFO>(cbPKT_SYSINFO&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SS_STATUS>(cbPKT_SS_STATUS&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SS_ARTIF_REJECT>(cbPKT_SS_ARTIF_REJECT&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SS_DETECT>(cbPKT_SS_DETECT&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SS_STATISTICS>(cbPKT_SS_STATISTICS&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_LNC>(cbPKT_LNC&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_FILTINFO>(cbPKT_FILTINFO&, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_AOUT_WAVEFORM>(cbPKT_AOUT_WAVEFORM&, QString);

template QVariant ccf::GetCCFXmlItem<cbPKT_CHANINFO>(cbPKT_CHANINFO * const, int, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_NTRODEINFO>(cbPKT_NTRODEINFO * const, int, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_SS_NOISE_BOUNDARY>(cbPKT_SS_NOISE_BOUNDARY * const, int, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_FILTINFO>(cbPKT_FILTINFO * const, int, QString);
template QVariant ccf::GetCCFXmlItem<cbPKT_AOUT_WAVEFORM>(cbPKT_AOUT_WAVEFORM * const, int, QString);

// For link to proceed here we have to mention possible template types if not used in this unit
template void ccf::ReadItem<cbPKT_ADAPTFILTINFO>(XmlFile * const, cbPKT_ADAPTFILTINFO &, QString);
template void ccf::ReadItem<cbPKT_SYSINFO>(XmlFile * const, cbPKT_SYSINFO &, QString);
template void ccf::ReadItem<cbPKT_SS_STATUS>(XmlFile * const, cbPKT_SS_STATUS &, QString);
template void ccf::ReadItem<cbPKT_SS_ARTIF_REJECT>(XmlFile * const, cbPKT_SS_ARTIF_REJECT &, QString);
template void ccf::ReadItem<cbPKT_SS_DETECT>(XmlFile * const, cbPKT_SS_DETECT &, QString);
template void ccf::ReadItem<cbPKT_SS_STATISTICS>(XmlFile * const, cbPKT_SS_STATISTICS &, QString);
template void ccf::ReadItem<cbPKT_LNC>(XmlFile * const, cbPKT_LNC &, QString);
template void ccf::ReadItem<cbPKT_FILTINFO>(XmlFile * const, cbPKT_FILTINFO &, QString);
template void ccf::ReadItem<cbPKT_AOUT_WAVEFORM>(XmlFile * const, cbPKT_AOUT_WAVEFORM &, QString);

template void ccf::ReadItem<cbPKT_CHANINFO>(XmlFile * const, cbPKT_CHANINFO * const, int, QString);
template void ccf::ReadItem<cbPKT_NTRODEINFO>(XmlFile * const, cbPKT_NTRODEINFO * const, int, QString);
template void ccf::ReadItem<cbPKT_SS_NOISE_BOUNDARY>(XmlFile * const, cbPKT_SS_NOISE_BOUNDARY * const, int, QString);
template void ccf::ReadItem<cbPKT_FILTINFO>(XmlFile * const, cbPKT_FILTINFO * const, int, QString);
template void ccf::ReadItem<cbPKT_AOUT_WAVEFORM>(XmlFile * const, cbPKT_AOUT_WAVEFORM * const, int, QString);
