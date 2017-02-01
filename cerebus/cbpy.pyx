'''
Created on March 9, 2013

@author: dashesy

Purpose: Python module for cbsdk_cython

'''

from cbsdk_cython cimport *
from libcpp cimport bool
import sys
import numpy as np
cimport numpy as np
cimport cython

def version(instance=0):
    '''Get library version
    Inputs:
        instance - (optional) library instance number
    Outputs:"
        dictionary with following keys
             major - major API version
             minor - minor API version
             release - release API version
             beta - beta API version (0 if a non-beta)
             protocol_major - major protocol version
             protocol_minor - minor protocol version
             nsp_major - major NSP firmware version
             nsp_minor - minor NSP firmware version
             nsp_release - release NSP firmware version
             nsp_beta - beta NSP firmware version (0 if non-beta))
             nsp_protocol_major - major NSP protocol version
             nsp_protocol_minor - minor NSP protocol version
    '''

    cdef cbSdkResult res
    cdef cbSdkVersion ver

    res = cbSdkGetVersion(<uint32_t>instance, &ver)
    handle_result(res)
    
    ver_dict = {'major':ver.major, 'minor':ver.minor, 'release':ver.release, 'beta':ver.beta,
                'protocol_major':ver.majorp, 'protocol_minor':ver.majorp,
                'nsp_major':ver.nspmajor, 'nsp_minor':ver.nspminor, 'nsp_release':ver.nsprelease, 'nsp_beta':ver.nspbeta,
                'nsp_protocol_major':ver.nspmajorp, 'nsp_protocol_minor':ver.nspmajorp
                }
    return res, ver_dict

def defaultConParams():
    #Note: Defaulting to 255.255.255.255 assumes the client is connected to the NSP via a switch.
    #A direct connection might require the client-addr to be "192.168.137.1"
    con_parms = {
        'client-addr': str(cbNET_UDP_ADDR_BCAST.decode("utf-8")) if sys.platform == 'linux2' else '255.255.255.255',
        'client-port': cbNET_UDP_PORT_BCAST,
        'inst-addr': cbNET_UDP_ADDR_CNT.decode("utf-8"),
        'inst-port': cbNET_UDP_PORT_CNT,
        'receive-buffer-size': (8 * 1024 * 1024) if sys.platform == 'win32' else (6 * 1024 * 1024)
    }
    return con_parms

def open(instance=0, connection='default', parameter={}):
    '''Open library.
    Inputs:
       connection - connection type, string can be one the following
               'default': tries slave then master connection
               'master': tries master connection (UDP)
               'slave': tries slave connection (needs another master already open)
       parameter - dictionary with following keys (all optional)
               'inst-addr': instrument IPv4 address.
               'inst-port': instrument port number.
               'client-addr': client IPv4 address.
               'client-port': client port number.
               'receive-buffer-size': override default network buffer size (low value may result in drops).
       instance - (optional) library instance number
    Outputs:
        Same as "get_connection_type" command output

    Test with:
from cerebus import cbpy
cbpy.open(parameter=cbpy.defaultConParams())
    '''
    
    cdef cbSdkResult res
    
    wconType = {'default': CBSDKCONNECTION_DEFAULT, 'slave': CBSDKCONNECTION_CENTRAL, 'master': CBSDKCONNECTION_UDP} 
    if not connection in wconType.keys():
        raise RuntimeError("invalid connection %s" % connection)

    cdef cbSdkConnectionType conType = wconType[connection]
    cdef cbSdkConnection con

    cdef bytes szOutIP = parameter.get('inst-addr', cbNET_UDP_ADDR_CNT).encode()
    cdef bytes szInIP  = parameter.get('client-addr', '').encode()
    
    con.szOutIP = szOutIP
    con.nOutPort = parameter.get('inst-port', cbNET_UDP_PORT_CNT)
    con.szInIP = szInIP
    con.nInPort = parameter.get('client-port', cbNET_UDP_PORT_BCAST)
    con.nRecBufSize = parameter.get('receive-buffer-size', 0)

    res = cbSdkOpen(<uint32_t>instance, conType, con)

    handle_result(res)
    
    return res, get_connection_type(instance=instance)

def close(instance=0):
    '''Close library.
    Inputs:
       instance - (optional) library instance number
    '''
    return handle_result(cbSdkClose(<uint32_t>instance))
    
