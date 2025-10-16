// =STS=> cbsdk.cpp[5021].aa03   open     SMID:3 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2021 Blackrock Microsystems, LLC
//
// $Workfile: cbsdk.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/cbsdk.cpp $
// $Revision: 1 $
// $Date: 03/23/11 11:06p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
//  Notes:
//   Do NOT throw exceptions, they are evil if cross the shared library.
//    catch possible exceptions and handle them the earliest possible in this library,
//    then return error code if the library cannot recover.
//   Only functions are exported, no data, and definitely NO classes
//
/**
* \file cbsdk.cpp
* \brief Cerebus SDK main file.
*/

#include "StdAfx.h"
#include <algorithm>  // Use C++ default min and max implementation.
#include <chrono>     // For std::chrono::milliseconds
#include "SdkApp.h"
#include "../central/BmiVersion.h"
#include "cbHwlibHi.h"
#include "debugmacs.h"
#include <cmath>
#include "../../bindings/cbmex/res/cbmex.rc2"

#ifndef WIN32
#ifndef Sleep
    #define Sleep(x) usleep((x) * 1000)
#endif
#endif

// The sdk instances
SdkApp * g_app[cbMAXOPEN] = {nullptr};

#ifdef WIN32
// Author & Date:   Ehsan Azar     31 March 2011
// Purpose: Dll entry initialization
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // Only if FreeLibrary is called (extension usage)
        //  for non-extension usage OS takes better care of cleanup on exit.
        if (lpReserved == nullptr)
        {
            for (int i = 0; i < cbMAXOPEN; ++i)
                cbSdkClose(i);
        }
        break;
    }
    return TRUE;
}
#endif

// Author & Date:   Kirk Korver     03 Aug 2005

/** Called when a Sample Group packet (aka continuous data point or LFP) comes in.
*
* @param[in] pkt the sample group packet
*/
void SdkApp::OnPktGroup(const cbPKT_GROUP * const pkt)
{
    uint32_t nChanProcStart = 0;
    uint32_t nChanProcMax = 0;
    cbPROCINFO isProcInfo;
    uint32_t nInstrument = 0;
#ifndef CBPROTO_311
    nInstrument = pkt->cbpkt_header.instrument;
#endif
    if (IsStandAlone())
        nInstrument = 0;

    if (!m_bWithinTrial || m_CD == nullptr)
        return;

    const int group = pkt->cbpkt_header.type;

    if (group >= SMPGRP_RAW)  // TODO: Why >= ?!
        return;

#ifndef CBPROTO_311
    if (pkt->cbpkt_header.instrument >= cbMAXPROCS)
        nInstrument = 0;
    for (uint32_t nProc = 0; nProc < cbMAXPROCS; ++nProc)
    {
        if (NSP_FOUND == cbGetNspStatus(nProc + 1))
        {
            if (cbRESULT_OK == ::cbGetProcInfo(nProc + 1, &isProcInfo))
                nChanProcMax += isProcInfo.chancount;

            if (pkt->cbpkt_header.instrument == nProc)
                break;

            nChanProcStart = nChanProcMax;
        }
    }
#endif

    // Get information about this group...
    uint32_t  period;  // sampling period for the group
    uint32_t  nChans;  // number of channels in the group
    uint16_t  chanIds[cbNUM_ANALOG_CHANS];
    if (cbGetSampleGroupInfo(nInstrument + 1, group, nullptr, &period, &nChans, m_nInstance) != cbRESULT_OK)
        return;
    if (cbGetSampleGroupList(nInstrument + 1, group, &nChans, chanIds, m_nInstance) != cbRESULT_OK)
        return;

    const uint16_t rate = static_cast<int>((cbSdk_TICKS_PER_SECOND / static_cast<double>(period)));

    bool bOverFlow = false;

    m_lockTrial.lock();
    // double check if buffer is still valid
    if (m_CD)
    {
        const auto grp_idx = group - 1;  // Convert to 0-based index
        auto& grp = m_CD->groups[grp_idx];

        // Check if we need to allocate or reallocate
        if (grp.needsReallocation(chanIds, nChans))
        {
            // Use existing size if already allocated, otherwise use default
            const uint32_t buffer_size = grp.getSize() ? grp.getSize() : m_CD->default_size;
            if (!grp.allocate(buffer_size, nChans, chanIds, rate))
            {
                m_lockTrial.unlock();
                return;  // Allocation failed
            }
        }

        // Write sample to ring buffer (returns true if overflow occurred)
        bOverFlow = grp.writeSample(pkt->cbpkt_header.time, pkt->data, nChans);
    }
    m_lockTrial.unlock();

    if (bOverFlow)
    {
        /// \todo trial continuous buffer overflow event
    }
}

// Author & Date:   Ehsan Azar     24 March 2011
/** Called when a spike, digital or serial packet (aka event data) comes in.
*
* Also, the trial start and stop are set.
*
* @param[in] pPkt the event packet
*/
void SdkApp::OnPktEvent(const cbPKT_GENERIC * const pPkt)
{
    // check for trial beginning notification
    if (pPkt->cbpkt_header.chid == m_uTrialBeginChannel)
    {
        if ( (m_uTrialBeginMask & pPkt->data[0]) == m_uTrialBeginValue )
        {
            // reset the trial data cache if WithinTrial is currently False
            if (!m_bWithinTrial)
            {
                cbGetSystemClockTime(&m_uTrialStartTime, m_nInstance);
            }

            m_bWithinTrial = true;
        }
    }

    if (m_ED && m_bWithinTrial)
    {
        bool bOverFlow = false;

        uint16_t ch = pPkt->cbpkt_header.chid - 1;

        m_lockTrialEvent.lock();
        // double check if buffer is still valid
        if (m_ED)
        {
            // Add a sample...
            uint32_t old_write_index = m_ED->write_index[ch];
            // If there's room for more data...
            uint32_t new_write_index = old_write_index + 1;
            if (new_write_index >= m_ED->size)
                new_write_index = 0;

            if (new_write_index != m_ED->write_start_index[ch])
            {
                // Store more data
                m_ED->timestamps[ch][old_write_index] = pPkt->cbpkt_header.time;
                if (IsChanDigin(pPkt->cbpkt_header.chid) || IsChanSerial(pPkt->cbpkt_header.chid))
                    m_ED->units[ch][old_write_index] = static_cast<uint16_t>(pPkt->data[0] & 0x0000ffff);  // Store the 0th data sample (truncated to 16-bit).
                else
                    m_ED->units[ch][old_write_index] = pPkt->cbpkt_header.type; // Store the type.
                m_ED->write_index[ch] = new_write_index;

                if (m_bPacketsEvent)
                {
                    m_lockGetPacketsEvent.lock();
                        if (pPkt->cbpkt_header.time > m_uTrialStartTime)
                        {
                            m_bPacketsEvent = false;
                            m_waitPacketsEvent.notify_all();
                        }
                    m_lockGetPacketsEvent.unlock();
                }
            }
            else if (m_bChannelMask[pPkt->cbpkt_header.chid - 1])
                bOverFlow = true;
        }
        m_lockTrialEvent.unlock();

        if (bOverFlow)
        {
            /// \todo trial event buffer overflow event
        }
    }

    // check for trial end notification
    if (pPkt->cbpkt_header.chid == m_uTrialEndChannel)
    {
        if ( (m_uTrialEndMask & pPkt->data[0]) == m_uTrialEndValue )
            m_bWithinTrial = false;
    }
}

// Author & Date:   Ehsan Azar     27 Oct 2011
/** Called when a comment packet comes in.
*
* @param[in] pPkt the comment packet
*/
void SdkApp::OnPktComment(const cbPKT_COMMENT * const pPkt)
{
    if (m_CMT && m_bWithinTrial)
    {
        m_lockTrialComment.lock();
        // double check if buffer is still valid
        if (m_CMT)
        {
            // Add a sample...
            // If there's room for more data...
            uint32_t new_write_index = m_CMT->write_index + 1;
            if (new_write_index >= m_CMT->size)
                new_write_index = 0;

            if (new_write_index != m_CMT->write_start_index)
            {
                uint32_t write_index = m_CMT->write_index;
                // Store more data
                m_CMT->charset[write_index]  = pPkt->info.charset;
#ifndef CBPROTO_311
                m_CMT->timestamps[write_index] = pPkt->timeStarted;
                m_CMT->rgba[write_index] = pPkt->rgba;
#endif

                strncpy(reinterpret_cast<char *>(m_CMT->comments[write_index]), (const char *)(&pPkt->comment[0]), cbMAX_COMMENT);
                m_CMT->write_index = new_write_index;

                if (m_bPacketsCmt)
                {
                    m_lockGetPacketsCmt.lock();
                        if (pPkt->cbpkt_header.time > m_uTrialStartTime)
                        {
                            m_bPacketsCmt = false;
                            m_waitPacketsCmt.notify_all();
                        }
                    m_lockGetPacketsCmt.unlock();
                }
            }
        }
        m_lockTrialComment.unlock();
    }
}

// Author & Date:   Hyrum L. Sessions   10 June 2016
/** Called when a comment packet comes in.
*
* @param[in] pPkt the log packet
*/
void SdkApp::OnPktLog(const cbPKT_LOG * const pPkt)
{
    if (m_CMT && m_bWithinTrial)
    {
        m_lockTrialComment.lock();
        // double check if buffer is still valid
        if (m_CMT)
        {
            // Add a sample...
            // If there's room for more data...
            uint32_t new_write_index = m_CMT->write_index + 1;
            if (new_write_index >= m_CMT->size)
                new_write_index = 0;

            if (new_write_index != m_CMT->write_start_index)
            {
                uint32_t write_index = m_CMT->write_index;
                // Store more data
                m_CMT->charset[write_index]  = 0;   // force to ANSI charset
                m_CMT->rgba[write_index]  = 0xFFFFFFFF;
                m_CMT->timestamps[write_index] = pPkt->cbpkt_header.time;

                strncpy(reinterpret_cast<char *>(m_CMT->comments[write_index]), (const char *)(&pPkt->desc[0]), cbMAX_LOG);
                m_CMT->write_index = new_write_index;

                if (m_bPacketsCmt)
                {
                    m_lockGetPacketsCmt.lock();
                        if (pPkt->cbpkt_header.time > m_uTrialStartTime)
                        {
                            m_bPacketsCmt = false;
                            m_waitPacketsCmt.notify_all();
                        }
                    m_lockGetPacketsCmt.unlock();
                }
            }
        }
        m_lockTrialComment.unlock();
    }
}

// Author & Date:   Ehsan Azar     27 Oct 2011
/** Called when a video tracking packet comes in.
*
* Fills tracking global cache considering the last synchronization packet.
*
* @param[in] pPkt the video packet
*/
void SdkApp::OnPktTrack(const cbPKT_VIDEOTRACK * const pPkt)
{
    if (m_TR && m_bWithinTrial && m_lastPktVideoSynch.cbpkt_header.chid == cbPKTCHAN_CONFIGURATION)
    {
        const uint16_t id = pPkt->nodeID; // 0-based node id
        // double check if buffer is still valid
        if (m_TR == nullptr)
            return;
        uint16_t node_type = 0;
        // safety checks
        if (id >= cbMAXTRACKOBJ)
            return;
        if (cbGetTrackObj(nullptr, &node_type, nullptr, id + 1, m_nInstance) != cbRESULT_OK)
        {
            m_TR->node_type[id] = 0;
            m_TR->max_point_counts[id] = 0;
            return;
        }
        m_lockTrialTracking.lock();
        // New tracking data or tracking type changed
        if (node_type != m_TR->node_type[id])
        {
            cbGetTrackObj(reinterpret_cast<char *>(m_TR->node_name[id]), &m_TR->node_type[id], &m_TR->max_point_counts[id], id + 1, m_nInstance);
        }
        if (m_TR->node_type[id])
        {
            // Add a sample...
            // If there's room for more data...
            uint32_t new_write_index = m_TR->write_index[id] + 1;
            if (new_write_index >= m_TR->size)
                new_write_index = 0;

            if (new_write_index != m_TR->write_start_index[id])
            {
                const uint32_t write_index = m_TR->write_index[id];
                // Store more data
                m_TR->timestamps[id][write_index] = pPkt->cbpkt_header.time;
                m_TR->synch_timestamps[id][write_index] = m_lastPktVideoSynch.etime;
                m_TR->synch_frame_numbers[id][write_index] = m_lastPktVideoSynch.frame;
                m_TR->point_counts[id][write_index] = pPkt->pointCount;

                bool bWordData = false; // if data is of word-length
                int dim_count = 2; // number of dimensions for each point
                switch(m_TR->node_type[id])
                {
                case cbTRACKOBJ_TYPE_2DMARKERS:
                case cbTRACKOBJ_TYPE_2DBLOB:
                case cbTRACKOBJ_TYPE_2DBOUNDARY:
                    dim_count = 2;
                    break;
                case cbTRACKOBJ_TYPE_1DSIZE:
                    bWordData = true;
                    dim_count = 1;
                    break;
                default:
                    dim_count = 3;
                    break;
                }
                uint32_t pointCount = pPkt->pointCount * dim_count;
                if (bWordData)
                {
                    if (pointCount > cbMAX_TRACKCOORDS / 2)
                        pointCount = cbMAX_TRACKCOORDS / 2;
                    memcpy(m_TR->coords[id][write_index], pPkt->sizes, pointCount * sizeof(uint32_t));
                }
                else
                {
                    if (pointCount > cbMAX_TRACKCOORDS)
                        pointCount = cbMAX_TRACKCOORDS;
                    memcpy(m_TR->coords[id][write_index], pPkt->coords, pointCount * sizeof(uint16_t));
                }
                m_TR->write_index[id] = new_write_index;

                if (m_bPacketsTrack)
                {
                    m_lockGetPacketsTrack.lock();
                        if (pPkt->cbpkt_header.time > m_uTrialStartTime)
                        {
                            m_bPacketsTrack = false;
                            m_waitPacketsTrack.notify_all();
                        }
                    m_lockGetPacketsTrack.unlock();
                }
            }
        }
        m_lockTrialTracking.unlock();
    }
}

