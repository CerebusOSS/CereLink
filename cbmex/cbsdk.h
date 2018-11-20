/* =STS=> cbsdk.h[4901].aa20   submit     SMID:22 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: cbsdk.h $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/cbsdk.h $
// $Revision: 1 $
// $Date: 2/17/11 3:15p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// Notes:
//  Only functions are exported, no data, and no classes
//  No exception is thrown, every function returns an error code that should be consulted by caller
//  Each instance will run one light thread that is responsible for networking, buffering and callbacks
//  Additional threads may be created by Qt based on platforms
// Note for developers:
//   Data structure in cbsdk should be decoupled from cbhlib as much as possible because cbhlib may change at any version
//   for some functions it is easier to just rely on the data structure as defined in cbhwlib (e.g. callbacks and CCF),
//   but this may change in future to give cbsdk a more stable and independent API
//
/**
* \file cbsdk.h
* \brief Cerebus SDK - This header file is distributed as part of the SDK.
*/

#ifndef CBSDK_H_INCLUDED
#define CBSDK_H_INCLUDED

#include "cbhwlib.h"

#ifdef STATIC_CBSDK_LINK
#undef CBSDK_EXPORTS
#endif

#ifdef WIN32
// Windows shared library
#ifdef CBSDK_EXPORTS
    #define CBSDKAPI __declspec(dllexport)
#elif ! defined(STATIC_CBSDK_LINK)
    #define CBSDKAPI __declspec(dllimport)
#else
    #define CBSDKAPI
#endif
#else
// Non Windows shared library
#ifdef CBSDK_EXPORTS
    #define CBSDKAPI __attribute__ ((visibility ("default")))
#else
    #define CBSDKAPI
#endif
#endif

/**
* \brief Library version information.
*/
typedef struct _cbSdkVersion
{
    // Library version
    uint32_t major;
    uint32_t minor;
    uint32_t release;
    uint32_t beta;
    // Protocol version
    uint32_t majorp;
    uint32_t minorp;
    // NSP version
    uint32_t nspmajor;
    uint32_t nspminor;
    uint32_t nsprelease;
    uint32_t nspbeta;
    // NSP protocol version
    uint32_t nspmajorp;
    uint32_t nspminorp;
} cbSdkVersion;

/// cbSdk return values
typedef enum _cbSdkResult
{
    CBSDKRESULT_WARNCONVERT            =     3, ///< If file conversion is needed
    CBSDKRESULT_WARNCLOSED             =     2, ///< Library is already closed
    CBSDKRESULT_WARNOPEN               =     1, ///< Library is already opened
    CBSDKRESULT_SUCCESS                =     0, ///< Successful operation
    CBSDKRESULT_NOTIMPLEMENTED         =    -1, ///< Not implemented
    CBSDKRESULT_UNKNOWN                =    -2, ///< Unknown error
    CBSDKRESULT_INVALIDPARAM           =    -3, ///< Invalid parameter
    CBSDKRESULT_CLOSED                 =    -4, ///< Interface is closed cannot do this operation
    CBSDKRESULT_OPEN                   =    -5, ///< Interface is open cannot do this operation
    CBSDKRESULT_NULLPTR                =    -6, ///< Null pointer
    CBSDKRESULT_ERROPENCENTRAL         =    -7, ///< Unable to open Central interface
    CBSDKRESULT_ERROPENUDP             =    -8, ///< Unable to open UDP interface (might happen if default)
    CBSDKRESULT_ERROPENUDPPORT         =    -9, ///< Unable to open UDP port
    CBSDKRESULT_ERRMEMORYTRIAL         =   -10, ///< Unable to allocate RAM for trial cache data
    CBSDKRESULT_ERROPENUDPTHREAD       =   -11, ///< Unable to open UDP timer thread
    CBSDKRESULT_ERROPENCENTRALTHREAD   =   -12, ///< Unable to open Central communication thread
    CBSDKRESULT_INVALIDCHANNEL         =   -13, ///< Invalid channel number
    CBSDKRESULT_INVALIDCOMMENT         =   -14, ///< Comment too long or invalid
    CBSDKRESULT_INVALIDFILENAME        =   -15, ///< Filename too long or invalid
    CBSDKRESULT_INVALIDCALLBACKTYPE    =   -16, ///< Invalid callback type
    CBSDKRESULT_CALLBACKREGFAILED      =   -17, ///< Callback register/unregister failed
    CBSDKRESULT_ERRCONFIG              =   -18, ///< Trying to run an unconfigured method
    CBSDKRESULT_INVALIDTRACKABLE       =   -19, ///< Invalid trackable id, or trackable not present
    CBSDKRESULT_INVALIDVIDEOSRC        =   -20, ///< Invalid video source id, or video source not present
    CBSDKRESULT_ERROPENFILE            =   -21, ///< Cannot open file
    CBSDKRESULT_ERRFORMATFILE          =   -22, ///< Wrong file format
    CBSDKRESULT_OPTERRUDP              =   -23, ///< Socket option error (possibly permission issue)
    CBSDKRESULT_MEMERRUDP              =   -24, ///< Socket memory assignment error
    CBSDKRESULT_INVALIDINST            =   -25, ///< Invalid range or instrument address
    CBSDKRESULT_ERRMEMORY              =   -26, ///< library memory allocation error
    CBSDKRESULT_ERRINIT                =   -27, ///< Library initialization error
    CBSDKRESULT_TIMEOUT                =   -28, ///< Conection timeout error
    CBSDKRESULT_BUSY                   =   -29, ///< Resource is busy
    CBSDKRESULT_ERROFFLINE             =   -30, ///< Instrument is offline
    CBSDKRESULT_INSTOUTDATED           =   -31, ///<  The instrument runs an outdated protocol version
    CBSDKRESULT_LIBOUTDATED            =   -32, ///<  The library is outdated
} cbSdkResult;

