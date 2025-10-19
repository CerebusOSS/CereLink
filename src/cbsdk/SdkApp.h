/* =STS=> SdkApp.h[5022].aa11   submit   SMID:12 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2017 Blackrock Microsystems
//
// $Workfile: SdkApp.h $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/SdkApp.h $
// $Revision: 1 $
// $Date: 4/29/12 1:21p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
/**
* \file SdkApp.h
* \brief Cerebus SDK - This header file is distributed as part of the SDK.
*/
#ifndef SDKAPP_H_INCLUDED
#define SDKAPP_H_INCLUDED

#include "InstNetwork.h"
#include "../include/cerelink/cbsdk.h"
#include "../include/cerelink/CCFUtils.h"
#include "ContinuousData.h"
#include "EventData.h"
#include <mutex>
#include <condition_variable>

/// Wrapper class for SDK Qt application
class SdkApp : public InstNetwork, public InstNetwork::Listener
{
public:
    SdkApp();
    ~SdkApp() override;
public:
    // Override the Listener::ProcessIncomingPacket virtual function
    void ProcessIncomingPacket(const cbPKT_GENERIC * pPkt) override; // Process incoming packets
    uint32_t GetInstInfo() const {return m_instInfo;}
    cbRESULT GetLastCbErr() const {return m_lastCbErr;}
    void Open(uint32_t nInstance, int nInPort = cbNET_UDP_PORT_BCAST, int nOutPort = cbNET_UDP_PORT_CNT,
        LPCSTR szInIP = cbNET_UDP_ADDR_INST, LPCSTR szOutIP = cbNET_UDP_ADDR_CNT, int nRecBufSize = NSP_REC_BUF_SIZE, int nRange = 0);
private:
    void OnPktGroup(const cbPKT_GROUP * pkt);
    void OnPktEvent(const cbPKT_GENERIC * pPkt);
    void OnPktComment(const cbPKT_COMMENT * pPkt);
    void OnPktLog(const cbPKT_LOG * pPkt);
    void OnPktTrack(const cbPKT_VIDEOTRACK * pPkt);

    void LateBindCallback(cbSdkCallbackType callbackType);
    void LinkFailureEvent(const cbSdkPktLostEvent & lost);
    void InstInfoEvent(uint32_t instInfo);
    cbSdkResult unsetTrialConfig(cbSdkTrialType type);

public:
    // ---------------------------
    // All SDK functions come here
    // ---------------------------