def get_connection_type(instance=0):
    ''' Get connection type
    Inputs:
       instance - (optional) library instance number
    Outputs:
       dictionary with following keys
           'connection': Final established connection; can be any of:
                          'Default', 'Slave', 'Master', 'Closed', 'Unknown'
           'instrument': Instrument connected to; can be any of:
                          'NSP', 'nPlay', 'Local NSP', 'Remote nPlay', 'Unknown')
    '''
    
    cdef cbSdkResult res
    
    cdef cbSdkConnectionType conType
    cdef cbSdkInstrumentType instType

    res = cbSdkGetType(<uint32_t>instance, &conType, &instType)

    handle_result(res)
    
    connections = ["Default", "Slave", "Master", "Closed", "Unknown"]
    instruments = ["NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"]

    con_idx = conType
    if con_idx < 0 or con_idx >= len(connections):
        con_idx = len(connections) - 1
    inst_idx = instType 
    if inst_idx < 0 or inst_idx >= len(instruments):
        inst_idx = len(instruments) - 1
        
    return {'connection':connections[con_idx], 'instrument':instruments[inst_idx]}

def trial_config(instance=0, reset=True, 
                 buffer_parameter={}, 
                 range_parameter={},
                 noevent=0, nocontinuous=0):
    '''Configure trial settings.
    Inputs:
       reset - boolean, set True to flush data cache and start collecting data immediately,
               set False to stop collecting data immediately
       buffer_parameter - (optional) dictionary with following keys (all optional)
               'double': boolean, if specified, the data is in double precision format
               'absolute': boolean, if specified event timing is absolute (new polling will not reset time for events)
               'continuous_length': set the number of continuous data to be cached
               'event_length': set the number of events to be cached
               'comment_length': set number of comments to be cached
               'tracking_length': set the number of video tracking events to be cached
       range_parameter - (optional) dictionary with following keys (all optional)
               'begin_channel': integer, channel to start polling if certain value seen
               'begin_mask': integer, channel mask to start polling if certain value seen
               'begin_value': value to start polling
               'end_channel': channel to end polling if certain value seen
               'end_mask': channel mask to end polling if certain value seen
               'end_value': value to end polling
       'noevent': equivalent of setting 'event_length' to 0
       'nocontinuous': equivalent of setting 'continuous_length' to 0
       instance - (optional) library instance number
    Outputs:
       reset - (boolean) if it is reset
    '''    
    
    cdef int res
    cdef cbSdkConfigParam cfg_param
    
    # retrieve old values
    res = cbsdk_get_trial_config(<int>instance, &cfg_param)
    handle_result(<cbSdkResult>res)
    
    cfg_param.bActive = reset
    
    # Fill cfg_param with provided buffer_parameter values or default.
    cfg_param.bDouble = buffer_parameter.get('double', cfg_param.bDouble)
    cfg_param.uWaveforms = 0 # does not work anyways
    cfg_param.uConts = 0 if nocontinuous else buffer_parameter.get('continuous_length', cbSdk_CONTINUOUS_DATA_SAMPLES)
    cfg_param.uEvents = 0 if noevent else buffer_parameter.get('event_length', cbSdk_EVENT_DATA_SAMPLES)
    cfg_param.uComments = buffer_parameter.get('comment_length', 0)
    cfg_param.uTrackings = buffer_parameter.get('tracking_length', 0)
    cfg_param.bAbsolute = buffer_parameter.get('absolute', 0)
    
    # Fill cfg_param mask-related parameters with provided range_parameter or default.
    cfg_param.Begchan = range_parameter.get('begin_channel', 0)
    cfg_param.Begmask = range_parameter.get('begin_mask', 0)
    cfg_param.Begval = range_parameter.get('begin_value', 0)
    cfg_param.Endchan = range_parameter.get('end_channel', 0)
    cfg_param.Endmask = range_parameter.get('end_mask', 0)
    cfg_param.Endval = range_parameter.get('end_value', 0)
    
    res = cbsdk_set_trial_config(<int>instance, &cfg_param)

    handle_result(<cbSdkResult>res)
    
    return res, reset
    
