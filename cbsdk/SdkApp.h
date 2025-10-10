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
#include "cbsdk.h"
#include "CCFUtils.h"
#include <mutex>
#include <condition_variable>

/// Wrapper class for SDK Qt application
class SdkApp : public InstNetwork, public InstNetwork::Listener
{
public:
    SdkApp();
    ~SdkApp();
public:
    void ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt); // Process incoming packets
    uint32_t GetInstInfo() {return m_instInfo;}
    cbRESULT GetLastCbErr() {return m_lastCbErr;}
    void Open(uint32_t id, int nInPort = cbNET_UDP_PORT_BCAST, int nOutPort = cbNET_UDP_PORT_CNT,
        LPCSTR szInIP = cbNET_UDP_ADDR_INST, LPCSTR szOutIP = cbNET_UDP_ADDR_CNT, int nRecBufSize = NSP_REC_BUF_SIZE, int nRange = 0);
private:
    void OnPktGroup(const cbPKT_GROUP * const pkt);
    void OnPktEvent(const cbPKT_GENERIC * const pPkt);
    void OnPktComment(const cbPKT_COMMENT * const pPkt);
    void OnPktLog(const cbPKT_LOG * const pPkt);
    void OnPktTrack(const cbPKT_VIDEOTRACK * const pPkt);

    void LateBindCallback(cbSdkCallbackType callbacktype);
    void LinkFailureEvent(cbSdkPktLostEvent & lost);
    void InstInfoEvent(uint32_t instInfo);
    cbSdkResult unsetTrialConfig(cbSdkTrialType type);