/// cbSdk Connection Type (Central, UDP, other)
typedef enum _cbSdkConnectionType
{
    CBSDKCONNECTION_DEFAULT = 0, ///< Try Central then UDP
    CBSDKCONNECTION_CENTRAL,     ///< Use Central
    CBSDKCONNECTION_UDP,         ///< Use UDP
    CBSDKCONNECTION_CLOSED,      ///< Closed
    CBSDKCONNECTION_COUNT ///< Allways the last value (Unknown)
} cbSdkConnectionType;

/// Instrument Type
typedef enum _cbSdkInstrumentType
{
    CBSDKINSTRUMENT_NSP = 0,       ///< NSP
    CBSDKINSTRUMENT_NPLAY,         ///< Local nPlay
    CBSDKINSTRUMENT_LOCALNSP,      ///< Local NSP
    CBSDKINSTRUMENT_REMOTENPLAY,   ///< Remote nPlay
    CBSDKINSTRUMENT_COUNT ///< Allways the last value (Invalid)
} cbSdkInstrumentType;

/// CCF operation progress and status
typedef struct _cbSdkCCFEvent
{
    cbStateCCF state;           ///< CCF state
    cbSdkResult result;         ///< Last result
    LPCSTR szFileName;          ///< CCF filename under operation
    uint8_t progress; ///< Progress (in percent)
} cbSdkCCFEvent;

/// Reason for packet loss
typedef enum _cbSdkPktLostEventType
{
    CBSDKPKTLOSTEVENT_UNKNOWN = 0,       ///< Unknown packet lost
    CBSDKPKTLOSTEVENT_LINKFAILURE,       ///< Link failure
    CBSDKPKTLOSTEVENT_PC2NSP,            ///< PC to NSP connection lost
    CBSDKPKTLOSTEVENT_NET,               ///< Network error
} cbSdkPktLostEventType;

/// Packet lost event
typedef struct _cbSdkPktLostEvent
{
    cbSdkPktLostEventType type;     ///< packet lost event type
} cbSdkPktLostEvent;

/// Instrument information
typedef struct _cbSdkInstInfo
{
    uint32_t instInfo;     ///< bitfield of cbINSTINFO_* (0 means closed)
} cbSdkInstInfo;