def trial_event(instance=0, reset=False):
    ''' Trial spike and event data.
    Inputs:
       reset - (optional) boolean 
               set False (default) to leave buffer intact.
               set True to clear all the data and reset the trial time to the current time.
       instance - (optional) library instance number
    Outputs:
       list of arrays [channel, {'timestamps':[unit0_ts, ..., unitN_ts], 'events':digital_events}]
           channel: integer, channel number (1-based)
           digital_events: array, digital event values for channel (if a digital or serial channel)
           unitN_ts: array, spike timestamps of unit N for channel (if an electrode channel));
    '''
    
    cdef int res
    cdef cbSdkConfigParam cfg_param
    cdef cbSdkTrialEvent trialevent
    
    trial = []
    
    # retrieve old values
    res = cbsdk_get_trial_config(<int>instance, &cfg_param)
    handle_result(<cbSdkResult>res)
    
    # get how many samples are avaialble
    res = cbsdk_init_trial_event(<int>instance, &trialevent)
    handle_result(<cbSdkResult>res)
    
    if trialevent.count == 0:
        return res, trial

    cdef np.double_t[:] mxa_d
    cdef np.uint32_t[:] mxa_u32
    cdef np.uint16_t[:] mxa_u16
    
    # allocate memory
    for channel in range(trialevent.count):
        ch = trialevent.chan[channel] # Actual channel number
        
        timestamps = []
        # Fill timestamps for non-empty channels
        for u in range(cbMAXUNITS+1):
            trialevent.timestamps[channel][u] = NULL
            num_samples = trialevent.num_samples[channel][u]
            ts = []
            if num_samples:
                if cfg_param.bDouble:
                    mxa_d = np.zeros(num_samples, dtype=np.double)
                    trialevent.timestamps[channel][u] = <void *>&mxa_d[0]
                    ts = np.asarray(mxa_d)
                else:
                    mxa_u32 = np.zeros(num_samples, dtype=np.uint32)
                    trialevent.timestamps[channel][u] = <void *>&mxa_u32[0]
                    ts = np.asarray(mxa_u32)
            timestamps.append(ts)
        
        trialevent.waveforms[channel] = NULL
        dig_events = []
        # Fill values for non-empty digital or serial channels
        if ch == MAX_CHANS_DIGITAL_IN or ch == MAX_CHANS_SERIAL:
            num_samples = trialevent.num_samples[channel][0]
            if num_samples:
                if cfg_param.bDouble:
                    mxa_d = np.zeros(num_samples, dtype=np.double)
                    trialevent.waveforms[channel] = <void *>&mxa_d[0]
                    dig_events = np.asarray(mxa_d)
                else:
                    mxa_u16 = np.zeros(num_samples, dtype=np.uint16)
                    trialevent.waveforms[channel] = <void *>&mxa_u16[0]
                    dig_events = np.asarray(mxa_u16)
        
        trial.append([ch, {'timestamps':timestamps, 'events':dig_events}])
    
    # get the trial
    res = cbsdk_get_trial_event(<int>instance, <int>reset, &trialevent)
    handle_result(<cbSdkResult>res)

    return res, trial

def trial_continuous(instance=0, reset=False):
    ''' Trial continuous data.
    Inputs:
       reset - (optional) boolean 
               set False (default) to leave buffer intact.
               set True to clear all the data and reset the trial time to the current time.
       instance - (optional) library instance number
    Outputs:
       list of the form [channel, continuous_array]
           channel: integer, channel number (1-based)
           continuous_array: array, continuous values for channel)
    '''
    
    cdef int res
    cdef cbSdkConfigParam cfg_param
    cdef cbSdkTrialCont trialcont
    
    trial = []
    
    # retrieve old values
    res = cbsdk_get_trial_config(<int>instance, &cfg_param)
    handle_result(<cbSdkResult>res)
    
    # get how many samples are avaialble
    res = cbsdk_init_trial_cont(<int>instance, &trialcont)
    handle_result(<cbSdkResult>res)
    
    if trialcont.count == 0:
        return res, trial

    cdef np.double_t[:] mxa_d
    cdef np.int16_t[:] mxa_i16

    # allocate memory
    for channel in range(trialcont.count):
        ch = trialcont.chan[channel] # Actual channel number
        
        row = [ch]
        
        trialcont.samples[channel] = NULL
        num_samples = trialcont.num_samples[channel]
        if cfg_param.bDouble:
            mxa_d = np.zeros(num_samples, dtype=np.double)
            if num_samples:
                trialcont.samples[channel] = <void *>&mxa_d[0]
            cont = np.asarray(mxa_d)
        else:
            mxa_i16 = np.zeros(num_samples, dtype=np.int16)
            if num_samples:
                trialcont.samples[channel] = <void *>&mxa_i16[0]
            cont = np.asarray(mxa_i16)
            
        row.append(cont)
        trial.append(row)
        
    # get the trial
    res = cbsdk_get_trial_cont(<int>instance, <int>reset, &trialcont)
    handle_result(<cbSdkResult>res)

    return res, trial
    
