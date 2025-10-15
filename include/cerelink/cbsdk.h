/* =STS=> cbsdk.h[4901].aa20   submit     SMID:22 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2021 Blackrock Microsystems, LLC
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
#include "CCFUtils.h"

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
    CBSDKCALLBACK_IMPEDANCE,    ///< Monitor impedance events
    CBSDKCALLBACK_SYSHEARTBEAT, ///< Monitor system heartbeats (100 times a second)
    CBSDKCALLBACK_LOG,		    ///< Monitor system heartbeats (100 times a second)
    CBSDKCALLBACK_COUNT  ///< Always the last value
} cbSdkCallbackType;

/// Trial type
typedef enum _cbSdkTrialType
{
    CBSDKTRIAL_CONTINUOUS,
    CBSDKTRIAL_EVENTS,
    CBSDKTRIAL_COMMENTS,
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
    // TODO: Go back to using cbNUM_ANALOG_CHANS + 2 after we have m_ChIdxInType
    uint16_t count; ///< Number of valid channels in this trial (up to cbNUM_ANALOG_CHANS+2)
    uint16_t chan[cbMAXCHANS]; ///< channel numbers (1-based)
    uint32_t num_samples[cbMAXCHANS][cbMAXUNITS + 1]; ///< number of samples
    void * timestamps[cbMAXCHANS][cbMAXUNITS + 1];   ///< Buffer to hold time stamps
    void * waveforms[cbMAXCHANS]; ///< Buffer to hold waveforms or digital values
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
        nRange = 0;
    }
    int nInPort;  ///< Client port number
    int nOutPort; ///< Instrument port number
    int nRecBufSize; ///< Receive buffer size (0 to ignore altogether)
    LPCSTR szInIP;  ///< Client IPv4 address
    LPCSTR szOutIP; ///< Instrument IPv4 address
    int nRange; ///< Range of IP addresses to try to open
} cbSdkConnection;

/// Trial continuous data
typedef struct _cbSdkTrialCont
{
    uint16_t count; ///< Number of valid channels in this trial (up to cbNUM_ANALOG_CHANS)
    uint16_t chan[cbNUM_ANALOG_CHANS]; ///< Channel numbers (1-based)
    uint16_t sample_rates[cbNUM_ANALOG_CHANS]; ///< Current sample rate (samples per second)
    uint32_t num_samples[cbNUM_ANALOG_CHANS]; ///< Number of samples
    PROCTIME time;  ///< Start time for trial continuous data
    void * samples[cbNUM_ANALOG_CHANS]; ///< Buffer to hold sample vectors
} cbSdkTrialCont;

/// Trial comment data
typedef struct _cbSdkTrialComment
{
    uint16_t num_samples; ///< Number of comments
    uint8_t * charsets;   ///< Buffer to hold character sets
    uint32_t * rgbas;     ///< Buffer to hold rgba values (actually tbgr)
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
    bool bTrack; ///< If true then monitor last tracked channel
    bool bSpike; ///< If spike or continuous should be monitored
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
    cbSdkExtCmd_INPUT,      // Input to RPC command
    cbSdkExtCmd_END_PLUGIN, // Signal to end plugin
    cbSdkExtCmd_NSP_REBOOT, // Restart the NSP
    cbSdkExtCmd_PLUGINFO,   // Get plugin info
} cbSdkExtCmdType;

/// Extension command
typedef struct _cbSdkExtCmd
{
    cbSdkExtCmdType cmd;
    char szCmd[cbMAX_LOG];
} cbSdkExtCmd;


extern "C"
{
/**
 * @brief Retrieve SDK library version and, if connected, instrument protocol/firmware versions.
 *
 * This may be called before cbSdkOpen(); in that case only the library fields are populated and
 * a CBSDKRESULT_WARNCLOSED may be returned to indicate no active instrument connection. When the
 * instance is open the structure is filled with both library and instrument (NSP) version info.
 * @param nInstance SDK instance index
 * @param version Pointer to cbSdkVersion structure to populate
 * @return cbSdkResult Success or warning/error (CBSDKRESULT_NULLPTR if version is NULL)
 */