/// Packet type information
typedef enum _cbSdkPktType
{
    cbSdkPkt_PACKETLOST = 0, ///< will be received only by the first registered callback
                             ///< data points to cbSdkPktLostEvent
    cbSdkPkt_INSTINFO,       ///< data points to cbSdkInstInfo
    cbSdkPkt_SPIKE,          ///< data points to cbPKT_SPK
    cbSdkPkt_DIGITAL,        ///< data points to cbPKT_DINP
    cbSdkPkt_SERIAL,         ///< data points to cbPKT_DINP
    cbSdkPkt_CONTINUOUS,     ///< data points to cbPKT_GROUP
    cbSdkPkt_TRACKING,       ///< data points to cbPKT_VIDEOTRACK
    cbSdkPkt_COMMENT,        ///< data points to cbPKT_COMMENT
    cbSdkPkt_GROUPINFO,      ///< data points to cbPKT_GROUPINFO
    cbSdkPkt_CHANINFO,       ///< data points to cbPKT_CHANINFO
    cbSdkPkt_FILECFG,        ///< data points to cbPKT_FILECFG
    cbSdkPkt_POLL,           ///< data points to cbPKT_POLL
    cbSdkPkt_SYNCH,          ///< data points to cbPKT_VIDEOSYNCH
    cbSdkPkt_NM,             ///< data points to cbPKT_NM
    cbSdkPkt_CCF,            ///< data points to cbSdkCCFEvent
    cbSdkPkt_IMPEDANCE,      ///< data points to cbPKT_IMPEDANCE
    cbSdkPkt_SYSHEARTBEAT,   ///< data points to cbPKT_SYSHEARTBEAT
    cbSdkPkt_LOG,		     ///< data points to cbPKT_LOG
    cbSdkPkt_COUNT ///< Always the last value
} cbSdkPktType;

/// Type of events to monitor
typedef enum _cbSdkCallbackType
{
    CBSDKCALLBACK_ALL = 0,      ///< Monitor all events
    CBSDKCALLBACK_INSTINFO,     ///< Monitor instrument connection information
    CBSDKCALLBACK_SPIKE,        ///< Monitor spike events
    CBSDKCALLBACK_DIGITAL,      ///< Monitor digital input events
    CBSDKCALLBACK_SERIAL,       ///< Monitor serial input events
    CBSDKCALLBACK_CONTINUOUS,   ///< Monitor continuous events
    CBSDKCALLBACK_TRACKING,     ///< Monitor video tracking events
    CBSDKCALLBACK_COMMENT,      ///< Monitor comment or custom events
    CBSDKCALLBACK_GROUPINFO,    ///< Monitor channel group info events
    CBSDKCALLBACK_CHANINFO,     ///< Monitor channel info events
    CBSDKCALLBACK_FILECFG,      ///< Monitor file config events
    CBSDKCALLBACK_POLL,         ///< respond to poll
    CBSDKCALLBACK_SYNCH,        ///< Monitor video synchronizarion events
    CBSDKCALLBACK_NM,           ///< Monitor NeuroMotive events
    CBSDKCALLBACK_CCF,          ///< Monitor CCF events
    CBSDKCALLBACK_IMPEDENCE,    ///< Monitor impedence events
    CBSDKCALLBACK_SYSHEARTBEAT, ///< Monitor system heartbeats (100 times a second)
    CBSDKCALLBACK_LOG,		    ///< Monitor system heartbeats (100 times a second)
    CBSDKCALLBACK_COUNT  ///< Always the last value
} cbSdkCallbackType;

/// Trial type
typedef enum _cbSdkTrialType
{
    CBSDKTRIAL_CONTINUOUS,
    CBSDKTRIAL_EVENTS,
    CBSDKTRIAL_COMMETNS,
    CBSDKTRIAL_TRACKING,
} cbSdkTrialType;

// Central uses them for their file name extensions (ns1, ns2, ..., ns5)
typedef enum _DefaultSampleGroup {
    SDK_SMPGRP_NONE = 0,
    SDK_SMPGRP_RATE_500 = 1,
    SDK_SMPGRP_RATE_1K = 2,
    SDK_SMPGRP_RATE_2K = 3,
    SDK_SMPGRP_RATE_10k = 4,
    SDK_SMPGRP_RATE_30k = 5,
    SDK_SMPGRP_RAW = 6
} DefaultSampleGroup;

/** Callback details.
 * \n pEventData points to a cbPkt_* structure depending on the type
 * \n pCallbackData is what is used to register the callback
 */
typedef void (* cbSdkCallback)(uint32_t nInstance, const cbSdkPktType type, const void* pEventData, void* pCallbackData);


/// The default number of continuous samples that will be stored per channel in the trial buffer
#define cbSdk_CONTINUOUS_DATA_SAMPLES 102400 // multiple of 4096
/// The default number of events that will be stored per channel in the trial buffer
#define cbSdk_EVENT_DATA_SAMPLES (2 * 8192) // multiple of 4096

