'''
@date March 9, 2014

@author: dashesy

Purpose: C interface for cbpy wrapper 

'''

from libc.stdint cimport uint32_t, uint16_t

cdef extern from "cbpy.h":
    
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
        
    
    int cbpy_version(int nInstance, cbSdkVersion * ver)

    ctypedef enum cbSdkConnectionType:
        CBSDKCONNECTION_DEFAULT = 0  # Try Central then UDP
        CBSDKCONNECTION_CENTRAL = 1  # Use Central
        CBSDKCONNECTION_UDP = 2      # Use UDP
        CBSDKCONNECTION_CLOSED = 3   # Closed
    
    ctypedef struct cbSdkConnection:
        int nInPort          # Client port number
        int nOutPort         # Instrument port number
        int nRecBufSize      # Receive buffer size (0 to ignore altogether)
        char * szInIP        # Client IPv4 address
        char * szOutIP       # Instrument IPv4 address
        
        
    int cbpy_open(int nInstance, cbSdkConnectionType conType, cbSdkConnection con)
    
    ctypedef enum cbSdkInstrumentType:
        CBSDKINSTRUMENT_NSP = 0           # NSP
        CBSDKINSTRUMENT_NPLAY = 1         # Local nPlay
        CBSDKINSTRUMENT_LOCALNSP = 2      # Local NSP
        CBSDKINSTRUMENT_REMOTENPLAY = 3   # Remote nPlay
        
    int cbpy_gettype(int nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
    
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
    
    cdef enum sdk_bufer_range:
        cbSdk_CONTINUOUS_DATA_SAMPLES = 102400
        cbSdk_EVENT_DATA_SAMPLES = (2 * 8192)
                    
    int cbpy_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param)
    int cbpy_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param)

    # need to find a better way for cbhwlib constants
    cdef enum cbhwlib_consts:
        cbNUM_FE_CHANS = 128
        cbNUM_ANAIN_CHANS = 16
        cbNUM_ANALOG_CHANS =  (cbNUM_FE_CHANS+cbNUM_ANAIN_CHANS)
        cbMAXUNITS = 5
        MAX_CHANS_DIGITAL_IN = (cbNUM_FE_CHANS+cbNUM_ANAIN_CHANS+4+2+1)
        MAX_CHANS_SERIAL = (MAX_CHANS_DIGITAL_IN+1)
    
    ctypedef struct cbSdkTrialEvent:
        uint16_t count
        uint16_t chan[cbNUM_ANALOG_CHANS + 2]
        uint32_t num_samples[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1]
        void * timestamps[cbNUM_ANALOG_CHANS + 2][cbMAXUNITS + 1]
        void * waveforms[cbNUM_ANALOG_CHANS + 2]
    
    int cbpy_init_trial_event(int nInstance, cbSdkTrialEvent * tiralevent)
    int cbpy_get_trial_event(int nInstance, int reset, cbSdkTrialEvent * tiralevent)
    
            