CBSDKAPI    cbSdkResult cbSdkGetVersion(uint32_t nInstance, cbSdkVersion * version);

/**
 * @brief Read CCF configuration from file or NSP.
 *
 * If a filename is provided, reads from file (library can be closed, but a warning is returned). If nullptr, reads from NSP (library must be open).
 * Optionally converts, sends, or runs in a separate thread. Progress callback should be registered beforehand if needed.
 * @param nInstance SDK instance number
 * @param pData Pointer to cbSdkCCF structure to receive data
 *   pData->data	CCF data will be copied here
 *   pData->ccfver the internal version of the original data
 * @param szFileName CCF filename to read (or nullptr to read from NSP)
 * @param bConvert Allow conversion if needed
 * @param bSend Send CCF after successful read
 * @param bThreaded Run operation in a separate thread
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkReadCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bConvert, bool bSend = false, bool bThreaded = false);

/**
 * @brief Write CCF configuration to file or send to NSP.
 *
 * If a filename is provided, writes to file.
 * If nullptr, sends CCF to NSP (library must be open).
 * Optionally runs in a separate thread.
 * Progress callback should be registered beforehand if needed.
 * @param nInstance SDK instance number
 * @param pData Pointer to cbSdkCCF structure with data to write
 *   pData->data	CCF data to use
 *   pData->ccfver Last internal version used
 * @param szFileName CCF filename to write (or nullptr to send to NSP)
 * @param bThreaded Run operation in a separate thread
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkWriteCCF(uint32_t nInstance, cbSdkCCF * pData, const char * szFileName, bool bThreaded = false);

/**
 * @brief Open (initialize) an SDK instance connection to an instrument.
 *
 * Allocates internal structures, starts background network / processing threads, and attempts to
 * establish communication using the requested connection type. If CBSDKCONNECTION_DEFAULT is used the
 * library will first try a Central (shared memory / service) connection and fall back to UDP on failure.
 * Calling cbSdkOpen() on an already open instance returns CBSDKRESULT_WARNOPEN and leaves the existing
 * connection intact. Once open you may register callbacks and configure trials.
 * @note No exceptions are thrown; all failures are reported via the returned cbSdkResult.
 * @param nInstance Zero-based instance index (0 .. cbMAXOPEN-1)
 * @param conType Desired connection strategy (DEFAULT, CENTRAL, UDP)
 * @param con Connection parameters (ports, IPs, receive buffer size, IP range). A default-constructed
 *            cbSdkConnection supplies sensible defaults.
 * @return cbSdkResult Success (CBSDKRESULT_SUCCESS or CBSDKRESULT_WARNOPEN) or error such as
 *         CBSDKRESULT_ERROPENCENTRAL, CBSDKRESULT_ERROPENUDP, CBSDKRESULT_TIMEOUT, CBSDKRESULT_INVALIDINST.
 */
CBSDKAPI    cbSdkResult cbSdkOpen(uint32_t nInstance,
                                  cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT,
                                  cbSdkConnection con = cbSdkConnection());

/**
 * @brief Get the current connection and instrument type for a given instance.
 *
 * @param nInstance SDK instance number
 * @param conType Pointer to receive the connection type
 * @param instType Pointer to receive the instrument type
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkGetType(uint32_t nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType);

/**
 * @brief Close the SDK library for a given instance.
 *
 * Releases resources and unregisters callbacks. Safe to call multiple times.
 * @param nInstance SDK instance number
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkClose(uint32_t nInstance);

/**
 * @brief Query the current instrument sample clock time.
 *
 * Returns the current 30 kHz (or system-configured) tick count from the instrument. The value can
 * be converted to seconds using cbSdk_SECONDS_PER_TICK. Requires an open connection.
 * @param nInstance SDK instance index
 * @param cbtime Pointer to PROCTIME variable to receive current tick value
 * @return cbSdkResult Success or error (CBSDKRESULT_CLOSED if instance not open, CBSDKRESULT_NULLPTR if cbtime null)
 */
CBSDKAPI    cbSdkResult cbSdkGetTime(uint32_t nInstance, PROCTIME * cbtime);