/// Maximum file size (in bytes) that is allowed to upload to NSP
#define cbSdk_MAX_UPOLOAD_SIZE (1024 * 1024 * 1024)

/// \todo these should become functions as we may introduce different instruments
#define cbSdk_TICKS_PER_SECOND  30000.0
/// The number of seconds corresponding to one cb clock tick
#define cbSdk_SECONDS_PER_TICK  (1.0 / cbSdk_TICKS_PER_SECOND)

/// Trial spike events
typedef struct _cbSdkTrialEvent
{
    uint16_t count; ///< Number of valid channels in this trial (up to cbNUM_ANALOG_CHANS+2)
    uint16_t chan[cbNUM_ANALOG_CHANS + 2]; ///< channel numbers (1-based)
    uint32_t num_samples[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1]; ///< number of samples
    void * timestamps[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1];   ///< Buffer to hold time stamps
    void * waveforms[cbNUM_ANALOG_CHANS + 2]; ///< Buffer to hold waveforms or digital values
} cbSdkTrialEvent;

/// Connection information
typedef struct _cbSdkConnection
{
    _cbSdkConnection()
    {
        nInPort = cbNET_UDP_PORT_BCAST;
        nOutPort = cbNET_UDP_PORT_CNT;
        #ifdef __APPLE__
			nRecBufSize = (6 * 1024 * 1024); // Despite setting kern.ipc.maxsockbuf=8388608, 8MB is still too much.
        #else
			nRecBufSize = (8 * 1024 * 1024); // 8MB default needed for best performance
        #endif
        szInIP = "";
        szOutIP = "";
    }
    int nInPort;  ///< Client port number
    int nOutPort; ///< Instrument port number
    int nRecBufSize; ///< Receive buffer size (0 to ignore altogether)
    LPCSTR szInIP;  ///< Client IPv4 address
    LPCSTR szOutIP; ///< Instrument IPv4 address
} cbSdkConnection;

/// Trial continuous data
typedef struct _cbSdkTrialCont
{
    uint16_t count; ///< Number of valid channels in this trial (up to cbNUM_ANALOG_CHANS)
    uint16_t chan[cbNUM_ANALOG_CHANS]; ///< Channel numbers (1-based)
    uint16_t sample_rates[cbNUM_ANALOG_CHANS]; ///< Current sample rate (samples per second)
    uint32_t num_samples[cbNUM_ANALOG_CHANS]; ///< Number of samples
    uint32_t time;  ///< Start time for trial continuous data
    void * samples[cbNUM_ANALOG_CHANS]; ///< Buffer to hold sample vectors
} cbSdkTrialCont;

/// Trial comment data
typedef struct _cbSdkTrialComment
{
    uint16_t num_samples; ///< Number of comments
    uint8_t * charsets;   ///< Buffer to hold character sets
    uint32_t * rgbas;     ///< Buffer to hold rgba values
    uint8_t * * comments; ///< Pointer to comments
    void * timestamps;  ///< Buffer to hold time stamps
} cbSdkTrialComment;

/// Trial video tracking data
typedef struct _cbSdkTrialTracking
{
    uint16_t count;								///< Number of valid trackable objects (up to cbMAXTRACKOBJ)
    uint16_t ids[cbMAXTRACKOBJ];					///< Node IDs (holds count elements)
    uint16_t max_point_counts[cbMAXTRACKOBJ];		///< Maximum point counts (holds count elements)
    uint16_t types[cbMAXTRACKOBJ];				///< Node types (can be cbTRACKOBJ_TYPE_* and determines coordinate counts) (holds count elements)
    uint8_t  names[cbMAXTRACKOBJ][cbLEN_STR_LABEL + 1];   ///< Node names (holds count elements)
    uint16_t num_samples[cbMAXTRACKOBJ];			///< Number of samples
    uint16_t * point_counts[cbMAXTRACKOBJ];		///< Buffer to hold number of valid points (up to max_point_counts) (holds count*num_samples elements)
    void * * coords[cbMAXTRACKOBJ] ;			///< Buffer to hold tracking points (holds count*num_samples tarackables, each of max_point_counts points
    uint32_t * synch_frame_numbers[cbMAXTRACKOBJ];///< Buffer to hold synch frame numbers (holds count*num_samples elements)
    uint32_t * synch_timestamps[cbMAXTRACKOBJ];   ///< Buffer to hold synchronized tracking time stamps (in milliseconds) (holds count*num_samples elements)
    void  * timestamps[cbMAXTRACKOBJ];          ///< Buffer to hold tracking time stamps (holds count*num_samples elements)
} cbSdkTrialTracking;