public:
    // ---------------------------
    // All SDK functions come here
    // ---------------------------

    void SdkAsynchCCF(ccf::ccfResult res, LPCSTR szFileName, cbStateCCF state, uint32_t nProgress);
    cbSdkResult SdkGetVersion(cbSdkVersion *version);
    cbSdkResult SdkReadCCF(cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend, bool bThreaded);  // From file or device if szFileName is null
    cbSdkResult SdkFetchCCF(cbSdkCCF * pData);  // from device only
    cbSdkResult SdkWriteCCF(cbSdkCCF * pData, const char * szFileName, bool bThreaded);  // to file or device if szFileName is null
    cbSdkResult SdkSendCCF(cbSdkCCF * pData, bool bAutosort = false);  // to device only
    cbSdkResult SdkOpen(uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con);
    cbSdkResult SdkGetType(cbSdkConnectionType * conType, cbSdkInstrumentType * instType);
    cbSdkResult SdkUnsetTrialConfig(cbSdkTrialType type);
    cbSdkResult SdkClose();
    cbSdkResult SdkGetTime(PROCTIME * cbtime);
    cbSdkResult SdkGetSpkCache(uint16_t channel, cbSPKCACHE **cache);
    cbSdkResult SdkGetTrialConfig(uint32_t * pbActive, uint16_t * pBegchan, uint32_t * pBegmask, uint32_t * pBegval,
                                  uint16_t * pEndchan, uint32_t * pEndmask, uint32_t * pEndval, bool * pbDouble,
                                  uint32_t * puWaveforms, uint32_t * puConts, uint32_t * puEvents,
                                  uint32_t * puComments, uint32_t * puTrackings, bool * pbAbsolute);
    cbSdkResult SdkSetTrialConfig(uint32_t bActive, uint16_t begchan, uint32_t begmask, uint32_t begval,
                                  uint16_t endchan, uint32_t endmask, uint32_t endval, bool bDouble,
                                  uint32_t uWaveforms, uint32_t uConts, uint32_t uEvents, uint32_t uComments, uint32_t uTrackings,
                                  bool bAbsolute);
    cbSdkResult SdkGetChannelLabel(uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position);
    cbSdkResult SdkSetChannelLabel(uint16_t channel, const char * label, uint32_t userflags, int32_t * position);
    cbSdkResult SdkGetTrialData(uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);
    cbSdkResult SdkInitTrialData(uint32_t bActive, cbSdkTrialEvent* trialevent, cbSdkTrialCont * trialcont,
                                 cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, unsigned long wait_for_comment_msec = 250);
    cbSdkResult SdkSetFileConfig(const char * filename, const char * comment, uint32_t bStart, uint32_t options);
    cbSdkResult SdkGetFileConfig(char * filename, char * username, bool * pbRecording);
    cbSdkResult SdkSetPatientInfo(const char * ID, const char * firstname, const char * lastname,
                                  uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear);
    cbSdkResult SdkInitiateImpedance();
    cbSdkResult SdkSendPoll(const char* appname, uint32_t mode, uint32_t flags, uint32_t extra);
    cbSdkResult SdkSendPacket(void * ppckt);
    cbSdkResult SdkSetSystemRunLevel(uint32_t runlevel, uint32_t locked, uint32_t resetque);
    cbSdkResult SdkGetSystemRunLevel(uint32_t * runlevel, uint32_t * runflags, uint32_t * resetque);
    cbSdkResult SdkSetDigitalOutput(uint16_t channel, uint16_t value);
    cbSdkResult SdkSetSynchOutput(uint16_t channel, uint32_t nFreq, uint32_t nRepeats);
    cbSdkResult SdkExtDoCommand(cbSdkExtCmd * extCmd);
    cbSdkResult SdkSetAnalogOutput(uint16_t channel, cbSdkWaveformData * wf, cbSdkAoutMon * mon);
    cbSdkResult SdkSetChannelMask(uint16_t channel, uint32_t bActive);
    cbSdkResult SdkSetComment(uint32_t rgba, uint8_t charset, const char * comment);
    cbSdkResult SdkSetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo);
    cbSdkResult SdkGetChannelConfig(uint16_t channel, cbPKT_CHANINFO * chaninfo);
    cbSdkResult SdkGetSampleGroupList(uint32_t proc, uint32_t group, uint32_t *length, uint16_t *list);
    cbSdkResult SdkGetSampleGroupInfo(uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length);
    cbSdkResult SdkSetAinpSampling(uint32_t chan, uint32_t filter, uint32_t group);
    cbSdkResult SdkSetAinpSpikeOptions(uint32_t chan, uint32_t flags, uint32_t filter);
    cbSdkResult SdkGetFilterDesc(uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc);
    cbSdkResult SdkGetTrackObj(char * name, uint16_t * type, uint16_t * pointCount, uint32_t id);
    cbSdkResult SdkGetVideoSource(char * name, float * fps, uint32_t id);
    cbSdkResult SdkSetSpikeConfig(uint32_t spklength, uint32_t spkpretrig);
    cbSdkResult SdkGetSysConfig(uint32_t * spklength, uint32_t * spkpretrig, uint32_t * sysfreq);
    cbSdkResult SdkSystem(cbSdkSystemType cmd);
    cbSdkResult SdkCallbackStatus(cbSdkCallbackType callbacktype);
    cbSdkResult SdkRegisterCallback(cbSdkCallbackType callbacktype, cbSdkCallback pCallbackFn, void * pCallbackData);
    cbSdkResult SdkUnRegisterCallback(cbSdkCallbackType callbacktype);
    cbSdkResult SdkAnalogToDigital(uint16_t channel, const char * szVoltsUnitString, int32_t * digital);


