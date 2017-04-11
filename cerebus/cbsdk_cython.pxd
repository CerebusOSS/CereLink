'''
@date March 9, 2014

@author: dashesy

Purpose: Cython interface for cbsdk_small

'''

from libc.stdint cimport uint32_t, int32_t, uint16_t, int16_t, uint8_t
from libcpp cimport bool

cdef extern from "cbhwlib.h":

    cdef char* cbNET_UDP_ADDR_INST  "cbNET_UDP_ADDR_INST"   # Cerebus default address
    cdef char* cbNET_UDP_ADDR_CNT   "cbNET_UDP_ADDR_CNT"    # NSP default control address
    cdef char* cbNET_UDP_ADDR_BCAST "cbNET_UDP_ADDR_BCAST"  # NSP default broadcast address
    cdef int cbNET_UDP_PORT_BCAST   "cbNET_UDP_PORT_BCAST"  # Neuroflow Data Port
    cdef int cbNET_UDP_PORT_CNT     "cbNET_UDP_PORT_CNT"    # Neuroflow Control Port

    # Have to put constants that are used as array sizes in an enum,
    # otherwise they are considered non-const and can't be used.
    cdef enum cbhwlib_consts:
        cbNUM_FE_CHANS = 128
        cbNUM_ANAIN_CHANS = 16
        cbNUM_ANALOG_CHANS = (cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS)
        cbMAXUNITS = 5
        MAX_CHANS_DIGITAL_IN = (cbNUM_FE_CHANS+cbNUM_ANAIN_CHANS+4+2+1)
        MAX_CHANS_SERIAL = (MAX_CHANS_DIGITAL_IN+1)
        cbMAXTRACKOBJ = 20 # maximum number of trackable objects
        cbMAXHOOPS = 4

    cdef enum cbwlib_strconsts:
        cbLEN_STR_UNIT          = 8
        cbLEN_STR_LABEL         = 16
        #cbLEN_STR_FILT_LABEL    = 16
        cbLEN_STR_IDENT         = 64
    
    cdef enum cbStateCCF:
        CCFSTATE_READ = 0           # Reading in progress
        CCFSTATE_WRITE = 1          # Writing in progress
        CCFSTATE_SEND = 2           # Sendign in progress
        CCFSTATE_CONVERT = 3        # Conversion in progress
        CCFSTATE_THREADREAD = 4     # Total threaded read progress
        CCFSTATE_THREADWRITE = 5    # Total threaded write progress
        CCFSTATE_UNKNOWN = 6        # (Always the last) unknown state

    # TODO: use cdef for each item instead?
    cdef enum cbhwlib_cbFILECFG:
        cbFILECFG_OPT_NONE =         0x00000000
        cbFILECFG_OPT_KEEPALIVE =    0x00000001
        cbFILECFG_OPT_REC =          0x00000002
        cbFILECFG_OPT_STOP =         0x00000003
        cbFILECFG_OPT_NMREC =        0x00000004
        cbFILECFG_OPT_CLOSE =        0x00000005
        cbFILECFG_OPT_SYNCH =        0x00000006
        cbFILECFG_OPT_OPEN =         0x00000007

    ctypedef struct cbSCALING:
        int16_t digmin     # digital value that cooresponds with the anamin value
        int16_t digmax     # digital value that cooresponds with the anamax value
        int32_t anamin     # the minimum analog value present in the signal
        int32_t anamax     # the maximum analog value present in the signal
        int32_t anagain    # the gain applied to the default analog values to get the analog values
        char    anaunit[cbLEN_STR_UNIT+0] # the unit for the analog signal (eg, "uV" or "MPa")

    ctypedef struct cbFILTDESC:
        char        label[cbLEN_STR_LABEL+0]
        uint32_t    hpfreq     # high-pass corner frequency in milliHertz
        uint32_t    hporder    # high-pass filter order
        uint32_t    hptype     # high-pass filter type
        uint32_t    lpfreq     # low-pass frequency in milliHertz
        uint32_t    lporder    # low-pass filter order
        uint32_t    lptype     # low-pass filter type

    ctypedef struct cbMANUALUNITMAPPING:
        int16_t       nOverride
        int16_t       afOrigin[3]
        int16_t       afShape[3][3]
        int16_t       aPhi
        uint32_t      bValid # is this unit in use at this time?

    ctypedef struct cbHOOP:
        uint16_t valid # 0=undefined, 1 for valid
        int16_t  time  # time offset into spike window
        int16_t  min   # minimum value for the hoop window
        int16_t  max   # maximum value for the hoop window
        
    ctypedef struct cbPKT_CHANINFO:
        uint32_t            time                    # system clock timestamp
        uint16_t            chid                    # 0x8000
        uint8_t             type                    # cbPKTTYPE_AINP*
        uint8_t             dlen                    # cbPKT_DLENCHANINFO
        uint32_t            chan                    # actual channel id of the channel being configured
        uint32_t            proc                    # the address of the processor on which the channel resides
        uint32_t            bank                    # the address of the bank on which the channel resides
        uint32_t            term                    # the terminal number of the channel within it's bank
        uint32_t            chancaps                # general channel capablities (given by cbCHAN_* flags)
        uint32_t            doutcaps                # digital output capablities (composed of cbDOUT_* flags)
        uint32_t            dinpcaps                # digital input capablities (composed of cbDINP_* flags)
        uint32_t            aoutcaps                # analog output capablities (composed of cbAOUT_* flags)
        uint32_t            ainpcaps                # analog input capablities (composed of cbAINP_* flags)
        uint32_t            spkcaps                 # spike processing capabilities
        cbSCALING           physcalin               # physical channel scaling information
        cbFILTDESC          phyfiltin               # physical channel filter definition
        cbSCALING           physcalout              # physical channel scaling information
        cbFILTDESC          phyfiltout              # physical channel filter definition
        char                label[cbLEN_STR_LABEL+0]# Label of the channel (null terminated if <16 characters)
        uint32_t            userflags               # User flags for the channel state
        int32_t             position[4]             # reserved for future position information
        cbSCALING           scalin                  # user-defined scaling information for AINP
        cbSCALING           scalout                 # user-defined scaling information for AOUT
        uint32_t            doutopts                # digital output options (composed of cbDOUT_* flags)
        uint32_t            dinpopts                # digital input options (composed of cbDINP_* flags)
        uint32_t            aoutopts                # analog output options
        uint32_t            eopchar                 # digital input capablities (given by cbDINP_* flags)
        uint32_t              monsource
        int32_t               outvalue       # output value
        uint16_t              lowsamples     # address of channel to monitor
        uint16_t              highsamples    # address of channel to monitor
        int32_t               offset
        uint8_t             trigtype        # trigger type (see cbDOUT_TRIGGER_*)
        uint16_t            trigchan        # trigger channel
        uint16_t            trigval         # trigger value
        uint32_t            ainpopts        # analog input options (composed of cbAINP* flags)
        uint32_t            lncrate         # line noise cancellation filter adaptation rate
        uint32_t            smpfilter       # continuous-time pathway filter id
        uint32_t            smpgroup        # continuous-time pathway sample group
        int32_t             smpdispmin      # continuous-time pathway display factor
        int32_t             smpdispmax      # continuous-time pathway display factor
        uint32_t            spkfilter       # spike pathway filter id
        int32_t             spkdispmax      # spike pathway display factor
        int32_t             lncdispmax      # Line Noise pathway display factor
        uint32_t            spkopts         # spike processing options
        int32_t             spkthrlevel     # spike threshold level
        int32_t             spkthrlimit
        uint32_t            spkgroup        # NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping
        int16_t             amplrejpos      # Amplitude rejection positive value
        int16_t             amplrejneg      # Amplitude rejection negative value
        uint32_t            refelecchan     # Software reference electrode channel
        cbMANUALUNITMAPPING unitmapping[cbMAXUNITS+0]             # manual unit mapping
        cbHOOP              spkhoops[cbMAXUNITS+0][cbMAXHOOPS+0]    # spike hoop sorting set
        
    ctypedef struct cbFILTDESC:
        char        label[cbLEN_STR_LABEL+0]
        uint32_t    hpfreq      # high-pass corner frequency in milliHertz
        uint32_t    hporder     # high-pass filter order
        uint32_t    hptype      # high-pass filter type
        uint32_t    lpfreq      # low-pass frequency in milliHertz
        uint32_t    lporder     # low-pass filter order
        uint32_t    lptype      # low-pass filter type