/// Output waveform type
typedef enum _cbSdkWaveformType
{
    cbSdkWaveform_NONE = 0,
    cbSdkWaveform_PARAMETERS,     ///< Parameters
    cbSdkWaveform_SINE,           ///< Sinusoid
    cbSdkWaveform_COUNT, ///< Always the last value
} cbSdkWaveformType;

/// Trigger type
typedef enum _cbSdkWaveformTriggerType
{
    cbSdkWaveformTrigger_NONE = 0, ///< Instant software trigger
    cbSdkWaveformTrigger_DINPREG,      ///< Digital input rising edge trigger
    cbSdkWaveformTrigger_DINPFEG,      ///< Digital input falling edge trigger
    cbSdkWaveformTrigger_SPIKEUNIT,    ///< Spike unit
    cbSdkWaveformTrigger_COMMENTCOLOR, ///< Custom colored event (e.g. NeuroMotive event)
    cbSdkWaveformTrigger_SOFTRESET,    ///< Soft-reset trigger (e.g. file recording start)
    cbSdkWaveformTrigger_EXTENSION,    ///< Extension trigger
    cbSdkWaveformTrigger_COUNT, ///< Always the last value
} cbSdkWaveformTriggerType;

/// Extended pointer-form of cbWaveformData
typedef struct _cbSdkWaveformData
{
    cbSdkWaveformType type;
    uint32_t  repeats;
    cbSdkWaveformTriggerType  trig;
    uint16_t  trigChan;
    uint16_t  trigValue;
    uint8_t   trigNum;
    int16_t   offset;
    union {
        struct {
            uint16_t sineFrequency;
            int16_t  sineAmplitude;
        };
        struct {
            uint16_t phases;
            uint16_t  * duration;
            int16_t   * amplitude;
        };
    };
} cbSdkWaveformData;

/// Analog output monitor
typedef struct _cbSdkAoutMon
{
    uint16_t  chan; ///< (1-based) channel to monitor
    BOOL bTrack; ///< If should monitor last tracked channel
    BOOL bSpike; ///< If spike or continuous should be monitored
} cbSdkAoutMon;

/// CCF data
typedef struct _cbSdkCCF
{
    int ccfver; ///< CCF internal version
    cbCCF data;
} cbSdkCCF;

typedef enum _cbSdkSystemType
{
    cbSdkSystem_RESET = 0,
    cbSdkSystem_SHUTDOWN,
    cbSdkSystem_STANDBY,
} cbSdkSystemType;

/// Extension command type
typedef enum _cbSdkExtCmdType
{
    cbSdkExtCmd_RPC = 0,    // RPC command
    cbSdkExtCmd_UPLOAD,     // Upload the file
    cbSdkExtCmd_TERMINATE,  // Signal last RPC command to terminate
    cbSdkExtCmd_INPUT,		// Input to RPC command
    cbSdkExtCmd_END_PLUGIN, // Signal to end plugin
    cbSdkExtCmd_NSP_REBOOT, // Restart the NSP
} cbSdkExtCmdType;

/// Extension command
typedef struct _cbSdkExtCmd
{
    cbSdkExtCmdType cmd;
    char szCmd[cbMAX_LOG];
} cbSdkExtCmd;


extern "C"
{
// Can call this even with closed library to get library information
/*! Get the library version (and nsp version if library is open) */
CBSDKAPI    cbSdkResult cbSdkGetVersion(uint32_t nInstance, cbSdkVersion * version);

/*! Read configuration file. */
CBSDKAPI    cbSdkResult cbSdkReadCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend = false, bool bThreaded = false); // Read batch config from CCF (or nsp if filename is null)
/*! Write configuration file. */
CBSDKAPI    cbSdkResult cbSdkWriteCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bThreaded = false); // Write batch config to CCF (or nsp if filename is null)

/*! Open the library */
CBSDKAPI    cbSdkResult cbSdkOpen(uint32_t nInstance,
                                  cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT,
                                  cbSdkConnection con = cbSdkConnection());

CBSDKAPI    cbSdkResult cbSdkGetType(uint32_t nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType); // Get connection and instrument type

/*! Close the library */
CBSDKAPI    cbSdkResult cbSdkClose(uint32_t nInstance);