/**
 * @brief Get direct access to the internal spike cache shared memory for a channel.
 *
 * Note: The spike cache is volatile and should not be used for critical operations such as recording.
 * @param nInstance SDK instance number
 * @param channel Channel number (1-based)
 * @param cache Pointer to receive the spike cache pointer
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkGetSpkCache(uint32_t nInstance, uint16_t channel, cbSPKCACHE **cache);

/**
 * @brief Get the current trial setup configuration.
 *
 * Retrieves the configuration for trial data collection, including active channels and event settings.
 * Refer to cbSdkSetTrialConfig() for parameter descriptions.
 * @param nInstance SDK instance number
 * @param pbActive Pointer to receive trial active status
 * @param pBegchan Pointer to receive trial begin channel (optional)
 * @param pBegmask Pointer to receive trial begin mask (optional)
 * @param pBegval Pointer to receive trial begin value (optional)
 * @param pEndchan Pointer to receive trial end channel (optional)
 * @param pEndmask Pointer to receive trial end mask (optional)
 * @param pEndval Pointer to receive trial end value (optional)
 * @param pbDouble Pointer to receive double trial flag (optional)
 * @param puWaveforms Pointer to receive waveform count (optional)
 * @param puConts Pointer to receive continuous sample count (optional)
 * @param puEvents Pointer to receive event sample count (optional)
 * @param puComments Pointer to receive comment sample count (optional)
 * @param puTrackings Pointer to receive tracking sample count (optional)
 * @param pbAbsolute Pointer to receive absolute time flag (optional)
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkGetTrialConfig(uint32_t nInstance,
                                         uint32_t * pbActive, uint16_t * pBegchan = nullptr, uint32_t * pBegmask = nullptr, uint32_t * pBegval = nullptr,
                                         uint16_t * pEndchan = nullptr, uint32_t * pEndmask = nullptr, uint32_t * pEndval = nullptr, bool * pbDouble = nullptr,
                                         uint32_t * puWaveforms = nullptr, uint32_t * puConts = nullptr, uint32_t * puEvents = nullptr,
                                         uint32_t * puComments = nullptr, uint32_t * puTrackings = nullptr, bool * pbAbsolute = nullptr);

/**
 * @brief Set the trial setup configuration for data collection.
 * Configures the trial parameters, including channel range, event settings, and buffer sizes.
 * A 'trial' is a time period during which data is collected and stored in memory buffers for later retrieval.
 * It can be started and stopped based on specific channel events or manually.
 * Each data type (continuous, event, comment, tracking) can be enabled or disabled independently.
 *
 * @param nInstance SDK instance number
 * @param bActive Enable or disable trial.
 *      If false, then any active trial is ended and the system will monitor for triggers if configured.
 *      If true and already in a trial then do nothing.
 *      If true and not already in a trial: Resets trial start time to now, write_index and write_start_index are reset to `0`.
 * @param begchan Channel ID that is watched for the trial begin notification (1-based, 0 for all)
 * @param begmask Mask ANDed with channel data to check for trial beginning
 * @param begval Value the masked data is compared to identify trial beginning
 * @param endchan Channel ID that is watched for the trial end notification (1-based, 0 for all)
 * @param endmask Mask ANDed with channel data to check for trial end
 * @param endval Value the masked data is compared to identify trial end
 * @param bDouble If data storage (spike or continuous) is double
 *      IMO this does not work well and should be avoided.
 * @param uWaveforms Number of waveforms
 * @param uConts Number of continuous samples
 * @param uEvents Number of event samples
 * @param uComments Number of comment samples
 * @param uTrackings Number of tracking samples
 * @param bAbsolute Use absolute time
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkSetTrialConfig(uint32_t nInstance,
                                         uint32_t bActive, uint16_t begchan = 0, uint32_t begmask = 0, uint32_t begval = 0,
                                         uint16_t endchan = 0, uint32_t endmask = 0, uint32_t endval = 0, bool bDouble = false,
                                         uint32_t uWaveforms = 0, uint32_t uConts = cbSdk_CONTINUOUS_DATA_SAMPLES, uint32_t uEvents = cbSdk_EVENT_DATA_SAMPLES,
                                         uint32_t uComments = 0, uint32_t uTrackings = 0, bool bAbsolute = false);

/**
 * @brief Close the given trial if configured.
 *
 * Closes the specified trial type and releases associated resources.
 * @param nInstance SDK instance number
 * @param type Trial type to close
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkUnsetTrialConfig(uint32_t nInstance, cbSdkTrialType type);

/**
 * @brief Retrieve label and metadata for a channel.
 *
 * The caller can pass nullptr for any optional output parameter. If label is supplied it must have
 * capacity of at least cbLEN_STR_LABEL bytes. The bValid array (length 6) returns per-field validity flags:
 * [0]=label, [1]=userflags, [2]=position(XYZ), etc. (See implementation for exact mapping.)
 * @param nInstance SDK instance index
 * @param channel 1-based channel number
 * @param bValid Output array of validity flags (length >= 6)
 * @param label Optional buffer to receive ASCII label (cbLEN_STR_LABEL)
 * @param userflags Optional user flag value
 * @param position Optional array of 4 int32_t values (implementation-defined meaning)
 * @return cbSdkResult Success or error code (CBSDKRESULT_INVALIDCHANNEL on bad channel)
 */