    void SdkAsynchCCF(ccf::ccfResult res, LPCSTR szFileName, cbStateCCF state, uint32_t nProgress);
    cbSdkResult SdkGetVersion(cbSdkVersion *version) const;
    cbSdkResult SdkReadCCF(cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend, bool bThreaded);  // From file or device if szFileName is null
    cbSdkResult SdkFetchCCF(cbSdkCCF * pData) const;  // from device only
    cbSdkResult SdkWriteCCF(cbSdkCCF * pData, const char * szFileName, bool bThreaded);  // to file or device if szFileName is null
    cbSdkResult SdkSendCCF(cbSdkCCF * pData, bool bAutosort = false);  // to device only
    cbSdkResult SdkOpen(uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con);
    cbSdkResult SdkGetType(cbSdkConnectionType * conType, cbSdkInstrumentType * instType) const;
    cbSdkResult SdkUnsetTrialConfig(cbSdkTrialType type);
    cbSdkResult SdkClose();
    cbSdkResult SdkGetTime(PROCTIME * cbtime) const;
    cbSdkResult SdkGetSpkCache(uint16_t channel, cbSPKCACHE **cache) const;
    cbSdkResult SdkGetTrialConfig(uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                  uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval,
                                  uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                  uint32_t * puComments, uint32_t * puTrackings) const;
    cbSdkResult SdkSetTrialConfig(uint32_t bActive, uint16_t begchan, uint32_t begmask, uint32_t begval,
                                  uint16_t endchan, uint32_t endmask, uint32_t endval,
                                  uint32_t uWaveforms, uint32_t uConts, uint32_t uEvents, uint32_t uComments, uint32_t uTrackings);
    cbSdkResult SdkGetChannelLabel(uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position) const;
    cbSdkResult SdkSetChannelLabel(uint16_t channel, const char * label, uint32_t userflags, int32_t * position) const;
    cbSdkResult SdkGetTrialData(uint32_t bSeek, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);
    cbSdkResult SdkInitTrialData(uint32_t bResetClock, cbSdkTrialEvent* trialevent, cbSdkTrialCont * trialcont,
                                 cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, unsigned long wait_for_comment_msec = 250);
    cbSdkResult SdkSetFileConfig(const char * filename, const char * comment, uint32_t bStart, uint32_t options);
    cbSdkResult SdkGetFileConfig(char * filename, char * username, bool * pbRecording) const;
    cbSdkResult SdkSetPatientInfo(const char * ID, const char * firstname, const char * lastname,
                                  uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear) const;
    cbSdkResult SdkInitiateImpedance() const;
    cbSdkResult SdkSendPoll(const char* appname, uint32_t mode, uint32_t flags, uint32_t extra);
    cbSdkResult SdkSendPacket(void * ppckt) const;
    cbSdkResult SdkSetSystemRunLevel(uint32_t runlevel, uint32_t locked, uint32_t resetque) const;
    cbSdkResult SdkGetSystemRunLevel(uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque) const;
    cbSdkResult SdkSetDigitalOutput(uint16_t channel, uint16_t value) const;
    cbSdkResult SdkSetSynchOutput(uint16_t channel, uint32_t nFreq, uint32_t nRepeats) const;
    cbSdkResult SdkExtDoCommand(const cbSdkExtCmd * extCmd) const;
    cbSdkResult SdkSetAnalogOutput(uint16_t channel, const cbSdkWaveformData * wf, const cbSdkAoutMon * mon) const;
    cbSdkResult SdkSetChannelMask(uint16_t channel, uint32_t bActive);
    cbSdkResult SdkSetComment(uint32_t rgba, uint8_t charset, const char * comment) const;
    cbSdkResult SdkSetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo) const;
    cbSdkResult SdkGetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo) const;
    cbSdkResult SdkGetSampleGroupList(uint32_t proc, uint32_t group, uint32_t *length, uint16_t *list) const;
    cbSdkResult SdkGetSampleGroupInfo(uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length) const;
    cbSdkResult SdkSetAinpSampling(uint32_t chan, uint32_t filter, uint32_t group) const;
    cbSdkResult SdkSetAinpSpikeOptions(uint32_t chan, uint32_t flags, uint32_t filter) const;
    cbSdkResult SdkGetFilterDesc(uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc) const;
    cbSdkResult SdkGetTrackObj(char * name, uint16_t * type, uint16_t * pointCount, uint32_t id) const;
    cbSdkResult SdkGetVideoSource(char * name, float * fps, uint32_t id) const;
    cbSdkResult SdkSetSpikeConfig(uint32_t spklength, uint32_t spkpretrig) const;
    cbSdkResult SdkGetSysConfig(uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq) const;
    cbSdkResult SdkSystem(cbSdkSystemType cmd) const;
    cbSdkResult SdkCallbackStatus(cbSdkCallbackType callbackType) const;
    cbSdkResult SdkRegisterCallback(cbSdkCallbackType callbackType, cbSdkCallback pCallbackFn, void * pCallbackData);
    cbSdkResult SdkUnRegisterCallback(cbSdkCallbackType callbackType);
    cbSdkResult SdkAnalogToDigital(uint16_t channel, const char * szVoltsUnitString, int32_t * digital) const;


