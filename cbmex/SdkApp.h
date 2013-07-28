/* =STS=> SdkApp.h[5022].aa01   open     SMID:1 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
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

#ifndef SDKAPP_H_INCLUDED
#define SDKAPP_H_INCLUDED

#include "InstNetwork.h"
#include "cbsdk.h"
#include "CCFUtils.h"
#include <QMutex>
#include <QWaitCondition>

// Wrapper class for SDK Qt application
class SdkApp : public InstNetwork, public InstNetwork::Listener
{
public:
    SdkApp();
    ~SdkApp();
public:
    void ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt); // Process incoming packets
    UINT32 GetInstInfo() {return m_instInfo;}
    cbRESULT GetLastCbErr() {return m_lastCbErr;}
    void Open(UINT32 id, int nInPort = cbNET_UDP_PORT_BCAST, int nOutPort = cbNET_UDP_PORT_CNT,
        LPCSTR szInIP = cbNET_UDP_ADDR_INST, LPCSTR szOutIP = cbNET_UDP_ADDR_CNT, int nRecBufSize = NSP_REC_BUF_SIZE);
private:
    void OnPktGroup(const cbPKT_GROUP * const pkt);
    void OnPktEvent(const cbPKT_GENERIC * const pPkt);
    void OnPktComment(const cbPKT_COMMENT * const pPkt);
    void OnPktTrack(const cbPKT_VIDEOTRACK * const pPkt);

    void LateBindCallback(cbSdkCallbackType callbacktype);
    void LinkFailureEvent(cbSdkPktLostEvent & lost);
    void InstInfoEvent(UINT32 instInfo);
    cbSdkResult unsetTrialConfig(cbSdkTrialType type);

public:
    // ---------------------------
    // All SDK functions come here
    // ---------------------------

    void SdkAsynchCCF(const ccf::ccfResult res, LPCSTR szFileName, const cbStateCCF state, const UINT32 nProgress);
    cbSdkResult SdkGetVersion(cbSdkVersion *version);
    cbSdkResult SdkReadCCF(cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend, bool bThreaded);
    cbSdkResult SdkWriteCCF(cbSdkCCF * pData, const char * szFileName, bool bThreaded);
    cbSdkResult SdkOpen(UINT32 nInstance, cbSdkConnectionType conType, cbSdkConnection con);
    cbSdkResult SdkGetType(cbSdkConnectionType * conType, cbSdkInstrumentType * instType);
    cbSdkResult SdkUnsetTrialConfig(cbSdkTrialType type);
    cbSdkResult SdkClose();
    cbSdkResult SdkGetTime(UINT32 * cbtime);
    cbSdkResult SdkGetSpkCache(UINT16 channel, cbSPKCACHE **cache);
    cbSdkResult SdkGetTrialConfig(UINT32 * pbActive, UINT16 * pBegchan, UINT32 * pBegmask, UINT32 * pBegval,
                                  UINT16 * pEndchan, UINT32 * pEndmask, UINT32 * pEndval, bool * pbDouble,
                                  UINT32 * puWaveforms, UINT32 * puConts, UINT32 * puEvents,
                                  UINT32 * puComments, UINT32 * puTrackings, bool * pbAbsolute);
    cbSdkResult SdkSetTrialConfig(UINT32 bActive, UINT16 begchan, UINT32 begmask, UINT32 begval,
                                  UINT16 endchan, UINT32 endmask, UINT32 endval, bool bDouble,
                                  UINT32 uWaveforms, UINT32 uConts, UINT32 uEvents, UINT32 uComments, UINT32 uTrackings,
                                  bool bAbsolute);
    cbSdkResult SdkGetChannelLabel(UINT16 channel, UINT32 * bValid, char * label, UINT32 * userflags, INT32 * position);
    cbSdkResult SdkSetChannelLabel(UINT16 channel, const char * label, UINT32 userflags, INT32 * position);
    cbSdkResult SdkGetTrialData(UINT32 bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);
    cbSdkResult SdkInitTrialData(cbSdkTrialEvent* trialevent, cbSdkTrialCont * trialcont,
                                 cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);
    cbSdkResult SdkSetFileConfig(const char * filename, const char * comment, UINT32 bStart, UINT32 options);
    cbSdkResult SdkGetFileConfig(char * filename, char * username, bool * pbRecording);
    cbSdkResult SdkSetPatientInfo(const char * ID, const char * firstname, const char * lastname,
                                  UINT32 DOBMonth, UINT32 DOBDay, UINT32 DOBYear);
    cbSdkResult SdkInitiateImpedance();
    cbSdkResult SdkSendPoll(const char* appname, UINT32 mode, UINT32 flags, UINT32 extra);
    cbSdkResult SdkSendPacket(void * ppckt);
    cbSdkResult SdkSetSystemRunLevel(UINT32 runlevel, UINT32 locked, UINT32 resetque);
    cbSdkResult SdkSetDigitalOutput(UINT16 channel, UINT16 value);
    cbSdkResult SdkSetSynchOutput(UINT16 channel, UINT32 nFreq, UINT32 nRepeats);
    cbSdkResult SdkExtDoCommand(cbSdkExtCmd * extCmd);
    cbSdkResult SdkSetAnalogOutput(UINT16 channel, cbSdkWaveformData * wf, cbSdkAoutMon * mon);
    cbSdkResult SdkSetChannelMask(UINT16 channel, UINT32 bActive);
    cbSdkResult SdkSetComment(UINT32 rgba, UINT8 charset, const char * comment);
    cbSdkResult SdkSetChannelConfig(UINT16 channel, cbPKT_CHANINFO * chaninfo);
    cbSdkResult SdkGetChannelConfig(UINT16 channel, cbPKT_CHANINFO * chaninfo);
    cbSdkResult SdkGetSampleGroupInfo(UINT32 proc, UINT32 group, char *label, UINT32 *period, UINT32 *length);
    cbSdkResult SdkGetSampleGroupList(UINT32 proc, UINT32 group, UINT32 *length, UINT32 *list);
    cbSdkResult SdkGetFilterDesc(UINT32 proc, UINT32 filt, cbFILTDESC * filtdesc);
    cbSdkResult SdkGetTrackObj(char * name, UINT16 * type, UINT16 * pointCount, UINT32 id);
    cbSdkResult SdkGetVideoSource(char * name, float * fps, UINT32 id);
    cbSdkResult SdkSetSpikeConfig(UINT32 spklength, UINT32 spkpretrig);
    cbSdkResult SdkGetSysConfig(UINT32 * spklength, UINT32 * spkpretrig, UINT32 * sysfreq);
    cbSdkResult SdkSystem(cbSdkSystemType cmd);
    cbSdkResult SdkRegisterCallback(cbSdkCallbackType callbacktype, cbSdkCallback pCallbackFn, void * pCallbackData);
    cbSdkResult SdkUnRegisterCallback(cbSdkCallbackType callbacktype);
    cbSdkResult SdkAnalogToDigital(UINT16 channel, const char * szVoltsUnitString, INT32 * digital);


private:
    void OnInstNetworkEvent(NetEventType type, unsigned int code); // Event from the instrument network
    bool m_bInitialized; // If initialized
    cbRESULT m_lastCbErr; // Last error

    // Wait condition for connection open
    QWaitCondition m_connectWait;
    QMutex m_connectLock;

    // Which channels to listen to
    bool m_bChannelMask[cbMAXCHANS];
    cbPKT_VIDEOSYNCH m_lastPktVideoSynch; // last video synchronization packet

    cbSdkPktLostEvent m_lastLost; // Last lost event
    cbSdkInstInfo m_lastInstInfo; // Last instrument info event

    // Lock for accessing the callbacks
    QMutex m_lockCallback;
    // Actual registered callbacks
    cbSdkCallback m_pCallback[CBSDKCALLBACK_COUNT];
    void * m_pCallbackParams[CBSDKCALLBACK_COUNT];
    // Late bound versions are internal
    cbSdkCallback m_pLateCallback[CBSDKCALLBACK_COUNT];
    void * m_pLateCallbackParams[CBSDKCALLBACK_COUNT];

    /////////////////////////////////////////////////////////////////////////////
    // Declarations for tracking the beginning and end of trials

    QMutex m_lockTrial;
    QMutex m_lockTrialEvent;
    QMutex m_lockTrialComment;
    QMutex m_lockTrialTracking;

    UINT16 m_uTrialBeginChannel;  // Channel ID that is watched for the trial begin notification
    UINT32 m_uTrialBeginMask;     // Mask ANDed with channel data to check for trial beginning
    UINT32 m_uTrialBeginValue;    // Value the masked data is compared to identify trial beginning
    UINT16 m_uTrialEndChannel;    // Channel ID that is watched for the trial end notification
    UINT32 m_uTrialEndMask;       // Mask ANDed with channel data to check for trial end
    UINT32 m_uTrialEndValue;      // Value the masked data is compared to identify trial end
    bool   m_bTrialDouble;        // If data storage (spike or continuous) is double
    bool   m_bTrialAbsolute;      // Absolute trial timing all events
    UINT32 m_uTrialWaveforms;     // If spike waveform should be stored and returned
    UINT32 m_uTrialConts;         // Number of continuous data to buffer
    UINT32 m_uTrialEvents;        // Number of events to buffer
    UINT32 m_uTrialComments;      // Number of comments to buffer
    UINT32 m_uTrialTrackings;     // Number of tracking data to buffer

    UINT32 m_bWithinTrial;        // True is we are within a trial, False if not within a trial

    UINT32 m_uTrialStartTime;     // Holds the 32-bit Cerebus timestamp of the trial start time

    UINT32 m_uCbsdkTime;            // Holds the 32-bit Cerebus timestamp of the last packet received

    /////////////////////////////////////////////////////////////////////////////
    // Declarations for the data caching structures and variables

    // Structure to store all of the variables associated with the continuous data
    struct ContinuousData
    {
        UINT32 size; // default is cbSdk_CONTINUOUS_DATA_SAMPLES
        UINT16 current_sample_rates[cbNUM_ANALOG_CHANS];        // The continuous sample rate on each channel, in samples/s
        INT16 * continuous_channel_data[cbNUM_ANALOG_CHANS];
        UINT32 write_index[cbNUM_ANALOG_CHANS];                 // next index location to write data
        UINT32 write_start_index[cbNUM_ANALOG_CHANS];           // index location that writing began

        void reset()
        {
            if (size)
            {
                for (UINT32 i = 0; i < cbNUM_ANALOG_CHANS; ++i)
                    memset(continuous_channel_data[i], 0, size * sizeof(INT16));
            }
            memset(current_sample_rates, 0, sizeof(current_sample_rates));
            memset(write_index, 0, sizeof(write_index));
            memset(write_start_index, 0, sizeof(write_start_index));
        }

    } * m_CD;

    // Structure to store all of the variables associated with the event data
    struct EventData
    {
        UINT32 size; // default is cbSdk_EVENT_DATA_SAMPLES
        UINT32 * timestamps[cbNUM_ANALOG_CHANS + 2];
        UINT16 * units[cbNUM_ANALOG_CHANS + 2];
        INT16  * waveform_data; // buffer with maximum size of array [cbNUM_ANALOG_CHANS][size][cbMAX_PNTS]
        UINT32 write_index[cbNUM_ANALOG_CHANS + 2];                  // next index location to write data
        UINT32 write_start_index[cbNUM_ANALOG_CHANS + 2];            // index location that writing began

        void reset()
        {
            if (size)
            {
                for (UINT32 i = 0; i < cbNUM_ANALOG_CHANS + 2; ++i)
                {
                    memset(timestamps[i], 0, size * sizeof(UINT32));
                    memset(units[i], 0, size * sizeof(UINT16));
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
        UINT32 size; // default is 0
        UINT8 * charset;
        UINT32 * rgba;
        UINT8 * * comments;
        UINT32 * timestamps;
        UINT32 write_index;
        UINT32 write_start_index;

        void reset()
        {
            for (UINT32 i = 0; i < size; ++i)
            {
                memset(comments[i], 0, (cbMAX_COMMENT + 1) * sizeof(UINT8));
                timestamps[i] = rgba[i] =  charset[i] = 0;
            }
            write_index = write_start_index = 0;
        }

    } * m_CMT;

    // Structure to store all of the variables associated with the video tracking data
    struct TrackingData
    {
        UINT32 size; // default is 0
        UINT16 max_point_counts[cbMAXTRACKOBJ];
        UINT8  node_name[cbMAXTRACKOBJ][cbLEN_STR_LABEL + 1];
        UINT16 node_type[cbMAXTRACKOBJ]; // cbTRACKOBJ_TYPE_* (note that 0 means undefined)
        UINT16 * point_counts[cbMAXTRACKOBJ];
        void * * coords[cbMAXTRACKOBJ];
        UINT32 * timestamps[cbMAXTRACKOBJ];
        UINT32 * synch_frame_numbers[cbMAXTRACKOBJ];
        UINT32 * synch_timestamps[cbMAXTRACKOBJ];
        UINT32 write_index[cbMAXTRACKOBJ];
        UINT32 write_start_index[cbMAXTRACKOBJ];

        void reset()
        {
            if (size)
            {
                for (UINT32 i = 0; i < cbMAXTRACKOBJ; ++i)
                {
                    memset(point_counts[i], 0, size * sizeof(UINT16));
                    memset(timestamps[i], 0, size * sizeof(UINT32));
                    memset(synch_frame_numbers[i], 0, size * sizeof(UINT32));
                    memset(synch_timestamps[i], 0, size * sizeof(UINT32));
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