CBSDKAPI    cbSdkResult cbSdkGetChannelLabel(uint32_t nInstance, uint16_t channel, uint32_t * bValid, char * label = nullptr, uint32_t * userflags = nullptr, int32_t * position = nullptr);

/**
 * @brief Set label and metadata for a channel.
 * @param nInstance SDK instance index
 * @param channel 1-based channel number
 * @param label Null-terminated ASCII label (truncated internally if longer than cbLEN_STR_LABEL-1)
 * @param userflags Application-defined 32-bit flags stored with the channel
 * @param position Optional pointer to 4-element int32_t array (e.g. XYZ + reserved) or nullptr to leave unchanged
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetChannelLabel(uint32_t nInstance, uint16_t channel, const char * label, uint32_t userflags, int32_t * position); // Set channel label

/* Get channel capabilities */
// CBSDKAPI    cbSdkResult cbSdkIsChanAnalogIn(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanFEAnalogIn(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanAIAnalogIn(uint32_t nInstance, uint16_t channel, uint32_t* bResult);

/**
 * @brief Determine if channel is any digital input (DINP or serial).
 * @param nInstance SDK instance number
 * @param channel Channel number (1-based)
 * @param bResult Returns non-zero if channel is digital input
 */
CBSDKAPI    cbSdkResult cbSdkIsChanAnyDigIn(uint32_t nInstance, uint16_t channel, uint32_t* bResult);

/**
 * @brief Determine if channel is a serial channel.
 * @param nInstance SDK instance number
 * @param channel Channel number (1-based)
 * @param bResult Returns non-zero if channel is serial input
 */