cdef extern from "cbsdk.h":

    cdef int cbSdk_CONTINUOUS_DATA_SAMPLES = 102400
    cdef int cbSdk_EVENT_DATA_SAMPLES = (2 * 8192)
    cdef int cbSdk_MAX_UPOLOAD_SIZE = (1024 * 1024 * 1024)
    cdef float cbSdk_TICKS_PER_SECOND = 30000.0
    cdef float cbSdk_SECONDS_PER_TICK = 1.0 / cbSdk_TICKS_PER_SECOND

    ctypedef enum cbSdkResult:
        CBSDKRESULT_WARNCONVERT            =     3  # If file conversion is needed
        CBSDKRESULT_WARNCLOSED             =     2  # Library is already closed
        CBSDKRESULT_WARNOPEN               =     1  # Library is already opened
        CBSDKRESULT_SUCCESS                =     0  # Successful operation
        CBSDKRESULT_NOTIMPLEMENTED         =    -1  # Not implemented
        CBSDKRESULT_UNKNOWN                =    -2  # Unknown error
        CBSDKRESULT_INVALIDPARAM           =    -3  # Invalid parameter
        CBSDKRESULT_CLOSED                 =    -4  # Interface is closed cannot do this operation
        CBSDKRESULT_OPEN                   =    -5  # Interface is open cannot do this operation
        CBSDKRESULT_NULLPTR                =    -6  # Null pointer
        CBSDKRESULT_ERROPENCENTRAL         =    -7  # Unable to open Central interface
        CBSDKRESULT_ERROPENUDP             =    -8  # Unable to open UDP interface (might happen if default)
        CBSDKRESULT_ERROPENUDPPORT         =    -9  # Unable to open UDP port
        CBSDKRESULT_ERRMEMORYTRIAL         =   -10  # Unable to allocate RAM for trial cache data
        CBSDKRESULT_ERROPENUDPTHREAD       =   -11  # Unable to open UDP timer thread
        CBSDKRESULT_ERROPENCENTRALTHREAD   =   -12  # Unable to open Central communication thread
        CBSDKRESULT_INVALIDCHANNEL         =   -13  # Invalid channel number
        CBSDKRESULT_INVALIDCOMMENT         =   -14  # Comment too long or invalid
        CBSDKRESULT_INVALIDFILENAME        =   -15  # Filename too long or invalid
        CBSDKRESULT_INVALIDCALLBACKTYPE    =   -16  # Invalid callback type
        CBSDKRESULT_CALLBACKREGFAILED      =   -17  # Callback register/unregister failed
        CBSDKRESULT_ERRCONFIG              =   -18  # Trying to run an unconfigured method
        CBSDKRESULT_INVALIDTRACKABLE       =   -19  # Invalid trackable id, or trackable not present
        CBSDKRESULT_INVALIDVIDEOSRC        =   -20  # Invalid video source id, or video source not present
        CBSDKRESULT_ERROPENFILE            =   -21  # Cannot open file
        CBSDKRESULT_ERRFORMATFILE          =   -22  # Wrong file format
        CBSDKRESULT_OPTERRUDP              =   -23  # Socket option error (possibly permission issue)
        CBSDKRESULT_MEMERRUDP              =   -24  # Socket memory assignment error
        CBSDKRESULT_INVALIDINST            =   -25  # Invalid range or instrument address
        CBSDKRESULT_ERRMEMORY              =   -26  # library memory allocation error
        CBSDKRESULT_ERRINIT                =   -27  # Library initialization error
        CBSDKRESULT_TIMEOUT                =   -28  # Conection timeout error
        CBSDKRESULT_BUSY                   =   -29  # Resource is busy
        CBSDKRESULT_ERROFFLINE             =   -30  # Instrument is offline
        CBSDKRESULT_INSTOUTDATED           =   -31  # The instrument runs an outdated protocol version
        CBSDKRESULT_LIBOUTDATED            =   -32  # The library is outdated

    ctypedef enum cbSdkConnectionType:
        CBSDKCONNECTION_DEFAULT = 0  # Try Central then UDP
        CBSDKCONNECTION_CENTRAL = 1  # Use Central
        CBSDKCONNECTION_UDP = 2      # Use UDP
        CBSDKCONNECTION_CLOSED = 3   # Closed

    ctypedef enum cbSdkInstrumentType:
        CBSDKINSTRUMENT_NSP = 0           # NSP
        CBSDKINSTRUMENT_NPLAY = 1         # Local nPlay
        CBSDKINSTRUMENT_LOCALNSP = 2      # Local NSP
        CBSDKINSTRUMENT_REMOTENPLAY = 3   # Remote nPlay

    #  cbSdkCCFEvent. Skipping because it depends on cbStateCCF

    ctypedef enum cbSdkTrialType:
        CBSDKTRIAL_CONTINUOUS = 0
        CBSDKTRIAL_EVENTS = 1
        CBSDKTRIAL_COMMETNS = 2
        CBSDKTRIAL_TRACKING = 3

    ctypedef enum cbSdkWaveformType:
        cbSdkWaveform_NONE = 0
        cbSdkWaveform_PARAMETERS = 1
        cbSdkWaveform_SINE = 2
        cbSdkWaveform_COUNT = 3

    ctypedef enum cbSdkWaveformTriggerType:
        cbSdkWaveformTrigger_NONE           = 0  # Instant software trigger
        cbSdkWaveformTrigger_DINPREG        = 1  # digital input rising edge trigger
        cbSdkWaveformTrigger_DINPFEG        = 2  # digital input falling edge trigger
        cbSdkWaveformTrigger_SPIKEUNIT      = 3  # spike unit
        cbSdkWaveformTrigger_COMMENTCOLOR   = 4  # custom colored event (e.g. NeuroMotive event)
        cbSdkWaveformTrigger_SOFTRESET      = 5  # Soft-reset trigger (e.g. file recording start)
        cbSdkWaveformTrigger_EXTENSION      = 6  # Extension trigger
        cbSdkWaveformTrigger_COUNT          = 7  # Always the last value

    ctypedef struct cbSdkVersion:
        # Library version
        uint32_t major
        uint32_t minor
        uint32_t release
        uint32_t beta
        # Protocol version
        uint32_t majorp
        uint32_t minorp
        # NSP version
        uint32_t nspmajor
        uint32_t nspminor
        uint32_t nsprelease
        uint32_t nspbeta
        # NSP protocol version
        uint32_t nspmajorp
        uint32_t nspminorp
        
    ctypedef struct cbSdkCCFEvent:
        cbStateCCF state            # CCF state
        cbSdkResult result          # Last result
        const char * szFileName     # CCF filename under operation
        uint8_t progress            # Progress (in percent)

    ctypedef struct cbSdkTrialEvent:
        uint16_t count
        uint16_t chan[cbNUM_ANALOG_CHANS + 2]
        uint32_t num_samples[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1]
        void * timestamps[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1]
        void * waveforms[cbNUM_ANALOG_CHANS + 2]

    ctypedef struct cbSdkConnection:
        int nInPort          # Client port number
        int nOutPort         # Instrument port number
        int nRecBufSize      # Receive buffer size (0 to ignore altogether)
        const char * szInIP  # Client IPv4 address
        const char * szOutIP # Instrument IPv4 address
    cdef cbSdkConnection cbSdkConnection_DEFAULT = {cbNET_UDP_PORT_BCAST, cbNET_UDP_PORT_CNT, (4096 * 2048), "", ""}

    # Trial continuous data
    ctypedef struct cbSdkTrialCont:
        uint16_t    count                               # Number of valid channels in this trial (up to cbNUM_ANALOG_CHANS)
        uint16_t    chan[cbNUM_ANALOG_CHANS+0]          # channel numbers (1-based)
        uint16_t    sample_rates[cbNUM_ANALOG_CHANS+0]  # current sample rate (samples per second)
        uint32_t    num_samples[cbNUM_ANALOG_CHANS+0]   # number of samples
        uint32_t    time                                # start time for trial continuous data
        void *      samples[cbNUM_ANALOG_CHANS+0]       # Buffer to hold sample vectors
        
    ctypedef struct cbSdkTrialComment:
        uint16_t num_samples    # Number of comments
        uint8_t * charsets      # Buffer to hold character sets
        uint32_t * rgbas        # Buffer to hold rgba values
        uint8_t * * comments    # Pointer to comments
        void * timestamps       # Buffer to hold time stamps
        
    # Trial video tracking data
    ctypedef struct cbSdkTrialTracking:
        uint16_t    count                                   # Number of valid trackable objects (up to cbMAXTRACKOBJ)
        uint16_t    ids[cbMAXTRACKOBJ+0]                    # Node IDs (holds count elements)
        uint16_t    max_point_counts[cbMAXTRACKOBJ+0]       # Maximum point counts (holds count elements)
        uint16_t    types[cbMAXTRACKOBJ+0]                  # Node types (can be cbTRACKOBJ_TYPE_* and determines coordinate counts) (holds count elements)
        uint8_t     names[cbMAXTRACKOBJ+0][16 + 1]          # Node names (holds count elements)
        uint16_t    num_samples[cbMAXTRACKOBJ+0]            # Number of samples
        uint16_t *  point_counts[cbMAXTRACKOBJ+0]           # Buffer to hold number of valid points (up to max_point_counts) (holds count*num_samples elements)
        void * *    coords[cbMAXTRACKOBJ+0]                 # Buffer to hold tracking points (holds count*num_samples tarackables, each of max_point_counts points
        uint32_t *  synch_frame_numbers[cbMAXTRACKOBJ+0]    # Buffer to hold synch frame numbers (holds count*num_samples elements)
        uint32_t *  synch_timestamps[cbMAXTRACKOBJ+0]       # Buffer to hold synchronized tracking time stamps (in milliseconds) (holds count*num_samples elements)
        void *      timestamps[cbMAXTRACKOBJ+0]             # Buffer to hold tracking time stamps (holds count*num_samples elements)

    ctypedef struct cbSdkAoutMon:
        uint16_t chan   # (1-based) channel to monitor
        bint bTrack     # If should monitor last tracked channel
        bint bSpike     # If spike or continuous should be monitored

    ctypedef struct cbSdkWaveformData:
        cbSdkWaveformType           type
        uint32_t                    repeats
        cbSdkWaveformTriggerType    trig
        uint16_t                    trigChan
        uint16_t                    trigValue
        uint8_t                     trigNum
        int16_t                     offset
        uint16_t    sineFrequency
        int16_t     sineAmplitude
        uint16_t    phases
        uint16_t*   duration
        int16_t*    amplitude

    cbSdkResult cbSdkGetVersion(uint32_t nInstance, cbSdkVersion * version)
    # Read and write CCF requires a lot of baggage from cbhwlib.h. Skipping.
    cbSdkResult cbSdkOpen(uint32_t nInstance, cbSdkConnectionType conType, cbSdkConnection con) nogil
    cbSdkResult cbSdkGetType(uint32_t nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)  # Get connection and instrument type
    cbSdkResult cbSdkClose(uint32_t nInstance)  # Close the library
    cbSdkResult cbSdkGetTime(uint32_t nInstance, uint32_t * cbtime)  # Get the instrument sample clock time
    #cbSdkGetSpkCache
    #cbSdkGetTrialConfig and cbSdkSetTrialConfig are better handled by cbsdk_helper due to optional arguments.
    cbSdkResult cbSdkUnsetTrialConfig(uint32_t nInstance, cbSdkTrialType type)
    cbSdkResult cbSdkGetChannelLabel(int nInstance, uint16_t channel, uint32_t * bValid, char * label, uint32_t * userflags, int32_t * position)
    cbSdkResult cbSdkSetChannelLabel(uint32_t nInstance, uint16_t channel, const char * label, uint32_t userflags, int32_t * position)
    # Retrieve data of a trial (NULL means ignore), user should allocate enough buffers beforehand, and trial should not be closed during this call
    cbSdkResult cbSdkGetTrialData(  uint32_t nInstance,
                                    uint32_t bActive, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                    cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
    # Initialize the structures (and fill with information about active channels, comment pointers and samples in the buffer)
    cbSdkResult cbSdkInitTrialData( uint32_t nInstance,
                                    cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont,
                                    cbSdkTrialComment * trialcomment, cbSdkTrialTracking * trialtracking)
    #cbSdkSetFileConfig
    cbSdkResult cbSdkGetFileConfig(uint32_t nInstance, char * filename, char * username, bool * pbRecording)
    cbSdkResult cbSdkSetPatientInfo(uint32_t nInstance, const char * ID, const char * firstname, const char * lastname, uint32_t DOBMonth, uint32_t DOBDay, uint32_t DOBYear)
    cbSdkResult cbSdkInitiateImpedance(uint32_t nInstance)
    cbSdkResult cbSdkSendPoll(uint32_t nInstance, const char* appname, uint32_t mode, uint32_t flags, uint32_t extra)
    #cbSdkSendPacket
    #cbSdkSetSystemRunLevel
    cbSdkResult cbSdkSetDigitalOutput(uint32_t nInstance, uint16_t channel, uint16_t value)
    cbSdkResult cbSdkSetSynchOutput(uint32_t nInstance, uint16_t channel, uint32_t nFreq, uint32_t nRepeats)
    #cbSdkExtDoCommand
    cbSdkResult cbSdkSetAnalogOutput(uint32_t nInstance, uint16_t channel, cbSdkWaveformData* wf, cbSdkAoutMon* mon)
    cbSdkResult cbSdkSetChannelMask(uint32_t nInstance, uint16_t channel, uint32_t bActive)
    cbSdkResult cbSdkSetComment(uint32_t nInstance, uint32_t rgba, uint8_t charset, const char * comment)
    cbSdkResult cbSdkSetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo)
    cbSdkResult cbSdkGetChannelConfig(uint32_t nInstance, uint16_t channel, cbPKT_CHANINFO * chaninfo)
    cbSdkResult cbSdkGetFilterDesc(uint32_t nInstance, uint32_t proc, uint32_t filt, cbFILTDESC * filtdesc)
    cbSdkResult cbSdkGetSampleGroupList(uint32_t nInstance, uint32_t proc, uint32_t group, uint32_t *length, uint32_t *list)
    cbSdkResult cbSdkGetSampleGroupInfo(uint32_t nInstance, uint32_t proc, uint32_t group, char *label, uint32_t *period, uint32_t *length)


cdef extern from "cbsdk_helper.h":

    ctypedef struct cbSdkConfigParam:
        uint32_t bActive
        uint16_t Begchan
        uint32_t Begmask
        uint32_t Begval
        uint16_t Endchan
        uint32_t Endmask
        uint32_t Endval
        int bDouble
        uint32_t uWaveforms
        uint32_t uConts
        uint32_t uEvents
        uint32_t uComments
        uint32_t uTrackings
        int bAbsolute

    cbSdkResult cbsdk_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param)
    cbSdkResult cbsdk_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param)

    cbSdkResult cbsdk_init_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent)
    cbSdkResult cbsdk_get_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent)

    cbSdkResult cbsdk_init_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont)
    cbSdkResult cbsdk_get_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont)

    cbSdkResult cbsdk_init_trial_comment(int nInstance, int reset, cbSdkTrialComment * trialcomm)
    cbSdkResult cbsdk_get_trial_comment(int nInstance, int reset, cbSdkTrialComment * trialcomm)

    cbSdkResult cbsdk_file_config(int instance, const char * filename, const char * comment, int start, unsigned int options)