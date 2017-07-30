// =STS=> cbsdk.cpp[5021].aa03   open     SMID:3 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2016 Blackrock Microsystems
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
//    then return error code if cannot recover.
//   Only functions are exported, no data, and definitely NO classes
//
/**
* \file cbsdk.cpp
* \brief Cerebus SDK main file.
*/

#include "StdAfx.h"
#include <algorithm>  // Use C++ default min and max implementation.
#include "SdkApp.h"
#include "../CentralCommon/BmiVersion.h"
#include "cbHwlibHi.h"
#include "debugmacs.h"
#include <math.h>
#include <QCoreApplication>
#include "res/cbmex.rc2"

#ifndef WIN32
#ifndef Sleep
    #define Sleep(x) usleep((x) * 1000)
#endif
#endif

// The sdk instances
SdkApp * g_app[cbMAXOPEN] = {NULL};

// Private Qt application
namespace QAppPriv
{
    static int argc = 1;
    static char appname[] = "cbsdk.app";
    static char* argv[] = {appname, NULL};
    static QCoreApplication * pApp = NULL;
};

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
        if (lpReserved == NULL)
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

/** Called when a Sample Group packet (aka continuous data point or LPF) comes in.
*
* @param[in] pkt the sample group packet
*/
void SdkApp::OnPktGroup(const cbPKT_GROUP * const pkt)
{
    if (!m_bWithinTrial || m_CD == NULL)
        return;

    int group = pkt->type;

    /// \todo FIXME: add raw group to cbsdk
    if (group >= 6)
        return;

    // Get information about this group...
    uint32_t  period;
    uint32_t  length;
    uint32_t  list[cbNUM_ANALOG_CHANS];
    if (cbGetSampleGroupInfo(1, group, NULL, &period, &length, m_nInstance) != cbRESULT_OK)
        return;
    if (cbGetSampleGroupList(1, group, &length, list, m_nInstance) != cbRESULT_OK)
        return;

    int rate = (int)(cbSdk_TICKS_PER_SECOND / double(period) );

    bool bOverFlow = false;

    m_lockTrial.lock();
    // double check if buffer is still valid
    if (m_CD)
    {
        // Check whether our information for each channel is up-to-date...
        for(uint32_t i = 0; i < length; i++)
        {
            if (list[i] == 0 || list[i] > cbNUM_ANALOG_CHANS)
                continue;

            int ch = list[i] - 1;

            if (m_CD->write_index[ch] == m_CD->write_start_index[ch]) // New continuous channel
            {
                // Need to make sure there are no samples here yet...
                m_CD->current_sample_rates[ch] = rate;
            }

            // Check for sample size changes...
            if (m_CD->current_sample_rates[ch] != rate) // New rate for channel
            {
                m_CD->current_sample_rates[ch] = rate;
                m_CD->write_index[ch] = m_CD->write_start_index[ch];        // reset buffer to
            }

            // Add a sample...
            // If there's room for more data...
            uint32_t new_write_index = m_CD->write_index[ch] + 1;
            if (new_write_index >= m_CD->size)
                new_write_index = 0;

            if (new_write_index != m_CD->write_start_index[ch])
            {
                // Store more data
                m_CD->continuous_channel_data[ch][m_CD->write_index[ch]] = pkt->data[i];
                m_CD->write_index[ch] = new_write_index;
            }
            else if (m_bChannelMask[ch])
                bOverFlow = true;
        }
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
* Also the trial start and stop are set.
*
* @param[in] pPkt the event packet
*/
void SdkApp::OnPktEvent(const cbPKT_GENERIC * const pPkt)
{
    // check for trial beginning notification
    if (pPkt->chid == m_uTrialBeginChannel)
    {
        if ( (m_uTrialBeginMask & pPkt->data[0]) == m_uTrialBeginValue )
        {
            // reset the trial data cache if WithinTrial is currently False
            if (!m_bWithinTrial)
            {
                cbGetSystemClockTime(&m_uTrialStartTime, m_nInstance);
            }

            m_bWithinTrial = TRUE;
        }
    }

    if (m_ED && m_bWithinTrial)
    {
        bool bOverFlow = false;

        uint16_t ch = pPkt->chid - 1;
        if (pPkt->chid == MAX_CHANS_DIGITAL_IN)
            ch = cbNUM_ANALOG_CHANS;
        else if (pPkt->chid == MAX_CHANS_SERIAL)
            ch = cbNUM_ANALOG_CHANS + 1;

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
                m_ED->timestamps[ch][old_write_index] = pPkt->time;
                if (pPkt->chid == MAX_CHANS_DIGITAL_IN || pPkt->chid == MAX_CHANS_SERIAL)
                    m_ED->units[ch][old_write_index] = (uint16_t)(pPkt->data[0] & 0x0000ffff);
                else
                    m_ED->units[ch][old_write_index] = pPkt->type;
                m_ED->write_index[ch] = new_write_index;

                if (m_bPacketsEvent)
                {	

                    m_lockGetPacketsEvent.lock();
                        if (pPkt->time > m_uTrialStartTime)
                        {
                            m_bPacketsEvent = FALSE;
                            m_waitPacketsEvent.wakeAll();
                        }						
                    m_lockGetPacketsEvent.unlock();
                }

            }
            else if (m_bChannelMask[pPkt->chid - 1])
                bOverFlow = true;
        }
        m_lockTrialEvent.unlock();

        if (bOverFlow)
        {
            /// \todo trial continuous buffer overflow event
        }
    }

    // check for trial end notification
    if (pPkt->chid == m_uTrialEndChannel)
    {
        if ( (m_uTrialEndMask & pPkt->data[0]) == m_uTrialEndValue )
            m_bWithinTrial = FALSE;
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
                if (pPkt->info.flags == cbCOMMENT_FLAG_TIMESTAMP)
                {
                    m_CMT->rgba[write_index] = 0;
                    m_CMT->timestamps[write_index] = pPkt->data;
                }
                else
                {
                    m_CMT->rgba[write_index]  = pPkt->data;
                    m_CMT->timestamps[write_index] = pPkt->time;
                }

                strncpy((char *)m_CMT->comments[write_index], (const char *)(&pPkt->comment[0]), cbMAX_COMMENT);
                m_CMT->write_index = new_write_index;

                if (m_bPacketsCmt)
                {	

                    m_lockGetPacketsCmt.lock();
                        if (pPkt->time > m_uTrialStartTime)
                        {
                            m_bPacketsCmt = FALSE;
                            m_waitPacketsCmt.wakeAll();
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
                m_CMT->timestamps[write_index] = pPkt->time;

                strncpy((char *)m_CMT->comments[write_index], (const char *)(&pPkt->desc[0]), cbMAX_LOG);
                m_CMT->write_index = new_write_index;

                if (m_bPacketsCmt)
                {	

                    m_lockGetPacketsCmt.lock();
                        if (pPkt->time > m_uTrialStartTime)
                        {
                            m_bPacketsCmt = FALSE;
                            m_waitPacketsCmt.wakeAll();
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
    if (m_TR && m_bWithinTrial && m_lastPktVideoSynch.chid == cbPKTCHAN_CONFIGURATION)
    {
        uint16_t id = pPkt->nodeID; // 0-based node id
        // double check if buffer is still valid
        if (m_TR == NULL)
            return;
        uint16_t node_type = 0;
        // safety checks
        if (id >= cbMAXTRACKOBJ)
            return;
        if (cbGetTrackObj(NULL, &node_type, NULL, id + 1, m_nInstance) != cbRESULT_OK)
        {
            m_TR->node_type[id] = 0;
            m_TR->max_point_counts[id] = 0;
            return;
        }
        m_lockTrialTracking.lock();
        // New tracking data or tracking type changed
        if (node_type != m_TR->node_type[id])
        {
            cbGetTrackObj((char *)m_TR->node_name[id], &m_TR->node_type[id], &m_TR->max_point_counts[id], id + 1, m_nInstance);
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
                uint32_t write_index = m_TR->write_index[id];
                // Store more data
                m_TR->timestamps[id][write_index] = pPkt->time;
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
                        if (pPkt->time > m_uTrialStartTime)
                        {
                            m_bPacketsTrack = FALSE;
                            m_waitPacketsTrack.wakeAll();
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
* Make sure this is called before any callback invokation.
*
* @param[in] callbacktype the calback type to bind
*/
void SdkApp::LateBindCallback(cbSdkCallbackType callbacktype)
{
    if (m_pCallback[callbacktype] != m_pLateCallback[callbacktype])
    {
        m_lockCallback.lock();
        m_pLateCallback[callbacktype] = m_pCallback[callbacktype];
        m_pLateCallbackParams[callbacktype] = m_pCallbackParams[callbacktype];
        m_lockCallback.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
// Author & Date:   Ehsan Azar     29 March 2011
/** Signal packet-lost event.
* Only the first registered callback receives packet lost events.
*/
void SdkApp::LinkFailureEvent(cbSdkPktLostEvent & lost)
{
    m_lastLost = lost;
    cbSdkCallback pCallback = NULL;
    void * pCallbackParams = NULL;

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
void SdkApp::InstInfoEvent(uint32_t instInfo)
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
//        All of the SDK functions must come after this comment
/////////////////////////////////////////////////////////////////////////////

// Author & Date:   Ehsan Azar     23 Feb 2011
/** Get cbsdk version information.
*
* If library is closed only the library version is returned and warns.
* If library is open NSP versin is returned too.
*
* @param[out] version the pointer to get library version

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetVersion(cbSdkVersion *version)
{
    memset(version, 0, sizeof(cbSdkVersion));
    version->major = BMI_VERSION_MAJOR;
    version->minor = BMI_VERSION_MINOR;
    version->release = BMI_VERSION_BUILD_NUMBER;   //RELEASE;
    version->beta = 0;  //BMI_VERSION_BETA;
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


/// sdk stub for SdkApp::SdkGetVersion.
CBSDKAPI    cbSdkResult cbSdkGetVersion(uint32_t nInstance, cbSdkVersion *version)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (version == NULL)
        return CBSDKRESULT_NULLPTR;
    memset(version, 0, sizeof(cbSdkVersion));
    version->major = BMI_VERSION_MAJOR;
    version->minor = BMI_VERSION_MINOR;
    version->release = BMI_VERSION_BUILD_NUMBER;   //RELEASE;
    version->beta = 0;  //BMI_VERSION_BETA;
    version->majorp = cbVERSION_MAJOR;
    version->minorp = cbVERSION_MINOR;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_WARNCLOSED;

    return g_app[nInstance]->SdkGetVersion(version);
}

// Author & Date:   Ehsan Azar     12 April 2012
/// CCF error to CBSDK error.
cbSdkResult cbSdkErrorFromCCFError(ccf::ccfResult err)
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
*
* This is the intermediate function that passes the result to registered callback.
*/
void SdkApp::SdkAsynchCCF(const ccf::ccfResult res, LPCSTR szFileName, const cbStateCCF state, const uint32_t nProgress)
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

/// sdk stub for SdkApp::SdkAsynchCCF
void cbSdkAsynchCCF(uint32_t nInstance, const ccf::ccfResult res, LPCSTR szFileName, const cbStateCCF state, const uint32_t nProgress)
{
    if (nInstance >= cbMAXOPEN)
        return;
    if (g_app[nInstance] == NULL)
        return;
    g_app[nInstance]->SdkAsynchCCF(res, szFileName, state, nProgress);
}

// Author & Date:   Ehsan Azar     12 April 2012
/** Get CCF configuration information from file or NSP.
*
* If filename is specified then library can be closed, but warning is returned.
* If library is open and filename is NULL reads from NSP.
* Note: progress callback if required should be registered beforehand.
* Note: for threaded read make sure the provided cbSdkCCF variable lifespan covers the thread context.
*
* @param[in] szFileName		CCF filename to read (or NULL to read from NSP)
* @param[in] bConvert		If conversion is allowed
* @param[in] bSend			If should send CCF after successful read
* @param[in] bThreaded		If a thread should do the task
* @param[out] pData			The pointer to get CCF structure
* @param[out] pData->data	CCF data will be copied here
* @param[out] pData->ccfver the internal version of the original data

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkReadCCF(cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend, bool bThreaded)
{
    memset(&pData->data, 0, sizeof(pData->data));
    pData->ccfver = 0;
    if (szFileName == NULL)
    {
        // Library must be open to read from NSP
        if (m_instInfo == 0)
            return CBSDKRESULT_CLOSED;
    }
    CCFUtils config(bSend, bThreaded, &pData->data, m_pCallback[CBSDKCALLBACK_CCF] ? &cbSdkAsynchCCF : NULL, m_nInstance);
    ccf::ccfResult res = config.ReadCCF(szFileName, bConvert);
    if (res)
        return cbSdkErrorFromCCFError(res);
    pData->ccfver = config.GetInternalOriginalVersion();
    if (m_instInfo == 0)
        return CBSDKRESULT_WARNCLOSED;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetVersion
CBSDKAPI    cbSdkResult cbSdkReadCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend, bool bThreaded)
{
    if (pData == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    // TODO: make it possible to convert even with library closed
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkReadCCF(pData, szFileName, bConvert, bSend, bThreaded);
}

// Author & Date:   Ehsan Azar     12 April 2012
/** Write CCF configuration information to file or send to NSP.
*
* If filename is specified CCF is written, if library is closed it warns
* If library is open and filename is NULL sends CCF to NSP
* Note: progress callback if required should be registered beforehand
*
* @param[in] pData			The pointer to get CCF structure
* @param[in] pData->data	CCF data to use
* @param[in] szFileName		CCF filename to write to (or NULL to send to NSP)
* @param[in] bThreaded		If a thread should do the task
* @param[out] pData->ccfver Last internal version used

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkWriteCCF(cbSdkCCF * pData, const char * szFileName, bool bThreaded)
{
    pData->ccfver = 0;
    if (szFileName == NULL)
    {
        // Library must be open to write to NSP
        if (m_instInfo == 0)
            return CBSDKRESULT_CLOSED;
    }
    bool bSend = (szFileName == NULL);
    ccf::ccfResult res;
    CCFUtils config(bSend, bThreaded, &pData->data, m_pCallback[CBSDKCALLBACK_CCF] ? &cbSdkAsynchCCF : NULL, m_nInstance);
    if (bSend)
        res = config.SendCCF();
    else
        res = config.WriteCCFNoPrompt(szFileName);
    if (res)
        return cbSdkErrorFromCCFError(res);
    pData->ccfver = config.GetInternalVersion();
    if (m_instInfo == 0)
        return CBSDKRESULT_WARNCLOSED;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkWriteCCF
CBSDKAPI    cbSdkResult cbSdkWriteCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bThreaded)
{
    if (pData == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    /// \todo make it possible to convert even with library closed
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkWriteCCF(pData, szFileName, bThreaded);
}

// Author & Date:   Ehsan Azar     23 Feb 2011
/** Open cbsdk library.
*
* @param[in] conType		The requested connection type to open
* @param[in] id				Instance ID
* @param[in] con			Connection details

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkOpen(uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con)
{
    // check if the library is already open
    if (m_instInfo != 0)
        return CBSDKRESULT_WARNOPEN;

    // Some sanity checks
    if (con.szInIP == NULL || con.szInIP[0] == 0)
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
    if (con.szOutIP == NULL || con.szOutIP[0] == 0)
        con.szOutIP = cbNET_UDP_ADDR_CNT;

    // Null the trial buffers
    m_CD = NULL;
    m_ED = NULL;

    // Unregister all callbacks
    m_lockCallback.lock();
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallbackParams[i] = NULL;
        m_pCallback[i] = NULL;
    }
    m_lockCallback.unlock();

    // Unmask all channels
    for (int i = 0; i < cbMAXCHANS; ++i)
        m_bChannelMask[i] = true;

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
    m_bTrialDouble       = false;
    m_uTrialWaveforms    = 0;
    m_uTrialConts        = cbSdk_CONTINUOUS_DATA_SAMPLES;
    m_uTrialEvents       = cbSdk_EVENT_DATA_SAMPLES;
    m_uTrialComments     = 0;
    m_uTrialTrackings    = 0;

    // make sure that cache data storage is switched off so that the monitoring thread will
    // not be saving data and then set up the cache control variables
    m_bWithinTrial = FALSE;
    m_uTrialStartTime = 0;

    // If this is not part of another Qt application, and the-only Qt app intance is not present
    if (QCoreApplication::instance() == NULL && QAppPriv::pApp == NULL)
        QAppPriv::pApp = new QCoreApplication(QAppPriv::argc, QAppPriv::argv);

    if (conType == CBSDKCONNECTION_UDP)
    {
        m_connectLock.lock();
        Open(nInstance, con.nInPort, con.nOutPort, con.szInIP, con.szOutIP, con.nRecBufSize);
    }
    else if (conType == CBSDKCONNECTION_CENTRAL)
    {
        m_connectLock.lock();
        Open(nInstance);
    }
    else
        return CBSDKRESULT_NOTIMPLEMENTED;

    // Wait for (dis)connection to happen
    bool bWait = m_connectWait.wait(&m_connectLock, 15000);
    m_connectLock.unlock();
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
            }
            return sdkres;
        }
    }
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkOpen
CBSDKAPI    cbSdkResult cbSdkOpen(uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con)
{
    // check if the library is already open
    if (conType < 0 || conType >= CBSDKCONNECTION_CLOSED)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
    {
        try {
            g_app[nInstance] = new SdkApp();
        } catch (...) {
            g_app[nInstance] = NULL;
        }
    }
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_ERRMEMORY;

    return g_app[nInstance]->SdkOpen(nInstance, conType, con);
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Close cbsdk library.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::SdkClose()
{
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    if (m_instInfo == 0)
        res = CBSDKRESULT_WARNCLOSED;

    // clear the data cache if it exists
    // first disable writing to the cache
    m_bWithinTrial = FALSE;

    if (m_CD != NULL)
        SdkUnsetTrialConfig(CBSDKTRIAL_CONTINUOUS);

    if (m_ED != NULL)
        SdkUnsetTrialConfig(CBSDKTRIAL_EVENTS);

    if (m_CMT != NULL)
        SdkUnsetTrialConfig(CBSDKTRIAL_COMMETNS);

    if (m_TR != NULL)
        SdkUnsetTrialConfig(CBSDKTRIAL_TRACKING);

    // Unregister all callbacks
    m_lockCallback.lock();
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallbackParams[i] = NULL;
        m_pCallback[i] = NULL;
    }
    m_lockCallback.unlock();

    // Close the app
    Close();

    return res;
}

/// sdk stub for SdkApp::SdkClose
CBSDKAPI    cbSdkResult cbSdkClose(uint32_t nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_WARNCLOSED;

    cbSdkResult res = g_app[nInstance]->SdkClose();

    // Delete this instance
    delete g_app[nInstance];
    g_app[nInstance] = NULL;

    /// \todo see if this is necessary and useful before removing
#if 0
    // Close application if this is the last
    if (QAppPriv::pApp)
    {
        bool bLastInstance = false;
        for (int i = 0; i < cbMAXOPEN; ++i)
        {
            if (g_app[i] != NULL)
            {
                bLastInstance = true;
                break;
            }
        }
        if (bLastInstance)
        {
            delete QAppPriv::pApp;
            QAppPriv::pApp = NULL;
        }
    }
#endif

    return res;
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Get cbsdk connection and instrument type.
*
* @param[out] conType	The actual connection type
* @param[out] instType	The instrument type

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetType(cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
{
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

/// sdk stub for SdkApp::SdkGetType
CBSDKAPI    cbSdkResult cbSdkGetType(uint32_t nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
{
    if (instType == NULL && conType == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetType(conType, instType);
}

// Author & Date:   Ehsan Azar     25 Oct 2011
/** Internal lock-less function to deallocate given trial construct.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::unsetTrialConfig(cbSdkTrialType type)
{
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    switch (type)
    {
    case CBSDKTRIAL_CONTINUOUS:
        if (m_CD == NULL)
            return CBSDKRESULT_ERRCONFIG;
        for (uint32_t i = 0; i < cbNUM_ANALOG_CHANS; ++i)
        {
            if (m_CD->continuous_channel_data[i])
            {
                delete[] m_CD->continuous_channel_data[i];
                m_CD->continuous_channel_data[i] = NULL;
            }
        }
        m_CD->size = 0;
        delete m_CD;
        m_CD = NULL;
        break;
    case CBSDKTRIAL_EVENTS:
        if (m_ED == NULL)
            return CBSDKRESULT_ERRCONFIG;
        for (uint32_t i = 0; i < cbNUM_ANALOG_CHANS + 2; ++i)
        {
            if (m_ED->timestamps[i])
            {
                delete[] m_ED->timestamps[i];
                m_ED->timestamps[i] = NULL;
            }
            if (m_ED->units[i])
            {
                delete[] m_ED->units[i];
                m_ED->units[i] = NULL;
            }
        }
        if (m_ED->waveform_data != NULL)
        {
            if (m_uTrialWaveforms > cbPKT_SPKCACHEPKTCNT)
                delete[] m_ED->waveform_data;
            m_ED->waveform_data = NULL;
        }
        m_ED->size = 0;
        delete m_ED;
        m_ED = NULL;
        break;
    case CBSDKTRIAL_COMMETNS:
        if (m_CMT == NULL)
            return CBSDKRESULT_ERRCONFIG;
        if (m_CMT->timestamps)
        {
            delete[] m_CMT->timestamps;
            m_CMT->timestamps = NULL;
        }
        if (m_CMT->rgba)
        {
            delete[] m_CMT->rgba;
            m_CMT->rgba = NULL;
        }
        if (m_CMT->charset)
        {
            delete[] m_CMT->charset;
            m_CMT->charset = NULL;
        }
        if (m_CMT->comments)
        {
            for (uint32_t i = 0; i < m_CMT->size; ++i)
            {
                if (m_CMT->comments[i])
                {
                    delete[] m_CMT->comments[i];
                    m_CMT->comments[i] = NULL;
                }
            }
            delete[] m_CMT->comments;
            m_CMT->comments = NULL;
        }
        m_CMT->size = 0;
        delete m_CMT;
        m_CMT = NULL;
        break;
    case CBSDKTRIAL_TRACKING:
        if (m_TR == NULL)
            return CBSDKRESULT_ERRCONFIG;
        for (uint32_t i = 0; i < cbMAXTRACKOBJ; ++i)
        {
            if (m_TR->timestamps[i])
            {
                delete[] m_TR->timestamps[i];
                m_TR->timestamps[i] = NULL;
            }
            if (m_TR->synch_timestamps[i])
            {
                delete[] m_TR->synch_timestamps[i];
                m_TR->synch_timestamps[i] = NULL;
            }
            if (m_TR->synch_frame_numbers[i])
            {
                delete[] m_TR->synch_frame_numbers[i];
                m_TR->synch_frame_numbers[i] = NULL;
            }
            if (m_TR->point_counts[i])
            {
                delete[] m_TR->point_counts[i];
                m_TR->point_counts[i] = NULL;
            }
            if (m_TR->coords[i])
            {
                for (uint32_t j = 0; j < m_TR->size; ++j)
                {
                    if (m_TR->coords[i][j])
                    {
                        delete[] (uint16_t *)m_TR->coords[i][j];
                        m_TR->coords[i][j] = NULL;
                    }
                }
                delete[] m_TR->coords[i];
                m_TR->coords[i] = NULL;
            }
        } // end for (uint32_t i = 0
        m_TR->size = 0;
        delete m_TR;
        m_TR = NULL;
        break;
    }
    return res;
}

// Author & Date:   Ehsan Azar     25 Oct 2011
/** Deallocate given trial construct.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::SdkUnsetTrialConfig(cbSdkTrialType type)
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
    case CBSDKTRIAL_COMMETNS:
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

/// sdk stub for SdkApp::SdkUnsetTrialConfig
CBSDKAPI    cbSdkResult cbSdkUnsetTrialConfig(uint32_t nInstance, cbSdkTrialType type)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkUnsetTrialConfig(type);
}

// Author & Date:   Ehsan Azar     24 Feb 2011
/** Get time from instrument.
*
* @param[out] cbtime the clock time of the instrument

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetTime(uint32_t * cbtime)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    *cbtime = m_uCbsdkTime;
    if (cbGetSystemClockTime(cbtime, m_nInstance) != cbRESULT_OK)
        return CBSDKRESULT_CLOSED;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetTime
CBSDKAPI    cbSdkResult cbSdkGetTime(uint32_t nInstance, uint32_t * cbtime)
{
    if (cbtime == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTime(cbtime);
}

// Author & Date:   Ehsan Azar     29 April 2012
/** Get direct access to spike cache shared memory.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::SdkGetSpkCache(uint16_t channel, cbSPKCACHE **cache)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT res = cbGetSpkCache(channel, cache, m_nInstance);
    if (res != cbRESULT_OK)
        return CBSDKRESULT_CLOSED;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkkGetSpkCache
CBSDKAPI    cbSdkResult cbSdkGetSpkCache(uint32_t nInstance, uint16_t channel, cbSPKCACHE **cache)
{
    if (cache == NULL)
        return CBSDKRESULT_NULLPTR;
    if (*cache == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSpkCache(channel, cache);
}

// Author & Date:   Ehsan Azar     25 June 2012
/** Get information about configured data collection trial and its active status.
*
* @param[in,out]	pbActive	pointer to get active status
* @param[in,out]	pBegchan	pointer (if non-NULL) to get start channel
* @param[in,out]	pBegmask	pointer (if non-NULL) to get mask for start value
* @param[in,out]	pBegval		pointer (if non-NULL) to get start value
* @param[in,out]	pEndchan	pointer (if non-NULL) to get last channel
* @param[in,out]	pEndmask	pointer (if non-NULL) to get mask for end value
* @param[in,out]	pEndval		pointer (if non-NULL) to get end value
* @param[in,out]	pbDouble	pointer (if non-NULL) to get if data array is double precision
* @param[in,out]	puWaveforms pointer (if non-NULL) to get number of spike waveform to buffer
* @param[in,out]	puConts     pointer (if non-NULL) to get number of continuous data to buffer
* @param[in,out]	puEvents    pointer (if non-NULL) to get number of events to buffer
* @param[in,out]	puComments  pointer (if non-NULL) to get number of comments to buffer
* @param[in,out]	puTrackings pointer (if non-NULL) to get number of tracking data to buffer
* @param[in,out]	pbAbsolute  pointer (if non-NULL) to get if timing is absolute

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetTrialConfig(uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                      uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval, bool * pbDouble,
                                      uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                      uint32_t * puComments, uint32_t * puTrackings, bool * pbAbsolute)
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
    if (pbDouble)
        *pbDouble = m_bTrialDouble;
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
    if (pbAbsolute)
        *pbAbsolute = m_bTrialAbsolute;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetTrialConfig
CBSDKAPI    cbSdkResult cbSdkGetTrialConfig(uint32_t nInstance,
                                            uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                            uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval, bool * pbDouble,
                                            uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                            uint32_t * puComments, uint32_t * puTrackings, bool * pbAbsolute)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTrialConfig(pbActive, pBegchan, pBegmask, pBegval,
                                               pEndchan, pEndmask, pEndval, pbDouble,
                                               puWaveforms, puConts, puEvents,
                                               puComments, puTrackings, pbAbsolute);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Allocate buffers and start a data collection trial.
*
* @param[in]	bActive		Start if true, stop otherwise
* @param[in]	begchan		Start channel
* @param[in]	begmask		Mask for start value
* @param[in]	begval		Start value
* @param[in]	endchan		Last channel
* @param[in]	endmask		Mask for end value
* @param[in]	endval		End value
* @param[in]	bDouble		If data array is double precision
* @param[in]	uWaveforms  Number of spike waveform to buffer
* @param[in]	uConts      Number of continuous data to buffer
* @param[in]	uEvents     Number of events to buffer
* @param[in]	uComments   Number of comments to buffer
* @param[in]	uTrackings  Number of tracking data to buffer
* @param[in]	bAbsolute   If event timing is absolute or relative to trial

* \n This function returns the error code.
*/
cbSdkResult SdkApp::SdkSetTrialConfig(uint32_t bActive, uint16_t begchan, uint32_t begmask, uint32_t begval,
                                      uint16_t endchan, uint32_t endmask, uint32_t endval, bool bDouble,
                                      uint32_t uWaveforms, uint32_t uConts, uint32_t uEvents, uint32_t uComments, uint32_t uTrackings,
                                      bool bAbsolute)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // load the values into the global trial control variables for use by the
    // real-time data monitoring thread.. Trim them to 8-bit and 16-bit values
    m_uTrialBeginChannel = begchan;
    m_uTrialBeginMask    = begmask;
    m_uTrialBeginValue   = begval;
    m_uTrialEndChannel   = endchan;
    m_uTrialEndMask      = endmask;
    m_uTrialEndValue     = endval;
    m_bTrialDouble       = bDouble;
    m_uTrialWaveforms    = uWaveforms;
    m_bTrialAbsolute     = bAbsolute;

    if (uConts && m_CD == NULL)
    {
        m_lockTrial.lock();
        try {
            m_CD = new ContinuousData;
        } catch (...) {
            m_CD = NULL;
        }
        if (m_CD)
        {
            memset(m_CD->continuous_channel_data, 0, sizeof(m_CD->continuous_channel_data));
            m_CD->size = uConts;
            bool bErr = false;
            try {
                for (uint32_t i = 0; i < cbNUM_ANALOG_CHANS; ++i)
                {
                    m_CD->continuous_channel_data[i] = new int16_t[m_CD->size];
                    if (m_CD->continuous_channel_data[i] == NULL)
                    {
                        bErr = true;
                        break;
                    }
                }
            } catch (...) {
                bErr = true;
            }
            if (bErr)
                unsetTrialConfig(CBSDKTRIAL_CONTINUOUS);
        }
        if (m_CD) m_CD->reset();
        m_lockTrial.unlock();
        if (m_CD == NULL)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialConts = uConts;
    }

    if (uEvents && m_ED == NULL)
    {
        m_lockTrialEvent.lock();
        try {
            m_ED = new EventData;
        } catch (...) {
            m_ED = NULL;
        }
        if (m_ED)
        {
            memset(m_ED->timestamps, 0, sizeof(m_ED->timestamps));
            memset(m_ED->units, 0, sizeof(m_ED->units));
            m_ED->size = uEvents;
            bool bErr = false;
            try {
                for (uint32_t i = 0; i < cbNUM_ANALOG_CHANS + 2; ++i)
                {
                    m_ED->timestamps[i] = new uint32_t[m_ED->size];
                    m_ED->units[i] = new uint16_t[m_ED->size];
                    if (m_ED->timestamps[i] == NULL || m_ED->units[i] == NULL)
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
        if (m_ED == NULL)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialEvents = uEvents;
    }

    if (uComments && m_CMT == NULL)
    {
        m_lockTrialComment.lock();
        try {
            m_CMT = new CommentData;
        } catch (...) {
            m_CMT = NULL;
        }
        if (m_CMT)
        {
            m_CMT->size = uComments;
            bool bErr = false;
            m_CMT->timestamps = new uint32_t[m_CMT->size];
            m_CMT->rgba = new uint32_t[m_CMT->size];
            m_CMT->charset = new uint8_t[m_CMT->size];
            m_CMT->comments = new uint8_t * [m_CMT->size]; // uint8_t * array[m_CMT->size]
            if (m_CMT->timestamps == NULL || m_CMT->rgba == NULL || m_CMT->charset == NULL || m_CMT->comments == NULL)
                bErr = true;
            try {
                if (!bErr)
                {
                    for (uint32_t i = 0; i < m_CMT->size; ++i)
                    {
                        m_CMT->comments[i] = new uint8_t[cbMAX_COMMENT + 1];
                        if (m_CMT->comments[i] == NULL)
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
                unsetTrialConfig(CBSDKTRIAL_COMMETNS);
        }
        if (m_CMT) m_CMT->reset();
        m_lockTrialComment.unlock();
        if (m_CMT == NULL)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialComments     = uComments;
    }

    if (uTrackings && m_TR == NULL)
    {
        m_lockTrialTracking.lock();
        try {
            m_TR = new TrackingData;
        } catch (...) {
            m_TR = NULL;
        }
        if (m_TR)
        {
            m_TR->size = uTrackings;
            bool bErr = false;
            for (uint32_t i = 0; i < cbMAXTRACKOBJ; ++i)
            {
                try {
                    m_TR->timestamps[i] = new uint32_t[m_TR->size];
                    m_TR->synch_timestamps[i] = new uint32_t[m_TR->size];
                    m_TR->synch_frame_numbers[i] = new uint32_t[m_TR->size];
                    m_TR->point_counts[i] = new uint16_t[m_TR->size];
                    m_TR->coords[i] = new void * [m_TR->size];

                    if (m_TR->timestamps[i] == NULL || m_TR->synch_timestamps[i] == NULL || m_TR->synch_frame_numbers[i] == NULL ||
                            m_TR->point_counts[i]== NULL || m_TR->coords[i] == NULL)
                        bErr = true;

                    if (!bErr)
                    {
                        for (uint32_t j = 0; j < m_TR->size; ++j)
                        {
                            // This is equivalant to uint32_t[cbMAX_TRACKCOORDS/2] used for word-size union
                            m_TR->coords[i][j] = new uint16_t[cbMAX_TRACKCOORDS];
                            if (m_TR->coords[i][j] == NULL)
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
        if (m_TR == NULL)
            return CBSDKRESULT_ERRMEMORYTRIAL;
        m_uTrialTrackings    = uTrackings;
    }

    if (m_ED)
    {
        if (uWaveforms && m_ED->waveform_data == NULL)
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
    if (bActive == FALSE)
    {
        m_bWithinTrial = FALSE;
    }
    else
    { // otherwise set the status to true
        // reset the trial data cache if WithinTrial is currently False
        if (!m_bWithinTrial)
        {
            cbGetSystemClockTime(&m_uTrialStartTime, m_nInstance);

            if (m_CD)
            {
                // Clear continuous data array
                m_lockTrial.lock();
                memset(m_CD->write_index, 0, sizeof(m_CD->write_index));
                memset(m_CD->write_start_index, 0, sizeof(m_CD->write_start_index));
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
        m_bWithinTrial = TRUE;
    }

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetTrialConfig
CBSDKAPI    cbSdkResult cbSdkSetTrialConfig(uint32_t nInstance,
                                            uint32_t bActive, uint16_t begchan, uint32_t begmask, uint32_t begval,
                                            uint16_t endchan, uint32_t endmask, uint32_t endval, bool bDouble,
                                            uint32_t uWaveforms, uint32_t uConts, uint32_t uEvents, uint32_t uComments, uint32_t uTrackings,
                                            bool bAbsolute)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetTrialConfig(bActive, begchan, begmask, begval,
                                               endchan, endmask, endval, bDouble,
                                               uWaveforms, uConts, uEvents, uComments, uTrackings,
                                               bAbsolute);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Get channel label for a given channel
*
* @param[in]	channel					the channel number (1-based)
* @param[out]	bValid[6]				spike and Hoops validity (NULL means ignore)
* @param[out]	label[cbLEN_STR_LABEL]  channel label
* @param[out]	userflags				channel user flag
* @param[out]	position[4]				channel position
*/
cbSdkResult SdkApp::SdkGetChannelLabel(uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    if (label) memset(label, 0, cbLEN_STR_LABEL);
    cbRESULT cbres = cbGetChanLabel(channel, label, userflags, position, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    if (bValid)
    {
        for (int i = 0; i < 6; ++i)
            bValid[i] = 0;
        cbHOOP hoops[cbMAXUNITS][cbMAXHOOPS];
        if (channel <= cbNUM_ANALOG_CHANS)
        {
            cbGetAinpSpikeHoops(channel, &hoops[0][0], m_nInstance);
            bValid[0] = IsSpikeProcessingEnabled(channel);
            for (int i = 0; i < cbMAXUNITS; ++i)
                bValid[i + 1] = hoops[i][0].valid;
        }
        else if ( (channel == MAX_CHANS_DIGITAL_IN) || (channel == MAX_CHANS_SERIAL) )
        {
            uint32_t options;
            cbGetDinpOptions(channel, &options, NULL, m_nInstance);
            bValid[0] = (options != 0);
        }
    }
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetChannelLabel
CBSDKAPI    cbSdkResult cbSdkGetChannelLabel(uint32_t nInstance,
                                             uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (label == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetChannelLabel(channel, bValid, label, userflags, position);
}

// Author & Date:   Ehsan Azar     21 March 2011
/** Set channel label for a given channel.
*
* @param[in]	channel   the channel number (1-based)
* @param[in]	label     new channel label
* @param[in]	userflags channel user flag
* @param[in]	position  position of the channel (NULL means ignore)

* \n This function returns the error code
*/

cbSdkResult SdkApp::SdkSetChannelLabel(uint16_t channel, const char * label, uint32_t userflags, int32_t * position)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbSetChanLabel(channel, label, userflags, position, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetChannelLabel
CBSDKAPI    cbSdkResult cbSdkSetChannelLabel(uint32_t nInstance,
                                             uint16_t channel, const char * label, uint32_t userflags, int32_t * position)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (label == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetChannelLabel(channel, label, userflags, position);
}

// Author & Date:   Ehsan Azar     11 March 2011
/** Retrieve data of a configured trial.
*
* @param[in]	bActive						if should reset buffer
* @param[in]	trialevent->num_samples		requested number of event samples
* @param[in]	trialcont->num_samples		requested number of continuous samples
* @param[in]	trialcomment->num_samples	requested number of comment samples
* @param[in]	trialtracking->num_samples  requested number of tracking samples
* @param[out]	trialevent->num_samples		retrieved number of events
* @param[out]	trialevent->timestamps		timestamps for events
* @param[out]	trialevent->waveforms		waveform or digital data
* @param[out]	trialcont->num_samples		retrieved number of continuous samples
* @param[out]	trialcont->time				start time for retrieved continuous samples
* @param[out]	trialcont->samples			continuous samples
* @param[out]	trialcomment->num_samples	retrieved number of comments samples
* @param[out]	trialcomment->timestamps    timestamps for comments
* @param[out]	trialcomment->rgbas         rgba for comments
* @param[out]	trialcomment->charsets      character set for comments
* @param[out]	trialcomment->comments      pointer to the comments
* @param[out]	trialtracking->num_samples  retrieved number of tracking samples
* @param[out]	trialtracking->synch_frame_numbers   retrieved frame numbers
* @param[out]	trialtracking->synch_timestamps      retrieved synchronized timesamps
* @param[out]	trialtracking->timestamps      timestamps for tracking
* @param[out]	trialtracking->coords          tracking coordinates

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetTrialData(uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                      cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;


    uint32_t prevStartTime = m_uPrevTrialStartTime;

    if (trialcont)
    {
        uint32_t read_end_index[cbNUM_ANALOG_CHANS];
        uint32_t read_start_index[cbNUM_ANALOG_CHANS];
        if (m_CD == NULL)
            return CBSDKRESULT_ERRCONFIG;
        trialcont->time = prevStartTime;
        // Take a snashot
        memcpy(read_start_index, m_CD->write_start_index, sizeof(read_start_index));
        m_lockTrial.lock();
        memcpy(read_end_index, m_CD->write_index, sizeof(read_end_index));
        m_lockTrial.unlock();

        // copy the data from the "cache" to the allocated memory.
        for (uint32_t channel = 0; channel < trialcont->count; channel++)
        {
            uint16_t ch = trialcont->chan[channel]; // channel number (index + 1 in cache)
            if (ch == 0 || ch > cbNUM_ANALOG_CHANS)
                return CBSDKRESULT_INVALIDCHANNEL;
            // Ignore masked channels
            if (!m_bChannelMask[ch - 1])
            {
                trialcont->num_samples[channel] = 0;
                continue;
            }

            uint32_t read_index = m_CD->write_start_index[ch - 1];
            int num_samples = read_end_index[ch - 1] - read_index;
            if (num_samples < 0)
                num_samples += m_CD->size;
            // See which one finishes first
            num_samples = std::min((uint32_t)num_samples, trialcont->num_samples[channel]);
            // retrieved number of samples
            trialcont->num_samples[channel] = num_samples;

            void * dataptr = trialcont->samples[channel];
            // Null means ignore
            if (dataptr)
            {
                for (int i = 0; i < num_samples; ++i)
                {
                    if (m_bTrialDouble)
                        *((double *)dataptr + i) = m_CD->continuous_channel_data[ch - 1][read_index];
                    else
                        *((int16_t *)dataptr + i) = m_CD->continuous_channel_data[ch - 1][read_index];

                    read_index++;
                    if (read_index >= m_CD->size)
                        read_index = 0;
                }
            }
            // Flush the buffer and start a new 'trial'...
            if (bActive)
                read_start_index[ch - 1] = read_index;
        }
        if (bActive)
        {
            m_lockTrial.lock();
            memcpy(m_CD->write_start_index, read_start_index, sizeof(m_CD->write_start_index));
            m_lockTrial.unlock();
        }
    }

    if (trialevent)
    {
        uint32_t read_end_index[cbNUM_ANALOG_CHANS + 2];
        uint32_t read_start_index[cbNUM_ANALOG_CHANS + 2];
        if (m_ED == NULL)
            return CBSDKRESULT_ERRCONFIG;
        // Take a snashot
        memcpy(read_start_index, m_ED->write_start_index, sizeof(read_start_index));
        m_lockTrialEvent.lock();
        memcpy(read_end_index, m_ED->write_index, sizeof(read_end_index));
        m_lockTrialEvent.unlock();

        // copy the data from the "cache" to the allocated memory.
        for (uint32_t channel = 0; channel < trialevent->count; channel++)
        {
            uint16_t ch = trialevent->chan[channel]; // channel number
            if (ch == MAX_CHANS_DIGITAL_IN)
                ch = cbNUM_ANALOG_CHANS + 1; //index + 1 in cache
            else if (ch == MAX_CHANS_SERIAL)
                ch = cbNUM_ANALOG_CHANS + 2; //index + 1 in cache
            if (ch == 0 || (ch > cbNUM_ANALOG_CHANS + 2))
                return CBSDKRESULT_INVALIDCHANNEL;
            // Ignore masked channels
            if (!m_bChannelMask[trialevent->chan[channel] - 1])
            {
                memset(trialevent->num_samples[channel], 0, sizeof(trialevent->num_samples[channel]));
                continue;
            }

            uint32_t read_index = m_ED->write_start_index[ch - 1];
            int num_samples = read_end_index[ch - 1] - read_index;
            if (num_samples < 0)
                num_samples += m_ED->size;

            uint32_t num_samples_unit[cbMAXUNITS + 1];
            memset(num_samples_unit, 0, sizeof(num_samples_unit));

            for (int i = 0; i < num_samples; ++i)
            {
                uint16_t unit = m_ED->units[ch - 1][read_index];
                if (unit <= cbMAXUNITS + 1)     // remove noise unit
                {
                    // Digital or serial data
                    if (ch > cbNUM_ANALOG_CHANS)
                    {
                        if (num_samples_unit[0] < trialevent->num_samples[channel][0])
                        {
                            void * dataptr = trialevent->waveforms[channel];
                            // Null means ignore
                            if (dataptr)
                            {
                                if (m_bTrialDouble)
                                    *((double *)dataptr + num_samples_unit[0]) = unit;
                                else
                                    *((uint16_t *)dataptr + num_samples_unit[0]) = unit;
                            }
                        }
                        unit = 0;
                    }
                    // Timestamps
                    if (num_samples_unit[unit] < trialevent->num_samples[channel][unit])
                    {
                        void * dataptr = trialevent->timestamps[channel][unit];
                        // Null means ignore
                        if (dataptr)
                        {
                            uint32_t ts = m_ED->timestamps[ch - 1][read_index];
                            // If time wraps or due to reset, time will restart amidst trial
                            if (!m_bTrialAbsolute && ts >= prevStartTime)
                                ts -= prevStartTime;
                            if (m_bTrialDouble)
                                *((double *)dataptr + num_samples_unit[unit]) = cbSdk_SECONDS_PER_TICK * ts;
                            else
                                *((uint32_t *)dataptr + num_samples_unit[unit]) = ts;
                        }
                        num_samples_unit[unit]++;
                    } 
                }

                read_index++;
                if (read_index >= m_ED->size)
                    read_index = 0;
            }
            // retrieved number of samples
            memcpy(trialevent->num_samples[channel], num_samples_unit, sizeof(num_samples_unit));
            // Flush the buffer and start a new 'trial'...
            if (bActive)
                read_start_index[ch - 1] = read_index;
        }
        if (bActive)
        {
            m_lockTrialEvent.lock();
            memcpy(m_ED->write_start_index, read_start_index, sizeof(m_ED->write_start_index));
            m_lockTrialEvent.unlock();
        }
    }

    if (trialcomment)
    {
        uint32_t read_end_index;
        uint32_t read_start_index;
        if (m_CMT == NULL)
            return CBSDKRESULT_ERRCONFIG;
        // Take a snashot
        read_start_index = m_CMT->write_start_index;
        m_lockTrialComment.lock();
        read_end_index = m_CMT->write_index;
        m_lockTrialComment.unlock();

        // copy the data from the "cache" to the allocated memory.
        uint32_t read_index = m_CMT->write_start_index;
        int num_samples = read_end_index - read_index;
        if (num_samples < 0)
            num_samples += m_CMT->size;
        // See which one finishes first
        num_samples = std::min((uint16_t)num_samples, trialcomment->num_samples);
        // retrieved number of samples
        trialcomment->num_samples = num_samples;

        for (int i = 0; i < num_samples; ++i)
        {
            void * dataptr = trialcomment->timestamps;
            // Null means ignore
            if (dataptr)
            {
                uint32_t ts = m_CMT->timestamps[read_index];
                // If time wraps or due to reset, time will restart amidst trial
                if (!m_bTrialAbsolute && ts >= prevStartTime)
                    ts -= prevStartTime;
                if (m_bTrialDouble)
                    *((double *)dataptr + i) = cbSdk_SECONDS_PER_TICK * ts;
                else
                    *((uint32_t *)dataptr + i) = ts;
            }
            dataptr = trialcomment->rgbas;
            if (dataptr)
                *((uint32_t *)dataptr + i) = m_CMT->rgba[read_index];
            dataptr = trialcomment->charsets;
            if (dataptr)
                *((uint8_t *)dataptr + i) = m_CMT->charset[read_index];

            // Must take a copy because it might get overridden
            if (trialcomment->comments != NULL && trialcomment->comments[i] != NULL)
                strncpy((char *)trialcomment->comments[i], (const char *)m_CMT->comments[read_index], cbMAX_COMMENT);

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
        if (m_TR == NULL)
            return CBSDKRESULT_ERRCONFIG;
        // Take a snashot
        memcpy(read_start_index, m_TR->write_start_index, sizeof(read_start_index));
        m_lockTrialTracking.lock();
        memcpy(read_end_index, m_TR->write_index, sizeof(read_end_index));
        m_lockTrialTracking.unlock();

        // copy the data from the "cache" to the allocated memory.
        for (uint16_t idx = 0; idx < trialtracking->count; ++idx)
        {
            uint16_t id = trialtracking->ids[idx]; // actual ID

            uint32_t read_index = m_TR->write_start_index[id];
            int num_samples = read_end_index[id] - read_index;
            if (num_samples < 0)
                num_samples += m_TR->size;
            // See which one finishes first
            num_samples = std::min((uint16_t)num_samples, trialtracking->num_samples[id]);
            // retrieved number of samples
            trialtracking->num_samples[id] = num_samples;

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
                    void * dataptr = trialtracking->timestamps[id];
                    // Null means ignore
                    if (dataptr)
                    {
                        uint32_t ts = m_TR->timestamps[id][read_index];
                        // If time wraps or due to reset, time will restart amidst trial
                        if (!m_bTrialAbsolute && ts >= prevStartTime)
                            ts -= prevStartTime;
                        if (m_bTrialDouble)
                            *((double *)dataptr + i) = cbSdk_SECONDS_PER_TICK * ts;
                        else
                            *((uint32_t *)dataptr + i) = ts;
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
                            uint32_t * dataptr = (uint32_t *) trialtracking->coords[id][i];
                            if (dataptr)
                                memcpy(dataptr, m_TR->coords[id][read_index], pointCount * dim_count * sizeof(uint32_t));
                        }
                        else
                        {
                            uint16_t * dataptr = (uint16_t *) trialtracking->coords[id][i];
                            if (dataptr)
                                memcpy(dataptr, m_TR->coords[id][read_index], pointCount * dim_count * sizeof(uint16_t));
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
            memcpy(m_TR->write_start_index, read_start_index, sizeof(m_TR->write_start_index));
            m_lockTrialTracking.unlock();
        }
    }

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetTrialData
CBSDKAPI    cbSdkResult cbSdkGetTrialData(uint32_t nInstance,
                                          uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                          cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetTrialData(bActive, trialevent, trialcont, trialcomment, trialtracking);
}

// Author & Date:   Ehsan Azar     22 March 2011
/** Initialize the structures.
*
* Zero all the buffers.
* Fill the current number of samples.
* It can be called before data retrieval (cbSdkGetTrialData) to fill with latest number of samples.
* Note: No allocation is performed here, buffer pointers must be set to appropriate allocated buffers after a call to this function
*
* @param[out]	trialevent    - initialize channel count, channels, and number of buffered samples for each channel
* @param[out]	trialcont     - initialize channel count, channels, sample rate and number of buffered samples for each channel
* @param[out]	trialcomment  - initialize number of buffered comments
* @param[out]	trialtracking - initialize trackable count, trackable name, id, type,
* maximum point count and number of buffered samples for each trackable

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkInitTrialData(uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                     cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    // This time is used for relative timings,
    //  continuous as well as event relative timings reset any time bActive is set
    m_uPrevTrialStartTime = m_uTrialStartTime;
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
            if (m_ED == NULL)
                return CBSDKRESULT_ERRCONFIG;
            // Wait for packets to come in
            m_lockGetPacketsEvent.lock();
            m_bPacketsEvent = TRUE;
            m_lockGetPacketsEvent.unlock();

            uint32_t read_end_index[cbNUM_ANALOG_CHANS + 2];
            // Take a snapshot of the current write pointer
            m_lockTrialEvent.lock();
            memcpy(read_end_index, m_ED->write_index, sizeof(read_end_index));
            m_lockTrialEvent.unlock();
            int count = 0;
            for (uint32_t channel = 0; channel < cbNUM_ANALOG_CHANS + 2; channel++)
            {
                uint32_t i = m_ED->write_start_index[channel];
                int num_samples = read_end_index[channel] - i;
                if (num_samples < 0)
                    num_samples += m_ED->size;
                if (num_samples == 0)
                    continue;
                uint16_t ch = channel + 1; // Actual channel number
                if (ch > cbNUM_ANALOG_CHANS)
                    ch = (ch - cbNUM_ANALOG_CHANS + 150);
                if (!m_bChannelMask[ch - 1])
                    continue;
                trialevent->chan[count] = ch;
                // Count sample numbers for each unit seperately
                while (i != read_end_index[channel])
                {
                    uint16_t unit = m_ED->units[channel][i];
                    if (unit > cbMAXUNITS || channel >= cbNUM_ANALOG_CHANS)
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
        memset(trialcont->num_samples, 0, sizeof(trialcont->num_samples));
        if (m_instInfo == 0)
        {
            memset(trialcont->chan, 0, sizeof(trialcont->chan));
            memset(trialcont->sample_rates, 0, sizeof(trialcont->samples));
            return CBSDKRESULT_WARNCLOSED;
        }
        else
        {
            if (m_CD == NULL)
                return CBSDKRESULT_ERRCONFIG;
            uint32_t read_end_index[cbNUM_ANALOG_CHANS];
            // Take a snapshot of the current write pointer
            m_lockTrial.lock();
            memcpy(read_end_index, m_CD->write_index, sizeof(read_end_index));
            m_lockTrial.unlock();
            int count = 0;
            for (uint32_t channel = 0; channel < cbNUM_ANALOG_CHANS; channel++)
            {
                int num_samples = read_end_index[channel] - m_CD->write_start_index[channel];
                if (num_samples < 0)
                    num_samples += m_CD->size;
                if (num_samples && m_bChannelMask[channel])
                {
                    trialcont->chan[count] = channel + 1; // Actual channel number
                    trialcont->num_samples[count] = num_samples;
                    trialcont->sample_rates[count] = m_CD->current_sample_rates[channel];
                    count++;
                }
            }
            trialcont->count = count;
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
            if (m_CMT == NULL)
                return CBSDKRESULT_ERRCONFIG;

            // Wait for packets to come in
            m_lockGetPacketsCmt.lock();
            m_bPacketsCmt = TRUE;
            m_waitPacketsCmt.wait(&m_lockGetPacketsCmt, 250);		
            m_lockGetPacketsCmt.unlock();

            // Take a snapshot of the current write pointer
            m_lockTrialComment.lock();
            uint32_t read_end_index = m_CMT->write_index;
            uint32_t read_index = m_CMT->write_start_index;
            m_lockTrialComment.unlock();
            int num_samples = read_end_index - read_index;
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
            if (m_TR == NULL)
                return CBSDKRESULT_ERRCONFIG;
            uint32_t read_end_index[cbMAXTRACKOBJ];

            // Wait for packets to come in
            m_lockGetPacketsTrack.lock();
            m_bPacketsTrack = TRUE;
            m_waitPacketsTrack.wait(&m_lockGetPacketsTrack, 250);		
            m_lockGetPacketsTrack.unlock();

            // Take a snapshot of the current write pointer
            m_lockTrialTracking.lock();
            memcpy(read_end_index, m_TR->write_index, sizeof(read_end_index));
            m_lockTrialTracking.unlock();
            int count = 0;
            for (uint16_t id = 0; id < cbMAXTRACKOBJ; ++id)
            {
                int num_samples = read_end_index[id] - m_TR->write_start_index[id];
                if (num_samples < 0)
                    num_samples += m_TR->size;
                uint16_t type = m_TR->node_type[id];
                if (num_samples && type)
                {
                    trialtracking->ids[count] = id; // Actual trackable ID
                    trialtracking->num_samples[count] = num_samples;
                    trialtracking->types[count] = type;
                    trialtracking->max_point_counts[count] = m_TR->max_point_counts[id];
                    strncpy((char *)trialtracking->names[count], (char *)m_TR->node_name[id], cbLEN_STR_LABEL);
                    count++;
                }
            }
            trialtracking->count = count;
        }
    }
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkInitTrialData
CBSDKAPI    cbSdkResult cbSdkInitTrialData(uint32_t nInstance, uint32_t bActive,
                                           cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                           cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkInitTrialData(bActive, trialevent, trialcont, trialcomment, trialtracking);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Start or stop file recording.
*
* @param[in]	filename  file name string
* @param[in]	comment   comment string
* @param[in]	bStart    start or stop recording
* @param[in]	options   the recording options

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetFileConfig(const char * filename, const char * comment, uint32_t bStart, uint32_t options)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_FILECFG fcpkt;
    memset(&fcpkt, 0, sizeof(cbPKT_FILECFG));
    fcpkt.time    = 1;
    fcpkt.chid    = 0x8000;
    fcpkt.type    = cbPKTTYPE_SETFILECFG;
    fcpkt.dlen    = cbPKTDLEN_FILECFG;
    fcpkt.options = options;
    fcpkt.extctrl = 0;
    fcpkt.filename[0] = 0;
    memcpy(fcpkt.filename, filename, strlen(filename));
    memcpy(fcpkt.comment, comment, strlen(comment));
    // get computer name
#ifdef WIN32
    DWORD cchBuff = sizeof(fcpkt.username);
    GetComputerName(fcpkt.username, &cchBuff) ;
#else
    char * szHost = getenv("HOSTNAME");
    strncpy(fcpkt.username, szHost == NULL ? "" : szHost, sizeof(fcpkt.username));
#endif
    fcpkt.username[sizeof(fcpkt.username) - 1] = 0;

    // fill in the boolean recording control field
    fcpkt.recording = bStart ? 1 : 0;

    // send the packet
    cbRESULT cbres = cbSendPacket(&fcpkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetFileConfig
CBSDKAPI    cbSdkResult cbSdkSetFileConfig(uint32_t nInstance,
                                           const char * filename, const char * comment, uint32_t bStart, uint32_t options)
{
    if (filename == NULL || comment == NULL)
        return CBSDKRESULT_NULLPTR;
    if (strlen(comment) >= 256)
        return CBSDKRESULT_INVALIDCOMMENT;
    if (strlen(filename) >= 256)
        return CBSDKRESULT_INVALIDFILENAME;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetFileConfig(filename, comment, bStart, options);
}

// Author & Date:   Ehsan Azar     20 Feb 2013
/** Get file recording information.
*
* @param[out]	filename      file name being recorded
* @param[out]	username      user name recording the file
* @param[out]	pbRecording   If recording is in progress (depends on keep-alive mechanism)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetFileConfig(char * filename, char * username, bool * pbRecording)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_FILECFG filecfg;
    cbRESULT cbres = cbGetFileInfo(&filecfg, m_nInstance);
    if (cbres)
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
CBSDKAPI    cbSdkResult cbSdkGetFileConfig(uint32_t nInstance,
                                           char * filename, char * username, bool * pbRecording)
{
    if (filename == NULL && username == NULL && pbRecording == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
                                      uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_PATIENTINFO fcpkt;
    memset(&fcpkt, 0, sizeof(cbPKT_PATIENTINFO));
    fcpkt.time    = 1;
    fcpkt.chid    = 0x8000;
    fcpkt.type    = cbPKTTYPE_SETPATIENTINFO;
    fcpkt.dlen    = cbPKTDLEN_PATIENTINFO;
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
    cbRESULT cbres = cbSendPacket(&fcpkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetPatientInfo
CBSDKAPI    cbSdkResult cbSdkSetPatientInfo(uint32_t nInstance,
                                            const char * ID, const char * firstname, const char * lastname,
                                            uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear)
{
    if (ID == NULL || firstname == NULL || lastname == NULL)
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
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetPatientInfo(ID, firstname, lastname, DOBMonth, DOBDay, DOBYear);
}

// Author & Date:   Tom Richins     13 Apr 2011
/** Initiate autoimpedance through central.
*
* \nThis function returns the error code
*/
cbSdkResult SdkApp::SdkInitiateImpedance()
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_INITIMPEDANCE iipkt;
    memset(&iipkt, 0, sizeof(cbPKT_INITIMPEDANCE));
    iipkt.time    = 1;
    iipkt.chid    = 0x8000;
    iipkt.type    = cbPKTTYPE_SETINITIMPEDANCE;
    iipkt.dlen    = cbPKTDLEN_INITIMPEDANCE;

    iipkt.initiate = 1;        // start autoimpedance
    // send the packet
    cbRESULT cbres = cbSendPacket(&iipkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkInitiateImpedance
CBSDKAPI    cbSdkResult cbSdkInitiateImpedance(uint32_t nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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

* \n This function returns the error code
*
*/
cbSdkResult SdkApp::SdkSendPoll(const char* appname, uint32_t mode, uint32_t flags, uint32_t extra)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_POLL polepkt;
    memset(&polepkt, 0, sizeof(cbPKT_POLL));
    polepkt.chid = 0x8000;
    polepkt.type = cbPKTTYPE_SETPOLL;
    polepkt.dlen = cbPKTDLEN_POLL;
    polepkt.mode = mode;
    polepkt.flags = flags;
    polepkt.extra = extra;
    strncpy(polepkt.appname, appname, sizeof(polepkt.appname));
    // get computer name
#ifdef WIN32
    DWORD cchBuff = sizeof(polepkt.username);
    GetComputerName(polepkt.username, &cchBuff) ;
#else
    strncpy(polepkt.username, getenv("HOSTNAME"), sizeof(polepkt.username));
#endif
    polepkt.username[sizeof(polepkt.username) - 1] = 0;

    // send the packet
    cbRESULT cbres = cbSendPacket(&polepkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSendPoll
CBSDKAPI    cbSdkResult SdkSendPoll(uint32_t nInstance,
                                    const char* appname, uint32_t mode, uint32_t flags, uint32_t extra)
{
    if (appname == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSendPoll(appname, mode, flags, extra);
}

// Author & Date:   Tom Richins     26 May 2011
/** Send packet.
*
* This is used by any process that needs to send a packet using cbSdk rather
* than directly thru the library.
* @param[in]	ppckt void * packet to send

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSendPacket(void * ppckt)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // send the packet
    cbRESULT cbres = cbSendPacket(ppckt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSendPacket
CBSDKAPI    cbSdkResult cbSdkSendPoll(uint32_t nInstance, void * ppckt)
{
    if (ppckt == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkSetSystemRunLevel(uint32_t runlevel, uint32_t locked, uint32_t resetque)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // send the packet
    cbPKT_SYSINFO sysinfo;
    sysinfo.time     = 1;
    sysinfo.chid     = 0x8000;
    sysinfo.type     = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.dlen     = cbPKTDLEN_SYSINFO;
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = locked;

    // Enter the packet into the XMT buffer queue
    cbRESULT cbres = cbSendPacket(&sysinfo, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSystemRunLevel
CBSDKAPI    cbSdkResult cbSdkSetSystemRunLevel(uint32_t nInstance, uint32_t runlevel, uint32_t locked, uint32_t resetque)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetSystemRunLevel(uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT cbres = cbGetSystemRunLevel(runlevel, runflags, resetque, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres == cbRESULT_HARDWAREOFFLINE)
        return CBSDKRESULT_ERROFFLINE;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSystemRunLevel
CBSDKAPI    cbSdkResult cbSdkGetSystemRunLevel(uint32_t nInstance, uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque)
{
    if (runlevel == NULL && runflags == NULL && resetque == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkSetDigitalOutput(uint16_t channel, uint16_t value)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    // declare the packet that will be sent
    cbPKT_SET_DOUT dopkt;
    dopkt.time = 1;
    dopkt.chid = 0x8000;
    dopkt.type = cbPKTTYPE_SET_DOUTSET;
    dopkt.dlen = cbPKTDLEN_SET_DOUT;

    // get the channel number
    dopkt.chan = channel;

    // fill in the boolean on/off field
    dopkt.value = value;

    // send the packet
    cbRESULT cbres = cbSendPacket(&dopkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetDigitalOutput
CBSDKAPI    cbSdkResult cbSdkSetDigitalOutput(uint32_t nInstance, uint16_t channel, uint16_t value)
{
    if (channel < MIN_CHANS_DIGITAL_OUT || channel > MAX_CHANS_DIGITAL_OUT)
        return CBSDKRESULT_INVALIDCHANNEL; // Not a digital output channel
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetDigitalOutput(channel, value);
}

// Author & Date:   Ehsan Azar     25 Feb 2013
/** Send a synch output.
*
* @param[in]	channel    the channel number (1-based) must be synch output channel
* @param[in]	nFreq      frequency in mHz (0 means stop the clock)
* @param[in]	nRepeats   number of pulses to generate (0 means forever)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetSynchOutput(uint16_t channel, uint32_t nFreq, uint32_t nRepeats)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // Only CerePlex currently supports this, for NSP digital output may be used with simillar results
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
    nmpkt.chid = 0x8000;
    nmpkt.type = cbPKTTYPE_NMSET;
    nmpkt.dlen = cbPKTDLEN_NM;
    nmpkt.mode = cbNM_MODE_SYNCHCLOCK;
    nmpkt.value = nFreq;
    nmpkt.opt[0] = nRepeats;

    // send the packet
    cbRESULT cbres = cbSendPacket(&nmpkt, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSynchOutput
CBSDKAPI    cbSdkResult cbSdkSetSynchOutput(uint32_t nInstance, uint16_t channel, uint32_t nFreq, uint32_t nRepeats)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
* Must not shutdown NSP or communications in midst of operation
* @param[in]	szSrc      path to file to be uploaded to NSP
* @param[in]	szDstDir   destination directory (should not include the file name, nor path ending)
* @param[in]	nInstance  library instance number

* \n This function returns the error code
*/
cbSdkResult cbSdkUpload(const char * szSrc, const char * szDstDir, uint32_t nInstance)
{
    uint32_t runlevel;
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    cbRESULT cbres;
    cbres = cbGetSystemRunLevel(&runlevel, NULL, NULL);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    // If running or even updating do not use this, only when standby this function should be used
    if (runlevel != cbRUNLEVEL_STANDBY)
        return CBSDKRESULT_INVALIDINST;

    uint32_t cbRead = 0;
    uint32_t cbFile = 0;
    FILE * pFile = fopen(szSrc, "rb");
    if (pFile == NULL)
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
    BYTE * pFileData = NULL;
    try {
        pFileData = new BYTE[cbFile];
    } catch (...) {
        pFileData = NULL;
    }
    if (pFileData == NULL)
    {
        fclose(pFile);
        return CBSDKRESULT_ERRMEMORY;
    }
    // Read entire file into memory
    cbRead = (uint32_t)fread(pFileData, sizeof(BYTE), cbFile, pFile);
    fclose(pFile);
    if (cbFile != cbRead)
    {
        free(pFileData);
        return CBSDKRESULT_ERRFORMATFILE;
    }
    const char * szBaseName = &szSrc[0];
    // Find the base name and use it on destination too
    uint32_t nLen = (uint32_t)strlen(szSrc);
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
    nLen = (uint32_t)strlen(szBaseName) + (uint32_t)strlen(szDstDir) + 1;
    if (nLen >= 64)
    {
        free(pFileData);
        return CBSDKRESULT_INVALIDFILENAME;
    }

    cbPKT_UPDATE upkt;
    memset(&upkt, 0, sizeof(upkt));
    upkt.time = 0;
    upkt.chan = 0x8000;
    upkt.type = cbPKTTYPE_UPDATESET;
    upkt.dlen = cbPKTDLEN_UPDATE;
    sprintf(upkt.filename, "%s/%s", szDstDir, szBaseName);

    uint32_t b, blocks = (cbFile / 512) + 1;
    for (b = 0; b < blocks; ++b)
    {
        upkt.blockseq = b;
        upkt.blockend = (b == (blocks - 1));
        upkt.blocksiz = std::min((int32_t) (cbFile - (b * 512)), (int32_t) 512);
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
cbSdkResult SdkApp::SdkExtDoCommand(cbSdkExtCmd * extCmd)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // Only NSP1.5 compatible devices currently supports this
    if (m_instInfo & cbINSTINFO_NSP1)
        return CBSDKRESULT_NOTIMPLEMENTED;

    cbRESULT cbres;
    cbPKT_LOG pktLog;
    memset(&pktLog, 0, sizeof(pktLog));
    strcpy(pktLog.name, "cbSDK");
    pktLog.chid = 0x8000;
    pktLog.type = cbPKTTYPE_LOGSET;
    pktLog.dlen = cbPKTDLEN_LOG;

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
    default:
        return CBSDKRESULT_INVALIDPARAM;
        break;
    }

    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::cbSdkExtCmd
CBSDKAPI    cbSdkResult cbSdkExtDoCommand(uint32_t nInstance, cbSdkExtCmd * extCmd)
{
    if (extCmd == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkSetAnalogOutput(uint16_t channel, cbSdkWaveformData * wf, cbSdkAoutMon * mon)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (wf != NULL)
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
    cbRESULT cbres = cbGetAoutOptions(channel, &dwOptions, &nMonitoredChan, NULL, m_nInstance);
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
    if (wf != NULL)
    {
        cbPKT_AOUT_WAVEFORM wfPkt;
        memset(&wfPkt, 0, sizeof(wfPkt));
        wfPkt.mode = wf->type;
        if (wfPkt.mode == cbWAVEFORM_MODE_PARAMETERS)
        {
            if (wf->phases > cbMAX_WAVEFORM_PHASES)
                return CBSDKRESULT_INVALIDPARAM;
            wfPkt.set(channel, wf->phases, wf->duration, wf->amplitude, wf->trigChan, wf->trig,
                wf->repeats, 1, 0, wf->offset, wf->trigValue, wf->trigNum);
        }
        else if (wfPkt.mode == cbWAVEFORM_MODE_SINE)
        {
            wfPkt.set(channel, wf->offset, wf->sineFrequency, wf->sineAmplitude, wf->trigChan, wf->trig,
                wf->repeats, wf->trigValue, wf->trigNum);
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
    else if (mon != NULL)
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
        dwOptions &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK);
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
CBSDKAPI    cbSdkResult cbSdkSetAnalogOutput(uint32_t nInstance,
                                             uint16_t channel, cbSdkWaveformData * wf, cbSdkAoutMon * mon)
{
    if (wf != NULL && mon != NULL)
        return CBSDKRESULT_INVALIDPARAM; // cannot specify both
    if (channel < MIN_CHANS_ANALOG_OUT || channel > MAX_CHANS_AUDIO)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetAnalogOutput(channel, wf, mon);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Activate or deactivate channels. Only activated channels are monitored.
*
* @param[in]	channel     channel number (1-based), zero means all channels
* @param[in]	bActive		the character set of the comment

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetChannelMask(uint16_t channel, uint32_t bActive)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    // zero means all
    if (channel == 0)
    {
        for (int i = 0; i < cbMAXCHANS; ++i)
            m_bChannelMask[i] = (bActive > 0);
    }
    else
        m_bChannelMask[channel - 1] = (bActive > 0);
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetChannelMask
CBSDKAPI    cbSdkResult cbSdkSetChannelMask(uint32_t nInstance, uint16_t channel, uint32_t bActive)
{
    if (channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetChannelMask(channel, bActive);
}

// Author & Date:   Ehsan Azar     25 Feb 2011
/** Send a comment or custom event.
*
* @param[in]	rgba     the color or custom event number
* @param[in]	charset	 the character set of the comment
* @param[in]	comment	 comment string (can be NULL)

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetComment(uint32_t rgba, uint8_t charset, const char * comment)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT cbres = cbSetComment(charset, cbCOMMENT_FLAG_RGBA, rgba, comment, m_nInstance);
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetComment
CBSDKAPI    cbSdkResult cbSdkSetComment(uint32_t nInstance, uint32_t rgba, uint8_t charset, const char * comment)
{
    if (comment)
    {
        if (strlen(comment) >= cbMAX_COMMENT)
            return CBSDKRESULT_INVALIDCOMMENT;
    }
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetComment(rgba, charset, comment);
}

// Author & Date:   Ehsan Azar     3 March 2011
/** Send a full channel configuration packet.
*
* @param[in]	channel     channel number (1-based)
* @param[in]	chaninfo	the full channel configuration

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (cb_cfg_buffer_ptr[m_nIdx]->chaninfo[channel - 1].chid == 0)
        return CBSDKRESULT_INVALIDCHANNEL;

    chaninfo->type = cbPKTTYPE_CHANSET;
    chaninfo->dlen = cbPKTDLEN_CHANINFO;
    cbRESULT cbres = cbSendPacket(chaninfo, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetChannelConfig
CBSDKAPI    cbSdkResult cbSdkSetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (chaninfo == NULL)
        return CBSDKRESULT_NULLPTR;
    if (chaninfo->chid != cbPKTCHAN_CONFIGURATION)
        return CBSDKRESULT_INVALIDPARAM;
    if (chaninfo->dlen > cbPKTDLEN_CHANINFO || chaninfo->dlen < cbPKTDLEN_CHANINFOSHORT)
        return CBSDKRESULT_INVALIDPARAM;
    if (chaninfo->chan != channel)
        return CBSDKRESULT_INVALIDCHANNEL;

    /// \todo do more validation before sending the packet

    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!cb_library_initialized[m_nIdx])
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetChanInfo(channel, chaninfo, m_nInstance);
    if (cbres == cbRESULT_INVALIDCHANNEL)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetChannelConfig
CBSDKAPI    cbSdkResult cbSdkGetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (chaninfo == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetSampleGroupList(uint32_t proc, uint32_t group, uint32_t *length, uint32_t *list)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetSampleGroupList(proc, group, length, list, m_nInstance);
    if (cbres == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSampleGroupList
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupList(uint32_t nInstance,
                                                uint32_t proc, uint32_t group, uint32_t *length, uint32_t *list)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetSampleGroupInfo(uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetSampleGroupInfo(proc, group, label, period, length, m_nInstance);
    if (cbres == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSampleGroupInfo
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupInfo(uint32_t nInstance,
                                                uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetFilterDesc(uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetFilterDesc(proc, filt, filtdesc, m_nInstance);
    if (cbres == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDPARAM;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetFilterDesc
CBSDKAPI    cbSdkResult cbSdkGetFilterDesc(uint32_t nInstance, uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc)
{
    if (filtdesc == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetTrackObj(char * name, uint16_t * type, uint16_t * pointCount, uint32_t id)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetTrackObj(name, type, pointCount, id, m_nInstance);
    if (cbres == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDTRACKABLE;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetTrackObj
CBSDKAPI    cbSdkResult cbSdkGetTrackObj(uint32_t nInstance, char * name, uint16_t * type, uint16_t * pointCount, uint32_t id)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkGetVideoSource(char * name, float * fps, uint32_t id)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;

    cbRESULT cbres = cbGetVideoSource(name, fps, id, m_nInstance);
    if (cbres == cbRESULT_INVALIDADDRESS)
        return CBSDKRESULT_INVALIDVIDEOSRC;
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetVideoSource
CBSDKAPI    cbSdkResult cbSdkGetVideoSource(uint32_t nInstance, char * name, float * fps, uint32_t id)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
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
cbSdkResult SdkApp::SdkSetSpikeConfig(uint32_t spklength, uint32_t spkpretrig)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT cbres = cbSetSpikeLength(spklength, spkpretrig, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSetSpikeConfig
CBSDKAPI    cbSdkResult cbSdkSetSpikeConfig(uint32_t nInstance, uint32_t spklength, uint32_t spkpretrig)
{
    if (spklength > cbMAX_PNTS || spkpretrig >= spklength)
        return CBSDKRESULT_INVALIDPARAM;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSetSpikeConfig(spklength, spkpretrig);
}

// Author & Date:   Ehsan Azar     30 March 2011
/** Get global system configuration.
*
* @param[out]		spklength		spike length
* @param[out]		spkpretrig		spike pre-trigger number of samples
* @param[out]		sysfreq			system clock frequency in Hz

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkGetSysConfig(uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbRESULT cbres = cbGetSpikeLength(spklength, spkpretrig, sysfreq, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkGetSysConfig
CBSDKAPI    cbSdkResult cbSdkGetSysConfig(uint32_t nInstance, uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq)
{
    if (spklength == NULL && spkpretrig == NULL && sysfreq == NULL)
        return CBSDKRESULT_NULLPTR;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkGetSysConfig(spklength, spkpretrig, sysfreq);
}

// Author & Date:   Ehsan Azar     11 MAy 2012
/** Perform given runlevel system command.
*
* @param[out]		cmd		system command to perform

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkSystem(cbSdkSystemType cmd)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    cbPKT_SYSINFO pktsysinfo;
    pktsysinfo.time = 0;
    pktsysinfo.chid = 0x8000;
    pktsysinfo.type = cbPKTTYPE_SYSSETRUNLEV;
    pktsysinfo.dlen = cbPKTDLEN_SYSINFO;
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
    cbRESULT cbres = cbSendPacket(&pktsysinfo, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkSystem
CBSDKAPI    cbSdkResult cbSdkSystem(uint32_t nInstance, cbSdkSystemType cmd)
{
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkSystem(cmd);
}

// Author & Date:   Ehsan Azar     28 Feb 2011
/** Register a callback function.
*
* @param[in]	callbacktype		the calback type to register function against
* @param[in]	m_pCallbackFn		callback function
* @param[in]	m_pCallbackData		custom parameter callback is called with

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkRegisterCallback(cbSdkCallbackType callbacktype, cbSdkCallback pCallbackFn, void * pCallbackData)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (m_pCallback[callbacktype]) // Already registered
        return CBSDKRESULT_CALLBACKREGFAILED;

    m_lockCallback.lock();
    m_pCallbackParams[callbacktype] = pCallbackData;
    m_pCallback[callbacktype] = pCallbackFn;
    m_lockCallback.unlock();
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkRegisterCallback
CBSDKAPI    cbSdkResult cbSdkRegisterCallback(uint32_t nInstance,
                                              cbSdkCallbackType callbacktype, cbSdkCallback pCallbackFn, void * pCallbackData)
{
    if (!pCallbackFn)
        return CBSDKRESULT_NULLPTR;
    if (callbacktype >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkRegisterCallback(callbacktype, pCallbackFn, pCallbackData);
}

// Author & Date:   Ehsan Azar     28 Feb 2011
/** Unregister the current callback.
*
* @param[in]	callbacktype		the calback type to unregister

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkUnRegisterCallback(cbSdkCallbackType callbacktype)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!m_pCallback[callbacktype]) // Already unregistered
        return CBSDKRESULT_CALLBACKREGFAILED;

    m_lockCallback.lock();
    m_pCallback[callbacktype] = NULL;
    m_pCallbackParams[callbacktype] = NULL;
    m_lockCallback.unlock();
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkUnRegisterCallback
CBSDKAPI    cbSdkResult cbSdkUnRegisterCallback(uint32_t nInstance, cbSdkCallbackType callbacktype)
{
    if (callbacktype >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkUnRegisterCallback(callbacktype);
}

// Author & Date:   Ehsan Azar     7 Aug 2013
/** Get callback status.
* if unregistered returns success, and means a register should not fail
* @param[in]	callbacktype		the calback type to unregister

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkCallbackStatus(cbSdkCallbackType callbacktype)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (m_pCallback[callbacktype])
        return CBSDKRESULT_CALLBACKREGFAILED; // Already registered
    return CBSDKRESULT_SUCCESS;
}

// Purpose: sdk stub for SdkApp::SdkCallbackStatus
CBSDKAPI    cbSdkResult cbSdkCallbackStatus(uint32_t nInstance, cbSdkCallbackType callbacktype)
{
    if (callbacktype >= CBSDKCALLBACK_COUNT)
        return CBSDKRESULT_INVALIDCALLBACKTYPE;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;

    return g_app[nInstance]->SdkCallbackStatus(callbacktype);
}

// Author & Date:   Ehsan Azar     21 Feb 2013
/** Convert volts string (e.g. '5V', '-65mV', ...) to its raw digital value equivalent for given channel.
*
* @param[in]	channel				the channel number (1-based)
* @param[in]	szVoltsUnitString	the volts string
* @param[out]	digital				the raw digital value

* \n This function returns the error code
*/
cbSdkResult SdkApp::SdkAnalogToDigital(uint16_t channel, const char * szVoltsUnitString, int32_t * digital)
{
    if (m_instInfo == 0)
        return CBSDKRESULT_CLOSED;
    if (!cb_library_initialized[m_nIdx])
        return CBSDKRESULT_CLOSED;
    int len = (int)strlen(szVoltsUnitString);
    if (len > 16)
        return CBSDKRESULT_INVALIDPARAM;
    int32_t nFactor = 0;
    std::string strVolts = szVoltsUnitString;
    std::string strUnit = "";
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
    else if (strVolts.rfind("V") != std::string::npos)
    {
        nFactor = 1;
        strUnit = "V";
    }
    char * pEnd = NULL;
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
        *digital = (int32_t)nValue;
        return CBSDKRESULT_SUCCESS;
    }
    cbSCALING scale;
    cbRESULT cbres;
    if (IsChanAnalogIn(channel))
        cbres = cbGetAinpScaling(channel, &scale, m_nInstance);
    else
        cbres = cbGetAoutScaling(channel, &scale, m_nInstance);
    if (cbres == cbRESULT_NOLIBRARY)
        return CBSDKRESULT_CLOSED;
    if (cbres)
        return CBSDKRESULT_UNKNOWN;
    strUnit = scale.anaunit;
    double chan_factor = 1;
    if (strUnit.compare("mV") == 0)
        chan_factor = 1000;
    else if (strUnit.compare("uV") == 0)
        chan_factor = 1000000;
    // TODO: see if anagain needs to be used
    *digital = (int32_t)floor(((dValue * scale.digmax) * chan_factor) / ((double)nFactor * scale.anamax));
    return CBSDKRESULT_SUCCESS;
}

/// sdk stub for SdkApp::SdkAnalogToDigital
CBSDKAPI    cbSdkResult cbSdkAnalogToDigital(uint32_t nInstance, uint16_t channel, const char * szVoltsUnitString, int32_t * digital)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return CBSDKRESULT_INVALIDCHANNEL;
    if (nInstance >= cbMAXOPEN)
        return CBSDKRESULT_INVALIDPARAM;
    if (g_app[nInstance] == NULL)
        return CBSDKRESULT_CLOSED;
    if (szVoltsUnitString == NULL || digital == NULL)
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
    , m_bTrialDouble(false), m_bTrialAbsolute(false), m_uTrialWaveforms(0)
    , m_uTrialConts(0), m_uTrialEvents(0), m_uTrialComments(0)
    , m_uTrialTrackings(0), m_bWithinTrial(FALSE), m_uTrialStartTime(0)
    , m_uCbsdkTime(0), m_CD(NULL), m_ED(NULL), m_CMT(NULL), m_TR(NULL)
{
    memset(&m_lastPktVideoSynch, 0, sizeof(m_lastPktVideoSynch));
    memset(&m_bChannelMask, 0, sizeof(m_bChannelMask));
    memset(&m_lastPktVideoSynch, 0, sizeof(m_lastPktVideoSynch));
    memset(&m_lastLost, 0, sizeof(m_lastLost));
    memset(&m_lastInstInfo, 0, sizeof(m_lastInstInfo));
    for (int i = 0; i < CBSDKCALLBACK_COUNT; ++i)
    {
        m_pCallback[i] = 0;
        m_pCallbackParams[i] = 0;
        m_pLateCallback[i] = 0;
        m_pLateCallbackParams[i] = 0;
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
*/
void SdkApp::Open(uint32_t nInstance, int nInPort, int nOutPort, LPCSTR szInIP, LPCSTR szOutIP, int nRecBufSize)
{
    // clear las library error
    m_lastCbErr = cbRESULT_OK;
    // Close networking thread if already running
    Close();
    // One-time initialization
    if (!m_bInitialized)
    {
        m_bInitialized = true;
        // Add myself as the sole listener
        InstNetwork::Open(this);
        // connect the network events and commands
        QObject::connect(this, SIGNAL(InstNetworkEvent(NetEventType, unsigned int)),
                this, SLOT(OnInstNetworkEvent(NetEventType, unsigned int)), Qt::DirectConnection);
    }
    // instance id and connection details are persistent in the process
    m_nInstance = nInstance;
    m_nInPort = nInPort;
    m_nOutPort = nOutPort;
    m_nRecBufSize = nRecBufSize;
    m_strInIP = szInIP;
    m_strOutIP = szOutIP;

#ifndef WIN32
    // On Linux bind ot broadcast
    if (m_strInIP.endsWith(".255"))
        m_bBroadcast = true;
#endif

    // Restart networking thread
    start(QThread::HighPriority);
}

// Author & Date: Ehsan Azar       29 April 2012
/// Proxy for all incomming packets
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

    // check for configuration class packets
    if (pPkt->chid & cbPKTCHAN_CONFIGURATION)
    {
        // Check for configuration packets
        if (pPkt->chid == cbPKTCHAN_CONFIGURATION)
        {
            if (pPkt->type == cbPKTTYPE_SYSHEARTBEAT)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SYSHEARTBEAT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_SYSHEARTBEAT);
                if (m_pLateCallback[CBSDKCALLBACK_SYSHEARTBEAT])
                    m_pLateCallback[CBSDKCALLBACK_SYSHEARTBEAT](m_nInstance, cbSdkPkt_SYSHEARTBEAT, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SYSHEARTBEAT]);
            }
            else if (pPkt->type == cbPKTTYPE_REPIMPEDANCE)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_IMPEDANCE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_IMPEDENCE);
                if (m_pLateCallback[CBSDKCALLBACK_IMPEDENCE])
                    m_pLateCallback[CBSDKCALLBACK_IMPEDENCE](m_nInstance, cbSdkPkt_IMPEDANCE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_IMPEDENCE]);
            }
            else if ((pPkt->type & 0xF0) == cbPKTTYPE_CHANREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_CHANINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_CHANINFO);
                if (m_pLateCallback[CBSDKCALLBACK_CHANINFO])
                    m_pLateCallback[CBSDKCALLBACK_CHANINFO](m_nInstance, cbSdkPkt_CHANINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_CHANINFO]);
            }
            else if (pPkt->type == cbPKTTYPE_NMREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_NM, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_NM);
                if (m_pLateCallback[CBSDKCALLBACK_NM])
                    m_pLateCallback[CBSDKCALLBACK_NM](m_nInstance, cbSdkPkt_NM, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_NM]);
            }
            else if (pPkt->type == cbPKTTYPE_GROUPREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_GROUPINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_GROUPINFO);
                if (m_pLateCallback[CBSDKCALLBACK_GROUPINFO])
                    m_pLateCallback[CBSDKCALLBACK_GROUPINFO](m_nInstance, cbSdkPkt_GROUPINFO, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_GROUPINFO]);
            }
            else if (pPkt->type == cbPKTTYPE_COMMENTREP)
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
            else if (pPkt->type == cbPKTTYPE_REPFILECFG)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_FILECFG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_FILECFG);
                if (m_pLateCallback[CBSDKCALLBACK_FILECFG])
                    m_pLateCallback[CBSDKCALLBACK_FILECFG](m_nInstance, cbSdkPkt_FILECFG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_COMMENT]);
            }
            else if (pPkt->type == cbPKTTYPE_REPPOLL)
            {
                // The callee should check flags to find if it is a response to poll, and do accordingly
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_POLL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_POLL);
                if (m_pLateCallback[CBSDKCALLBACK_POLL])
                    m_pLateCallback[CBSDKCALLBACK_POLL](m_nInstance, cbSdkPkt_POLL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_POLL]);
            }
            else if (pPkt->type == cbPKTTYPE_VIDEOTRACKREP)
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
            else if (pPkt->type == cbPKTTYPE_VIDEOSYNCHREP)
            {
                m_lastPktVideoSynch = *reinterpret_cast<const cbPKT_VIDEOSYNCH*>(pPkt);
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SYNCH, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_SYNCH);
                if (m_pLateCallback[CBSDKCALLBACK_SYNCH])
                    m_pLateCallback[CBSDKCALLBACK_SYNCH](m_nInstance, cbSdkPkt_SYNCH, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SYNCH]);
            }
            else if (pPkt->type == cbPKTTYPE_LOGREP)
            {
                if (m_pLateCallback[CBSDKCALLBACK_ALL])
                    m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_LOG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
                // Late bind before usage
                LateBindCallback(CBSDKCALLBACK_LOG);
                if (m_pLateCallback[CBSDKCALLBACK_LOG])
                    m_pLateCallback[CBSDKCALLBACK_LOG](m_nInstance, cbSdkPkt_LOG, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_LOG]);
                // Fillout trial if setup
                OnPktLog(reinterpret_cast<const cbPKT_LOG*>(pPkt));
                //OnPktComment(reinterpret_cast<const cbPKT_COMMENT*>(pPkt));
            }
        } // end if (pPkt->chid==0x8000
    } // end if (pPkt->chid & 0x8000
    else if (pPkt->chid == 0)
    {
        // No mask applied here
        // Inside the callback cbPKT_GROUP.type can be used to find the sample group number
        uint8_t smpgroup = ((cbPKT_GROUP *)pPkt)->type; // sample group
        if (smpgroup > 0 && smpgroup <= cbMAXGROUPS)
        {
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_CONTINUOUS, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_CONTINUOUS);
            if (m_pLateCallback[CBSDKCALLBACK_CONTINUOUS])
                m_pLateCallback[CBSDKCALLBACK_CONTINUOUS](m_nInstance, cbSdkPkt_CONTINUOUS, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_CONTINUOUS]);
        }
    }
    // check for channel event packets cerebus channels 1-144
    else if (pPkt->chid < cbNUM_ANALOG_CHANS + 1)   // channels are 1 based
    {
        if (pPkt->chid > 0 && m_bChannelMask[pPkt->chid - 1])
        {
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_SPIKE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_SPIKE);
            if (m_pLateCallback[CBSDKCALLBACK_SPIKE])
                m_pLateCallback[CBSDKCALLBACK_SPIKE](m_nInstance, cbSdkPkt_SPIKE, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_SPIKE]);
        }
    }
    // catch digital input port events and save them as NSAS experiment event packets
    else if (pPkt->chid == MAX_CHANS_DIGITAL_IN)
    {
        if (m_bChannelMask[pPkt->chid - 1])
        {
            if (m_pLateCallback[CBSDKCALLBACK_ALL])
                m_pLateCallback[CBSDKCALLBACK_ALL](m_nInstance, cbSdkPkt_DIGITAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_ALL]);
            // Late bind before usage
            LateBindCallback(CBSDKCALLBACK_DIGITAL);
            if (m_pLateCallback[CBSDKCALLBACK_DIGITAL])
                m_pLateCallback[CBSDKCALLBACK_DIGITAL](m_nInstance, cbSdkPkt_DIGITAL, pPkt, m_pLateCallbackParams[CBSDKCALLBACK_DIGITAL]);
        }
    }
    // catch serial input port events and save them as NSAS experiment event packets
    else if (pPkt->chid == MAX_CHANS_SERIAL)
    {
        if (m_bChannelMask[pPkt->chid - 1])
        {
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
    // reset it's timer to 0 for cbGetSystemClockTime
    m_uCbsdkTime = pPkt->time;

    // Process continuous data if we're within a trial...
    if (pPkt->chid == 0)
        OnPktGroup(reinterpret_cast<const cbPKT_GROUP*>(pPkt));

    // and only look at event data packets
    if ( pPkt->chid == MAX_CHANS_DIGITAL_IN || pPkt->chid == MAX_CHANS_SERIAL || (pPkt->chid > 0 && pPkt->chid <= cbNUM_ANALOG_CHANS) )
        OnPktEvent(pPkt);
}

// Author & Date: Ehsan Azar       29 April 2012
/// Network events
void SdkApp::OnInstNetworkEvent(NetEventType type, unsigned int code)
{
    cbSdkPktLostEvent lostEvent;
    switch (type)
    {
    case NET_EVENT_INSTINFO:
        m_connectLock.lock();
        m_connectWait.wakeAll();
        m_connectLock.unlock();
        InstInfoEvent(m_instInfo);
        break;
    case NET_EVENT_CLOSE:
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.wakeAll();
        m_connectLock.unlock();
        InstInfoEvent(m_instInfo);
        break;
    case NET_EVENT_CBERR:
        m_lastCbErr = code;
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.wakeAll();
        m_connectLock.unlock();
        break;
    case NET_EVENT_NETOPENERR:
        m_lastCbErr = code;
        m_instInfo = 0;
        m_connectLock.lock();
        m_connectWait.wakeAll();
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