protected:
    void OnInstNetworkEvent(NetEventType type, unsigned int code) override; // Event from the instrument network
    bool m_bInitialized; // If initialized
    cbRESULT m_lastCbErr; // Last error

    // Wait condition for connection open
    std::condition_variable m_connectWait;
    std::mutex m_connectLock;

    // Which channels to listen to
    bool m_bChannelMask[cbMAXCHANS];
    cbPKT_VIDEOSYNCH m_lastPktVideoSynch; // last video synchronization packet

    cbSdkPktLostEvent m_lastLost; // Last lost event
    cbSdkInstInfo m_lastInstInfo; // Last instrument info event

    // Lock for accessing the callbacks
    std::mutex m_lockCallback;
    // Actual registered callbacks
    cbSdkCallback m_pCallback[CBSDKCALLBACK_COUNT];
    void * m_pCallbackParams[CBSDKCALLBACK_COUNT];
    // Late bound versions are internal
    cbSdkCallback m_pLateCallback[CBSDKCALLBACK_COUNT];
    void * m_pLateCallbackParams[CBSDKCALLBACK_COUNT];

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
    bool   m_bTrialDouble;        // If data storage (spike or continuous) is double
    bool   m_bTrialAbsolute;      // Absolute trial timing all events
    uint32_t m_uTrialWaveforms;     // If spike waveform should be stored and returned
    uint32_t m_uTrialConts;         // Number of continuous data to buffer
    uint32_t m_uTrialEvents;        // Number of events to buffer
    uint32_t m_uTrialComments;      // Number of comments to buffer
    uint32_t m_uTrialTrackings;     // Number of tracking data to buffer

    uint32_t m_bWithinTrial;        // True is we are within a trial, False if not within a trial

    PROCTIME m_uTrialStartTime;     // Holds theCerebus timestamp of the trial start time
    PROCTIME m_uPrevTrialStartTime;

    PROCTIME m_uCbsdkTime;            // Holds the Cerebus timestamp of the last packet received

    /////////////////////////////////////////////////////////////////////////////
    // Declarations for the data caching structures and variables

    // Structure to store all of the variables associated with the continuous data
    struct ContinuousData
    {
        uint32_t size; // default is cbSdk_CONTINUOUS_DATA_SAMPLES
        uint16_t current_sample_rates[cbNUM_ANALOG_CHANS];        // The continuous sample rate on each channel, in samples/s
        int16_t * continuous_channel_data[cbNUM_ANALOG_CHANS];
        uint32_t write_index[cbNUM_ANALOG_CHANS];                 // next index location to write data
        uint32_t write_start_index[cbNUM_ANALOG_CHANS];           // index location that writing began
        uint32_t read_end_index[cbNUM_ANALOG_CHANS];              // index location that reading will end. set in InitTrialData and used in GetTrialData

        void reset()
        {
            if (size)
            {
                for (uint32_t i = 0; i < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans(); ++i)
                    memset(continuous_channel_data[i], 0, size * sizeof(int16_t));
            }
            memset(current_sample_rates, 0, sizeof(current_sample_rates));
            memset(write_index, 0, sizeof(write_index));
            memset(write_start_index, 0, sizeof(write_start_index));
        }

    } * m_CD;

    // Structure to store all of the variables associated with the event data
    struct EventData
    {
        // We use cbMAXCHANS to size the arrays,
        // even though that's more than the analog + digin + serial channels than are required,
        // simply so we can index into these arrays using the channel number (-1).
        // The alternative is to map between channel number and array index, but
        // this is problematic with the recent change to 2-NSP support.
        // Later I may add a m_ChIdxInType or m_ChIdxInBuff for such a map.
        // - chadwick.boulay@gmail.com
        uint32_t size; // default is cbSdk_EVENT_DATA_SAMPLES
        uint32_t * timestamps[cbMAXCHANS];
        uint16_t * units[cbMAXCHANS];
        int16_t  * waveform_data; // buffer with maximum size of array [cbNUM_ANALOG_CHANS][size][cbMAX_PNTS]
        uint32_t write_index[cbMAXCHANS];                  // next index location to write data
        uint32_t write_start_index[cbMAXCHANS];            // index location that writing began

        void reset()
        {
            if (size)
            {
                for (uint32_t i = 0; i < (cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans() + 2); ++i)
                {
                    memset(timestamps[i], 0, size * sizeof(uint32_t));
                    memset(units[i], 0, size * sizeof(uint16_t));
                }
            }
            waveform_data = NULL;
            memset(write_index, 0, sizeof(write_index));
            memset(write_start_index, 0, sizeof(write_start_index));
        }

    } * m_ED;

    // Structure to store all of the variables associated with the comment data
    struct CommentData
    {
        uint32_t size; // default is 0
        uint8_t * charset;
        uint32_t * rgba;
        uint8_t * * comments;
        uint32_t * timestamps;
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

    // Structure to store all of the variables associated with the video tracking data
    struct TrackingData
    {
        uint32_t size; // default is 0
        uint16_t max_point_counts[cbMAXTRACKOBJ];
        uint8_t  node_name[cbMAXTRACKOBJ][cbLEN_STR_LABEL + 1];
        uint16_t node_type[cbMAXTRACKOBJ]; // cbTRACKOBJ_TYPE_* (note that 0 means undefined)
        uint16_t * point_counts[cbMAXTRACKOBJ];
        void * * coords[cbMAXTRACKOBJ];
        uint32_t * timestamps[cbMAXTRACKOBJ];
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
                    memset(point_counts[i], 0, size * sizeof(uint16_t));
                    memset(timestamps[i], 0, size * sizeof(uint32_t));
                    memset(synch_frame_numbers[i], 0, size * sizeof(uint32_t));
                    memset(synch_timestamps[i], 0, size * sizeof(uint32_t));
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

#endif // include guard
