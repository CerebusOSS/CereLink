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

#include <algorithm>  // Use C++ default min and max implementation.
#include <type_traits>  // For std::is_same_v
#include "CCFUtilsXmlItems.h"
#include "CCFUtilsXmlItemsGenerate.h"
#include "CCFUtilsXmlItemsParse.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif


// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_CHANINFO & pkt, std::string strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert({"label", std::string(pkt.label)});
    m_xmlAttribs.insert({"Type", "cbPKT_CHANINFO"});
    if (pkt.chan == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.chan, "chan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.proc, "proc"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.bank, "bank"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.term, "term"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.chancaps, "caps/chancaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.doutcaps, "caps/doutcaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.dinpcaps, "caps/dinpcaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.aoutcaps, "caps/aoutcaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.ainpcaps, "caps/ainpcaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkcaps, "caps/spkcaps"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.physcalin, "scale/physcalin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.phyfiltin, "filterdesc/phyfiltin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.physcalout, "scale/physcalout"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.phyfiltout, "filterdesc/phyfiltout"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.userflags, "userflags"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.position, 4, "position"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.scalin, "scale/scalin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.scalout, "scale/scalout"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.doutopts, "options/doutopts"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.dinpopts, "options/dinpopts"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.aoutopts, "options/aoutopts"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.eopchar, "eopchar"));
    { // For union we recoord the most informative one
        lst.push_back(ccf::GetCCFXmlItem(pkt.lowsamples, "monitor/lowsamples"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.highsamples, "monitor/highsamples"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.offset, "monitor/offset"));
    }
    lst.push_back(ccf::GetCCFXmlItem(pkt.ainpopts, "options/ainpopts"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lncrate, "lnc/rate"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.smpfilter, "sample/filter"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.smpgroup, "sample/group"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.smpdispmin, "sample/dispmin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.smpdispmax, "sample/dispmax"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkfilter, "spike/filter"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkdispmax, "spike/dispmax"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lncdispmax, "lnc/dispmax"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkopts, "options/spkopts"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkthrlevel, "spike/threshold/level"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkthrlimit, "spike/threshold/limit"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkgroup, "spike/group"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.amplrejpos, "spike/amplituderejecet/positive"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.amplrejneg, "spike/amplituderejecet/negative"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.refelecchan, "refelecchan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.unitmapping, cbMAXUNITS, "unitmapping"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spkhoops, cbMAXUNITS, cbMAXHOOPS, "spike/hoops", "hoop"));
	lst.push_back(ccf::GetCCFXmlItem(pkt.trigtype, "douttrig/type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trigchan, "douttrig/chan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trigval, "douttrig/val"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_ADAPTFILTINFO & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_ADAPTFILTINFO"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.chan, "chan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nMode, "mode"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.dLearningRate, "LearningRate"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nRefChan1, "RefChan/RefChan_item"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nRefChan2, "RefChan/RefChan_item<1>"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_DETECT & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SS_DETECT"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fThreshold, "Threshold"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fMultiplier, "Multiplier"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_ARTIF_REJECT & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SS_ARTIF_REJECT"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nMaxSimulChans, "MaxSimulChans"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nRefractoryCount, "RefractoryCount"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_NOISE_BOUNDARY & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SS_NOISE_BOUNDARY"});
    if (pkt.chan == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.chan, "chan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.afc, 3, "center"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.afS, 3, 3, "axes", "axis"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_STATISTICS & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SS_STATISTICS"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nUpdateSpikes, "UpdateSpikes"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nAutoalg, "Autoalg"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nMode, "mode"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fMinClusterPairSpreadFactor, "Cluster/MinClusterPairSpreadFactor"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fMaxSubclusterSpreadFactor, "Cluster/MaxSubclusterSpreadFactor"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fMinClusterHistCorrMajMeasure, "Cluster/MinClusterHistCorrMajMeasure"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fMaxClusterPairHistCorrMajMeasure, "Cluster/MaxClusterPairHistCorrMajMeasure"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fClusterHistValleyPercentage, "Cluster/ClusterHistValleyPercentage"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fClusterHistClosePeakPercentage, "Cluster/ClusterHistClosePeakPercentage"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fClusterHistMinPeakPercentage, "Cluster/ClusterHistMinPeakPercentage"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nWaveBasisSize, "WaveBasisSize"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nWaveSampleSize, "WaveSampleSize"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SS_STATUS & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SS_STATUS"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cntlUnitStats, "cntlUnitStats"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cntlNumUnits, "cntlNumUnits"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_SYSINFO & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_SYSINFO"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sysfreq, "sysfreq"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spikelen, "spike/length"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.spikepre, "spike/pretrigger"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.resetque, "resetque"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.runlevel, "runlevel"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.runflags, "runflags"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_NTRODEINFO & pkt, std::string strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert({"label", std::string(pkt.label)});
    m_xmlAttribs.insert({"Type", "cbPKT_NTRODEINFO"});
    if (pkt.ntrode == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.ntrode, "ntrode"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.ellipses, cbMAXSITEPLOTS, cbMAXUNITS, "ellipses", "ellipse"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nSite, "site"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fs, "featurespace"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.nChan, cbMAXSITES, "chan"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_LNC & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_LNC"});
    if (pkt.lncFreq == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lncFreq, "frequency"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lncRefChan, "RefChan/RefChan_item"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lncGlobalMode, "GlobalMode"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_FILTINFO & pkt, std::string strName)
{
    m_xmlTag = strName;
    pkt.label[cbLEN_STR_LABEL - 1] = 0;
    m_xmlAttribs.insert({"label", std::string(pkt.label)});
    m_xmlAttribs.insert({"Type", "cbPKT_FILTINFO"});
    if (pkt.cbpkt_header.type == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.proc, "proc"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.filt, "filter"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_LABEL, "label"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hpfreq, "filterdesc/digfilt/hpfreq"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hporder, "filterdesc/digfilt/hporder"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hptype, "filterdesc/digfilt/hptype"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lpfreq, "filterdesc/digfilt/lpfreq"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lporder, "filterdesc/digfilt/lporder"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lptype, "filterdesc/digfilt/lptype"));
    // Although we can save as an array such as sos[2][2], I do not think that is good idea
    //  because we never plan to go higher than second-order (already IIR is problematic)
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos1a1, "filterdesc/digfilt/sos1/a1"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos1a2, "filterdesc/digfilt/sos1/a2"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos1b1, "filterdesc/digfilt/sos1/b1"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos1b2, "filterdesc/digfilt/sos1/b2"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos2a1, "filterdesc/digfilt/sos2/a1"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos2a2, "filterdesc/digfilt/sos2/a2"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos2b1, "filterdesc/digfilt/sos2/b1"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.sos2b2, "filterdesc/digfilt/sos2/b2"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.gain, "filterdesc/digfilt/gain"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbPKT_AOUT_WAVEFORM & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbPKT_AOUT_WAVEFORM"});
    if (pkt.chan == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.chid, "chid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.type, "type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.cbpkt_header.dlen, "dlen"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.chan, "chan"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.repeats, "repeats"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trig, "trigger/type"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trigChan, "trigger/channel"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trigValue, "trigger/value"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.trigNum, "number"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.active, "active"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.mode, "wave/mode"));
    std::any var = CCFXmlItem(pkt.wave, pkt.mode, "wave");
    lst.push_back(var);

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbSCALING & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbSCALING"});

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.digmin, "digmin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.digmax, "digmax"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.anamin, "anamin"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.anamax, "anamax"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.anagain, "anagain"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.anaunit, cbLEN_STR_UNIT, "anaunit"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbFILTDESC & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbFILTDESC"});

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.label, cbLEN_STR_FILT_LABEL, "label"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hpfreq, "hpfreq"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hporder, "hporder"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.hptype, "hptype"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lpfreq, "lpfreq"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lporder, "lporder"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.lptype, "lptype"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbMANUALUNITMAPPING & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbMANUALUNITMAPPING"});

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.nOverride, "Override"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.afOrigin, 3, "center"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.afShape, 3, 3, "axes", "axis"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.aPhi, "Phi"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.bValid, "Valid"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbHOOP & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbHOOP"});

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.valid, "valid"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.time, "time"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.min, "min"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.max, "max"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       17 April 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbAdaptControl & pkt, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbAdaptControl"});

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.nMode, "mode"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fTimeOutMinutes, "TimeOutMinutes"));
    lst.push_back(ccf::GetCCFXmlItem(pkt.fElapsedMinutes, "ElapsedMinutes"));

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: CCF XML item constructor
CCFXmlItem::CCFXmlItem(cbWaveformData & pkt, uint16_t mode, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cbWaveformData"});
    if (mode == 0)
        return;

    std::vector<std::any> lst;
    lst.push_back(ccf::GetCCFXmlItem(pkt.offset, "offset"));
    if (mode == cbWAVEFORM_MODE_SINE)
    {
        lst.push_back(ccf::GetCCFXmlItem(pkt.sineFrequency, "sine/frequency"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.sineAmplitude, "sine/amplitude"));
    }
    else if (mode == cbWAVEFORM_MODE_PARAMETERS)
    {
        lst.push_back(ccf::GetCCFXmlItem(pkt.seqTotal, "parameters/total"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.seq, "parameters/seq"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.phases, "parameters/phases"));
        // Partially save these parameters
        uint16_t phases = pkt.phases;
        if (phases > cbMAX_WAVEFORM_PHASES)
            phases = cbMAX_WAVEFORM_PHASES;
        lst.push_back(ccf::GetCCFXmlItem(pkt.duration, phases, "parameters/duration"));
        lst.push_back(ccf::GetCCFXmlItem(pkt.amplitude, phases, "parameters/amplitude"));
        // future sequences will go to the parameters<n>
    }

    // Now use the list
    m_xmlValue = lst;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(uint32_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", std::string("uint32_t")});

    m_xmlValue = (unsigned int) number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(int32_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "int32_t"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(uint16_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "uint16_t"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(int16_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "int16_t"});

    m_xmlValue = number;
}

    // Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(uint8_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "uint8_t"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(int8_t & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "int8_t"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(float & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "single"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(double & number, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "double"});

    m_xmlValue = number;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: CCF XML basic array constructor
CCFXmlItem::CCFXmlItem(char cstr[], int count, std::string strName)
{
    m_xmlTag = strName;
    m_xmlAttribs.insert({"Type", "cstring"});

    cstr[count - 1] = 0;
    m_xmlValue = cstr;
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML basic number constructor
CCFXmlItem::CCFXmlItem(std::vector<std::any> lst, std::string strName)
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
std::any ccf::GetCCFXmlItem(T & pkt, std::string strName)
{
    std::any var = CCFXmlItem(pkt, strName);
    return var;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct Xml item aray
// Inputs:
//   pkt     - array
//   count   - number of elements
//   strName - array name
template <typename T>
std::any ccf::GetCCFXmlItem(T pkt[], int count, std::string strName)
{
    std::vector<std::any> lst;
    for (int i = 0; i < count; ++i)
    {
        CCFXmlItem item(pkt[i]);
        if (item.IsValid())
            lst.push_back(item.XmlValue());
    }
    std::any var = CCFXmlItem(lst, strName);
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
std::any ccf::GetCCFXmlItem(T ppkt[], int count1, int count2, std::string strName1, std::string strName2)
{
    std::vector<std::any> sublst;
    for (int i = 0; i < count1; ++i)
        sublst.push_back(ccf::GetCCFXmlItem(ppkt[i], count2, strName2));
    std::any var = CCFXmlItem(sublst, strName1);
    return var;
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Construct Xml item character string (specialize array for char type)
template <>
std::any ccf::GetCCFXmlItem<char>(char pkt[], int count, std::string strName)
{
    std::any var = CCFXmlItem(pkt, count, strName);
    return var;
}

//------------------------------------------------------------------------------------
//                    Abstract interface for reading CCFUtilsXmlItems
//------------------------------------------------------------------------------------

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read generic version of item
//          (item should be something that can be parsed from string or value)
// Inputs:
//   xml      - the xml file in the item group
//   item     - item to read
//   strName  - item name
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T & item)
{
    std::any var = xml->value();

    // Check if var is empty (node doesn't exist or has no value)
    if (!var.has_value()) {
        if constexpr (std::is_arithmetic_v<T>) {
            item = T{};
        }
        return;
    }

    // Unlike QVariant, std::any does not auto-convert types.
    // XML parsing always returns strings, so we need to convert them to numeric types.
    if (var.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(var);
        // Handle empty strings for numeric types (default to 0)
        if (str.empty() && std::is_arithmetic_v<T>) {
            item = T{};
            return;
        }
        // Convert string to the target type using compile-time type checks
        if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) {
                // Signed integral types
                if constexpr (sizeof(T) == 1) {
                    item = static_cast<T>(std::stoi(str));
                } else if constexpr (sizeof(T) == 2) {
                    item = static_cast<T>(std::stoi(str));
                } else if constexpr (sizeof(T) == 4) {
                    item = static_cast<T>(std::stoi(str));
                } else {
                    item = static_cast<T>(std::stoll(str));
                }
            } else {
                // Unsigned integral types
                if constexpr (sizeof(T) == 1) {
                    item = static_cast<T>(std::stoul(str));
                } else if constexpr (sizeof(T) == 2) {
                    item = static_cast<T>(std::stoul(str));
                } else if constexpr (sizeof(T) == 4) {
                    item = static_cast<T>(std::stoul(str));
                } else {
                    item = static_cast<T>(std::stoull(str));
                }
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            // Floating point types
            if constexpr (sizeof(T) == 4) {
                item = static_cast<T>(std::stof(str));
            } else {
                item = static_cast<T>(std::stod(str));
            }
        } else {
            // For non-numeric types, try direct cast
            item = std::any_cast<T>(var);
        }
    } else {
        // If not a string, try direct cast (shouldn't normally happen from XML)
        item = std::any_cast<T>(var);
    }
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
	ccf::ReadItem(xml, item.trigtype, "douttrig/type");
    ccf::ReadItem(xml, item.trigchan, "douttrig/chan");
    ccf::ReadItem(xml, item.trigval, "douttrig/val");
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_ADAPTFILTINFO & item)
{
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
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

// Author & Date: Ehsan Azar       12 Sept 2012
// Purpose: Enumerate this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
// Outputs:
//  Returns the item number (1-based)
template<>
int ccf::ItemNumber(XmlFile * const xml, cbPKT_AOUT_WAVEFORM & item)
{
    ccf::ReadItem(xml, item.trigNum, "number");
    if (item.trigNum < cbMAX_AOUT_TRIGGER)
    {
        return (item.trigNum + 1);
    }
    // return zero means invalid
    return 0;
}

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbPKT_AOUT_WAVEFORM & item)
{
    ccf::ReadItem(xml, item.cbpkt_header.chid, "chid");
    ccf::ReadItem(xml, item.cbpkt_header.type, "type");
    ccf::ReadItem(xml, item.cbpkt_header.dlen, "dlen");
    ccf::ReadItem(xml, item.chan, "chan");
    ccf::ReadItem(xml, item.mode, "wave/mode");
    ccf::ReadItem(xml, item.repeats, "repeats");
    ccf::ReadItem(xml, item.trig, "trigger/type");
    ccf::ReadItem(xml, item.trigChan, "trigger/channel");
    ccf::ReadItem(xml, item.trigValue, "trigger/value");
    ccf::ReadItem(xml, item.trigNum, "number");
    ccf::ReadItem(xml, item.active, "active");
    ccf::ReadItem(xml, item.wave, "wave");
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

// Author & Date: Ehsan Azar       11 Sept 2012
// Purpose: Read this version of item
// Inputs:
//   xml      - the xml file in the item group
//   item     - item for this version
template<>
void ccf::ReadItem(XmlFile * const xml, cbWaveformData & item)
{
    uint16_t mode = 0;
    ccf::ReadItem(xml, item.offset, "offset");
    ccf::ReadItem(xml, mode, "mode");
    if (mode == cbWAVEFORM_MODE_SINE)
    {
        ccf::ReadItem(xml, item.sineFrequency, "sine/frequency");
        ccf::ReadItem(xml, item.sineAmplitude, "sine/amplitude");
    }
    else if (mode == cbWAVEFORM_MODE_PARAMETERS)
    {
        ccf::ReadItem(xml, item.seqTotal, "parameters/total");
        ccf::ReadItem(xml, item.seq, "parameters/seq");
        ccf::ReadItem(xml, item.phases, "parameters/phases");
        // No need to try to partially read, it will already ignore if some data is missing
        ccf::ReadItem(xml, item.duration, cbMAX_WAVEFORM_PHASES, "parameters/duration");
        ccf::ReadItem(xml, item.amplitude, cbMAX_WAVEFORM_PHASES, "parameters/amplitude");
    }
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read array of items
// Inputs:
//   xml      - the xml file in the item group
//   item     - array of items
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T item[], int count)
{
    std::map<std::string, int> mapItemCount;
    int subcount = 0;
    std::vector<std::string> lst = xml->childKeys();
    for (int i = 0; i < lst.size(); ++i)
    {
        std::string strSubKey = lst.at(i);
        subcount = mapItemCount[strSubKey];
        mapItemCount[strSubKey] = subcount + 1;
        if (subcount)
            strSubKey += std::to_string(subcount);
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
    std::any val = xml->value();
    std::string var;
    if (val.has_value() && val.type() == typeid(std::string)) {
        var = std::any_cast<std::string>(val);
    }
    // If val is empty or not a string, var will be empty string
    strncpy(item, var.c_str(), count);
    item[count - 1] = 0;  // '\0'
}

// Author & Date: Ehsan Azar       16 April 2012
// Purpose: Read array of items
// Inputs:
//   xml      - the xml file in the item group
//   item     - array of items
template <typename T>
void ccf::ReadItem(XmlFile * const xml, T item[], int count, std::string strName)
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
void ccf::ReadItem(XmlFile * const xml, T pItem[], int count1, int count2, std::string strName)
{
    if (!xml->beginGroup(strName))
    {
        std::map<std::string, int> mapItemCount;
        int subcount = 0;
        std::vector<std::string> lst = xml->childKeys();
        count1 = std::min((int)lst.size(), count1);
        for (int i = 0; i < count1; ++i)
        {
            std::string strSubKey = lst.at(i);
            subcount = mapItemCount[strSubKey];
            mapItemCount[strSubKey] = subcount + 1;
            if (subcount)
                strSubKey += std::to_string(subcount);
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
void ccf::ReadItem(XmlFile * const xml, T & item, std::string strName)
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
int ccf::ItemNumber(XmlFile * const xml, T & item, std::string strName)
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
template std::any ccf::GetCCFXmlItem<cbPKT_ADAPTFILTINFO>(cbPKT_ADAPTFILTINFO&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SYSINFO>(cbPKT_SYSINFO&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SS_STATUS>(cbPKT_SS_STATUS&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SS_ARTIF_REJECT>(cbPKT_SS_ARTIF_REJECT&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SS_DETECT>(cbPKT_SS_DETECT&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SS_STATISTICS>(cbPKT_SS_STATISTICS&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_LNC>(cbPKT_LNC&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_FILTINFO>(cbPKT_FILTINFO&, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_AOUT_WAVEFORM>(cbPKT_AOUT_WAVEFORM&, std::string);

template std::any ccf::GetCCFXmlItem<cbPKT_CHANINFO>(cbPKT_CHANINFO * const, int, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_NTRODEINFO>(cbPKT_NTRODEINFO * const, int, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_SS_NOISE_BOUNDARY>(cbPKT_SS_NOISE_BOUNDARY * const, int, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_FILTINFO>(cbPKT_FILTINFO * const, int, std::string);
template std::any ccf::GetCCFXmlItem<cbPKT_AOUT_WAVEFORM[cbMAX_AOUT_TRIGGER]>(cbPKT_AOUT_WAVEFORM (* const)[cbMAX_AOUT_TRIGGER], int, int, std::string, std::string);

// For link to proceed here we have to mention possible template types if not used in this unit
template void ccf::ReadItem<cbPKT_ADAPTFILTINFO>(XmlFile * const, cbPKT_ADAPTFILTINFO &, std::string);
template void ccf::ReadItem<cbPKT_SYSINFO>(XmlFile * const, cbPKT_SYSINFO &, std::string);
template void ccf::ReadItem<cbPKT_SS_STATUS>(XmlFile * const, cbPKT_SS_STATUS &, std::string);
template void ccf::ReadItem<cbPKT_SS_ARTIF_REJECT>(XmlFile * const, cbPKT_SS_ARTIF_REJECT &, std::string);
template void ccf::ReadItem<cbPKT_SS_DETECT>(XmlFile * const, cbPKT_SS_DETECT &, std::string);
template void ccf::ReadItem<cbPKT_SS_STATISTICS>(XmlFile * const, cbPKT_SS_STATISTICS &, std::string);
template void ccf::ReadItem<cbPKT_LNC>(XmlFile * const, cbPKT_LNC &, std::string);
template void ccf::ReadItem<cbPKT_FILTINFO>(XmlFile * const, cbPKT_FILTINFO &, std::string);
template void ccf::ReadItem<cbPKT_AOUT_WAVEFORM>(XmlFile * const, cbPKT_AOUT_WAVEFORM &, std::string);

template void ccf::ReadItem<cbPKT_CHANINFO>(XmlFile * const, cbPKT_CHANINFO * const, int, std::string);
template void ccf::ReadItem<cbPKT_NTRODEINFO>(XmlFile * const, cbPKT_NTRODEINFO * const, int, std::string);
template void ccf::ReadItem<cbPKT_SS_NOISE_BOUNDARY>(XmlFile * const, cbPKT_SS_NOISE_BOUNDARY * const, int, std::string);
template void ccf::ReadItem<cbPKT_FILTINFO>(XmlFile * const, cbPKT_FILTINFO * const, int, std::string);
template void ccf::ReadItem<cbPKT_AOUT_WAVEFORM[cbMAX_AOUT_TRIGGER]>(XmlFile * const, cbPKT_AOUT_WAVEFORM (* const)[cbMAX_AOUT_TRIGGER], int, int, std::string);