/*! Get the instrument sample clock time */
CBSDKAPI    cbSdkResult cbSdkGetTime(uint32_t nInstance, uint32_t * cbtime);

/*! Get direct access to internal spike cache shared memory */
CBSDKAPI    cbSdkResult cbSdkGetSpkCache(uint32_t nInstance, uint16_t channel, cbSPKCACHE **cache);
// Note that spike cache is volatile, thus should not be used for critical operations such as recording

/*! Get trial setup configuration */
CBSDKAPI    cbSdkResult cbSdkGetTrialConfig(uint32_t nInstance,
                                         uint32_t * pbActive, uint16_t * pBegchan = NULL, uint32_t * pBegmask = NULL, uint32_t * pBegval = NULL,
                                         uint16_t * pEndchan = NULL, uint32_t * pEndmask = NULL, uint32_t * pEndval = NULL, bool * pbDouble = NULL,
                                         uint32_t * puWaveforms = NULL, uint32_t * puConts = NULL, uint32_t * puEvents = NULL,
                                         uint32_t * puComments = NULL, uint32_t * puTrackings = NULL, bool * pbAbsolute = NULL);
/*! Setup a trial */
CBSDKAPI    cbSdkResult cbSdkSetTrialConfig(uint32_t nInstance,
                                         uint32_t bActive, uint16_t begchan = 0, uint32_t begmask = 0, uint32_t begval = 0,
                                         uint16_t endchan = 0, uint32_t endmask = 0, uint32_t endval = 0, bool bDouble = false,
                                         uint32_t uWaveforms = 0, uint32_t uConts = cbSdk_CONTINUOUS_DATA_SAMPLES, uint32_t uEvents = cbSdk_EVENT_DATA_SAMPLES,
                                         uint32_t uComments = 0, uint32_t uTrackings = 0, bool bAbsolute = false); // Configure a data collection trial
// begchan - first channel number (1-based), zero means all
// endchan - last channel number (1-based), zero means all

/*! Close given trial if configured */
CBSDKAPI    cbSdkResult cbSdkUnsetTrialConfig(uint32_t nInstance, cbSdkTrialType type);

/*! Get channel label */
// Pass NULL or allocate bValid[6] label[cbLEN_STR_LABEL] position[4]
CBSDKAPI    cbSdkResult cbSdkGetChannelLabel(uint32_t nInstance, uint16_t channel, uint32_t * bValid, char * label = NULL, uint32_t * userflags = NULL, int32_t * position = NULL); // Get channel label
/*! Set channel label */
CBSDKAPI    cbSdkResult cbSdkSetChannelLabel(uint32_t nInstance, uint16_t channel, const char * label, uint32_t userflags, int32_t * position); // Set channel label

/*! Retrieve data of a trial (NULL means ignore), user should allocate enough buffers beforehand, and trial should not be closed during this call */
CBSDKAPI    cbSdkResult cbSdkGetTrialData(uint32_t nInstance,
                                          uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                          cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);

/*! Initialize the structures (and fill with information about active channels, comment pointers and samples in the buffer) */
CBSDKAPI    cbSdkResult cbSdkInitTrialData(uint32_t nInstance, uint32_t bActive,
                                           cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                           cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, unsigned long wait_for_comment_msec = 250);

/*! Start/stop/open/close file recording */
CBSDKAPI    cbSdkResult cbSdkSetFileConfig(uint32_t nInstance, const char * filename, const char * comment, uint32_t bStart, uint32_t options = cbFILECFG_OPT_NONE);

/*! Get the state of file recording */
CBSDKAPI    cbSdkResult cbSdkGetFileConfig(uint32_t nInstance, char * filename, char * username, bool * pbRecording);

CBSDKAPI    cbSdkResult cbSdkSetPatientInfo(uint32_t nInstance, const char * ID, const char * firstname, const char * lastname, uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear);

CBSDKAPI    cbSdkResult cbSdkInitiateImpedance(uint32_t nInstance);

/*! This sends an arbitrary packet without any validation. Please use with care or it might break the system */
CBSDKAPI    cbSdkResult cbSdkSendPacket(uint32_t nInstance, void * ppckt);

/*! Get/Set the runlevel of the instrument */
CBSDKAPI    cbSdkResult cbSdkSetSystemRunLevel(uint32_t nInstance, uint32_t runlevel, uint32_t locked, uint32_t resetque);