// Author & Date:   Ehsan Azar     16 May 2012
/** Late binding of callback function when needed.
*
* Make sure this is called before any callback invocation.
*
* @param[in] callbackType the callback type to bind
*/
void SdkApp::LateBindCallback(const cbSdkCallbackType callbackType)
{
    if (m_pCallback[callbackType] != m_pLateCallback[callbackType])
    {
        m_lockCallback.lock();
        m_pLateCallback[callbackType] = m_pCallback[callbackType];
        m_pLateCallbackParams[callbackType] = m_pCallbackParams[callbackType];
        m_lockCallback.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
// Author & Date:   Ehsan Azar     29 March 2011
/** Signal packet-lost event.
* Only the first registered callback receives packet lost events.
*/
void SdkApp::LinkFailureEvent(const cbSdkPktLostEvent & lost)
{
    m_lastLost = lost;
    cbSdkCallback pCallback = nullptr;
    void * pCallbackParams = nullptr;

    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        if (m_pCallback[i])
        {
            m_lockCallback.lock();
            pCallback = m_pCallback[i];
            pCallbackParams = m_pCallbackParams[i];
            m_lockCallback.unlock();
            break;
        }
    }

    if (pCallback)
        pCallback(m_nInstance, cbSdkPkt_PACKETLOST, &m_lastLost, pCallbackParams);
}

/////////////////////////////////////////////////////////////////////////////
// Author & Date:   Ehsan Azar     29 April 2012

/** Signal instrument information event.
*
*/
void SdkApp::InstInfoEvent(const uint32_t instInfo)
{
    m_lastInstInfo.instInfo = instInfo;
    // Late bind the ones needed before usage
    LateBindCallback(CBSDKCALLBACK_ALL);
    LateBindCallback(CBSDKCALLBACK_INSTINFO);
    if (m_pLateCallback[CBSDKCALLBACK_ALL])
        m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_INSTINFO, &m_lastInstInfo, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
    if (m_pLateCallback[CBSDKCALLBACK_INSTINFO])
        m_pLateCallback[CBSDKCALLBACK_INSTINFO](m_nInstance, cbSdkPkt_INSTINFO, &m_lastInstInfo, m_pLateCallbackParams[CBSDKCALLBACK_INSTINFO]);
}

/////////////////////////////////////////////////////////////////////////////
//        All the SDK functions must come after this comment
/////////////////////////////////////////////////////////////////////////////

// Author & Date:   Ehsan Azar     23 Feb 2011
/** Get cbsdk version information.
* See docstring for cbSdkGetVersion in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetVersion(cbSdkVersion *version) const {
    memset(version, 0, sizeof(cbSdkVersion));
    version->major = BMI_VERSION_MAJOR;
    version->minor = BMI_VERSION_MINOR;
    version->release = BMI_VERSION_RELEASE;
    version->beta = BMI_VERSION_BETA;
    version->majorp = cbVERSION_MAJOR;
    version->minorp = cbVERSION_MINOR;
    if (m_instInfo == 0)
        return CBSDKRESULT_WARNCLOSED;
    cbPROCINFO isInfo;
    cbRESULT cbRet = cbGetProcInfo(cbNSP1, &isInfo, m_nInstance);
    if (cbRet != cbRESULT_OK)
        return CBSDKRESULT_UNKNOWN;
    version->nspmajor   = (isInfo.idcode & 0x000000ff);
    version->nspminor   = (isInfo.idcode & 0x0000ff00) >> 8;
    version->nsprelease = (isInfo.idcode & 0x00ff0000) >> 16;
    version->nspbeta    = (isInfo.idcode & 0xff000000) >> 24;
    version->nspmajorp  = (isInfo.version & 0xffff0000) >> 16;
    version->nspminorp  = (isInfo.version & 0x0000ffff);
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetVersion(const uint32_t nInstance, cbSdkVersion *version)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (version == nullptr)
        return CBSDKRESULT_NULLPTR;
    memset(version, 0, sizeof(cbSdkVersion));
    version->major = BMI_VERSION_MAJOR;
    version->minor = BMI_VERSION_MINOR;
    version->release = BMI_VERSION_RELEASE;
    version->beta = BMI_VERSION_BETA;
    version->majorp = cbVERSION_MAJOR;
    version->minorp = cbVERSION_MINOR;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_WARNCLOSED;

    return g_app[nInstance]->SdkGetVersion(version);
}

// Author & Date:   Ehsan Azar     12 April 2012
cbSdkResult cbSdkErrorFromCCFError(const ccf::ccfResult err)
{
    cbSdkResult res = CBSDKRESULT_UNKNOWN;
    switch(err)
    {
    case ccf::CCFRESULT_WARN_VERSION:
    case ccf::CCFRESULT_WARN_CONVERT:
        res = CBSDKRESULT_WARNCONVERT;
        break;
    case ccf::CCFRESULT_ERR_FORMAT:
        res = CBSDKRESULT_ERRFORMATFILE;
        break;
    case ccf::CCFRESULT_ERR_OPENFAILEDWRITE:
    case ccf::CCFRESULT_ERR_OPENFAILEDREAD:
        res = CBSDKRESULT_ERROPENFILE;
        break;
    case ccf::CCFRESULT_ERR_OFFLINE:
        res = CBSDKRESULT_CLOSED;
        break;
    default:
        res = CBSDKRESULT_UNKNOWN;
        break;
    }
    return res;
}

// Author & Date:   Ehsan Azar     10 June 2012
/** CCF callback function.
* See docstring for cbSdkAsynchCCF in cbsdk.h for more information.
*/
void SdkApp::SdkAsynchCCF(const ccf::ccfResult res, const LPCSTR szFileName, const cbStateCCF state, const uint32_t nProgress)
{
    cbSdkCCFEvent ev;
    ev.result = cbSdkErrorFromCCFError(res);
    ev.progress = nProgress;
    ev.state = state;
    ev.szFileName = szFileName;

    LateBindCallback(CBSDKCALLBACK_ALL);
    LateBindCallback(CBSDKCALLBACK_CCF);
    if (m_pLateCallback[CBSDKCALLBACK_ALL])
        m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_CCF, &ev, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
    if (m_pLateCallback[CBSDKCALLBACK_CCF])
        m_pLateCallback[CBSDKCALLBACK_CCF](m_nInstance, cbSdkPkt_CCF, &ev, m_pLateCallbackParams[CBSDKCALLBACK_CCF]);;
}

void cbSdkAsynchCCF(const uint32_t nInstance, const ccf::ccfResult res, const LPCSTR szFileName, const cbStateCCF state, const uint32_t nProgress)
{
    if (nInstance >= cbMAXOPEN)
        return;
    if (g_app[nInstance] == nullptr)
        return;
    g_app[nInstance]->SdkAsynchCCF(res, szFileName, state, nProgress);
}

// Author & Date:   Ehsan Azar     12 April 2012
/** Get CCF configuration information from file or NSP.
* See docstring for cbSdkReadCCF in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkReadCCF(cbSdkCCF * pData, const char * szFileName, const bool bConvert, const bool bSend, const bool bThreaded)
{
    memset(&pData->data, 0, sizeof(pData->data));
    pData->ccfver = 0;
    if (szFileName == nullptr)
    {
        // Library must be open to read from NSP
        if (m_instInfo == 0)
            return CBSDKRESULT_CLOSED;
    }
    CCFUtils config(bThreaded, &pData->data, m_pCallback[CBSDKCALLBACK_CCF] ? &cbSdkAsynchCCF : nullptr, m_nInstance);
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    if (szFileName == nullptr)
        res = SdkFetchCCF(pData);
    else
    {
        const ccf::ccfResult ccfRes = config.ReadCCF(szFileName, bConvert);  // Updates pData->data via config.m_pImpl->m_data;
        if (bSend)
            SdkSendCCF(pData, config.IsAutosort());
        res = cbSdkErrorFromCCFError(ccfRes);
    }
    if (res)
        return res;
    pData->ccfver = config.GetInternalOriginalVersion();
    if (m_instInfo == 0)
        return CBSDKRESULT_WARNCLOSED;
    return CBSDKRESULT_SUCCESS;
}

cbSdkResult SdkApp::SdkFetchCCF(cbSdkCCF * pData) const {
    ccf::ccfResult res = ccf::CCFRESULT_SUCCESS;
    uint32_t nIdx = cb_library_index[m_nInstance];
    if (!cb_library_initialized[nIdx] || cb_cfg_buffer_ptr[nIdx] == nullptr || cb_cfg_buffer_ptr[nIdx]->sysinfo.cbpkt_header.chid == 0)
        res = ccf::CCFRESULT_ERR_OFFLINE;
    else {
        cbCCF & data = pData->data;
        for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
            data.filtinfo[i] = cb_cfg_buffer_ptr[nIdx]->filtinfo[0][cbFIRST_DIGITAL_FILTER + i - 1];    // First is 1 based, but index is 0 based
        for (int i = 0; i < cbMAXCHANS; ++i)
            data.isChan[i] = cb_cfg_buffer_ptr[nIdx]->chaninfo[i];
        data.isAdaptInfo = cb_cfg_buffer_ptr[nIdx]->adaptinfo[cbNSP1];
        data.isSS_Detect = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktDetect;
        data.isSS_ArtifactReject = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktArtifReject;
        for (uint32_t i = 0; i < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans(); ++i)
            data.isSS_NoiseBoundary[i] = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktNoiseBoundary[i];
        data.isSS_Statistics = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatistics;
        {
            data.isSS_Status = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatus;
            data.isSS_Status.cntlNumUnits.fElapsedMinutes = 99;
            data.isSS_Status.cntlUnitStats.fElapsedMinutes = 99;
        }
        {
            data.isSysInfo = cb_cfg_buffer_ptr[nIdx]->sysinfo;
            // only set spike len and pre trigger len
            data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
        }
        for (int i = 0; i < cbMAXNTRODES; ++i)
            data.isNTrodeInfo[i] = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[i];
        data.isLnc = cb_cfg_buffer_ptr[nIdx]->isLnc[cbNSP1];
        for (int i = 0; i < AOUT_NUM_GAIN_CHANS; ++i)
        {
            for (int j = 0; j < cbMAX_AOUT_TRIGGER; ++j)
            {
                data.isWaveform[i][j] = cb_cfg_buffer_ptr[nIdx]->isWaveform[i][j];
                // Unset triggered state, so that when loading it does not start generating waveform
                data.isWaveform[i][j].active = 0;
            }
        }
    }
    return cbSdkErrorFromCCFError(res);
}

CBSDKAPI cbSdkResult cbSdkReadCCF(const uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, const bool bConvert, const bool bSend, const bool bThreaded)
{
    if (pData == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    // TODO: make it possible to convert even with library closed
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkReadCCF(pData, szFileName, bConvert, bSend, bThreaded);
}

// Author & Date:   Ehsan Azar     12 April 2012
/** Write CCF configuration information to file or send to NSP.
* See docstring for cbSdkWriteCCF in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkWriteCCF(cbSdkCCF * pData, const char * szFileName, const bool bThreaded)
{
    pData->ccfver = 0;
    if (szFileName == nullptr)
    {
        // Library must be open to write to NSP
        if (m_instInfo == 0)
            return CBSDKRESULT_CLOSED;
    }
    cbCCFCallback callbackFn = m_pCallback[CBSDKCALLBACK_CCF] ? &cbSdkAsynchCCF : nullptr;
    CCFUtils config(bThreaded, &pData->data, callbackFn, m_nInstance);
    // Set proc info on config object. This might be used by Write* operations (if XML).
    cbPROCINFO isInfo;
    cbGetProcInfo(cbNSP1, &isInfo, m_nInstance);
    config.SetProcInfo(isInfo);  // Ignore return. It works if XML, and fails otherwise.

    ccf::ccfResult res = ccf::CCFRESULT_SUCCESS;
    if (szFileName == nullptr)
    {
        // No filename, attempt to send CCF to NSP
        if (callbackFn)
            callbackFn(m_nInstance, res, nullptr, CCFSTATE_SEND, 0);
        SdkSendCCF(pData, false);
        if (callbackFn)
            callbackFn(m_nInstance, res, nullptr, CCFSTATE_SEND, 100);
    }
    else
        res = config.WriteCCFNoPrompt(szFileName);
    if (res)
        return cbSdkErrorFromCCFError(res);
    pData->ccfver = config.GetInternalVersion();
    if (m_instInfo == 0)
        return CBSDKRESULT_WARNCLOSED;
    return CBSDKRESULT_SUCCESS;
}

cbSdkResult SdkApp::SdkSendCCF(cbSdkCCF * pData, bool bAutosort)
{
    if (pData == nullptr)
        return CBSDKRESULT_NULLPTR;

    cbCCF data = pData->data;

    // Custom digital filters
    for (auto & info : data.filtinfo)
    {
        if (info.filt)
        {
            info.cbpkt_header.type = cbPKTTYPE_FILTSET;
            cbSendPacket(&info, m_nInstance);
        }
    }
    // Chaninfo
    int nAinChan = 1;
    int nAoutChan = 1;
    int nDinChan = 1;
    int nSerialChan = 1;
    int nDoutChan = 1;
    uint32_t nChannelNumber = 0;
    for (auto & info : data.isChan)
    {
        if (info.chan)
        {
            // this function is supposed to line up channels based on channel capabilities.  It doesn't
            // work with the multiple NSP setup.  TODO look into this at a future time
            nChannelNumber = 0;
            switch (info.chancaps)
            {
                case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_ISOLATED | cbCHAN_AINP:  // FE channels
#ifdef CBPROTO_311
                    nChannelNumber = cbGetExpandedChannelNumber(1, data.isChan[info].chan);
#else
                    nChannelNumber = cbGetExpandedChannelNumber(info.cbpkt_header.instrument + 1, info.chan);
#endif
                    break;
                case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_AINP:  // Analog input channels
                    nChannelNumber = GetAIAnalogInChanNumber(nAinChan++);
                    break;
                case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_AOUT:  // Analog & Audio output channels
                    nChannelNumber = GetAnalogOrAudioOutChanNumber(nAoutChan++);
                    break;
                case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_DINP:  // digital & serial input channels
                    if (info.dinpcaps & cbDINP_SERIALMASK)
                    {
                        nChannelNumber = GetSerialChanNumber(nSerialChan++);
                    }
                    else
                    {
                        nChannelNumber = GetDiginChanNumber(nDinChan++);
                    }
                    break;
                case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_DOUT:  // digital output channels
                    nChannelNumber = GetDigoutChanNumber(nDoutChan++);
                    break;
                default:
                    nChannelNumber = 0;
            }
            // send it if it's a valid channel number
            if ((0 != nChannelNumber))  // && (data.isChan[info].chan))
            {
                info.chan = nChannelNumber;
                info.cbpkt_header.type = cbPKTTYPE_CHANSET;
#ifndef CBPROTO_311
                info.cbpkt_header.instrument = info.proc - 1;   // send to the correct instrument
#endif
                cbSendPacket(&info, m_nInstance);
            }
        }
    }
    // Sorting
    {
        if (data.isSS_Statistics.cbpkt_header.type)
        {
            data.isSS_Statistics.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSSET;
            cbSendPacket(&data.isSS_Statistics, m_nInstance);
        }
        for (uint32_t nChan = 0; nChan < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans(); ++nChan)
        {
            if (data.isSS_NoiseBoundary[nChan].chan)
            {
                data.isSS_NoiseBoundary[nChan].cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYSET;
                cbSendPacket(&data.isSS_NoiseBoundary[nChan], m_nInstance);
            }
        }
        if (data.isSS_Detect.cbpkt_header.type)
        {
            data.isSS_Detect.cbpkt_header.type  = cbPKTTYPE_SS_DETECTSET;
            cbSendPacket(&data.isSS_Detect, m_nInstance);
        }
        if (data.isSS_ArtifactReject.cbpkt_header.type)
        {
            data.isSS_ArtifactReject.cbpkt_header.type = cbPKTTYPE_SS_ARTIF_REJECTSET;
            cbSendPacket(&data.isSS_ArtifactReject, m_nInstance);
        }
    }
    // Sysinfo
    if (data.isSysInfo.cbpkt_header.type)
    {
        data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
        cbSendPacket(&data.isSysInfo, m_nInstance);
    }
    // LNC
    if (data.isLnc.cbpkt_header.type)
    {
        data.isLnc.cbpkt_header.type = cbPKTTYPE_LNCSET;
        cbSendPacket(&data.isLnc, m_nInstance);
    }
    // Analog output waveforms
    for (uint32_t nChan = 0; nChan < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogoutChans(); ++nChan)
    {
        for (int nTrigger = 0; nTrigger < cbMAX_AOUT_TRIGGER; ++nTrigger)
        {
            if (data.isWaveform[nChan][nTrigger].chan)
            {
                data.isWaveform[nChan][nTrigger].chan = GetAnalogOutChanNumber(nChan + 1);
#ifndef CBPROTO_311
                data.isWaveform[nChan][nTrigger].cbpkt_header.instrument = cbGetChanInstrument(data.isWaveform[nChan][nTrigger].chan);
#endif
                data.isWaveform[nChan][nTrigger].cbpkt_header.type = cbPKTTYPE_WAVEFORMSET;
                cbSendPacket(&data.isWaveform[nChan][nTrigger], m_nInstance);
            }
        }
    }
    // NTrode
    for (int nNTrode = 0; nNTrode < cbMAXNTRODES; ++nNTrode)
    {
        char szNTrodeLabel[cbLEN_STR_LABEL + 1] = {};     // leave space for trailing null
        cbGetNTrodeInfo(nNTrode + 1, szNTrodeLabel, nullptr, nullptr, nullptr, nullptr);

        if (
                (0 != strlen(szNTrodeLabel))
#ifndef CBPROTO_311
                && (cbGetNTrodeInstrument(nNTrode + 1) == data.isNTrodeInfo->cbpkt_header.instrument + 1)
#endif
                )
        {
            if (data.isNTrodeInfo[nNTrode].ntrode)
            {
                data.isNTrodeInfo[nNTrode].cbpkt_header.type = cbPKTTYPE_SETNTRODEINFO;
                cbSendPacket(&data.isNTrodeInfo[nNTrode], m_nInstance);
            }
        }
    }
    // Adaptive filter
    if (data.isAdaptInfo.cbpkt_header.type)
    {
        data.isAdaptInfo.cbpkt_header.type = cbPKTTYPE_ADAPTFILTSET;
        cbSendPacket(&data.isAdaptInfo, m_nInstance);
    }
    // if any spike sorting packets were read and the protocol is before the combined firmware,
    // set all the channels to autosorting
    if (CCFUtils config(false, &pData->data, nullptr, m_nInstance); (config.GetInternalVersion() < 8) && !bAutosort)
    {
        cbPKT_SS_STATISTICS isSSStatistics;

        isSSStatistics.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
        isSSStatistics.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSSET;
        isSSStatistics.cbpkt_header.dlen = ((sizeof(*this) / 4) - cbPKT_HEADER_32SIZE);

        isSSStatistics.nUpdateSpikes = 300;
        isSSStatistics.nAutoalg = cbAUTOALG_HOOPS;
        isSSStatistics.nMode = cbAUTOALG_MODE_APPLY;
        isSSStatistics.fMinClusterPairSpreadFactor = 9;
        isSSStatistics.fMaxSubclusterSpreadFactor = 125;
        isSSStatistics.fMinClusterHistCorrMajMeasure = 0.80f;
        isSSStatistics.fMaxClusterPairHistCorrMajMeasure = 0.94f;
        isSSStatistics.fClusterHistValleyPercentage = 0.50f;
        isSSStatistics.fClusterHistClosePeakPercentage = 0.50f;
        isSSStatistics.fClusterHistMinPeakPercentage = 0.016f;
        isSSStatistics.nWaveBasisSize = 250;
        isSSStatistics.nWaveSampleSize = 0;

        cbSendPacket(&isSSStatistics, m_nInstance);

    }
    if (data.isSS_Status.cbpkt_header.type)
    {
        data.isSS_Status.cbpkt_header.type = cbPKTTYPE_SS_STATUSSET;
        data.isSS_Status.cntlNumUnits.nMode = ADAPT_NEVER; // Prevent rebuilding spike sorting when loading ccf.
        cbSendPacket(&data.isSS_Status, m_nInstance);
    }

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkWriteCCF(const uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, const bool bThreaded)
{
    if (pData == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    /// \todo make it possible to convert even with library closed
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkWriteCCF(pData, szFileName, bThreaded);
}

// Author & Date:   Ehsan Azar     23 Feb 2011
/** Open cbsdk library.
* See docstring for cbSdkOpen in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkOpen(const uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con)
{
    // check if the library is already open
    if (m_instInfo != 0)
        return CBSDKRESULT_WARNOPEN;

    // Some sanity checks
    if (con.szInIP == nullptr || con.szInIP[0] == 0)
    {
#ifdef WIN32
        con.szInIP = cbNET_UDP_ADDR_INST;
#elif __APPLE__
        con.szInIP = "255.255.255.255";
#else
        // On Linux bind to bcast
        con.szInIP = cbNET_UDP_ADDR_BCAST;
#endif
    }
    if (con.szOutIP == nullptr || con.szOutIP[0] == 0)
        con.szOutIP = cbNET_UDP_ADDR_CNT;

    // Null the trial buffers
    m_CD = nullptr;
    m_ED = nullptr;

    // Unregister all callbacks
    m_lockCallback.lock();
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallbackParams[i] = nullptr;
        m_pCallback[i] = nullptr;
    }
    m_lockCallback.unlock();

    // Unmask all channels
    for (bool & mask : m_bChannelMask)
        mask = true;

    // open the library and verify that the central application is there
    if (conType == CBSDKCONNECTION_DEFAULT || conType  == CBSDKCONNECTION_CENTRAL)
    {
        // Run cbsdk under Central
        char buf[64] = {0};
        if (nInstance == 0)
            _snprintf(buf, sizeof(buf), "cbSharedDataMutex");
        else
            _snprintf(buf, sizeof(buf), "cbSharedDataMutex%d", nInstance);
        if (cbCheckApp(buf) == cbRESULT_OK)
        {
            conType = CBSDKCONNECTION_CENTRAL;
        }
        else
        {
            if (conType == CBSDKCONNECTION_CENTRAL)
                return CBSDKRESULT_ERROPENCENTRAL;
            conType = CBSDKCONNECTION_UDP; // Now try UDP
        }
    }

    // reset synchronization packet
    memset(&m_lastPktVideoSynch, 0, sizeof(m_lastPktVideoSynch));

    // reset the trial begin/end notification variables.
    m_uTrialBeginChannel = 0;
    m_uTrialBeginMask    = 0;
    m_uTrialBeginValue   = 0;
    m_uTrialEndChannel   = 0;
    m_uTrialEndMask      = 0;
    m_uTrialEndValue     = 0;
    m_uTrialWaveforms    = 0;
    m_uTrialConts        = cbSdk_CONTINUOUS_DATA_SAMPLES;
    m_uTrialEvents       = cbSdk_EVENT_DATA_SAMPLES;
    m_uTrialComments     = 0;
    m_uTrialTrackings    = 0;

    // make sure that cache data storage is switched off so that the monitoring thread will
    // not be saving data and then set up the cache control variables
    m_bWithinTrial = false;
    m_uTrialStartTime = 0;

    std::unique_lock<std::mutex> connectLock(m_connectLock);

    if (conType == CBSDKCONNECTION_UDP)
    {
        Open(nInstance, con.nInPort, con.nOutPort, con.szInIP, con.szOutIP, con.nRecBufSize, con.nRange);
    }
    else if (conType == CBSDKCONNECTION_CENTRAL)
    {
        Open(nInstance);
    }
    else
        return CBSDKRESULT_NOTIMPLEMENTED;

    // Wait for (dis)connection to happen (wait forever if debug)
    bool bWait;
#ifndef NDEBUG
    m_connectWait.wait(connectLock);
    bWait = true;
#else
    bWait = (m_connectWait.wait_for(connectLock, std::chrono::milliseconds(15000)) == std::cv_status::no_timeout);
#endif

    if (!bWait)
        return CBSDKRESULT_TIMEOUT;
    if (IsStandAlone())
    {
        if (m_instInfo == 0)
        {
            cbSdkResult sdkres = CBSDKRESULT_UNKNOWN;
            // Try to make sense of the error
            switch (GetLastCbErr())
            {
            case cbRESULT_NOCENTRALAPP:
                sdkres = CBSDKRESULT_ERROPENCENTRAL;
                break;
            case cbRESULT_SOCKERR:
                sdkres = CBSDKRESULT_ERROPENUDPPORT;
                break;
            case cbRESULT_SOCKOPTERR:
                sdkres = CBSDKRESULT_OPTERRUDP;
                break;
            case cbRESULT_SOCKMEMERR:
                sdkres = CBSDKRESULT_MEMERRUDP;
                break;
            case cbRESULT_INSTINVALID:
                sdkres = CBSDKRESULT_INVALIDINST;
                break;
            case cbRESULT_SOCKBIND:
                sdkres = CBSDKRESULT_ERROPENUDP;
                break;
            case cbRESULT_LIBINITERROR:
            case cbRESULT_EVSIGERR:
                sdkres = CBSDKRESULT_ERRINIT;
                break;
            case cbRESULT_SYSLOCK:
                sdkres = CBSDKRESULT_BUSY;
                break;
            case cbRESULT_BUFRECALLOCERR:
            case cbRESULT_BUFGXMTALLOCERR:
            case cbRESULT_BUFLXMTALLOCERR:
            case cbRESULT_BUFCFGALLOCERR:
            case cbRESULT_BUFPCSTATALLOCERR:
            case cbRESULT_BUFSPKALLOCERR:
                sdkres = CBSDKRESULT_ERRMEMORY;
                break;
            case cbRESULT_INSTOUTDATED:
                sdkres = CBSDKRESULT_INSTOUTDATED;
                break;
            case cbRESULT_LIBOUTDATED:
                sdkres = CBSDKRESULT_LIBOUTDATED;
                break;
            case cbRESULT_OK:
                sdkres = CBSDKRESULT_ERROFFLINE;
                break;
            default: sdkres = CBSDKRESULT_UNKNOWN;
            }
            return sdkres;
        }
    }
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkOpen(const uint32_t nInstance, const cbSdkConnectionType conType, cbSdkConnection con)
{
    // check if the library is already open
    if (conType < 0 || conType >= CBSDKCONNECTION_CLOSED)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
    {
        try {
            g_app[nInstance] = new SdkApp();
        } catch (...) {
            g_app[nInstance] = nullptr;
        }
    }
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_ERRMEMORY;

    return g_app[nInstance]->SdkOpen(nInstance, conType, con);
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Close cbsdk library.
* See docstring for cbSdkClose in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkClose()
{
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    if (m_instInfo == 0)
        res = CBSDKRESULT_WARNCLOSED;

    // clear the data cache if it exists
    // first disable writing to the cache
    m_bWithinTrial = false;

    if (m_CD != nullptr)
        SdkUnsetTrialConfig(CBSDKTRIAL_CONTINUOUS);

    if (m_ED != nullptr)
        SdkUnsetTrialConfig(CBSDKTRIAL_EVENTS);

    if (m_CMT != nullptr)
        SdkUnsetTrialConfig(CBSDKTRIAL_COMMENTS);

    if (m_TR != nullptr)
        SdkUnsetTrialConfig(CBSDKTRIAL_TRACKING);

    // Unregister all callbacks
    m_lockCallback.lock();
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallbackParams[i] = nullptr;
        m_pCallback[i] = nullptr;
    }
    m_lockCallback.unlock();

    // Close the app
    Close();

    return res;
}

CBSDKAPI    cbSdkResult cbSdkClose(const uint32_t nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_WARNCLOSED;

    const cbSdkResult res = g_app[nInstance]->SdkClose();

    // Delete this instance
    delete g_app[nInstance];
    g_app[nInstance] = nullptr;

    /// \todo see if this is necessary and useful before removing
#if 0
    // Close application if this is the last
    if (QAppPriv::pApp)
    {
        bool bLastInstance = false;
        for (int i = 0; i < cbMAXOPEN; ++i)
        {
            if (g_app[i] != nullptr)
            {
                bLastInstance = true;
                break;
            }
        }
        if (bLastInstance)
        {
            delete QAppPriv::pApp;
            QAppPriv::pApp = nullptr;
        }
    }
#endif

    return res;
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Get cbsdk connection and instrument type.
* See docstring for cbSdkGetType in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetType(cbSdkConnectionType * conType, cbSdkInstrumentType * instType) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    if (instType)
    {
        uint32_t instInfo = 0;
        if (cbGetInstInfo(&instInfo, m_nInstance) == cbRESULT_NOLIBRARY)
            return CBSDKRESULT_CLOSED;
        if (instInfo & cbINSTINFO_NPLAY)
        {
            if (instInfo & cbINSTINFO_LOCAL)
                *instType = CBSDKINSTRUMENT_NPLAY;
            else
                *instType = CBSDKINSTRUMENT_REMOTENPLAY;
        }
        else
        {
            if (instInfo & cbINSTINFO_LOCAL)
                *instType = CBSDKINSTRUMENT_LOCALNSP;
            else
                *instType = CBSDKINSTRUMENT_NSP;
        }
    }

    if (conType)
    {
        *conType = CBSDKCONNECTION_CLOSED;
        if (IsStandAlone())
            *conType = CBSDKCONNECTION_UDP;
        else if (m_instInfo != 0)
            *conType = CBSDKCONNECTION_CENTRAL;
    }

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetType(const uint32_t nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
{
    if (instType == nullptr && conType == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetType(conType, instType);
}

// Author & Date:   Ehsan Azar     25 Oct 2011
/** Internal lock-less function to deallocate given trial construct.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::unsetTrialConfig(const cbSdkTrialType type)
{
    switch (type)
    {
    case CBSDKTRIAL_CONTINUOUS:
        if (m_CD == nullptr)
            return CBSDKRESULT_ERRCONFIG;
        // Cleanup all groups
        m_CD->cleanup();
        // Delete the structure itself
        delete m_CD;
        m_CD = nullptr;
        m_uTrialConts = 0;
        break;
    case CBSDKTRIAL_EVENTS:
        if (m_ED == nullptr)
            return CBSDKRESULT_ERRCONFIG;
        // TODO: Go back to using cbNUM_ANALOG_CHANS + 2 after we have m_ChIdxInType
        for (uint32_t i = 0; i < cbMAXCHANS; ++i)
        {
            if (m_ED->timestamps[i])
            {
                delete[] m_ED->timestamps[i];
                m_ED->timestamps[i] = nullptr;
            }
            if (m_ED->units[i])
            {
                delete[] m_ED->units[i];
                m_ED->units[i] = nullptr;
            }
        }
        if (m_ED->waveform_data != nullptr)
        {
            if (m_uTrialWaveforms > cbPKT_SPKCACHEPKTCNT)
                delete[] m_ED->waveform_data;
            m_ED->waveform_data = nullptr;
        }
        m_ED->size = 0;
        delete m_ED;
        m_ED = nullptr;
        break;
    case CBSDKTRIAL_COMMENTS:
        if (m_CMT == nullptr)
            return CBSDKRESULT_ERRCONFIG;
        if (m_CMT->timestamps)
        {
            delete[] m_CMT->timestamps;
            m_CMT->timestamps = nullptr;
        }
        if (m_CMT->rgba)
        {
            delete[] m_CMT->rgba;
            m_CMT->rgba = nullptr;
        }
        if (m_CMT->charset)
        {
            delete[] m_CMT->charset;
            m_CMT->charset = nullptr;
        }
        if (m_CMT->comments)
        {
            for (uint32_t i = 0; i < m_CMT->size; ++i)
            {
                if (m_CMT->comments[i])
                {
                    delete[] m_CMT->comments[i];
                    m_CMT->comments[i] = nullptr;
                }
            }
            delete[] m_CMT->comments;
            m_CMT->comments = nullptr;
        }
        m_CMT->size = 0;
        delete m_CMT;
        m_CMT = nullptr;
        break;
    case CBSDKTRIAL_TRACKING:
        if (m_TR == nullptr)
            return CBSDKRESULT_ERRCONFIG;
        for (uint32_t i = 0; i < cbMAXTRACKOBJ; ++i)
        {
            if (m_TR->timestamps[i])
            {
                delete[] m_TR->timestamps[i];
                m_TR->timestamps[i] = nullptr;
            }
            if (m_TR->synch_timestamps[i])
            {
                delete[] m_TR->synch_timestamps[i];
                m_TR->synch_timestamps[i] = nullptr;
            }
            if (m_TR->synch_frame_numbers[i])
            {
                delete[] m_TR->synch_frame_numbers[i];
                m_TR->synch_frame_numbers[i] = nullptr;
            }
            if (m_TR->point_counts[i])
            {
                delete[] m_TR->point_counts[i];
                m_TR->point_counts[i] = nullptr;
            }
            if (m_TR->coords[i])
            {
                for (uint32_t j = 0; j < m_TR->size; ++j)
                {
                    if (m_TR->coords[i][j])
                    {
                        delete[] static_cast<uint16_t *>(m_TR->coords[i][j]);
                        m_TR->coords[i][j] = nullptr;
                    }
                }
                delete[] m_TR->coords[i];
                m_TR->coords[i] = nullptr;
            }
        } // end for (uint32_t i = 0
        m_TR->size = 0;
        delete m_TR;
        m_TR = nullptr;
        break;
    }
    return CBSDKRESULT_SUCCESS;
}

// Author & Date:   Ehsan Azar     25 Oct 2011
/** Deallocate given trial construct.
* See docstring for cbSdkUnsetTrialConfig in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkUnsetTrialConfig(const cbSdkTrialType type)
{
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    switch (type)
    {
    case CBSDKTRIAL_CONTINUOUS:
        m_lockTrial.lock();
        res = unsetTrialConfig(type);
        m_lockTrial.unlock();
        break;
    case CBSDKTRIAL_EVENTS:
        m_lockTrialEvent.lock();
        res = unsetTrialConfig(type);
        m_lockTrialEvent.unlock();
        break;
    case CBSDKTRIAL_COMMENTS:
        m_lockTrialComment.lock();
        res = unsetTrialConfig(type);
        m_lockTrialComment.unlock();
        break;
    case CBSDKTRIAL_TRACKING:
        m_lockTrialTracking.lock();
        res = unsetTrialConfig(type);
        m_lockTrialTracking.unlock();
        break;
    }
    return res;
}

CBSDKAPI    cbSdkResult cbSdkUnsetTrialConfig(const uint32_t nInstance, const cbSdkTrialType type)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkUnsetTrialConfig(type);
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Get time from instrument.
* See docstring for cbSdkGetTime in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetTime(PROCTIME * cbtime) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    *cbtime = m_uCbsdkTime;
    if (cbGetSystemClockTime(cbtime, m_nInstance) != cbRESULT_OK)
        return CBSDKRESULT_CLOSED;

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetTime(const uint32_t nInstance, PROCTIME * cbtime)
{
    if (cbtime == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTime(cbtime);
}

// Author & Date:   Ehsan Azar     29 April 2012
/** Get direct access to spike cache shared memory.
* See docstring for cbSdkGetSpkCache in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetSpkCache(const uint16_t channel, cbSPKCACHE **cache) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (cbGetSpkCache(channel, cache, m_nInstance) != cbRESULT_OK)
        return CBSDKRESULT_CLOSED;

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetSpkCache(const uint32_t nInstance, const uint16_t channel, cbSPKCACHE **cache)
{
    if (cache == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (*cache == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSpkCache(channel, cache);
}

// Author & Date:   Ehsan Azar     25 June 2012
/** Get information about configured data collection trial and its active status.
* See docstring for cbSdkGetTrialConfig in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetTrialConfig(uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                      uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval,
                                      uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                      uint32_t * puComments, uint32_t * puTrackings) const
{
    if (pbActive)
        *pbActive = m_bWithinTrial;
    if (pBegchan)
        *pBegchan = m_uTrialBeginChannel;
    if (pBegmask)
        *pBegmask = m_uTrialBeginMask;
    if (pBegval)
        *pBegval = m_uTrialBeginValue;
    if (pEndchan)
        *pEndchan = m_uTrialEndChannel;
    if (pEndmask)
        *pEndmask = m_uTrialEndMask;
    if (pEndval)
        *pEndval = m_uTrialEndValue;
    if (puWaveforms)
        *puWaveforms = m_uTrialWaveforms;
    if (puConts)
        *puConts = m_uTrialConts;
    if (puEvents)
        *puEvents = m_uTrialEvents;
    if (puComments)
        *puComments = m_uTrialComments;
    if (puTrackings)
        *puTrackings = m_uTrialTrackings;
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetTrialConfig(const uint32_t nInstance,
                                            uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                            uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval,
                                            uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                            uint32_t * puComments, uint32_t * puTrackings)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTrialConfig(pbActive, pBegchan, pBegmask, pBegval,
                                               pEndchan, pEndmask, pEndval,
                                               puWaveforms, puConts, puEvents,
                                               puComments, puTrackings);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Allocate buffers and start a data collection trial.
* See docstring for cbSdkSetTrialConfig in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkSetTrialConfig(const uint32_t bActive, const uint16_t begchan, const uint32_t begmask, const uint32_t begval,
                                      const uint16_t endchan, const uint32_t endmask, const uint32_t endval,
                                      const uint32_t uWaveforms, const uint32_t uConts, const uint32_t uEvents, const uint32_t uComments, const uint32_t uTrackings)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // load the values into the global trial control variables for use by the
    // real-time data monitoring thread. Trim them to 8-bit and 16-bit values
    m_uTrialBeginChannel = begchan;
    m_uTrialBeginMask    = begmask;
    m_uTrialBeginValue   = begval;
    m_uTrialEndChannel   = endchan;
    m_uTrialEndMask      = endmask;
    m_uTrialEndValue     = endval;
    m_uTrialWaveforms    = uWaveforms;

    if (uConts && m_CD == nullptr)
    {
        m_lockTrial.lock();
        try {
            m_CD = new ContinuousData;
            m_CD->default_size = uConts;
            // Groups are initialized by their constructors
            // Buffers are allocated lazily in OnPktGroup when first packet arrives
        } catch (...) {
            if (m_CD)
            {
                delete m_CD;
                m_CD = nullptr;
            }
        }
        m_lockTrial.unlock();

        if (m_CD == nullptr)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialConts = uConts;
    }

    if (uEvents && m_ED == nullptr)
    {
        m_lockTrialEvent.lock();
        try {
            m_ED = new EventData;
        } catch (...) {
            m_ED = nullptr;
        }
        if (m_ED)
        {
            memset(m_ED->timestamps, 0, sizeof(m_ED->timestamps));
            memset(m_ED->units, 0, sizeof(m_ED->units));
            m_ED->size = uEvents;
            bool bErr = false;
            try {
                // TODO: Go back to using cbNUM_ANALOG_CHANS + 2 once we have a ChIdxInType map
                // for indexing m_ED while processing packets.
                for (uint32_t i = 0; i < cbMAXCHANS; ++i)
                {
                    m_ED->timestamps[i] = new PROCTIME[m_ED->size];
                    m_ED->units[i] = new uint16_t[m_ED->size];
                    if (m_ED->timestamps[i] == nullptr || m_ED->units[i] == nullptr)
                    {
                        bErr = true;
                        break;
                    }
                }
            } catch (...) {
                bErr = true;
            }
            if (bErr)
                unsetTrialConfig(CBSDKTRIAL_EVENTS);
        }
        if (m_ED) m_ED->reset();
        m_lockTrialEvent.unlock();
        if (m_ED == nullptr)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialEvents = uEvents;
    }

    if (uComments && m_CMT == nullptr)
    {
        m_lockTrialComment.lock();
        try {
            m_CMT = new CommentData;
        } catch (...) {
            m_CMT = nullptr;
        }
        if (m_CMT)
        {
            m_CMT->size = uComments;
            bool bErr = false;
            m_CMT->timestamps = new PROCTIME[m_CMT->size];
            m_CMT->rgba = new uint32_t[m_CMT->size];
            m_CMT->charset = new uint8_t[m_CMT->size];
            m_CMT->comments = new uint8_t * [m_CMT->size]; // uint8_t * array[m_CMT->size]
            if (m_CMT->timestamps == nullptr || m_CMT->rgba == nullptr || m_CMT->charset == nullptr || m_CMT->comments == nullptr)
                bErr = true;
            try {
                if (!bErr)
                {
                    for (uint32_t i = 0; i < m_CMT->size; ++i)
                    {
                        m_CMT->comments[i] = new uint8_t[cbMAX_COMMENT + 1];
                        if (m_CMT->comments[i] == nullptr)
                        {
                            bErr = true;
                            break;
                        }
                    }
                }
            } catch (...) {
                bErr = true;
            }
            if (bErr)
                unsetTrialConfig(CBSDKTRIAL_COMMENTS);
        }
        if (m_CMT) m_CMT->reset();
        m_lockTrialComment.unlock();
        if (m_CMT == nullptr)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialComments     = uComments;
    }

    if (uTrackings && m_TR == nullptr)
    {
        m_lockTrialTracking.lock();
        try {
            m_TR = new TrackingData;
        } catch (...) {
            m_TR = nullptr;
        }
        if (m_TR)
        {
            m_TR->size = uTrackings;
            bool bErr = false;
            for (uint32_t i = 0; i < cbMAXTRACKOBJ; ++i)
            {
                try {
                    m_TR->timestamps[i] = new PROCTIME[m_TR->size];
                    m_TR->synch_timestamps[i] = new uint32_t[m_TR->size];
                    m_TR->synch_frame_numbers[i] = new uint32_t[m_TR->size];
                    m_TR->point_counts[i] = new uint16_t[m_TR->size];
                    m_TR->coords[i] = new void * [m_TR->size];

                    if (m_TR->timestamps[i] == nullptr || m_TR->synch_timestamps[i] == nullptr || m_TR->synch_frame_numbers[i] == nullptr ||
                            m_TR->point_counts[i]== nullptr || m_TR->coords[i] == nullptr)
                        bErr = true;

                    if (!bErr)
                    {
                        for (uint32_t j = 0; j < m_TR->size; ++j)
                        {
                            // This is equivalent to uint32_t[cbMAX_TRACKCOORDS/2] used for word-size union
                            m_TR->coords[i][j] = new uint16_t[cbMAX_TRACKCOORDS];
                            if (m_TR->coords[i][j] == nullptr)
                            {
                                bErr = true;
                                break;
                            }
                        }
                    }
                } catch (...) {
                    bErr = true;
                }
                if (bErr)
                {
                    unsetTrialConfig(CBSDKTRIAL_TRACKING);
                    break;
                }
            }
        }
        if (m_TR) m_TR->reset();
        m_lockTrialTracking.unlock();
        if (m_TR == nullptr)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialTrackings    = uTrackings;
    }

    if (m_ED)
    {
        if (uWaveforms && m_ED->waveform_data == nullptr)
        {
            if (uWaveforms > m_ED->size)
                return CBSDKRESULT_ERRMEMORYTRIAL;
            /// \todo implement using cache
            return CBSDKRESULT_NOTIMPLEMENTED;
        }
    }
    else if (uWaveforms > cbPKT_SPKCACHEPKTCNT)
        return CBSDKRESULT_INVALIDPARAM; // Cannot cache waveforms if no cache is available

    // get the trial status, if zero, set the WithinTrial flag to off
    if (!bActive)
    {
        m_bWithinTrial = false;
    }
    else
    { // otherwise set the status to true
        // reset the trial data cache if WithinTrial is currently False
        if (!m_bWithinTrial)
        {
            cbGetSystemClockTime(&m_uTrialStartTime, m_nInstance);

            if (m_CD)
            {
                // Clear continuous data indices for all groups
                m_lockTrial.lock();
                for (auto& grp : m_CD->groups)
                {
                    grp.setWriteIndex(0);
                    grp.setWriteStartIndex(0);
                }
                m_lockTrial.unlock();
            }

            if (m_ED)
            {
                // Clear event data array
                m_lockTrialEvent.lock();
                memset(m_ED->write_index, 0, sizeof(m_ED->write_index));
                memset(m_ED->write_start_index, 0, sizeof(m_ED->write_start_index));
                m_lockTrialEvent.unlock();
            }

            if (m_CMT)
            {
                // Clear comment data array
                m_lockTrialComment.lock();
                m_CMT->write_index = 0;
                m_CMT->write_start_index = 0;
                m_lockTrialComment.unlock();
            }

            if (m_TR)
            {
                // Clear tracking data array
                m_lockTrialTracking.lock();
                memset(m_TR->write_index, 0, sizeof(m_TR->write_index));
                memset(m_TR->write_start_index, 0, sizeof(m_TR->write_start_index));
                m_lockTrialTracking.unlock();
            }
        }
        m_bWithinTrial = true;
    }

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkSetTrialConfig(const uint32_t nInstance,
                                            const uint32_t bActive, const uint16_t begchan, const uint32_t begmask, const uint32_t begval,
                                            const uint16_t endchan, const uint32_t endmask, const uint32_t endval,
                                            const uint32_t uWaveforms, const uint32_t uConts, const uint32_t uEvents, const uint32_t uComments, const uint32_t uTrackings)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetTrialConfig(bActive, begchan, begmask, begval,
                                               endchan, endmask, endval,
                                               uWaveforms, uConts, uEvents, uComments, uTrackings);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Get channel label for a given channel
* See docstring for cbSdkGetChannelLabel in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetChannelLabel(const uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    if (label) memset(label, 0, cbLEN_STR_LABEL);
    if (cbGetChanLabel(channel, label, userflags, position, m_nInstance))
        return CBSDKRESULT_UNKNOWN;
    if (bValid)
    {
        for (int i = 0; i < 6; ++i)
            bValid[i] = 0;
        cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS];
        if (channel <= cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans())
        {
            cbGetAinpSpikeHoops(channel, &hoops[0][0], m_nInstance);
            bValid[0] = IsSpikeProcessingEnabled(channel);
            for (int i = 0; i < cbMAXUNITS; ++i)
                bValid[i + 1] = hoops[i][0].valid;
        }
        else if (IsChanDigin(channel) || IsChanSerial(channel))
        {
            uint32_t options;
            cbGetDinpOptions(channel, &options, nullptr, m_nInstance);
            bValid[0] = (options != 0);
        }
    }
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetChannelLabel(const uint32_t nInstance,
                                             const uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (label == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetChannelLabel(channel, bValid, label, userflags, position);
}

// Author & Date:   Ehsan Azar     21 March 2011
/** Set channel label for a given channel.
* See docstring for cbSdkSetChannelLabel in cbsdk.h for more information.
*/

cbSdkResult SdkApp::SdkSetChannelLabel(const uint16_t channel, const char * label, const uint32_t userflags, int32_t * position) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    if (cbSetChanLabel(channel, label, userflags, position, m_nInstance))
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkSetChannelLabel(const uint32_t nInstance,
                                             const uint16_t channel, const char * label, const uint32_t userflags, int32_t * position)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (label == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetChannelLabel(channel, label, userflags, position);
}

CBSDKAPI    cbSdkResult cbSdkIsChanAnalogIn(const uint32_t nInstance, const uint16_t channel, uint32_t* bResult)
{
    *bResult = IsChanAnalogIn(channel, nInstance);
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkIsChanAnyDigIn(const uint32_t nInstance, const uint16_t channel, uint32_t* bResult)
{
    *bResult = IsChanAnyDigIn(channel, nInstance);
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkIsChanSerial(const uint32_t nInstance, const uint16_t channel, uint32_t* bResult)
{
    *bResult = IsChanSerial(channel, nInstance);
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkIsChanCont(const uint32_t nInstance, const uint16_t channel, uint32_t* bResult)
{
    *bResult = IsChanCont(channel, nInstance);
    return CBSDKRESULT_SUCCESS;
}

/*
bool IsChanFEAnalogIn(uint32_t dwChan, uint32_t nInstance = 0);           // TRUE means yes; FALSE, no
bool IsChanAIAnalogIn(uint32_t dwChan, uint32_t nInstance = 0);           // TRUE means yes; FALSE, no
bool IsChanSerial(uint32_t dwChan, uint32_t nInstance = 0); // TRUE means yes; FALSE, no
bool IsChanDigin(uint32_t dwChan, uint32_t nInstance = 0);  // TRUE means yes; FALSE, no
bool IsChanDigout(uint32_t dwChan, uint32_t nInstance = 0);               // TRUE means yes; FALSE, no
bool IsChanAnalogOut(uint32_t dwChan, uint32_t nInstance = 0);            // TRUE means yes; FALSE, no
bool IsChanAudioOut(uint32_t dwChan, uint32_t nInstance = 0);             // TRUE means yes; FALSE, no
*/


// Author & Date:   Ehsan Azar     11 March 2011
/** Retrieve data of a configured trial.
See cbSdkGetTrialData docstring in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkGetTrialData(const uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                      cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const PROCTIME prevStartTime = m_uPrevTrialStartTime;

    if (trialcont)
    {
        if (m_CD == nullptr)
            return CBSDKRESULT_ERRCONFIG;

        // Set trial start time
        trialcont->trial_start_time = m_uTrialStartTime;

        // Validate group index
        const uint32_t g = trialcont->group;
        if (g >= cbMAXGROUPS)
            return CBSDKRESULT_INVALIDPARAM;

        const auto& grp = m_CD->groups[g];
        if (!grp.isAllocated())
        {
            // Group not allocated - return success with num_samples=0
            trialcont->num_samples = 0;
            return CBSDKRESULT_SUCCESS;
        }

        // Take a thread-safe snapshot of write indices for this group
        m_lockTrial.lock();
        const uint32_t read_end_index = grp.getWriteIndex();
        const uint32_t read_start_index = grp.getWriteStartIndex();
        m_lockTrial.unlock();

        // Calculate available samples
        auto num_avail = static_cast<int32_t>(read_end_index - read_start_index);
        if (num_avail < 0)
            num_avail += static_cast<int32_t>(grp.getSize());  // Wrapped around

        // Don't read more than requested
        const uint32_t num_samples = std::min(static_cast<uint32_t>(num_avail), trialcont->num_samples);
        trialcont->num_samples = num_samples;

        // Copy data if buffers provided and there are samples
        if (num_samples > 0 && trialcont->samples && trialcont->timestamps)
        {
            const int16_t* channel_data = grp.getChannelData();
            const PROCTIME* timestamps = grp.getTimestamps();
            const uint32_t num_channels = grp.getNumChannels();
            const uint32_t buffer_size = grp.getSize();

            auto* output_samples = static_cast<int16_t*>(trialcont->samples);

            // Check if we wrap around the ring buffer
            if (const bool wraps = (read_start_index + num_samples) > buffer_size; !wraps)
            {
                // No wraparound - copy everything in bulk
                // Copy timestamps
                memcpy(trialcont->timestamps,
                       &timestamps[read_start_index],
                       num_samples * sizeof(PROCTIME));

                // Copy sample data (entire contiguous block)
                memcpy(output_samples,
                       &channel_data[read_start_index * num_channels],
                       num_samples * num_channels * sizeof(int16_t));
            }
            else
            {
                // Wraparound case - copy in two chunks
                const uint32_t first_chunk_size = buffer_size - read_start_index;
                const uint32_t second_chunk_size = num_samples - first_chunk_size;

                // Copy first chunk of timestamps (from read_start to end of buffer)
                memcpy(trialcont->timestamps,
                       &timestamps[read_start_index],
                       first_chunk_size * sizeof(PROCTIME));

                // Copy second chunk of timestamps (from start of buffer)
                memcpy(&trialcont->timestamps[first_chunk_size],
                       timestamps,
                       second_chunk_size * sizeof(PROCTIME));

                // Copy first chunk of sample data (from read_start to end of buffer)
                memcpy(output_samples,
                       &channel_data[read_start_index * num_channels],
                       first_chunk_size * num_channels * sizeof(int16_t));

                // Copy second chunk of sample data (from start of buffer)
                memcpy(&output_samples[first_chunk_size * num_channels],
                       channel_data,
                       second_chunk_size * num_channels * sizeof(int16_t));
            }
        }

        // Update write_start_index if consuming data (bActive)
        if (bActive && num_samples > 0)
        {
            const uint32_t new_start = (read_start_index + num_samples) % grp.getSize();
            m_lockTrial.lock();
            m_CD->groups[g].setWriteStartIndex(new_start);
            m_lockTrial.unlock();
        }
    }

    if (trialevent)
    {
        if (m_ED == nullptr)
            return CBSDKRESULT_ERRCONFIG;

        // Set trial start time
        trialevent->trial_start_time = m_uTrialStartTime;

        // Determine how many samples to copy on this iteration.
        // We will copy the minimum among (write_index-write_start_index) and
        // trialevent->num_samples which should correspond to the number of allocated samples.
        // TODO: Go back to using cbNUM_ANALOG_CHANS + 2 after we have m_ChIdxInType
        uint32_t read_end_index[cbMAXCHANS];
        m_lockTrialEvent.lock();
        memcpy(read_end_index, m_ED->write_index, sizeof(m_ED->write_index));
        m_lockTrialEvent.unlock();
        // We may stop reading before this write index; track how many samples we actually read.
        uint32_t final_read_index[cbMAXCHANS] = {0};

        // copy the data from the "cache" to the user-allocated memory.
        for (uint32_t ev_ix = 0; ev_ix < trialevent->count; ev_ix++)
        {
            const uint16_t ch = trialevent->chan[ev_ix]; // channel number, 1-based
            if (ch == 0 || (ch > cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()))
                return CBSDKRESULT_INVALIDCHANNEL;
            // Ignore masked channels
            if (!m_bChannelMask[ch - 1])
            {
                memset(trialevent->num_samples[ev_ix], 0, sizeof(trialevent->num_samples[ev_ix]));
                continue;
            }

            uint32_t read_index = m_ED->write_start_index[ch - 1];
            auto num_samples = read_end_index[ch - 1] - read_index;
            if (num_samples < 0)
                num_samples += m_ED->size;

            // We don't know for sure how many samples were allocated,
            // but this is what the client application was told to allocate.
            uint32_t num_allocated = 0;
            for (int unit_ix = 0; unit_ix < (cbMAXUNITS + 1); unit_ix++)
            {
                num_allocated += trialevent->num_samples[ev_ix][unit_ix];
            }
            num_samples = std::min(static_cast<uint32_t>(num_samples), num_allocated);

            uint32_t num_samples_unit[cbMAXUNITS + 1] = {};  // To keep track of how many samples are copied per unit.

            for (int sample_ix = 0; sample_ix < num_samples; ++sample_ix)
            {
                uint16_t unit = m_ED->units[ch - 1][read_index];  // pktType for anain, or the first data value for dig/serial.

                // For digital or serial data, 'unit' holds data, and is not indicating the unit number.
                // So here we copy the data to trialevent->waveforms, then set unit to 0.
                if (IsChanDigin(ch) || IsChanSerial(ch))
                {
                    if (num_samples_unit[0] < trialevent->num_samples[ev_ix][0])
                    {
                        // Null means ignore
                        if (void * dataptr = trialevent->waveforms[ev_ix])
                        {
                            *(static_cast<uint16_t *>(dataptr) + num_samples_unit[0]) = unit;
                        }
                    }
                    unit = 0;
                }

                // Timestamps
                if (unit <= cbMAXUNITS + 1)     // remove noise unit. why?
                {
                    if (num_samples_unit[unit] < trialevent->num_samples[ev_ix][unit])
                    {
                        // Null means ignore
                        if (void * dataptr = trialevent->timestamps[ev_ix][unit])
                        {
                            auto ts = m_ED->timestamps[ch - 1][read_index];
                            *(static_cast<PROCTIME *>(dataptr) + num_samples_unit[unit]) = ts;
                        }
                        num_samples_unit[unit]++;
                        
                        // Increment the read index.
                        read_index++;
                        if (read_index >= m_ED->size)
                            read_index = 0;
                    } 
                }
            }
            // retrieved number of samples
            memcpy(trialevent->num_samples[ev_ix], num_samples_unit, sizeof(num_samples_unit));
            // Store where we actually finished reading.
            final_read_index[ch - 1] = read_index;
        }
        if (bActive)
        {
            m_lockTrialEvent.lock();
            memcpy(m_ED->write_start_index, final_read_index, sizeof(final_read_index));
            m_lockTrialEvent.unlock();
        }
    }

    if (trialcomment)
    {
        if (m_CMT == nullptr)
            return CBSDKRESULT_ERRCONFIG;

        // Set trial start time
        trialcomment->trial_start_time = m_uTrialStartTime;

        // Take a snapshot
        uint32_t read_start_index = m_CMT->write_start_index;
        m_lockTrialComment.lock();
        const uint32_t read_end_index = m_CMT->write_index;
        m_lockTrialComment.unlock();

        // copy the data from the "cache" to the allocated memory.
        uint32_t read_index = m_CMT->write_start_index;
        auto num_samples = read_end_index - read_index;
        if (num_samples < 0)
            num_samples += m_CMT->size;
        // See which one finishes first
        num_samples = std::min(static_cast<uint16_t>(num_samples), trialcomment->num_samples);
        // retrieved number of samples
        trialcomment->num_samples = num_samples;

        for (int i = 0; i < num_samples; ++i)
        {
            void * dataptr = trialcomment->timestamps;
            // Null means ignore
            if (dataptr)
            {
                auto ts = m_CMT->timestamps[read_index];
                *(static_cast<PROCTIME *>(dataptr) + i) = ts;
            }
            dataptr = trialcomment->rgbas;
            if (dataptr)
                *(static_cast<uint32_t *>(dataptr) + i) = m_CMT->rgba[read_index];
            dataptr = trialcomment->charsets;
            if (dataptr)
                *(static_cast<uint8_t *>(dataptr) + i) = m_CMT->charset[read_index];

            // Must take a copy because it might get overridden
            if (trialcomment->comments != nullptr && trialcomment->comments[i] != nullptr)
                strncpy(reinterpret_cast<char *>(trialcomment->comments[i]), reinterpret_cast<const char *>(m_CMT->comments[read_index]), cbMAX_COMMENT);

            read_index++;
            if (read_index >= m_CMT->size)
                read_index = 0;
        }
        if (bActive)
        {
            read_start_index = read_index;
            m_lockTrialComment.lock();
            m_CMT->write_start_index = read_start_index;
            m_lockTrialComment.unlock();
        }
    }

    if (trialtracking)
    {
        uint32_t read_end_index[cbMAXTRACKOBJ];
        uint32_t read_start_index[cbMAXTRACKOBJ];
        if (m_TR == nullptr)
            return CBSDKRESULT_ERRCONFIG;

        // Set trial start time
        trialtracking->trial_start_time = m_uTrialStartTime;

        // Take a snapshot
        memcpy(read_start_index, m_TR->write_start_index, sizeof(m_TR->write_start_index));
        m_lockTrialTracking.lock();
        memcpy(read_end_index, m_TR->write_index, sizeof(m_TR->write_index));
        m_lockTrialTracking.unlock();

        // copy the data from the "cache" to the allocated memory.
        for (uint16_t idx = 0; idx < trialtracking->count; ++idx)
        {
            const uint16_t id = trialtracking->ids[idx]; // actual ID

            auto read_index = m_TR->write_start_index[id];
            auto num_samples = read_end_index[id] - read_index;
            if (num_samples < 0)
                num_samples += m_TR->size;

            // See which one finishes first. Do the ternary expression twice to minimize casting.
            trialtracking->num_samples[id] = trialtracking->num_samples[id] <= num_samples ? trialtracking->num_samples[id] : static_cast<uint16_t>(num_samples);
            num_samples = num_samples <= trialtracking->num_samples[id] ? num_samples : static_cast<uint32_t>(trialtracking->num_samples[id]);

            bool bWordData = false;
            int dim_count = 2; // number of dimensions for each point
            switch(m_TR->node_type[id])
            {
            case cbTRACKOBJ_TYPE_2DMARKERS:
            case cbTRACKOBJ_TYPE_2DBLOB:
            case cbTRACKOBJ_TYPE_2DBOUNDARY:
                dim_count = 2;
                break;
            case cbTRACKOBJ_TYPE_1DSIZE:
                bWordData = true;
                dim_count = 1;
                break;
            default:
                dim_count = 3;
                break;
            }

            for (int i = 0; i < num_samples; ++i)
            {
                {
                    if (void * dataptr = trialtracking->timestamps[id])
                    {
                        auto ts = m_TR->timestamps[id][read_index];
                        *(static_cast<PROCTIME *>(dataptr) + i) = ts;
                    }
                }
                {
                    uint32_t * dataptr = trialtracking->synch_timestamps[id];
                    if (dataptr)
                        *(dataptr + i) = m_TR->synch_timestamps[id][read_index];
                    dataptr = trialtracking->synch_frame_numbers[id];
                    if (dataptr)
                        *(dataptr + i) = m_TR->synch_frame_numbers[id][read_index];
                }
                {
                    uint16_t * dataptr = trialtracking->point_counts[id];
                    uint16_t pointCount = std::min(m_TR->point_counts[id][read_index], m_TR->max_point_counts[id]);
                    if (dataptr)
                        *(dataptr + i) = pointCount;
                    if (trialtracking->coords[id])
                    {
                        if (bWordData)
                        {
                            if (auto * coordsptr = static_cast<uint32_t *>(trialtracking->coords[id][i]))
                                memcpy(coordsptr, m_TR->coords[id][read_index], pointCount * dim_count * sizeof(uint32_t));
                        }
                        else
                        {
                            if (auto * coordsptr = static_cast<uint16_t *>(trialtracking->coords[id][i]))
                                memcpy(coordsptr, m_TR->coords[id][read_index], pointCount * dim_count * sizeof(uint16_t));
                        }
                    }
                }
                read_index++;
                if (read_index >= m_TR->size)
                    read_index = 0;
           }
            // Flush the buffer and start a new 'trial'...
            if (bActive)
                read_start_index[id] = read_index;
        }

        if (bActive)
        {
            m_lockTrialTracking.lock();
            memcpy(m_TR->write_start_index, read_start_index, sizeof(read_start_index));
            m_lockTrialTracking.unlock();
        }
    }

    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkGetTrialData(const uint32_t nInstance,
                                          const uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                          cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTrialData(bActive, trialevent, trialcont, trialcomment, trialtracking);
}

// Author & Date:   Ehsan Azar     22 March 2011
/** Initialize the structures.
* See cbSdkInitTrialData docstring in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkInitTrialData(const uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                     cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, const unsigned long wait_for_comment_msec)
{
    // This time is used for relative timings,
    //  continuous as well as event relative timings reset any time bActive is set
    m_uPrevTrialStartTime = m_uTrialStartTime;

    // Optionally get the current time as the new trial start time.
    if (bActive)
        cbGetSystemClockTime(&m_uTrialStartTime, m_nInstance);

    if (trialevent)
    {
        trialevent->count = 0;
        memset(trialevent->num_samples, 0, sizeof(trialevent->num_samples));
        if (m_instInfo == 0)
        {
            memset(trialevent->chan, 0, sizeof(trialevent->chan));
            return CBSDKRESULT_WARNCLOSED;
        }
        else
        {
            if (m_ED == nullptr)
                return CBSDKRESULT_ERRCONFIG;
            // Wait for packets to come in
            m_lockGetPacketsEvent.lock();
            m_bPacketsEvent = true;
            m_lockGetPacketsEvent.unlock();

            // TODO: Go back to using cbNUM_ANALOG_CHANS + 2 after we have m_ChIdxInType
            uint32_t read_end_index[cbMAXCHANS];
            // Take a snapshot of the current write pointer
            m_lockTrialEvent.lock();
            memcpy(read_end_index, m_ED->write_index, sizeof(m_ED->write_index));
            m_lockTrialEvent.unlock();
            int count = 0;
            for (uint32_t channel = 0; channel < cbMAXCHANS; channel++)
            {
                uint32_t i = m_ED->write_start_index[channel];
                auto num_samples = read_end_index[channel] - i;
                if (num_samples < 0)
                    num_samples += m_ED->size;
                if (num_samples == 0)
                    continue;
                if (!m_bChannelMask[channel])
                    continue;
                trialevent->chan[count] = channel + 1;
                // Count sample numbers for each unit seperately
                while (i != read_end_index[channel])
                {
                    uint16_t unit = m_ED->units[channel][i];
                    if (unit > cbMAXUNITS || IsChanDigin(channel + 1) || IsChanSerial(channel + 1))
                        unit = 0;
                    trialevent->num_samples[count][unit]++;
                    if (++i >= m_ED->size)
                        i = 0;
                }
                count++;
            }
            trialevent->count = count;
        }
    }
    if (trialcont)
    {
        trialcont->count = 0;
        trialcont->num_samples = 0;
        trialcont->sample_rate = 0;

        if (m_instInfo == 0)
        {
            memset(trialcont->chan, 0, sizeof(trialcont->chan));
            return CBSDKRESULT_WARNCLOSED;
        }
        else
        {
            if (m_CD == nullptr)
                return CBSDKRESULT_ERRCONFIG;

            // Validate group index (user must set this before calling Init)
            const uint32_t g = trialcont->group;
            if (g >= cbMAXGROUPS)
                return CBSDKRESULT_INVALIDPARAM;

            const auto& grp = m_CD->groups[g];
            if (!grp.isAllocated())
            {
                // Group not allocated - return success with count=0
                memset(trialcont->chan, 0, sizeof(trialcont->chan));
                return CBSDKRESULT_SUCCESS;
            }

            // Take a snapshot of the current write pointer for this group
            uint32_t read_end_index;
            m_lockTrial.lock();
            read_end_index = grp.getWriteIndex();
            m_CD->groups[g].setReadEndIndex(read_end_index);
            m_lockTrial.unlock();

            // Calculate available samples for this group
            auto num_samples = static_cast<int32_t>(read_end_index) -
                               static_cast<int32_t>(grp.getWriteStartIndex());
            if (num_samples < 0)
                num_samples += grp.getSize();

            // Populate trial structure with group info
            trialcont->count = grp.getNumChannels();
            trialcont->sample_rate = grp.getSampleRate();
            trialcont->num_samples = num_samples;

            // Copy channel IDs
            const uint16_t* chan_ids = grp.getChannelIds();
            memcpy(trialcont->chan, chan_ids, grp.getNumChannels() * sizeof(uint16_t));
        }
    }
    if (trialcomment)
    {
        trialcomment->num_samples = 0;
        if (m_instInfo == 0)
        {
            return CBSDKRESULT_WARNCLOSED;
        }
        else
        {
            if (m_CMT == nullptr)
                return CBSDKRESULT_ERRCONFIG;

            // Wait for packets to come in
            {
                std::unique_lock<std::mutex> lock(m_lockGetPacketsCmt);
                m_bPacketsCmt = true;
                m_waitPacketsCmt.wait_for(lock, std::chrono::milliseconds(wait_for_comment_msec));
            }

            // Take a snapshot of the current write pointer
            m_lockTrialComment.lock();
            uint32_t read_end_index = m_CMT->write_index;
            uint32_t read_index = m_CMT->write_start_index;
            m_lockTrialComment.unlock();
            auto num_samples = read_end_index - read_index;
            if (num_samples < 0)
                num_samples += m_CMT->size;
            if (num_samples)
            {
                trialcomment->num_samples = num_samples;
            }
        }
    }
    if (trialtracking)
    {
        trialtracking->count = 0;
        memset(trialtracking->num_samples, 0, sizeof(trialtracking->num_samples));
        if (m_instInfo == 0)
        {
            memset(trialtracking->ids, 0, sizeof(trialtracking->ids));
            memset(trialtracking->max_point_counts, 0, sizeof(trialtracking->max_point_counts));
            memset(trialtracking->types, 0, sizeof(trialtracking->types));
            memset(trialtracking->names, 0, sizeof(trialtracking->names));
            return CBSDKRESULT_WARNCLOSED;
        }
        else
        {
            if (m_TR == nullptr)
                return CBSDKRESULT_ERRCONFIG;
            uint32_t read_end_index[cbMAXTRACKOBJ];

            // Wait for packets to come in
            {
                std::unique_lock<std::mutex> lock(m_lockGetPacketsTrack);
                m_bPacketsTrack = true;
                m_waitPacketsTrack.wait_for(lock, std::chrono::milliseconds(250));
            }

            // Take a snapshot of the current write pointer
            m_lockTrialTracking.lock();
            memcpy(read_end_index, m_TR->write_index, sizeof(m_TR->write_index));
            m_lockTrialTracking.unlock();
            int count = 0;
            for (uint16_t id = 0; id < cbMAXTRACKOBJ; ++id)
            {
                auto num_samples = read_end_index[id] - m_TR->write_start_index[id];
                if (num_samples < 0)
                    num_samples += m_TR->size;
                uint16_t type = m_TR->node_type[id];
                if (num_samples && type)
                {
                    trialtracking->ids[count] = id; // Actual trackable ID
                    trialtracking->num_samples[count] = num_samples;
                    trialtracking->types[count] = type;
                    trialtracking->max_point_counts[count] = m_TR->max_point_counts[id];
                    strncpy(reinterpret_cast<char *>(trialtracking->names[count]), reinterpret_cast<char *>(m_TR->node_name[id]), cbLEN_STR_LABEL);
                    count++;
                }
            }
            trialtracking->count = count;
        }
    }
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkInitTrialData(const uint32_t nInstance, const uint32_t bActive,
                                           cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                           cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, const unsigned long wait_for_comment_msec)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkInitTrialData(bActive, trialevent, trialcont, trialcomment, trialtracking, wait_for_comment_msec);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Start or stop file recording.
* See cbSdkSetFileConfig docstring in cbsdk.h for more information.
*/
cbSdkResult SdkApp::SdkSetFileConfig(const char * filename, const char * comment, const uint32_t bStart, const uint32_t options)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_FILECFG fcpkt = {};
    fcpkt.cbpkt_header.time    = 1;
    fcpkt.cbpkt_header.chid    = 0x8000;
    fcpkt.cbpkt_header.type    = cbPKTTYPE_SETFILECFG;
    fcpkt.cbpkt_header.dlen    = cbPKTDLEN_FILECFG;
    fcpkt.options = options;
    fcpkt.extctrl = 0;
    fcpkt.filename[0] = 0;
    memcpy(fcpkt.filename, filename, strlen(filename));
    memcpy(fcpkt.comment, comment, strlen(comment));
    // get computer name
#ifdef WIN32
    DWORD cchBuff = sizeof(fcpkt.username);
    GetComputerNameA(fcpkt.username, &cchBuff) ;
#else
    char * szHost = getenv("HOSTNAME");
    strncpy(fcpkt.username, szHost == nullptr ? "" : szHost, sizeof(fcpkt.username));
#endif
    fcpkt.username[sizeof(fcpkt.username) - 1] = 0;

    // fill in the boolean recording control field
    fcpkt.recording = bStart ? 1 : 0;

    // send the packet
    if (cbSendPacket(&fcpkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetFileConfig
CBSDKAPI    cbSdkResult cbSdkSetFileConfig(const uint32_t nInstance,
                                           const char * filename, const char * comment, const uint32_t bStart, const uint32_t options)
{
    if (filename == nullptr || comment == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (strlen(comment) >= 256)
        return CBSDKRESULT_INVALIDCOMMENT;
    if (strlen(filename) >= 256)
        return CBSDKRESULT_INVALIDFILENAME;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetFileConfig(filename, comment, bStart, options);
}

// Author & Date:   Ehsan Azar     20 Feb 2013
/** Get file recording information.
*
* @param[out]	filename      file name being recorded
* @param[out]	username      username recording the file
* @param[out]	pbRecording   If recording is in progress (depends on keep-alive mechanism)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetFileConfig(char * filename, char * username, bool * pbRecording) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_FILECFG filecfg;
    if (cbGetFileInfo(&filecfg, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    if (filename)
        strncpy(filename, filecfg.filename, sizeof(filecfg.filename));
    if (username)
        strncpy(username, filecfg.username, sizeof(filecfg.username));
    if (pbRecording)
        *pbRecording = (filecfg.options == cbFILECFG_OPT_REC);

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetFileConfig
CBSDKAPI    cbSdkResult cbSdkGetFileConfig(const uint32_t nInstance,
                                           char * filename, char * username, bool * pbRecording)
{
    if (filename == nullptr && username == nullptr && pbRecording == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetFileConfig(filename, username, pbRecording);
}

// Author & Date:   Tom Richins     31 Mar 2011
/** Share Patient demographics for recording.
*
* @param[in]	ID - patient ID
* @param[in]	firstname - patient firstname
* @param[in]	lastname - patient lastname
* @param[in]	DOBMonth - patient Date of Birth month
* @param[in]	DOBDay - patient DOB day
* @param[in]	DOBYear - patient DOB year

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetPatientInfo(const char * ID, const char * firstname, const char * lastname,
                                      const uint32_t DOBMonth, const uint32_t DOBDay, const uint32_t DOBYear) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_PATIENTINFO fcpkt = {};
    fcpkt.cbpkt_header.time    = 1;
    fcpkt.cbpkt_header.chid    = 0x8000;
    fcpkt.cbpkt_header.type    = cbPKTTYPE_SETPATIENTINFO;
    fcpkt.cbpkt_header.dlen    = cbPKTDLEN_PATIENTINFO;
    fcpkt.ID[0] = 0;
    fcpkt.firstname[0] = 0;
    fcpkt.lastname[0] = 0;
    memset(fcpkt.ID, 0, 24);
    memset(fcpkt.firstname, 0, 24);
    memset(fcpkt.lastname, 0, 24);
    memcpy(fcpkt.ID, ID, strlen(ID));
    memcpy(fcpkt.firstname, firstname, strlen(firstname));
    memcpy(fcpkt.lastname, lastname, strlen(lastname));
    fcpkt.DOBDay = DOBDay;
    fcpkt.DOBMonth = DOBMonth;
    fcpkt.DOBYear = DOBYear;

    // send the packet
    if (cbSendPacket(&fcpkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetPatientInfo
CBSDKAPI    cbSdkResult cbSdkSetPatientInfo(const uint32_t nInstance,
                                            const char * ID, const char * firstname, const char * lastname,
                                            const uint32_t DOBMonth, const uint32_t DOBDay, const uint32_t DOBYear)
{
    if (ID == nullptr || firstname == nullptr || lastname == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (strlen(ID) >= 256)
        return CBSDKRESULT_INVALIDPARAM;
    if (strlen(firstname) >= 256)
        return CBSDKRESULT_INVALIDPARAM;
    if (strlen(lastname) >= 256)
        return CBSDKRESULT_INVALIDPARAM;
    if ( DOBMonth < 1 || DOBMonth > 12 )
        return CBSDKRESULT_INVALIDPARAM;
    if ( DOBDay < 1 || DOBDay > 31 )        // incomplete test
        return CBSDKRESULT_INVALIDPARAM;
    if ( DOBYear < 1900 )
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetPatientInfo(ID, firstname, lastname, DOBMonth, DOBDay, DOBYear);
}

// Author & Date:   Tom Richins     13 Apr 2011
/** Initiate autoimpedance through central.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::SdkInitiateImpedance() const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_INITIMPEDANCE iipkt = {};
    iipkt.cbpkt_header.time    = 1;
    iipkt.cbpkt_header.chid    = 0x8000;
    iipkt.cbpkt_header.type    = cbPKTTYPE_SETINITIMPEDANCE;
    iipkt.cbpkt_header.dlen    = cbPKTDLEN_INITIMPEDANCE;

    iipkt.initiate = 1;        // start autoimpedance
    // send the packet
    if (cbSendPacket(&iipkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkInitiateImpedance
CBSDKAPI    cbSdkResult cbSdkInitiateImpedance(const uint32_t nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkInitiateImpedance();
}

// Author & Date:   Tom Richins     18 Apr 2011
/** Send poll for running applications and request response.
*
* Running applications will respond.
* @param[in]	appname application name (zero ended)
* @param[in]	mode    Poll mode
* @param[in]	flags   Poll flags
* @param[in]    extra   Extra information

* \n This function returns the error code
*
*/
cbSdkResult SdkApp::SdkSendPoll(const char* appname, const uint32_t mode, const uint32_t flags, const uint32_t extra)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_POLL polepkt = {};
    polepkt.cbpkt_header.chid = 0x8000;
    polepkt.cbpkt_header.type = cbPKTTYPE_SETPOLL;
    polepkt.cbpkt_header.dlen = cbPKTDLEN_POLL;
    polepkt.mode = mode;
    polepkt.flags = flags;
    polepkt.extra = extra;
    strncpy(polepkt.appname, appname, sizeof(polepkt.appname));
    // get computer name
#ifdef WIN32
    DWORD cchBuff = sizeof(polepkt.username);
    GetComputerNameA(polepkt.username, &cchBuff) ;
#else
    strncpy(polepkt.username, getenv("HOSTNAME"), sizeof(polepkt.username));
#endif
    polepkt.username[sizeof(polepkt.username) - 1] = 0;

    // send the packet
    if (cbSendPacket(&polepkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSendPoll
CBSDKAPI    cbSdkResult SdkSendPoll(const uint32_t nInstance,
                                    const char* appname, const uint32_t mode, const uint32_t flags, const uint32_t extra)
{
    if (appname == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSendPoll(appname, mode, flags, extra);
}

// Author & Date:   Tom Richins     26 May 2011
/** Send packet.
*
* This is used by any process that needs to send a packet using cbSdk rather
* than directly through the library.
* @param[in]	ppckt void * packet to send

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSendPacket(void * ppckt) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // send the packet
    if (cbSendPacket(ppckt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSendPacket
CBSDKAPI    cbSdkResult cbSdkSendPoll(const uint32_t nInstance, void * ppckt)
{
    if (ppckt == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSendPacket(ppckt);
}

// Author & Date:   Tom Richins     2 Jun 2011
/** Set system runlevel.
*
* @param[in]	runlevel  cbRUNLEVEL_*
* @param[in]	locked    run nflag
* @param[in]	resetque  The channel for the reset to que on

* \n This function returns success or unknown failure
*/
cbSdkResult SdkApp::SdkSetSystemRunLevel(const uint32_t runlevel, const uint32_t locked, uint32_t resetque) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // send the packet
    cbPKT_SYSINFO sysinfo;
    sysinfo.cbpkt_header.time     = 1;
    sysinfo.cbpkt_header.chid     = 0x8000;
    sysinfo.cbpkt_header.type     = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.cbpkt_header.dlen     = cbPKTDLEN_SYSINFO;
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = locked;

    // Enter the packet into the XMT buffer queue
    if (cbSendPacket(&sysinfo, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSystemRunLevel
CBSDKAPI    cbSdkResult cbSdkSetSystemRunLevel(const uint32_t nInstance, const uint32_t runlevel, const uint32_t locked, uint32_t resetque)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetSystemRunLevel(runlevel, locked, resetque);
}

// Author & Date:   Johnluke Siska     15 May 2014
/** Wrapper to get the system runlevel information.
*
* @param[out]		runlevel  cbRUNLEVEL_*
* @param[out]		runflags  For instance if the system is locked for recording
* @param[out]		resetque  The channel for the reset to que on

* \n This function returns success or unknown failure
*/
cbSdkResult SdkApp::SdkGetSystemRunLevel(uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT cbRes = cbGetSystemRunLevel(runlevel, runflags, resetque, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes == cbRESULT_HARDWAREOFFLINE)
        return CBSDKRESULT_ERROFFLINE;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSystemRunLevel
CBSDKAPI    cbSdkResult cbSdkGetSystemRunLevel(const uint32_t nInstance, uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque)
{
    if (runlevel == nullptr && runflags == nullptr && resetque == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSystemRunLevel(runlevel, runflags, resetque);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Send a digital output.
*
* @param[in]	channel    the channel number (1-based) must be digital output channel
* @param[in]	value      the digital value to send

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetDigitalOutput(const uint16_t channel, const uint16_t value) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_SET_DOUT dopkt;
    dopkt.cbpkt_header.time = 1;
    dopkt.cbpkt_header.chid = 0x8000;
    dopkt.cbpkt_header.type = cbPKTTYPE_SET_DOUTSET;
    dopkt.cbpkt_header.dlen = cbPKTDLEN_SET_DOUT;
#ifndef CBPROTO_311
    dopkt.cbpkt_header.instrument = cbGetChanInstrument(channel) - 1;
#endif
    // get the channel number
    dopkt.chan = cb_cfg_buffer_ptr[0]->chaninfo[channel - 1].chan;

    // fill in the boolean on/off field
    dopkt.value = value;

    // send the packet
    if (cbSendPacket(&dopkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetDigitalOutput
CBSDKAPI    cbSdkResult cbSdkSetDigitalOutput(const uint32_t nInstance, const uint16_t channel, const uint16_t value)
{
    uint32_t nChan = channel;

    // verify that the connection is open
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    // get the channel number
    if (!IsChanDigout(channel))
        nChan = GetDigoutChanNumber(channel);

    // verify we didn't get an invalid channel back
    if (!IsChanDigout(nChan))
        return CBSDKRESULT_INVALIDCHANNEL; // Not a digital output channel

    return g_app[nInstance]->SdkSetDigitalOutput(nChan, value);
}

// Author & Date:   Ehsan Azar     25 Feb 2013
/** Send a synch output.
*
* @param[in]	channel    the channel number (1-based) must be synch output channel
* @param[in]	nFreq      frequency in mHz (0 means stop the clock)
* @param[in]	nRepeats   number of pulses to generate (0 means forever)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetSynchOutput(const uint16_t channel, const uint32_t nFreq, const uint32_t nRepeats) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // Only CerePlex currently supports this, for NSP digital output may be used with similar results
    if (!(m_instInfo & cbINSTINFO_CEREPLEX))
        return CBSDKRESULT_NOTIMPLEMENTED;
    // Currently only one synch output supported
    if (channel != 1)
        return CBSDKRESULT_INVALIDCHANNEL; // Not a synch output channel
    // Not supported
    if (nFreq > 100000)
        return CBSDKRESULT_INVALIDPARAM;

    // declare the packet that will be sent
    cbPKT_NM nmpkt;
    nmpkt.cbpkt_header.chid = 0x8000;
    nmpkt.cbpkt_header.type = cbPKTTYPE_NMSET;
    nmpkt.cbpkt_header.dlen = cbPKTDLEN_NM;
    nmpkt.mode = cbNM_MODE_SYNCHCLOCK;
    nmpkt.value = nFreq;
    nmpkt.opt[0] = nRepeats;

    // send the packet
    if (cbSendPacket(&nmpkt, m_nInstance))
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSynchOutput
CBSDKAPI    cbSdkResult cbSdkSetSynchOutput(const uint32_t nInstance, const uint16_t channel, const uint32_t nFreq, const uint32_t nRepeats)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetSynchOutput(channel, nFreq, nRepeats);
}

// Author & Date:   Ehsan Azar     5 May 2013
/** Upload a file to NSP.
*
* This could be used to even update the firmware itself
* Warning:
* Be very careful in using this mechanism,
* if system files or firmware is overwritten, factory image is needed
* Warning:
* Must not shut down NSP or communications in midst of operation
* @param[in]	szSrc      path to file to be uploaded to NSP
* @param[in]	szDstDir   destination directory (should not include the file name, nor path ending)
* @param[in]	nInstance  library instance number

* \n This function returns the error code
*/
cbSdkResult cbSdkUpload(const char * szSrc, const char * szDstDir, const uint32_t nInstance)
{
    uint32_t runlevel;
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    cbRESULT cbres = cbGetSystemRunLevel(&runlevel, nullptr, nullptr);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    // If running or even updating do not use this, only when standby this function should be used
    if (runlevel != cbRUNLEVEL_STANDBY)
        return CBSDKRESULT_INVALIDINST;

    uint32_t cbRead = 0;
    uint32_t cbFile = 0;
    FILE * pFile = fopen(szSrc, "rb");
    if (pFile == nullptr)
        return CBSDKRESULT_ERROPENFILE;

    fseek(pFile, 0, SEEK_END);
    cbFile = ftell(pFile);
    if (cbFile > cbSdk_MAX_UPOLOAD_SIZE)
    {
        // Too big of a file to upload
        fclose(pFile);
        return CBSDKRESULT_ERRFORMATFILE;
    }
    fseek(pFile, 0, SEEK_SET);
    uint8_t * pFileData = nullptr;
    try {
        pFileData = new uint8_t[cbFile];
    } catch (...) {
        pFileData = nullptr;
    }
    if (pFileData == nullptr)
    {
        fclose(pFile);
        return CBSDKRESULT_ERRMEMORY;
    }
    // Read entire file into memory
    cbRead = static_cast<uint32_t>(fread(pFileData, sizeof(uint8_t), cbFile, pFile));
    fclose(pFile);
    if (cbFile != cbRead)
    {
        free(pFileData);
        return CBSDKRESULT_ERRFORMATFILE;
    }
    const char * szBaseName = &szSrc[0];
    // Find the base name and use it on destination too
    auto nLen = static_cast<uint32_t>(strlen(szSrc));
    for (int i = nLen - 1; i >= 0; --i)
    {
#ifdef WIN32
        if (szSrc[i] == '/' || szSrc[i] == '\\')
#else
        if (szSrc[i] == '/')
#endif
        {
            szBaseName = &szSrc[i + 1];
            break;
        }
        else if (szSrc[i] == ' ')
        {
            szBaseName = &szSrc[i];
            break;
        }
    }
    // Do not accept any space in the file name
    if (szBaseName[0] == ' ' || szBaseName[0] == 0)
    {
        free(pFileData);
        return CBSDKRESULT_INVALIDFILENAME;
    }
    nLen = static_cast<uint32_t>(strlen(szBaseName)) + static_cast<uint32_t>(strlen(szDstDir)) + 1;
    if (nLen >= 64)
    {
        free(pFileData);
        return CBSDKRESULT_INVALIDFILENAME;
    }

    cbPKT_UPDATE upkt = {};
    upkt.cbpkt_header.time = 0;
    upkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    upkt.cbpkt_header.type = cbPKTTYPE_UPDATESET;
    upkt.cbpkt_header.dlen = cbPKTDLEN_UPDATE;
    _snprintf(upkt.filename, sizeof(upkt.filename), "%s/%s", szDstDir, szBaseName);

    const uint32_t blocks = (cbFile / 512) + 1;
    for (uint32_t b = 0; b < blocks; ++b)
    {
        upkt.blockseq = b;
        upkt.blockend = (b == (blocks - 1));
        upkt.blocksiz = std::min(static_cast<int32_t>(cbFile - (b * 512)), (int32_t) 512);
        memcpy(&upkt.block[0], pFileData + (b * 512), upkt.blocksiz);
        do {
            cbres = cbSendPacket(&upkt, nInstance);
        } while (cbres == cbRESULT_MEMORYUNAVAIL);
        if (cbres)
        {
            res = CBSDKRESULT_UNKNOWN;
            break;
        }
    }

    free(pFileData);

    if (res == CBSDKRESULT_SUCCESS)
    {
        /// \todo must wait for cbLOG_MODE_UPLOAD_RES because mount is asynch
    }

    return res;
}

// Author & Date:   Ehsan Azar     5 May 2013
/** Send an extension command.
*
* @param[in]	extCmd      the extension command

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkExtDoCommand(const cbSdkExtCmd * extCmd) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // Only NSP1.5 compatible devices currently supports this
    if (m_instInfo & cbINSTINFO_NSP1)
        return CBSDKRESULT_NOTIMPLEMENTED;

    cbRESULT cbres;
    cbPKT_LOG pktLog = {};
    strcpy(pktLog.name, "cbSDK");
    pktLog.cbpkt_header.chid = 0x8000;
    pktLog.cbpkt_header.type = cbPKTTYPE_LOGSET;
    pktLog.cbpkt_header.dlen = cbPKTDLEN_LOG;

    switch (extCmd->cmd)
    {
    case cbSdkExtCmd_INPUT:
        pktLog.mode = cbLOG_MODE_RPC_INPUT;
        strncpy(pktLog.desc, extCmd->szCmd, sizeof(pktLog.desc));
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    case cbSdkExtCmd_RPC:
        pktLog.mode = cbLOG_MODE_RPC;
        strncpy(pktLog.desc, extCmd->szCmd, sizeof(pktLog.desc));
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    case cbSdkExtCmd_UPLOAD:
        return cbSdkUpload(extCmd->szCmd, "/extnroot/root", m_nInstance);
        break;
    case cbSdkExtCmd_TERMINATE:
        pktLog.mode = cbLOG_MODE_RPC_KILL;
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    case cbSdkExtCmd_END_PLUGIN:
        pktLog.mode = cbLOG_MODE_ENDPLUGIN;
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    case cbSdkExtCmd_NSP_REBOOT:
        pktLog.mode = cbLOG_MODE_NSP_REBOOT;
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    case cbSdkExtCmd_PLUGINFO:
        pktLog.mode = cbLOG_MODE_PLUGINFO;
        // send the packet
        cbres = cbSendPacket(&pktLog, m_nInstance);
        if (cbres)
            return CBSDKRESULT_UNKNOWN;
        break;
    default:
        return CBSDKRESULT_INVALIDPARAM;
        break;
    }

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::cbSdkExtCmd
CBSDKAPI    cbSdkResult cbSdkExtDoCommand(const uint32_t nInstance, cbSdkExtCmd * extCmd)
{
    if (extCmd == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkExtDoCommand(extCmd);
}

// Author & Date:   Ehsan Azar     26 Oct 2011
/** Send an analog output waveform, or monitor a channel. If none is given then analog output channel is disabled.
*
* @param[in]	channel     the channel number (1-based) must be digital output channel
* @param[in]	wf			pointer to waveform structure
* @param[in]	mon			the structure to specify monitoring channel

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetAnalogOutput(const uint16_t channel, const cbSdkWaveformData * wf, const cbSdkAoutMon * mon) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (wf != nullptr)
    {
        if (wf->type < cbSdkWaveform_NONE || wf->type >= cbSdkWaveform_COUNT)
            return CBSDKRESULT_INVALIDPARAM;
        if (wf->trigNum >= cbMAX_AOUT_TRIGGER)
            return CBSDKRESULT_INVALIDPARAM;
        if (wf->trig < cbSdkWaveformTrigger_NONE || wf->trig >= cbSdkWaveformTrigger_COUNT)
            return CBSDKRESULT_INVALIDPARAM;
        if (wf->type == cbSdkWaveform_PARAMETERS && wf->phases > cbMAX_WAVEFORM_PHASES)
            return CBSDKRESULT_NOTIMPLEMENTED;
        switch (wf->trig)
        {
        case cbWAVEFORM_TRIGGER_DINPREG:
        case cbWAVEFORM_TRIGGER_DINPFEG:
            if (wf->trigChan == 0 || wf->trigChan > 16)
                return CBSDKRESULT_INVALIDPARAM;
            break;
        case cbWAVEFORM_TRIGGER_SPIKEUNIT:
            if (wf->trigChan == 0 || wf->trigChan > cbMAXCHANS)
                return CBSDKRESULT_INVALIDCHANNEL;
            break;
        default:
            break;
        }
    }

    uint32_t dwOptions;
    uint32_t nMonitoredChan;
    cbRESULT cbres = cbGetAoutOptions(channel, &dwOptions, &nMonitoredChan, nullptr, m_nInstance);
    switch (cbres)
    {
    case cbRESULT_OK:
        break;
    case cbRESULT_INVALIDCHANNEL:
        return CBSDKRESULT_INVALIDCHANNEL;
        break;
    default:
        return CBSDKRESULT_UNKNOWN;
        break;
    }
    if (wf != nullptr)
    {
        cbPKT_AOUT_WAVEFORM wfPkt = {};
        wfPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
        wfPkt.cbpkt_header.type = cbPKTTYPE_WAVEFORMSET;
        wfPkt.cbpkt_header.dlen = cbPKTDLEN_WAVEFORM;
#ifndef CBPROTO_311
        wfPkt.cbpkt_header.instrument = cbGetChanInstrument(channel) - 1;
#endif
        // Set common fields
        wfPkt.mode = wf->type;
        wfPkt.chan = cb_cfg_buffer_ptr[0]->chaninfo[channel - 1].chan;
        wfPkt.repeats       = wf->repeats;
        wfPkt.trig          = wf->trig;
        wfPkt.trigChan      = wf->trigChan;
        wfPkt.trigValue     = wf->trigValue;
        wfPkt.trigNum       = wf->trigNum;
        // Mode-specific fields
        if (wfPkt.mode == cbWAVEFORM_MODE_PARAMETERS)
        {
            if (wf->phases > cbMAX_WAVEFORM_PHASES)
                return CBSDKRESULT_INVALIDPARAM;

            memcpy(wfPkt.wave.duration, wf->duration, sizeof(uint16_t) * wf->phases);
            memcpy(wfPkt.wave.amplitude, wf->amplitude, sizeof(int16_t) * wf->phases);
            wfPkt.wave.seq      = 0;
            wfPkt.wave.seqTotal = 1;
            wfPkt.wave.phases   = wf->phases;
            wfPkt.wave.offset   = wf->offset;
            wfPkt.mode          = cbWAVEFORM_MODE_PARAMETERS;
        }
        else if (wfPkt.mode == cbWAVEFORM_MODE_SINE)
        {
            wfPkt.wave.offset        = wf->offset;
            wfPkt.wave.sineFrequency = wf->sineFrequency;
            wfPkt.wave.sineAmplitude = wf->sineAmplitude;
            wfPkt.mode          = cbWAVEFORM_MODE_SINE;
        }
        // Sending a none-trigger we turn it into instant activation
        if (wfPkt.trig == cbWAVEFORM_TRIGGER_NONE)
            wfPkt.active = 1;

        // send the waveform packet
        cbSendPacket(&wfPkt, m_nInstance);
        // Also make sure channel is to output waveform
        dwOptions &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK);
        dwOptions |= cbAOUT_WAVEFORM;
    }
    else if (mon != nullptr)
    {
        nMonitoredChan = mon->chan;
        dwOptions &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK | cbAOUT_TRACK);
        if (mon->bSpike)
            dwOptions |= cbAOUT_MONITORSPK;
        else
            dwOptions |= cbAOUT_MONITORSMP;
        if (mon->bTrack)
            dwOptions |= cbAOUT_TRACK;
    }
    else
    {
        dwOptions &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK | cbAOUT_WAVEFORM | cbAOUT_EXTENSION);
    }

    // Set monitoring option
    cbres = cbSetAoutOptions(channel, dwOptions, nMonitoredChan, 0, m_nInstance);
    switch (cbres)
    {
    case cbRESULT_OK:
        break;
    case cbRESULT_INVALIDCHANNEL:
        return CBSDKRESULT_INVALIDCHANNEL;
        break;
    default:
        return CBSDKRESULT_UNKNOWN;
        break;
    }

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetAnalogOutput
CBSDKAPI    cbSdkResult cbSdkSetAnalogOutput(const uint32_t nInstance,
                                             const uint16_t channel, const cbSdkWaveformData * wf, const cbSdkAoutMon * mon)
{
    uint32_t nChan = channel;

    // verify that the connection is open
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    if (!IsChanAnalogOut(channel) && !IsChanAudioOut(channel))
        nChan = GetAnalogOrAudioOutChanNumber(channel);

    if (wf != nullptr && mon != nullptr)
        return CBSDKRESULT_INVALIDPARAM; // cannot specify both
    if (!IsChanAnalogOut(nChan) && !IsChanAudioOut(nChan))
        return CBSDKRESULT_INVALIDCHANNEL;

    return g_app[nInstance]->SdkSetAnalogOutput(nChan, wf, mon);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Activate or deactivate channels. Only activated channels are monitored.
*
* @param[in]	channel     channel number (1-based), zero means all channels
* @param[in]	bActive		the character set of the comment

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetChannelMask(const uint16_t channel, const uint32_t bActive)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // zero means all
    if (channel == 0)
    {
        for (bool & i : m_bChannelMask)
            i = (bActive > 0);
    }
    else
        m_bChannelMask[channel - 1] = (bActive > 0);
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetChannelMask
CBSDKAPI    cbSdkResult cbSdkSetChannelMask(const uint32_t nInstance, const uint16_t channel, const uint32_t bActive)
{
    if (channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetChannelMask(channel, bActive);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Send a comment or custom event.
*
* @param[in]	rgba     the color or custom event number
* @param[in]	charset	 the character set of the comment
* @param[in]	comment	 comment string (can be nullptr)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetComment(const uint32_t rgba, const uint8_t charset, const char * comment) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (cbSetComment(charset, rgba, 0, comment, m_nInstance))
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetComment
CBSDKAPI    cbSdkResult cbSdkSetComment(const uint32_t nInstance, const uint32_t t_bgr, const uint8_t charset, const char * comment)
{
    if (comment)
    {
        if (strlen(comment) >= cbMAX_COMMENT)
            return CBSDKRESULT_INVALIDCOMMENT;
    }
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetComment(t_bgr, charset, comment);
}

// Author & Date:   Ehsan Azar     3 March 2011
/** Send a full channel configuration packet.
*
* @param[in]	channel     channel number (1-based)
* @param[in]	chaninfo	the full channel configuration

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetChannelConfig(const uint16_t channel, cbPKT_CHANINFO * chaninfo) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (cb_cfg_buffer_ptr[m_nIdx]->chaninfo[channel - 1].cbpkt_header.chid == 0)
        return CBSDKRESULT_INVALIDCHANNEL;

    chaninfo->cbpkt_header.type = cbPKTTYPE_CHANSET;
    chaninfo->cbpkt_header.dlen = cbPKTDLEN_CHANINFO;
    const cbRESULT cbRes = cbSendPacket(chaninfo, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetChannelConfig
CBSDKAPI    cbSdkResult cbSdkSetChannelConfig(const uint32_t nInstance, const uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (chaninfo == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (chaninfo->cbpkt_header.chid != cbPKTCHAN_CONFIGURATION)
        return CBSDKRESULT_INVALIDPARAM;
    if (chaninfo->cbpkt_header.dlen > cbPKTDLEN_CHANINFO || chaninfo->cbpkt_header.dlen < cbPKTDLEN_CHANINFOSHORT)
        return CBSDKRESULT_INVALIDPARAM;
    if (chaninfo->chan != channel)
        return CBSDKRESULT_INVALIDCHANNEL;

    /// \todo do more validation before sending the packet

    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetChannelConfig(channel, chaninfo);
}

// Author & Date:   Ehsan Azar     28 Feb 2011
// Purpose: Get a channel configuration packet
// Inputs:
//   channel  - channel number (1-based)
// Outputs:
//   chaninfo - the full channel configuration
//   returns the error code
/** Get a channel configuration packet.
*
* @param[in]	channel     channel number (1-based)
* @param[in]	chaninfo	the full channel configuration

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetChannelConfig(const uint16_t channel, cbPKT_CHANINFO * chaninfo) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!cb_library_initialized[m_nIdx])
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetChanInfo(channel, chaninfo, m_nInstance);
    if (cbRes == cbRESULT_INVALIDCHANNEL)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetChannelConfig
CBSDKAPI    cbSdkResult cbSdkGetChannelConfig(const uint32_t nInstance, const uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (chaninfo == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetChannelConfig(channel, chaninfo);
}

// Author & Date:   Tom Richins     11 Apr 2011
// Purpose: retrieve group list
//           wrapper to export cbGetSampleGroupList

/** Retrieve group list.
*
* @param[in]		proc
* @param[in]		group      group (1-5)
* @param[in,out]	length     length of group list
* @param[in,out]	list	   list of channels in selected group (1-based)


* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetSampleGroupList(const uint32_t proc, const uint32_t group, uint32_t *length, uint16_t *list) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetSampleGroupList(proc, group, length, list, m_nInstance);
    if (cbRes == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSampleGroupList
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupList(const uint32_t nInstance,
                                                const uint32_t proc, const uint32_t group, uint32_t *length, uint16_t *list)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSampleGroupList(proc, group, length, list);
}

// Author & Date:   Sylvana Alpert     13 Jan 2014
// Purpose: retrieve sample group info
//           wrapper to export cbGetSampleGroupInfo
/** Retrieve group info.
*
* @param[in]		proc
* @param[in]		group      group (1-5)
* @param[in,out]	label
* @param[in,out]	period
* @param[in,out]	length

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetSampleGroupInfo(const uint32_t proc, const uint32_t group, char *label, uint32_t *period, uint32_t *length) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetSampleGroupInfo(proc, group, label, period, length, m_nInstance);
    if (cbRes == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSampleGroupInfo
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupInfo(const uint32_t nInstance,
                                                const uint32_t proc, const uint32_t group, char *label, uint32_t *period, uint32_t *length)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSampleGroupInfo(proc, group, label, period, length);
}


// Author & Date:   Tom Richins        24 Jun 2011
// Purpose: Get filter description
//           wrapper to export cbGetFilterDesc

/** Get filter description.
*
* @param[in]		proc
* @param[in]		filt		filter number
* @param[out]		filtdesc	the filter description

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetFilterDesc(const uint32_t proc, const uint32_t filt, cbFILTDESC * filtdesc) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetFilterDesc(proc, filt, filtdesc, m_nInstance);
    if (cbRes == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetFilterDesc
CBSDKAPI    cbSdkResult cbSdkGetFilterDesc(const uint32_t nInstance, const uint32_t proc, const uint32_t filt, cbFILTDESC * filtdesc)
{
    if (filtdesc == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetFilterDesc(proc, filt, filtdesc);
}

// Author & Date:   Ehsan Azar     27 Oct 2011
// Purpose: retrieve group list
//           wrapper to export cbGetTrackObj

/** Retrieve tracking information.
*
* @param[in]		id			trackable object ID (1 to cbMAXTRACKOBJ)
* @param[out]		name		name of the video source
* @param[out]		type		type of the trackable object (start from 0)
* @param[out]		pointCount	the maximum number of points for this trackable

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetTrackObj(char * name, uint16_t * type, uint16_t * pointCount, const uint32_t id) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetTrackObj(name, type, pointCount, id, m_nInstance);
    if (cbRes == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDTRACKABLE;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetTrackObj
CBSDKAPI    cbSdkResult cbSdkGetTrackObj(const uint32_t nInstance, char * name, uint16_t * type, uint16_t * pointCount, const uint32_t id)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTrackObj(name, type, pointCount, id);
}

// Author & Date:   Ehsan Azar     27 Oct 2011
// Purpose: retrieve group list
//           wrapper to export cbGetVideoSource

/** Retrieve video source.
*
* @param[in]		id			video source ID (1 to cbMAXVIDEOSOURCE)
* @param[out]		name		name of the video source
* @param[out]		fps			the frame rate of the video source

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetVideoSource(char * name, float * fps, const uint32_t id) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    const cbRESULT cbRes = cbGetVideoSource(name, fps, id, m_nInstance);
    if (cbRes == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDVIDEOSRC;
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetVideoSource
CBSDKAPI    cbSdkResult cbSdkGetVideoSource(const uint32_t nInstance, char * name, float * fps, const uint32_t id)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetVideoSource(name, fps, id);
}

// Author & Date:   Ehsan Azar     30 March 2011
/** Send global spike configuration.
*
* @param[in]		spklength		spike length
* @param[in]		spkpretrig		spike pre-trigger number of samples

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetSpikeConfig(const uint32_t spklength, const uint32_t spkpretrig) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    const cbRESULT cbRes = cbSetSpikeLength(spklength, spkpretrig, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSpikeConfig
CBSDKAPI    cbSdkResult cbSdkSetSpikeConfig(const uint32_t nInstance, const uint32_t spklength, const uint32_t spkpretrig)
{
    if (spklength > cbMAX_PNTS || spkpretrig >= spklength)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetSpikeConfig(spklength, spkpretrig);
}

// Author & Date:   Ehsan Azar     30 March 2011
/** Send global spike configuration.
*
* @param[in]		chan		channel id (1-based)
* @param[in]		filter		filter id
* @param[in]        group		the sample group (1-6)
*
* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetAinpSampling(const uint32_t chan, const uint32_t filter, const uint32_t group) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    const cbRESULT cbRes = cbSetAinpSampling(chan, filter, group, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkSetAinpSampling(const uint32_t nInstance, const uint32_t chan, const uint32_t filter, const uint32_t group)
{
    if (chan > cbMAXCHANS || filter >= cbMAXFILTS)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;
    return g_app[nInstance]->SdkSetAinpSampling(chan, filter, group);
}

cbSdkResult SdkApp::SdkSetAinpSpikeOptions(const uint32_t chan, const uint32_t flags, const uint32_t filter) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    const cbRESULT cbRes = cbSetAinpSpikeOptions(chan, flags, filter);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

CBSDKAPI    cbSdkResult cbSdkSetAinpSpikeOptions(const uint32_t nInstance, const uint32_t chan, const uint32_t flags, const uint32_t filter)
{
    if (chan > cbMAXCHANS || filter >= cbMAXFILTS)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;
    return g_app[nInstance]->SdkSetAinpSpikeOptions(chan, flags, filter);
}

// Author & Date:   Ehsan Azar     30 March 2011
/** Get global system configuration.
*
* @param[out]		spklength		spike length
* @param[out]		spkpretrig		spike pre-trigger number of samples
* @param[out]		sysfreq			system clock frequency in Hz

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetSysConfig(uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    const cbRESULT cbRes = cbGetSpikeLength(spklength, spkpretrig, sysfreq, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSysConfig
CBSDKAPI    cbSdkResult cbSdkGetSysConfig(const uint32_t nInstance, uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq)
{
    if (spklength == nullptr && spkpretrig == nullptr && sysfreq == nullptr)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSysConfig(spklength, spkpretrig, sysfreq);
}

// Author & Date:   Ehsan Azar     11 MAy 2012
/** Perform given runlevel system command.
*
* @param[out]		cmd		system command to perform

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSystem(const cbSdkSystemType cmd) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbPKT_SYSINFO pktsysinfo;
    pktsysinfo.cbpkt_header.time = 0;
    pktsysinfo.cbpkt_header.chid = 0x8000;
    pktsysinfo.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
    pktsysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;
#ifndef CBPROTO_311
    pktsysinfo.cbpkt_header.instrument = 0;
#endif
    switch (cmd)
    {
    case cbSdkSystem_RESET:
        pktsysinfo.runlevel = cbRUNLEVEL_RESET;
        break;
    case cbSdkSystem_SHUTDOWN:
        pktsysinfo.runlevel = cbRUNLEVEL_SHUTDOWN;
        break;
    case cbSdkSystem_STANDBY:
        pktsysinfo.runlevel = cbRUNLEVEL_HARDRESET;
        break;
    default:
        return CBSDKRESULT_NOTIMPLEMENTED;
    }
    const cbRESULT cbRes = cbSendPacket(&pktsysinfo, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSystem
CBSDKAPI    cbSdkResult cbSdkSystem(const uint32_t nInstance, const cbSdkSystemType cmd)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSystem(cmd);
}

// Author & Date:   Ehsan Azar     28 Feb 2011
/** Register a callback function.
*
* @param[in]	callbackType		the callback type to register function against
* @param[in]	pCallbackFn		callback function
* @param[in]	pCallbackData		custom parameter callback is called with
*
* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkRegisterCallback(const cbSdkCallbackType callbackType, const cbSdkCallback pCallbackFn, void * pCallbackData)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (m_pCallback[callbackType]) // Already registered
        return CBSDKRESULT_CALLBACKREGFAILED;

    m_lockCallback.lock();
    m_pCallbackParams[callbackType] = pCallbackData;
    m_pCallback[callbackType] = pCallbackFn;
    m_lockCallback.unlock();
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkRegisterCallback
CBSDKAPI    cbSdkResult cbSdkRegisterCallback(const uint32_t nInstance,
                                              const cbSdkCallbackType callbackType, const cbSdkCallback pCallbackFn, void * pCallbackData)
{
    if (!pCallbackFn)
        return CBSDKRESULT_NULLPTR;
    if (callbackType >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkRegisterCallback(callbackType, pCallbackFn, pCallbackData);
}

// Author & Date:   Ehsan Azar     28 Feb 2011
/** Unregister the current callback.
*
* @param[in]	callbackType		the callback type to unregister

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkUnRegisterCallback(const cbSdkCallbackType callbackType)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!m_pCallback[callbackType]) // Already unregistered
        return CBSDKRESULT_CALLBACKREGFAILED;

    m_lockCallback.lock();
    m_pCallback[callbackType] = nullptr;
    m_pCallbackParams[callbackType] = nullptr;
    m_lockCallback.unlock();
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkUnRegisterCallback
CBSDKAPI    cbSdkResult cbSdkUnRegisterCallback(const uint32_t nInstance, const cbSdkCallbackType callbackType)
{
    if (callbackType >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkUnRegisterCallback(callbackType);
}

// Author & Date:   Ehsan Azar     7 Aug 2013
/** Get callback status.
* if unregistered returns success, and means a register should not fail
* @param[in]	callbackType		the callback type to unregister

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkCallbackStatus(const cbSdkCallbackType callbackType) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (m_pCallback[callbackType])
        return CBSDKRESULT_CALLBACKREGFAILED; // Already registered
    return CBSDKRESULT_SUCCESS;
}

// Purpose: sdk stub for SdkApp::SdkCallbackStatus
CBSDKAPI    cbSdkResult cbSdkCallbackStatus(const uint32_t nInstance, const cbSdkCallbackType callbackType)
{
    if (callbackType >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkCallbackStatus(callbackType);
}

// Author & Date:   Ehsan Azar     21 Feb 2013
/** Convert volts string (e.g. '5V', '-65mV', ...) to its raw digital value equivalent for given channel.
*
* @param[in]	channel				the channel number (1-based)
* @param[in]	szVoltsUnitString	the volts string
* @param[out]	digital				the raw digital value

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkAnalogToDigital(uint16_t channel, const char * szVoltsUnitString, int32_t * digital) const {
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!cb_library_initialized[m_nIdx])
        return CBSDKRESULT_CLOSED;
    if (static_cast<int>(strlen(szVoltsUnitString)) > 16)
        return CBSDKRESULT_INVALIDPARAM;
    int32_t nFactor = 0;
    std::string strVolts = szVoltsUnitString;
    std::string strUnit;
    if (strVolts.rfind("mV") != std::string::npos)
    {
        nFactor = 1000;
        strUnit = "mV";
    }
    else if (strVolts.rfind("uV") != std::string::npos)
    {
        nFactor = 1000000;
        strUnit = "uV";
    }
    else if (strVolts.rfind("nV") != std::string::npos)
    {
        nFactor = 1000000000;
        strUnit = "nV";
    }
    else if (strVolts.rfind('V') != std::string::npos)
    {
        nFactor = 1;
        strUnit = "V";
    }
    char * pEnd = nullptr;
    double dValue = 0;
    long nValue = 0;
    // If no units specified, assume raw integer passed as string
    if (nFactor == 0)
    {
        nValue = strtol(szVoltsUnitString, &pEnd, 0);
    }
    else
    {
        dValue = strtod(szVoltsUnitString, &pEnd);
    }
    if (pEnd == szVoltsUnitString)
        return CBSDKRESULT_INVALIDPARAM;
    // What remains should be just the unit string
    std::string strRest = pEnd;
    // Remove all spaces
    std::string::iterator end_pos = std::remove(strRest.begin(), strRest.end(), ' ');
    strRest.erase(end_pos, strRest.end());
    if (strRest != strUnit)
        return CBSDKRESULT_INVALIDPARAM;
    if (nFactor == 0)
    {
        *digital = static_cast<int32_t>(nValue);
        return CBSDKRESULT_SUCCESS;
    }
    cbSCALING scale;
    cbRESULT cbRes;
    if (IsChanAnalogIn(channel))
        cbRes = cbGetAinpScaling(channel, &scale, m_nInstance);
    else
        cbRes = cbGetAoutScaling(channel, &scale, m_nInstance);
    if (cbRes == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbRes)
        return CBSDKRESULT_UNKNOWN;
    strUnit = scale.anaunit;
    double chan_factor = 1;
    if (strUnit == "mV")
        chan_factor = 1000;
    else if (strUnit == "uV")
        chan_factor = 1000000;
    // TODO: see if anagain needs to be used
    *digital = static_cast<int32_t>(floor(((dValue * scale.digmax) * chan_factor) / (static_cast<double>(nFactor) * scale.anamax)));
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkAnalogToDigital
CBSDKAPI    cbSdkResult cbSdkAnalogToDigital(const uint32_t nInstance, const uint16_t channel, const char * szVoltsUnitString, int32_t * digital)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == nullptr)
        return CBSDKRESULT_CLOSED;
    if (szVoltsUnitString == nullptr || digital == nullptr)
        return CBSDKRESULT_NULLPTR;

    return g_app[nInstance]->SdkAnalogToDigital(channel, szVoltsUnitString, digital);
}


// Author & Date: Ehsan Azar       29 April 2012
/// Sdk app base constructor
SdkApp::SdkApp()
    : m_bInitialized(false), m_lastCbErr(cbRESULT_OK)
    , m_bPacketsEvent(false), m_bPacketsCmt(false), m_bPacketsTrack(false)
    , m_uTrialBeginChannel(0), m_uTrialBeginMask(0), m_uTrialBeginValue(0)
    , m_uTrialEndChannel(0), m_uTrialEndMask(0), m_uTrialEndValue(0)
    , m_uTrialWaveforms(0)
    , m_uTrialConts(0), m_uTrialEvents(0), m_uTrialComments(0)
    , m_uTrialTrackings(0), m_bWithinTrial(false), m_uTrialStartTime(0)
    , m_uCbsdkTime(0), m_CD(nullptr), m_ED(nullptr), m_CMT(nullptr), m_TR(nullptr)
{
    memset(&m_lastPktVideoSynch, 0, sizeof(m_lastPktVideoSynch));
    memset(&m_bChannelMask, 0, sizeof(m_bChannelMask));
    memset(&m_lastPktVideoSynch, 0, sizeof(m_lastPktVideoSynch));
    memset(&m_lastLost, 0, sizeof(m_lastLost));
    memset(&m_lastInstInfo, 0, sizeof(m_lastInstInfo));
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallback[i] = nullptr;
        m_pCallbackParams[i] = nullptr;
        m_pLateCallback[i] = nullptr;
        m_pLateCallbackParams[i] = nullptr;
    }
}

// Author & Date: Ehsan Azar       29 April 2012
/// Sdk app base destructor
SdkApp::~SdkApp()
{
    // Close networking
    Close();
}

// Author & Date: Ehsan Azar       29 April 2012
/** Open sdk application, network and Qt message loop.
*
* @param[in]	nInstance		instance ID
* @param[in]	nInPort			Client port number
* @param[in]	nOutPort		Instrument port number
* @param[in]	szInIP			Client IPv4 address
* @param[in]	szOutIP			Instrument IPv4 address
* @param[in]    nRecBufSize
* @param[in]    nRange          Unused
*/
void SdkApp::Open(const uint32_t nInstance, const int nInPort, const int nOutPort, const LPCSTR szInIP, const LPCSTR szOutIP, const int nRecBufSize, int nRange)
{
    // clear las library error
    m_lastCbErr = cbRESULT_OK;
    // Close networking thread if already running
    Close();
    // One-time initialization
    if (!m_bInitialized)
    {
        m_bInitialized = true;
        // InstNetworkEvent now directly calls OnInstNetworkEvent (virtual function call)
        // Add myself as the sole listener
        InstNetwork::Open(this);
    }
    // instance id and connection details are persistent in the process
    m_nInstance = nInstance;
    m_nInPort = nInPort;
    m_nOutPort = nOutPort;
    m_nRecBufSize = nRecBufSize;
    m_strInIP = szInIP;
    m_strOutIP = szOutIP;

#ifndef WIN32
    // On Linux bind to broadcast
    if (m_strInIP.size() >= 4 && m_strInIP.substr(m_strInIP.size() - 4) == ".255")
        m_bBroadcast = true;
#endif

    // Restart networking thread
    Start();
}

// Author & Date: Ehsan Azar       29 April 2012
/// Proxy for all incoming packets
void SdkApp::ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt)
{
    // This is a hot code path, and crosses the shared library
    //  we want as minimal locking as possible
    //  we also want to reduce deadlock if someone calls unregister within the callback itself
    //  thus we late bind callback when a change is noticed and only when needed,
    //   then use the late bound callback function
    //  As a rule of thumb, no locks should be active before any callback is called

    // This callback type needs to be bound only once
    LateBindCallback(CBSDKCALLBACK_ALL);
    bool b_checkEvent = false;

    // check for configuration class packets
    if (pPkt->cbpkt_header.chid & cbPKTCHAN_CONFIGURATION)
    {
        // Check for configuration packets
        if (pPkt->cbpkt_header.chid == cbPKTCHAN_CONFIGURATION)
        {
            if (pPkt->cbpkt_header.type == cbPKTTYPE_SYSHEARTBEAT)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SYSHEARTBEAT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_SYSHEARTBEAT);
                if (m_pLateCallback[CBSDKCALLBACK_SYSHEARTBEAT])
                    m_pLateCallback[CBSDKCALLBACK_SYSHEARTBEAT](m_nInstance, cbSdkPkt_SYSHEARTBEAT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SYSHEARTBEAT]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_REPIMPEDANCE)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_IMPEDANCE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_IMPEDANCE);
                if (m_pLateCallback[CBSDKCALLBACK_IMPEDANCE])
                    m_pLateCallback[CBSDKCALLBACK_IMPEDANCE](m_nInstance, cbSdkPkt_IMPEDANCE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_IMPEDANCE]);
            }
            else if ((pPkt->cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_CHANINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_CHANINFO);
                if (m_pLateCallback[CBSDKCALLBACK_CHANINFO])
                    m_pLateCallback[CBSDKCALLBACK_CHANINFO](m_nInstance, cbSdkPkt_CHANINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_CHANINFO]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_NMREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_NM, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_NM);
                if (m_pLateCallback[CBSDKCALLBACK_NM])
                    m_pLateCallback[CBSDKCALLBACK_NM](m_nInstance, cbSdkPkt_NM, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_NM]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_GROUPREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_GROUPINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_GROUPINFO);
                if (m_pLateCallback[CBSDKCALLBACK_GROUPINFO])
                    m_pLateCallback[CBSDKCALLBACK_GROUPINFO](m_nInstance, cbSdkPkt_GROUPINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_GROUPINFO]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_COMMENTREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_COMMENT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_COMMENT);
                if (m_pLateCallback[CBSDKCALLBACK_COMMENT])
                    m_pLateCallback[CBSDKCALLBACK_COMMENT](m_nInstance, cbSdkPkt_COMMENT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_COMMENT]);
                // Fillout trial if setup
                OnPktComment(reinterpret_cast<const cbPKT_COMMENT*>(pPkt));
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_REPFILECFG)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_FILECFG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_FILECFG);
                if (m_pLateCallback[CBSDKCALLBACK_FILECFG])
                    m_pLateCallback[CBSDKCALLBACK_FILECFG](m_nInstance, cbSdkPkt_FILECFG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_COMMENT]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_REPPOLL)
            {
                // The callee should check flags to find if it is a response to poll, and do accordingly
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_POLL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_POLL);
                if (m_pLateCallback[CBSDKCALLBACK_POLL])
                    m_pLateCallback[CBSDKCALLBACK_POLL](m_nInstance, cbSdkPkt_POLL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_POLL]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_VIDEOTRACKREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_TRACKING, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_TRACKING);
                if (m_pLateCallback[CBSDKCALLBACK_TRACKING])
                    m_pLateCallback[CBSDKCALLBACK_TRACKING](m_nInstance, cbSdkPkt_TRACKING, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_TRACKING]);
                // Fillout trial if setup
                OnPktTrack(reinterpret_cast<const cbPKT_VIDEOTRACK*>(pPkt));
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_VIDEOSYNCHREP)
            {
                m_lastPktVideoSynch = *reinterpret_cast<const cbPKT_VIDEOSYNCH*>(pPkt);
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SYNCH, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_SYNCH);
                if (m_pLateCallback[CBSDKCALLBACK_SYNCH])
                    m_pLateCallback[CBSDKCALLBACK_SYNCH](m_nInstance, cbSdkPkt_SYNCH, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SYNCH]);
            }
            else if (pPkt->cbpkt_header.type == cbPKTTYPE_LOGREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_LOG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_LOG);
                if (m_pLateCallback[CBSDKCALLBACK_LOG])
                    m_pLateCallback[CBSDKCALLBACK_LOG](m_nInstance, cbSdkPkt_LOG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_LOG]);
                // Fill out trial if setup
                OnPktLog(reinterpret_cast<const cbPKT_LOG*>(pPkt));
                //OnPktComment(reinterpret_cast<const cbPKT_COMMENT*>(pPkt));
            }
        } // end if (pPkt->chid==0x8000
    } // end if (pPkt->chid & 0x8000
    else if (pPkt->cbpkt_header.chid == 0)
    {
        // No mask applied here
        // Inside the callback cbPKT_GROUP.type can be used to find the sample group number
        if (const uint8_t smpGroup = ((cbPKT_GROUP *)pPkt)->cbpkt_header.type; smpGroup > 0 && smpGroup <= cbMAXGROUPS)
        {
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_CONTINUOUS, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_CONTINUOUS);
            if (m_pLateCallback[CBSDKCALLBACK_CONTINUOUS])
                m_pLateCallback[CBSDKCALLBACK_CONTINUOUS](m_nInstance, cbSdkPkt_CONTINUOUS, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_CONTINUOUS]);
        }
    }
    // check for channel event packets cerebus channels 1-272
    else if (IsChanAnalogIn(pPkt->cbpkt_header.chid))
    {
        if (m_bChannelMask[pPkt->cbpkt_header.chid - 1])
        {
            b_checkEvent = true;
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SPIKE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_SPIKE);
            if (m_pLateCallback[CBSDKCALLBACK_SPIKE])
                m_pLateCallback[CBSDKCALLBACK_SPIKE](m_nInstance, cbSdkPkt_SPIKE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SPIKE]);
        }
    }
    // catch digital input port events and save them as NSAS experiment event packets
    else if (IsChanDigin(pPkt->cbpkt_header.chid))
    {
        if (m_bChannelMask[pPkt->cbpkt_header.chid - 1])
        {
            b_checkEvent = true;
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_DIGITAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_DIGITAL);
            if (m_pLateCallback[CBSDKCALLBACK_DIGITAL])
                m_pLateCallback[CBSDKCALLBACK_DIGITAL](m_nInstance, cbSdkPkt_DIGITAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_DIGITAL]);
        }
    }
    // catch serial input port events and save them as NSAS experiment event packets
    else if (IsChanSerial(pPkt->cbpkt_header.chid))
    {
        if (m_bChannelMask[pPkt->cbpkt_header.chid - 1])
        {
            b_checkEvent = true;
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SERIAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_SERIAL);
            if (m_pLateCallback[CBSDKCALLBACK_SERIAL])
                m_pLateCallback[CBSDKCALLBACK_SERIAL](m_nInstance, cbSdkPkt_SERIAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SERIAL]);
        }
    }

    // save the timestamp to overcome the case where the reset button is pressed
    // (or recording started) which resets the timestamp to 0, but Central doesn't
    // reset its timer to 0 for cbGetSystemClockTime
    m_uCbsdkTime = pPkt->cbpkt_header.time;

    // Process continuous data if we're within a trial...
    if (pPkt->cbpkt_header.chid == 0)
        OnPktGroup(reinterpret_cast<const cbPKT_GROUP*>(pPkt));

    // and only look at event data packets
    if (b_checkEvent)
        OnPktEvent(pPkt);
}