CBSDKAPI    cbSdkResult cbSdkIsChanSerial(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanDigin(uint32_t nInstance, uint16_t channel, uint32_t* bValid);
// CBSDKAPI    cbSdkResult cbSdkIsChanDigout(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanAnalogOut(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanAudioOut(uint32_t nInstance, uint16_t channel, uint32_t* bResult);
// CBSDKAPI    cbSdkResult cbSdkIsChanCont(uint32_t nInstance, uint16_t channel, uint32_t* bResult);

/**
 * @brief Retrieve buffered trial data for configured modalities (spike/events, continuous, comments, tracking).
 *
 * For each non-null structure pointer, fills counts and copies/points to the samples accumulated since the last
 * cbSdkInitTrialData() call (or since activation if never initialized). The caller must have allocated the member
 * buffers (timestamps, waveforms, etc.) sized according to the configuration determined by cbSdkInitTrialData().
 * If bActive is non-zero the internal read indices advance (consuming the data). If zero, the call is a
 * non-destructive peek and data can be fetched again.
 * Thread-safety: Do not change trial configuration concurrently. Internally the library synchronizes access across
 * producer threads, but external synchronization is recommended for multi-threaded consumers.
 * @param nInstance SDK instance index
 * @param bActive Non-zero to advance (consume) read indices; zero to leave indices unchanged
 * @param trialevent Pointer to cbSdkTrialEvent (or nullptr to skip spike/event retrieval)
 *   in: trialevent->num_samples    requested number of event samples
 *   out: trialevent->num_samples	retrieved number of events
 *   out: trialevent->timestamps	timestamps for events
 *   out: trialevent->waveforms		waveform or digital data
 * @param trialcont Pointer to cbSdkTrialCont (or nullptr to skip continuous retrieval)
 *   in: trialcont->num_samples		requested number of continuous samples
 *   out: trialcont->num_samples	retrieved number of continuous samples
 *   out: trialcont->time			start time for trial -- user must keep track of number of samples since init.
 *   out: trialcont->samples		continuous samples
 * @param trialcomment Pointer to cbSdkTrialComment (or nullptr to skip comment retrieval)
 *   in: trialcomment->num_samples  requested number of comment samples
 *   out: trialcomment->num_samples	retrieved number of comments samples
 *   out: trialcomment->timestamps  timestamps for comments
 *   out: trialcomment->rgbas       rgba for comments
 *   out: trialcomment->charsets    character set for comments
 *   out: trialcomment->comments    pointer to the comments
 * @param trialtracking Pointer to cbSdkTrialTracking (or nullptr to skip tracking retrieval)
 *   in: trialtracking->num_samples              requested number of tracking samples
 *   out: trialtracking->num_samples             retrieved number of tracking samples
 *   out: trialtracking->synch_frame_numbers     retrieved frame numbers
 *   out: trialtracking->synch_timestamps        retrieved synchronized timesamps
 *   out: trialtracking->timestamps              timestamps for tracking
 *   out: trialtracking->coords                  tracking coordinates
 * @return cbSdkResult Success or error (CBSDKRESULT_ERRCONFIG if trial not configured, CBSDKRESULT_INVALIDPARAM on bad pointers)
 */
CBSDKAPI    cbSdkResult cbSdkGetTrialData(uint32_t nInstance,
                                          uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                          cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking);

/**
 * @brief Initialize user-provided trial data structures and prime buffer pointers.
 *
 * Populates channel lists, waveform/comment pointer arrays, and establishes the snapshot starting point
 * for subsequent cbSdkGetTrialData() calls. Also waits (up to wait_for_comment_msec) for at least one
 * comment if comment buffering is configured and bActive is non-zero.
 * Call before data retrieval (cbSdkGetTrialData) to fill sample counts.
 * Note: No allocation is performed here, buffer pointers must be set to appropriate allocated buffers after a call to this function.
 * @param nInstance SDK instance index
 * @param bActive Non-zero to reset internal trial start time to 'now'.
 *      Internal trial start time serves 2 functions:
 *      1) If we are waiting for a comment, it is the time after which we will accept a comment to trigger the end of the wait.
 *      2) Its previous value is used as the zero-time reference for relative timestamps if absolute time is not configured.
 * @param trialevent Event trial structure to initialize or nullptr
 * @param trialcont Continuous trial structure to initialize or nullptr
 * @param trialcomment Comment trial structure to initialize or nullptr
 * @param trialtracking Tracking trial structure to initialize or nullptr
 * @param wait_for_comment_msec Milliseconds to wait for a first comment (default 250)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkInitTrialData(uint32_t nInstance, uint32_t bActive,
                                           cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                           cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking, unsigned long wait_for_comment_msec = 250);

/**
 * @brief Control file recording on the instrument.
 *
 * Depending on options and bStart this can: start a new recording, stop current, or update metadata.
 * Passing filename or comment as nullptr leaves that field unchanged (unless starting recording where
 * a filename may be required). See cbFILECFG_OPT_* for options (e.g. append, split behavior).
 * @param nInstance SDK instance index
 * @param filename Recording file base name (without extension) or nullptr
 * @param comment Optional comment/annotation string or nullptr
 * @param bStart Non-zero to start (or continue) recording, zero to stop
 * @param options Bitfield of cbFILECFG_OPT_* flags
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetFileConfig(uint32_t nInstance, const char * filename, const char * comment, uint32_t bStart, uint32_t options = cbFILECFG_OPT_NONE);

/**
 * @brief Query current file recording state.
 * @param nInstance SDK instance index
 * @param filename Optional output buffer receiving current file name (cbLEN_STR_FILE) or nullptr
 * @param username Optional output buffer receiving username (cbLEN_STR_USER) or nullptr
 * @param pbRecording Output flag set non-zero if actively recording
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetFileConfig(uint32_t nInstance, char * filename, char * username, bool * pbRecording);

/**
 * @brief Set patient information (demographics) on the NSP.
 * @param nInstance SDK instance number
 * @param ID Patient ID string
 * @param firstname First name
 * @param lastname Last name
 * @param DOBMonth Month of birth (1-12)
 * @param DOBDay Day of birth (1-31)
 * @param DOBYear Year of birth (4-digit)
 */
CBSDKAPI    cbSdkResult cbSdkSetPatientInfo(uint32_t nInstance, const char * ID, const char * firstname, const char * lastname, uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear);

/**
 * @brief Begin an impedance measurement sequence.
 * Initiates impedance testing; progress delivered via impedance callback events.
 * @param nInstance SDK instance number
 */
CBSDKAPI    cbSdkResult cbSdkInitiateImpedance(uint32_t nInstance);

/*! This sends an arbitrary packet without any validation. Please use with care or it might break the system */
//CBSDKAPI    cbSdkResult cbSdkSendPacket(uint32_t nInstance, void * ppckt);

/**
 * @brief Set instrument run level and optional lock/reset queue.
 * @param nInstance SDK instance number
 * @param runlevel Target run level (cbRUNLEVEL_*)
 * @param locked Non-zero to lock run level
 * @param resetque Non-zero to clear pending resets
 */
CBSDKAPI    cbSdkResult cbSdkSetSystemRunLevel(uint32_t nInstance, uint32_t runlevel, uint32_t locked, uint32_t resetque);

/**
 * @brief Get instrument run level state.
 * @param nInstance SDK instance number
 * @param runlevel Current run level
 * @param runflags Optional run flags
 * @param resetque Optional reset queue state
 */
CBSDKAPI    cbSdkResult cbSdkGetSystemRunLevel(uint32_t nInstance, uint32_t * runlevel, uint32_t * runflags = nullptr, uint32_t * resetque = nullptr);

/**
 * @brief Set (pulse or latch) a digital output line.
 * Behavior depends on instrument firmware configuration; value usually 0/1.
 * @param nInstance SDK instance index
 * @param channel 1-based digital output channel
 * @param value Output value (bit pattern or level)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetDigitalOutput(uint32_t nInstance, uint16_t channel, uint16_t value);

/**
 * @brief Generate a synchronization pulse train on a sync output channel.
 * @param nInstance SDK instance index
 * @param channel 1-based sync channel
 * @param nFreq Pulse frequency (Hz) or implementation-defined units
 * @param nRepeats Number of pulses (0 may mean continuous depending on firmware)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetSynchOutput(uint32_t nInstance, uint16_t channel, uint32_t nFreq, uint32_t nRepeats);

/**
 * @brief Issue an extension / plugin command to the NSP.
 * @param nInstance SDK instance index
 * @param extCmd Pointer to command structure (cmd + payload string)
 * @return cbSdkResult Success or error code (CBSDKRESULT_INVALIDPARAM if extCmd null)
 */
CBSDKAPI    cbSdkResult cbSdkExtDoCommand(uint32_t nInstance, cbSdkExtCmd * extCmd);

/**
 * @brief Configure analog output channel for waveform generation or monitoring.
 *
 * Provide either wf (waveform definition) or mon (monitor specification). Passing both nullptr disables AO.
 * @param nInstance SDK instance index
 * @param channel 1-based analog output channel
 * @param wf Waveform description (type, repeats, trigger, parameters) or nullptr
 * @param mon Monitor specification (track spike/continuous) or nullptr
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetAnalogOutput(uint32_t nInstance, uint16_t channel, const cbSdkWaveformData * wf, const cbSdkAoutMon * mon);

/**
 * @brief Enable or disable channel activity for trial buffering and callbacks.
 * @param nInstance SDK instance index
 * @param channel 1-based channel number (0 applies to all)
 * @param bActive Non-zero to enable, zero to disable
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetChannelMask(uint32_t nInstance, uint16_t channel, uint32_t bActive);

/**
 * @brief Inject a comment/custom event packet.
 * @param nInstance SDK instance index
 * @param t_bgr Color encoded as (A<<24)|(B<<16)|(G<<8)|R (A=alpha)
 * @param charset Character set ID (e.g. 0=ANSI)
 * @param comment UTF-8/ASCII string (nullptr sends a color-only marker)
 * @return cbSdkResult Success or error code (CBSDKRESULT_INVALIDCOMMENT if too long)
 */
CBSDKAPI    cbSdkResult cbSdkSetComment(uint32_t nInstance, uint32_t t_bgr, uint8_t charset, const char * comment = nullptr);

/**
 * @brief Apply a full channel configuration.
 * @param nInstance SDK instance index
 * @param channel 1-based channel to configure
 * @param chaninfo Pointer to populated cbPKT_CHANINFO structure
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo);

/**
 * @brief Retrieve current full channel configuration.
 * @param nInstance SDK instance index
 * @param channel 1-based channel
 * @param chaninfo Output pointer to cbPKT_CHANINFO structure to fill
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo);

/**
 * @brief Obtain filter description metadata.
 * @param nInstance SDK instance index
 * @param proc Processor index (1-based)
 * @param filt Filter identifier
 * @param filtdesc Output pointer to cbFILTDESC structure
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetFilterDesc(uint32_t nInstance, uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc);

/**
 * @brief Retrieve channel list for a sample (continuous data) group.
 * @param nInstance SDK instance index
 * @param proc Processor index (1-based)
 * @param group Sample group identifier
 * @param length Input: capacity of list array; Output: number of channels returned
 * @param list Caller-allocated array of uint16_t channel numbers (size >= *length)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupList(uint32_t nInstance, uint32_t proc, uint32_t group, uint32_t *length, uint16_t *list);

/**
 * @brief Get sample group information.
 * @param nInstance SDK instance number
 * @param proc Processor index (1-based)
 * @param group Sample group id
 * @param label Optional buffer to receive label (cbLEN_STR_LABEL)
 * @param period Sample period (ticks)
 * @param length Number of channels in group
 */
CBSDKAPI    cbSdkResult cbSdkGetSampleGroupInfo(uint32_t nInstance, uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length);

/**
 * @brief Assign an analog input channel to a sample group and select filter.
 * @param nInstance SDK instance index
 * @param chan 1-based channel number
 * @param filter Filter identifier (0 = none or depends on firmware)
 * @param group Sample group ID (see SDK_SMPGRP_RATE_* constants)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetAinpSampling(uint32_t nInstance, uint32_t chan, uint32_t filter, uint32_t group);

/**
 * @brief Configure per-channel spike extraction / sorting options.
 * @param nInstance SDK instance index
 * @param chan 1-based channel number
 * @param flags Bitfield of spike option flags
 * @param filter Spike filter identifier
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetAinpSpikeOptions(uint32_t nInstance, uint32_t chan, uint32_t flags, uint32_t filter);

/**
 * @brief Retrieve metadata for a tracking (video) object.
 *
 * Provides information about a trackable (NeuroMotive node) specified by its 1-based ID. Any of the
 * optional output pointers (name, type, pointCount) may be nullptr to skip that field. The name buffer,
 * if provided, must have capacity of at least cbLEN_STR_LABEL bytes. On invalid or unavailable ID the
 * function returns CBSDKRESULT_INVALIDTRACKABLE.
 * @param nInstance SDK instance index
 * @param name Optional output buffer (size >= cbLEN_STR_LABEL) to receive the object's name/label
 * @param type Optional output pointer receiving the object's type (cbTRACKOBJ_TYPE_*)
 * @param pointCount Optional output pointer receiving the object's maximum point count
 * @param id 1-based trackable object ID (range: 1 .. cbMAXTRACKOBJ)
 * @return cbSdkResult Success or error code (CBSDKRESULT_INVALIDTRACKABLE if id is invalid or absent)
 */
CBSDKAPI    cbSdkResult cbSdkGetTrackObj(uint32_t nInstance, char *name, uint16_t *type, uint16_t *pointCount, uint32_t id);

/**
 * @brief Retrieve video source properties.
 * @param nInstance SDK instance index
 * @param name Output buffer (cbLEN_STR_LABEL) for source name
 * @param fps Output frames-per-second value
 * @param id 1-based video source identifier
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetVideoSource(uint32_t nInstance, char *name, float *fps, uint32_t id);

/**
 * @brief Set global spike waveform extraction parameters.
 * @param nInstance SDK instance index
 * @param spklength Waveform length in samples
 * @param spkpretrig Number of pre-trigger samples
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSetSpikeConfig(uint32_t nInstance, uint32_t spklength, uint32_t spkpretrig);

/**
 * @brief Query global spike/system configuration.
 * @param nInstance SDK instance index
 * @param spklength Output spike length (samples)
 * @param spkpretrig Optional output pre-trigger length (samples)
 * @param sysfreq Optional output system tick frequency (Hz)
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkGetSysConfig(uint32_t nInstance, uint32_t * spklength, uint32_t * spkpretrig = nullptr, uint32_t * sysfreq = nullptr);

/**
 * @brief Send a system-level command (reset/shutdown/standby).
 * @param nInstance SDK instance index
 * @param cmd Command enum value
 * @return cbSdkResult Success or error code
 */
CBSDKAPI    cbSdkResult cbSdkSystem(uint32_t nInstance, cbSdkSystemType cmd);

/**
 * @brief Register callback for specific event type.
 * Only one user callback per type per instance allowed.
 * @param nInstance SDK instance number
 * @param callbackType Enumerated callback type to register
 * @param pCallbackFn Function pointer invoked on events (nullptr unregisters and returns warning)
 * @param pCallbackData User data passed back to callback
 */
CBSDKAPI    cbSdkResult cbSdkRegisterCallback(uint32_t nInstance, cbSdkCallbackType callbackType, cbSdkCallback pCallbackFn, void* pCallbackData);

/**
 * @brief Unregister previously registered callback.
 * @param nInstance SDK instance number
 * @param callbackType Enumerated callback type
 */
CBSDKAPI    cbSdkResult cbSdkUnRegisterCallback(uint32_t nInstance, cbSdkCallbackType callbackType);

/**
 * @brief Query whether a callback is currently registered.
 * @param nInstance SDK instance number
 * @param callbackType Enumerated callback type
 */
CBSDKAPI    cbSdkResult cbSdkCallbackStatus(uint32_t nInstance, cbSdkCallbackType callbackType);
// At most one callback per each callback type per each connection

/**
 * @brief Convert human-readable voltage string to digital units for channel scaling.
 * Accepts unit suffixes (V, mV, uV) and optional sign. Space-insensitive.
 * @param nInstance SDK instance index
 * @param channel 1-based channel number
 * @param szVoltsUnitString String such as "5V", "-65mV", "250uV"
 * @param digital Output converted raw digital value
 * @return cbSdkResult Success or error code (CBSDKRESULT_INVALIDPARAM on parse failure)
 */
CBSDKAPI    cbSdkResult cbSdkAnalogToDigital(uint32_t nInstance, uint16_t channel, const char * szVoltsUnitString, int32_t * digital);

}

/**
 * @brief Send a poll packet (cbPKT_POLL) to the NSP.
 * Useful for synchronizing or identifying external client applications.
 * @param nInstance SDK instance number
 * @param ppckt Pointer to a poll packet structure to transmit (must be properly populated)
 * @return cbSdkResult error code
 */
CBSDKAPI    cbSdkResult cbSdkSendPoll(uint32_t nInstance, void * ppckt);

#endif /* CBSDK_H_INCLUDED */