CBSDKAPI    cbSdkResult cbSdkGetSystemRunLevel(uint32_t nInstance, uint32_t * runlevel, uint32_t * runflags = NULL, uint32_t * resetque = NULL);

/*! Send a digital output command */
CBSDKAPI    cbSdkResult cbSdkSetDigitalOutput(uint32_t nInstance, uint16_t channel, uint16_t value);

/*! Send a synch output waveform */
CBSDKAPI    cbSdkResult cbSdkSetSynchOutput(uint32_t nInstance, uint16_t channel, uint32_t nFreq, uint32_t nRepeats);

/*! Send an extension command */
CBSDKAPI    cbSdkResult cbSdkExtDoCommand(uint32_t nInstance, cbSdkExtCmd * extCmd);

/*! Send a analog output waveform or monitor a given channel, disable channel if both are null */
CBSDKAPI    cbSdkResult cbSdkSetAnalogOutput(uint32_t nInstance, uint16_t channel, cbSdkWaveformData * wf, cbSdkAoutMon * mon);

/*! * Mask channels (for both trial and callback)
* @param[in] channel channel number (1-based), zero means all channels
*/
CBSDKAPI    cbSdkResult cbSdkSetChannelMask(uint32_t nInstance, uint16_t channel, uint32_t bActive);

/*! Send a comment or custom event */
CBSDKAPI    cbSdkResult cbSdkSetComment(uint32_t nInstance, uint32_t rgba, uint8_t charset, const char * comment = NULL);

/*! Send a full channel configuration packet */
CBSDKAPI    cbSdkResult cbSdkSetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo);
/*! Get a full channel configuration packet */
CBSDKAPI    cbSdkResult cbSdkGetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo);

/*! Get filter description (proc = 1 for now) */
CBSDKAPI    cbSdkResult cbSdkGetFilterDesc(uint32_t nInstance, uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc);

/*! Get sample group list (proc = 1 for now) */
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupList(uint32_t nInstance, uint32_t proc, uint32_t group, uint32_t *length, uint16_t *list);

CBSDKAPI    cbSdkResult cbSdkGetSampleGroupInfo(uint32_t nInstance, uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length);

/*!
* Get information about given trackable object
* @param[in] id		trackable ID (1 to cbMAXTRACKOBJ)
* @param[in] name	string of length cbLEN_STR_LABEL
*/
CBSDKAPI    cbSdkResult cbSdkGetTrackObj(uint32_t nInstance, char *name, uint16_t *type, uint16_t *pointCount, uint32_t id);

/*! Get video source information */
// id     video source ID (1 to cbMAXVIDEOSOURCE)
// name   string of length cbLEN_STR_LABEL
CBSDKAPI    cbSdkResult cbSdkGetVideoSource(uint32_t nInstance, char *name, float *fps, uint32_t id);

/// Send global spike configuration
CBSDKAPI    cbSdkResult cbSdkSetSpikeConfig(uint32_t nInstance, uint32_t spklength, uint32_t spkpretrig);
/// Get global system configuration
CBSDKAPI    cbSdkResult cbSdkGetSysConfig(uint32_t nInstance, uint32_t * spklength, uint32_t * spkpretrig = NULL, uint32_t * sysfreq = NULL);
/// Perform given system command
CBSDKAPI    cbSdkResult cbSdkSystem(uint32_t nInstance, cbSdkSystemType cmd);

CBSDKAPI    cbSdkResult cbSdkRegisterCallback(uint32_t nInstance, cbSdkCallbackType callbacktype, cbSdkCallback pCallbackFn, void* pCallbackData);
CBSDKAPI    cbSdkResult cbSdkUnRegisterCallback(uint32_t nInstance, cbSdkCallbackType callbacktype);
CBSDKAPI    cbSdkResult cbSdkCallbackStatus(uint32_t nInstance, cbSdkCallbackType callbacktype);
// At most one callback per each callback type per each connection

/// Convert volts string (e.g. '5V', '-65mV', ...) to its raw digital value equivalent for given channel
CBSDKAPI    cbSdkResult cbSdkAnalogToDigital(uint32_t nInstance, uint16_t channel, const char * szVoltsUnitString, int32_t * digital);

}

CBSDKAPI    cbSdkResult cbSdkSendPoll(uint32_t nInstance, const char* appname, uint32_t mode, uint32_t flags, uint32_t extra);

#endif /* CBSDK_H_INCLUDED */