// Author & Date: Ehsan Azar       29 April 2012
/// Network events
void SdkApp::OnInstNetworkEvent(const NetEventType type, const unsigned int code)
{
    cbSdkPktLostEvent lostEvent;
    switch (type)
    {
    case NET_EVENT_INSTINFO:
        m_connectLock.lock();
        m_connectWait.notify_all();
        m_connectLock.unlock();
        InstInfoEvent(m_instInfo);
        break;
    case NET_EVENT_CLOSE:
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.notify_all();
        m_connectLock.unlock();
        InstInfoEvent(m_instInfo);
        break;
    case NET_EVENT_CBERR:
        m_lastCbErr = code;
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.notify_all();
        m_connectLock.unlock();
        break;
    case NET_EVENT_NETOPENERR:
        m_lastCbErr = code;
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.notify_all();
        m_connectLock.unlock();
        lostEvent.type = CBSDKPKTLOSTEVENT_NET;
        LinkFailureEvent(lostEvent);
        break;
    case NET_EVENT_LINKFAILURE:
        lostEvent.type = CBSDKPKTLOSTEVENT_LINKFAILURE;
        LinkFailureEvent(lostEvent);
        break;
    case NET_EVENT_PCTONSPLOST:
        lostEvent.type = CBSDKPKTLOSTEVENT_PC2NSP;
        LinkFailureEvent(lostEvent);
        break;
    default:
        // Ignore other events
        break;
    }
}