protected:
    void OnInstNetworkEvent(NetEventType type, unsigned int code) override; // Event from the instrument network
    bool m_bInitialized; // If initialized
    cbRESULT m_lastCbErr; // Last error

    // Wait condition for connection open
    std::condition_variable m_connectWait;
    std::mutex m_connectLock;

    // Which channels to listen to
    bool m_bChannelMask[cbMAXCHANS]{};
    cbPKT_VIDEOSYNCH m_lastPktVideoSynch{}; // last video synchronization packet

    cbSdkPktLostEvent m_lastLost{}; // Last lost event
    cbSdkInstInfo m_lastInstInfo{}; // Last instrument info event

    // Lock for accessing the callbacks
    std::mutex m_lockCallback;
    // Actual registered callbacks
    cbSdkCallback m_pCallback[CBSDKCALLBACK_COUNT]{};
    void * m_pCallbackParams[CBSDKCALLBACK_COUNT]{};
    // Late bound versions are internal
    cbSdkCallback m_pLateCallback[CBSDKCALLBACK_COUNT]{};
    void * m_pLateCallbackParams[CBSDKCALLBACK_COUNT]{};

    /////////////////////////////////////////////////////////////////////////////
    // Declarations for tracking the beginning and end of trials

    std::mutex m_lockTrial;
    std::mutex m_lockTrialEvent;
    std::mutex m_lockTrialComment;
    std::mutex m_lockTrialTracking;

    // For synchronization of threads
    std::mutex m_lockGetPacketsEvent;
    std::condition_variable m_waitPacketsEvent;
    bool m_bPacketsEvent;

    std::mutex m_lockGetPacketsCmt;
    std::condition_variable m_waitPacketsCmt;
    bool m_bPacketsCmt;

    std::mutex m_lockGetPacketsTrack;
    std::condition_variable m_waitPacketsTrack;
    bool m_bPacketsTrack;

    uint16_t m_uTrialBeginChannel;  // Channel ID that is watched for the trial begin notification
    uint32_t m_uTrialBeginMask;     // Mask ANDed with channel data to check for trial beginning
    uint32_t m_uTrialBeginValue;    // Value the masked data is compared to identify trial beginning
    uint16_t m_uTrialEndChannel;    // Channel ID that is watched for the trial end notification
    uint32_t m_uTrialEndMask;       // Mask ANDed with channel data to check for trial end
    uint32_t m_uTrialEndValue;      // Value the masked data is compared to identify trial end
    uint32_t m_uTrialWaveforms;     // If spike waveform should be stored and returned
    uint32_t m_uTrialConts;         // Number of continuous data to buffer
    uint32_t m_uTrialEvents;        // Number of events to buffer
    uint32_t m_uTrialComments;      // Number of comments to buffer
    uint32_t m_uTrialTrackings;     // Number of tracking data to buffer
    uint32_t m_bWithinTrial;        // True is we are within a trial, False if not within a trial
    PROCTIME m_uTrialStartTime;     // Holds the Cerebus timestamp of the trial start time
    PROCTIME m_uCbsdkTime;          // Holds the Cerebus timestamp of the last packet received
    PROCTIME m_nextTrialStartTime;  // TrialStartTime will be updated to this after GetData if InitData has bResetClock=true

    /////////////////////////////////////////////////////////////////////////////
    // Declarations for the data caching structures and variables

    // Continuous data structures are defined in ContinuousData.h
    ContinuousData * m_CD;

    // Event data structures are defined in EventData.h
    EventData * m_ED;

    // Structure to store all the variables associated with the comment data
    struct CommentData
    {
        uint32_t size; // default is 0
        uint8_t * charset;
        uint32_t * rgba;
        uint8_t * * comments;
        PROCTIME * timestamps;
        uint32_t write_index;
        uint32_t write_start_index;

        void reset()
        {
            for (uint32_t i = 0; i < size; ++i)
            {
                memset(comments[i], 0, (cbMAX_COMMENT + 1) * sizeof(uint8_t));
                timestamps[i] = rgba[i] =  charset[i] = 0;
            }
            write_index = write_start_index = 0;
        }

    } * m_CMT;

    // Structure to store all the variables associated with the video tracking data
    struct TrackingData
    {
        uint32_t size; // default is 0
        uint16_t max_point_counts[cbMAXTRACKOBJ];
        uint8_t  node_name[cbMAXTRACKOBJ][cbLEN_STR_LABEL + 1];
        uint16_t node_type[cbMAXTRACKOBJ]; // cbTRACKOBJ_TYPE_* (note that 0 means undefined)
        uint16_t * point_counts[cbMAXTRACKOBJ];
        void * * coords[cbMAXTRACKOBJ];
        PROCTIME * timestamps[cbMAXTRACKOBJ];
        uint32_t * synch_frame_numbers[cbMAXTRACKOBJ];
        uint32_t * synch_timestamps[cbMAXTRACKOBJ];
        uint32_t write_index[cbMAXTRACKOBJ];
        uint32_t write_start_index[cbMAXTRACKOBJ];

        void reset()
        {
            if (size)
            {
                for (uint32_t i = 0; i < cbMAXTRACKOBJ; ++i)
                {
                    std::fill_n(point_counts[i], size, 0);
                    std::fill_n(timestamps[i], size, 0);
                    std::fill_n(synch_frame_numbers[i], size, 0);
                    std::fill_n(synch_timestamps[i], size, 0);
                }
            }

            memset(max_point_counts, 0, sizeof(max_point_counts));
            memset(node_name, 0, sizeof(node_name));
            memset(node_type, 0, sizeof(node_type));
            memset(write_index, 0, sizeof(write_index));
            memset(write_start_index, 0, sizeof(write_start_index));
        }

    } * m_TR;
};

#endif // SDKAPP_H_INCLUDED