def file_config(instance=0, command='info', comment='', filename=''):
    ''' Configure remote file recording or get status of recording.
    Inputs:
       command - string, File configuration command, can be of of the following
               'info': (default) get File recording information
               'open': opens the File dialog if closed, ignoring other parameters
               'close': closes the File dialog if open
               'start': starts recording, opens dialog if closed
               'stop': stops recording
       filename - (optional) string, file name to use for recording
       comment - (optional) string, file comment to use for file recording
       instance - (optional) library instance number
    Outputs:
       Only if command is 'info' output is returned
       A dictionary with following keys:
           'Recording': boolean, if recording is in progress
           'FileName': string, file name being recorded
           'UserName': Computer that is recording
    '''
    
    
    cdef cbSdkResult res
    cdef char fname[256]
    cdef char username[256]
    cdef bool bRecording = False
    
    if command == 'info':

        res = cbSdkGetFileConfig(<uint32_t>instance, fname, username, &bRecording)
        handle_result(res)
        info = {'Recording':bRecording, 'FileName':<bytes>fname, 'UserName':<bytes>username}
        return res, info
   
    cdef int start = 0 
    cdef unsigned int options = cbFILECFG_OPT_NONE     
    if command == 'open':
        if filename or comment:
            raise RuntimeError('filename and comment should not be specified for open')
        options = cbFILECFG_OPT_OPEN
    elif command == 'close':
        options = cbFILECFG_OPT_CLOSE
    elif command == 'start':
        if not filename:
            raise RuntimeError('filename should be specified for start')
        start = 1
    elif command == 'stop':
        if not filename:
            raise RuntimeError('filename should be specified for stop')
        start = 0
    else:
        raise RuntimeError("invalid file config command %s" % command)

    cdef int set_res
    filename_string = filename.encode('UTF-8')
    comment_string = comment.encode('UTF-8')
    set_res = cbsdk_file_config(<int>instance, <const char *>filename_string, <char *>comment_string, start, options)
    
    return set_res


def time(instance=0, unit='samples'):
    '''Instrument time.
    Inputs:
       unit - time unit, string can be one the following
                'samples': (default) sample number integer
                'seconds' or 's': seconds calculated from samples
                'milliseconds' or 'ms': milliseconds calculated from samples
       instance - (optional) library instance number
    Outputs:
       time - time passed since last reset
    '''


    cdef cbSdkResult res

    if unit == 'samples':
        factor = 1
    elif unit in ['seconds', 's']:
        raise NotImplementedError("Use time unit of samples for now")
    elif unit in ['milliseconds', 'ms']:
        raise NotImplementedError("Use time unit of samples for now")
    else:
        raise RuntimeError("Invalid time unit %s" % unit)

    cdef uint32_t cbtime
    res = cbSdkGetTime(<uint32_t>instance, &cbtime)

    time = float(cbtime) / factor
    
    return res, time


def digital_out(channel, instance=0, value='low'):
    '''Digital output command.
    Inputs:
    channel - integer, digital output channel number (1-based)
               On NSP, 153 (dout1), 154 (dout2), 155 (dout3), 156 (dout4)
    value - (optional), depends on the command
           for command of 'set_value':
               string, can be 'high' or 'low' (default)
    instance - (optional) library instance number'''
    
    values = ['low', 'high']
    if value not in values:
        raise RuntimeError("Invalid value %s" % value)

    cdef cbSdkResult res
    cdef int int_val = values.index(value)
    res = cbSdkSetDigitalOutput(<uint32_t>instance, <uint16_t>channel, int_val)
    
    return res

def get_channel_config(channel, instance=0):
    '''
    '''
    cdef cbSdkResult res
    cdef cbPKT_CHANINFO chan_info
    res = cbSdkGetChannelConfig(<uint32_t>instance, <uint16_t>channel, &chan_info)
    print(chan_info)
    print(chan_info.time, chan_info.chid, chan_info.dlen,
        chan_info.chan, chan_info.proc, chan_info.bank, chan_info.term,
        chan_info.chancaps, chan_info.doutcaps)

cdef cbSdkResult handle_result(cbSdkResult res):
    if (res == CBSDKRESULT_WARNCLOSED):
        print("Library is already closed.")
    if (res < 0):
        errtext = ""
        if (res == CBSDKRESULT_ERROFFLINE):
            errtext = "Instrument is offline."
        elif (res == CBSDKRESULT_CLOSED):
            errtext = "Interface is closed; cannot do this operation."

        raise RuntimeError(("%d, " + errtext) % res)
    return res
