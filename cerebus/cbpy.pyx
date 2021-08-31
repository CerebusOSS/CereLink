# distutils: language = c++
# cython: language_level=2

"""
Created on March 9, 2013

@author: dashesy

Purpose: Python module for cbsdk_cython

"""

from cbsdk_cython cimport *
from libcpp cimport bool
from libc.stdlib cimport malloc, free
from libc.string cimport strncpy
import sys
import numpy as np
import locale
cimport numpy as np
cimport cython


def version(int instance=0):
    """Get library version
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
    """

    cdef cbSdkResult res
    cdef cbSdkVersion ver

    res = cbSdkGetVersion(<uint32_t>instance, &ver)
    handle_result(res)
    
    ver_dict = {'major':ver.major, 'minor':ver.minor, 'release':ver.release, 'beta':ver.beta,
                'protocol_major':ver.majorp, 'protocol_minor':ver.majorp,
                'nsp_major':ver.nspmajor, 'nsp_minor':ver.nspminor, 'nsp_release':ver.nsprelease, 'nsp_beta':ver.nspbeta,
                'nsp_protocol_major':ver.nspmajorp, 'nsp_protocol_minor':ver.nspmajorp
                }
    return <int>res, ver_dict


def defaultConParams():
    #Note: Defaulting to 255.255.255.255 assumes the client is connected to the NSP via a switch.
    #A direct connection might require the client-addr to be "192.168.137.1"
    con_parms = {
        'client-addr': str(cbNET_UDP_ADDR_BCAST.decode("utf-8"))\
            if ('linux' in sys.platform or 'linux2' in sys.platform) else '255.255.255.255',
        'client-port': cbNET_UDP_PORT_BCAST,
        'inst-addr': cbNET_UDP_ADDR_CNT.decode("utf-8"),
        'inst-port': cbNET_UDP_PORT_CNT,
        'receive-buffer-size': (8 * 1024 * 1024) if sys.platform == 'win32' else (6 * 1024 * 1024)
    }
    return con_parms


def open(int instance=0, connection='default', parameter={}):
    """Open library.
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
    """
    
    cdef cbSdkResult res
    
    wconType = {'default': CBSDKCONNECTION_DEFAULT, 'slave': CBSDKCONNECTION_CENTRAL, 'master': CBSDKCONNECTION_UDP} 
    if not connection in wconType.keys():
        raise RuntimeError("invalid connection %s" % connection)

    cdef cbSdkConnectionType conType = wconType[connection]
    cdef cbSdkConnection con

    cdef bytes szOutIP = parameter.get('inst-addr', cbNET_UDP_ADDR_CNT.decode("utf-8")).encode()
    cdef bytes szInIP  = parameter.get('client-addr', '').encode()
    
    con.szOutIP = szOutIP
    con.nOutPort = parameter.get('inst-port', cbNET_UDP_PORT_CNT)
    con.szInIP = szInIP
    con.nInPort = parameter.get('client-port', cbNET_UDP_PORT_BCAST)
    con.nRecBufSize = parameter.get('receive-buffer-size', 0)

    res = cbSdkOpen(<uint32_t>instance, conType, con)

    handle_result(res)
    
    return <int>res, get_connection_type(instance=instance)


def close(int instance=0):
    """Close library.
    Inputs:
       instance - (optional) library instance number
    """
    return handle_result(cbSdkClose(<uint32_t>instance))


def get_connection_type(int instance=0):
    """ Get connection type
    Inputs:
       instance - (optional) library instance number
    Outputs:
       dictionary with following keys
           'connection': Final established connection; can be any of:
                          'Default', 'Slave', 'Master', 'Closed', 'Unknown'
           'instrument': Instrument connected to; can be any of:
                          'NSP', 'nPlay', 'Local NSP', 'Remote nPlay', 'Unknown')
    """
    
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


def trial_config(int instance=0, reset=True,
                 buffer_parameter={}, 
                 range_parameter={},
                 noevent=0, nocontinuous=0, nocomment=0):
    """Configure trial settings.
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
       'noevent': equivalent of setting buffer_parameter['event_length'] to 0
       'nocontinuous': equivalent of setting buffer_parameter['continuous_length'] to 0
       'nocomment': equivalent of setting buffer_parameter['comment_length'] to 0
       instance - (optional) library instance number
    Outputs:
       reset - (boolean) if it is reset
    """
    
    cdef cbSdkResult res
    cdef cbSdkConfigParam cfg_param
    
    # retrieve old values
    res = cbsdk_get_trial_config(<uint32_t>instance, &cfg_param)
    handle_result(res)
    
    cfg_param.bActive = <uint32_t>reset
    
    # Fill cfg_param with provided buffer_parameter values or default.
    cfg_param.bDouble = buffer_parameter.get('double', cfg_param.bDouble)
    cfg_param.uWaveforms = 0 # does not work anyways
    cfg_param.uConts = 0 if nocontinuous else buffer_parameter.get('continuous_length', cbSdk_CONTINUOUS_DATA_SAMPLES)
    cfg_param.uEvents = 0 if noevent else buffer_parameter.get('event_length', cbSdk_EVENT_DATA_SAMPLES)
    cfg_param.uComments = 0 if nocomment else buffer_parameter.get('comment_length', 0)
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

    handle_result(res)
    
    return <int>res, reset


def trial_event(int instance=0, bool reset=False, bool reset_clock=False):
    """ Trial spike and event data.
    Inputs:
       instance - (optional) library instance number
       reset - (optional) boolean
               set False (default) to leave buffer intact.
               set True to clear all the data after it has been retrieved.
       reset_clock - (optional) boolean
                set False (default) to leave the trial clock alone.
                set True to update the trial time to the current time (seems inconsistent?)
    Outputs:
       list of arrays [channel, {'timestamps':[unit0_ts, ..., unitN_ts], 'events':digital_events}]
           channel: integer, channel number (1-based)
           digital_events: array, digital event values for channel (if a digital or serial channel)
           unitN_ts: array, spike timestamps of unit N for channel (if an electrode channel));
    """
    
    cdef cbSdkResult res
    cdef cbSdkConfigParam cfg_param
    cdef cbSdkTrialEvent trialevent
    cdef uint8_t ch_type
    
    trial = []
    
    # retrieve old values
    res = cbsdk_get_trial_config(<uint32_t>instance, &cfg_param)
    handle_result(res)
    
    # get how many samples are available
    res = cbsdk_init_trial_event(<uint32_t>instance, <int>reset_clock, &trialevent)
    handle_result(res)
    
    if trialevent.count == 0:
        return res, trial

    cdef np.double_t[:] mxa_d
    cdef np.uint32_t[:] mxa_u32
    cdef np.uint16_t[:] mxa_u16
    
    # allocate memory
    for ev_ix in range(trialevent.count):
        ch = trialevent.chan[ev_ix] # Actual channel number
        
        timestamps = []
        # Fill timestamps for non-empty events
        for u in range(cbMAXUNITS+1):
            trialevent.timestamps[ev_ix][u] = NULL
            num_samples = trialevent.num_samples[ev_ix][u]
            ts = []
            if num_samples:
                if cfg_param.bDouble:
                    mxa_d = np.zeros(num_samples, dtype=np.double)
                    trialevent.timestamps[ev_ix][u] = <void *>&mxa_d[0]
                    ts = np.asarray(mxa_d)
                else:
                    mxa_u32 = np.zeros(num_samples, dtype=np.uint32)
                    trialevent.timestamps[ev_ix][u] = <void *>&mxa_u32[0]
                    ts = np.asarray(mxa_u32)
            timestamps.append(ts)
        
        trialevent.waveforms[ev_ix] = NULL
        dig_events = []
        res = cbSdkGetChannelType(<uint32_t>instance, ch, &ch_type)
        handle_result(res)
        # Fill values for non-empty digital or serial channels
        if (ch_type == cbhwlib_cbCHANTYPES.cbCHANTYPE_DIGIN) or (ch_type == cbhwlib_cbCHANTYPES.cbCHANTYPE_SERIAL):
            num_samples = trialevent.num_samples[ev_ix][0]
            if num_samples:
                if cfg_param.bDouble:
                    mxa_d = np.zeros(num_samples, dtype=np.double)
                    trialevent.waveforms[ev_ix] = <void *>&mxa_d[0]
                    dig_events = np.asarray(mxa_d)
                else:
                    mxa_u16 = np.zeros(num_samples, dtype=np.uint16)
                    trialevent.waveforms[ev_ix] = <void *>&mxa_u16[0]
                    dig_events = np.asarray(mxa_u16)
        
        trial.append([ch, {'timestamps':timestamps, 'events':dig_events}])
    
    # get the trial
    res = cbsdk_get_trial_event(<uint32_t>instance, <int>reset, &trialevent)
    handle_result(res)

    return <int>res, trial


def trial_continuous(int instance=0, bool reset=False):
    """
    Trial continuous data.
    Inputs:
       reset - (optional) boolean 
               set False (default) to leave buffer intact.
               set True to clear all the data and reset the trial time to the current time.
       instance - (optional) library instance number
    Outputs:
       res   - result code
       trial - list of the form [channel, continuous_array]
           channel: integer, channel number (1-based)
           continuous_array: array, continuous values for channel)
       timestamp of sample 0
    """
    
    cdef cbSdkResult res
    cdef cbSdkConfigParam cfg_param
    cdef cbSdkTrialCont trialcont
    
    trial = []
    
    # retrieve old values
    res = cbsdk_get_trial_config(<uint32_t>instance, &cfg_param)
    handle_result(res)
    
    # get how many samples are available
    res = cbsdk_init_trial_cont(<uint32_t>instance, <int>reset, &trialcont)
    handle_result(res)
    
    if trialcont.count == 0:
        return res, trial, None

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
    res = cbsdk_get_trial_cont(<uint32_t>instance, <int>reset, &trialcont)
    handle_result(res)

    return <int>res, trial, trialcont.time


def trial_data(int instance=0, bool reset=False, bool reset_clock=False, bool is_double=False,
               bool do_event=True, bool do_cont=True, bool do_comment=False, unsigned long wait_for_comment_msec=250):
    """

    :param instance: (optional) library instance number
    :param reset: (optional) boolean
               set False (default) to leave buffer intact.
               set True to clear all the data and reset the trial time to the current time.
    :param reset_clock - (optional) boolean
                set False (default) to leave the trial clock alone.
                set True to update the trial time to the current time (seems inconsistent?)
    :param is_double: (optional) boolean
                set False (default) to use int16
                set True to use double
    :param do_event: (optional) boolean. Set to False to skip fetching events.
    :param do_cont: (optional) boolean. Set to False to skip fetching continuous data.
    :param do_comment: (optional) boolean. Set to True to fetch comments.
    :param wait_for_comment_msec: (optional) unsigned long. How long we should wait for new comments.
               Default (0) will not wait and will only return comments that existed prior to calling this.
    :return: (result, event_data, continuous_data, t_zero, comment_data)
             res: (int) returned by cbsdk
             event data: list of arrays [channel, {'timestamps':[unit0_ts, ..., unitN_ts], 'events':digital_events}]
                channel: integer, channel number (1-based)
                digital_events: array, digital event values for channel (if a digital or serial channel)
                unitN_ts: array, spike timestamps of unit N for channel (if an electrode channel));
             continuous data: list of the form [channel, continuous_array]
                channel: integer, channel number (1-based)
                continuous_array: array, continuous values for channel)
             t_zero: timestamp of sample 0
             comment_data: list of lists the form [timestamp, comment_str, charset, rgba]
                timestamp: ?
                comment_str: the comment in binary.
                             Use comment_str.decode('utf-16' if charset==1 else locale.getpreferredencoding())
                rgba: integer; the comment colour. 8 bits each for r, g, b, a
    """

    cdef cbSdkResult res
    cdef cbSdkTrialCont trialcont
    cdef cbSdkTrialEvent trialevent
    cdef cbSdkTrialComment trialcomm
    cdef uint8_t ch_type

    cdef uint32_t tzero = 0
    cdef int comm_ix
    cdef uint32_t num_samples
    cdef int channel
    cdef uint16_t ch
    cdef int u

    cdef np.double_t[:] mxa_d
    cdef np.int16_t[:] mxa_i16
    cdef np.uint32_t[:] mxa_u32
    cdef np.uint16_t[:] mxa_u16
    cdef np.uint8_t[:] mxa_u8

    trial_event = []
    trial_cont = []
    trial_comment = []

    # get how many samples are available
    res = cbsdk_init_trial_data(<uint32_t>instance, <int>reset_clock, &trialevent if do_event else NULL,
                                &trialcont if do_cont else NULL, &trialcomm if do_comment else NULL,
                                wait_for_comment_msec)
    handle_result(res)

    # Early return if none of the requested data are available.
    if (not do_event or (trialevent.count == 0)) and (not do_cont or (trialcont.count == 0))\
            and (not do_comment or (trialcomm.num_samples == 0)):
        return res, trial_event, trial_cont, tzero, trial_comment

    # Events #
    # ------ #
    if do_event:
        # allocate memory and prepare outputs.
        for channel in range(trialevent.count):
            # First the spike timestamps.
            ev_timestamps = []
            for u in range(cbMAXUNITS+1):
                trialevent.timestamps[channel][u] = NULL
                num_samples = trialevent.num_samples[channel][u]
                ts = []
                if num_samples > 0:
                    if is_double:
                        mxa_d = np.zeros(num_samples, dtype=np.double)
                        trialevent.timestamps[channel][u] = <void *>&mxa_d[0]
                        ts = np.asarray(mxa_d)
                    else:
                        mxa_u32 = np.zeros(num_samples, dtype=np.uint32)
                        trialevent.timestamps[channel][u] = <void *>&mxa_u32[0]
                        ts = np.asarray(mxa_u32)
                ev_timestamps.append(ts)

            ch = trialevent.chan[channel] # Actual channel number
            trialevent.waveforms[channel] = NULL
            dig_events = []
            res = cbSdkGetChannelType(<uint32_t>instance, ch, &ch_type)
            handle_result(res)
            # Fill values for non-empty digital or serial channels
            if (ch_type == cbhwlib_cbCHANTYPES.cbCHANTYPE_DIGIN) or (ch_type == cbhwlib_cbCHANTYPES.cbCHANTYPE_SERIAL):
                num_samples = trialevent.num_samples[channel][0]
                if num_samples > 0:
                    if is_double:
                        mxa_d = np.zeros(num_samples, dtype=np.double)
                        trialevent.waveforms[channel] = <void *>&mxa_d[0]
                        dig_events = np.asarray(mxa_d)
                    else:
                        mxa_u16 = np.zeros(num_samples, dtype=np.uint16)
                        trialevent.waveforms[channel] = <void *>&mxa_u16[0]
                        dig_events = np.asarray(mxa_u16)

            trial_event.append([ch, {'timestamps':ev_timestamps, 'events':dig_events}])

    # Continuous #
    # ---------- #
    if do_cont:
        # allocate memory and prepare outputs.
        for channel in range(trialcont.count):
            row = [trialcont.chan[channel]]  # each row will be [chan_id, dat_array]
            trialcont.samples[channel] = NULL
            num_samples = trialcont.num_samples[channel]
            if is_double:
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
            trial_cont.append(row)

    # Comments #
    # -------- #
    if do_comment and (trialcomm.num_samples > 0):
        # Allocate memory
        # For charsets;
        mxa_u8 = np.zeros(trialcomm.num_samples, dtype=np.uint8)
        trialcomm.charsets = <uint8_t *>&mxa_u8[0]
        my_charsets = np.asarray(mxa_u8)
        # For rgbas
        mxa_u32 = np.zeros(trialcomm.num_samples, dtype=np.uint32)
        trialcomm.rgbas = <uint32_t *>&mxa_u32[0]
        my_rgbas = np.asarray(mxa_u32)
        # For timestamps
        if is_double:
            mxa_d = np.zeros(trialcomm.num_samples, dtype=np.double)
            trialcomm.timestamps = <void *>&mxa_d[0]
            my_timestamps = np.asarray(mxa_d)
        else:
            mxa_u32 = np.zeros(trialcomm.num_samples, dtype=np.uint32)
            trialcomm.timestamps = <void *>&mxa_u32[0]
            my_timestamps = np.asarray(mxa_u32)
        # For comments
        trialcomm.comments = <uint8_t **>malloc(trialcomm.num_samples * sizeof(uint8_t*))
        for comm_ix in range(trialcomm.num_samples):
            trialcomm.comments[comm_ix] = <uint8_t *>malloc(256 * sizeof(uint8_t))
            trial_comment.append([my_timestamps[comm_ix], trialcomm.comments[comm_ix],
                                  my_charsets[comm_ix], my_rgbas[comm_ix]])

    # cbsdk get trial data
    try:
        res = cbsdk_get_trial_data(<uint32_t>instance, <int>reset,
                                   &trialevent if do_event else NULL,
                                   &trialcont if do_cont else NULL,
                                   &trialcomm if do_comment else NULL)
        handle_result(res)
        if do_cont:
            tzero = trialcont.time
    finally:
        if do_comment:
            free(trialcomm.comments)

    return <int>res, trial_event, trial_cont, tzero, trial_comment


def trial_comment(int instance=0, bool reset=False, unsigned long wait_for_comment_msec=250):
    """ Trial comment data.
    Inputs:
       reset - (optional) boolean
               set False (default) to leave buffer intact.
               set True to clear all the data and reset the trial time to the current time.
       instance - (optional) library instance number
    Outputs:
       list of lists the form [timestamp, comment_str, rgba]
           timestamp: ?
           comment_str: the comment as a py string
           rgba: integer; the comment colour. 8 bits each for r, g, b, a
    """

    cdef cbSdkResult res
    cdef cbSdkConfigParam cfg_param
    cdef cbSdkTrialComment trialcomm

    # retrieve old values
    res = cbsdk_get_trial_config(<uint32_t>instance, &cfg_param)
    handle_result(<cbSdkResult>res)

    # get how many comments are available
    res = cbsdk_init_trial_comment(<uint32_t>instance, <int>reset, &trialcomm, wait_for_comment_msec)
    handle_result(res)

    if trialcomm.num_samples == 0:
        return <int>res, []

    # allocate memory

    # types
    cdef np.uint8_t[:] mxa_u8_cs  # charsets
    cdef np.uint32_t[:] mxa_u32_rgbas
    cdef np.uint32_t[:] mxa_u32_ts
    cdef np.double_t[:] mxa_d_ts

    # For charsets;
    mxa_u8_cs = np.zeros(trialcomm.num_samples, dtype=np.uint8)
    trialcomm.charsets = <uint8_t *>&mxa_u8_cs[0]
    my_charsets = np.asarray(mxa_u8_cs)

    # For rgbas
    mxa_u32_rgbas = np.zeros(trialcomm.num_samples, dtype=np.uint32)
    trialcomm.rgbas = <uint32_t *>&mxa_u32_rgbas[0]
    my_rgbas = np.asarray(mxa_u32_rgbas)

    # For comments
    trialcomm.comments = <uint8_t **>malloc(trialcomm.num_samples * sizeof(uint8_t*))
    cdef int comm_ix
    for comm_ix in range(trialcomm.num_samples):
        trialcomm.comments[comm_ix] = <uint8_t *>malloc(256 * sizeof(uint8_t))

    # For timestamps
    if cfg_param.bDouble:
        mxa_d_ts = np.zeros(trialcomm.num_samples, dtype=np.double)
        trialcomm.timestamps = <void *>&mxa_d_ts[0]
        my_timestamps = np.asarray(mxa_d_ts)
    else:
        mxa_u32_ts = np.zeros(trialcomm.num_samples, dtype=np.uint32)
        trialcomm.timestamps = <void *>&mxa_u32_ts[0]
        my_timestamps = np.asarray(mxa_u32_ts)

    trial = []
    try:
        res = cbsdk_get_trial_comment(<int>instance, <int>reset, &trialcomm)
        handle_result(res)
        for comm_ix in range(trialcomm.num_samples):
            # this_enc = 'utf-16' if my_charsets[comm_ix]==1 else locale.getpreferredencoding()
            row = [my_timestamps[comm_ix], trialcomm.comments[comm_ix], my_rgbas[comm_ix]]  # .decode(this_enc)
            trial.append(row)
    finally:
        free(trialcomm.comments)

    return <int>res, trial


def file_config(int instance=0, command='info', comment='', filename='', patient_info=None):
    """ Configure remote file recording or get status of recording.
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
       patient_info - (optional) dict carrying patient info. If provided then valid keys and value types:
                'ID': str - required, 'firstname': str - defaults to 'J', 'lastname': str - defaults to 'Doe',
                'DOBMonth': int - defaults to 01, 'DOBDay': int - defaults to 01, 'DOBYear': int - defaults to 1970
    Outputs:
       Only if command is 'info' output is returned
       A dictionary with following keys:
           'Recording': boolean, if recording is in progress
           'FileName': string, file name being recorded
           'UserName': Computer that is recording
    """
    
    
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
            raise RuntimeError('filename and comment must not be specified for open')
        options = cbFILECFG_OPT_OPEN
    elif command == 'close':
        options = cbFILECFG_OPT_CLOSE
    elif command == 'start':
        if not filename:
            raise RuntimeError('filename must be specified for start')
        start = 1
    elif command == 'stop':
        if not filename:
            raise RuntimeError('filename must be specified for stop')
        start = 0
    else:
        raise RuntimeError("invalid file config command %s" % command)

    cdef cbSdkResult patient_res
    if start and patient_info is not None and 'ID' in patient_info:
        default_patient_info = {'firstname': 'J', 'lastname': 'Doe', 'DOBMonth': 1, 'DOBDay': 1, 'DOBYear': 1970}
        patient_info = {**default_patient_info, **patient_info}
        for k,v in patient_info.items():
            if type(v) is str:
                patient_info[k] = v.encode('UTF-8')

        patient_res = cbSdkSetPatientInfo(<uint32_t>instance,
                                          <const char *>patient_info['ID'],
                                          <const char *>patient_info['firstname'],
                                          <const char *>patient_info['lastname'],
                                          <uint32_t>patient_info['DOBMonth'],
                                          <uint32_t>patient_info['DOBDay'],
                                          <uint32_t>patient_info['DOBYear'])
        handle_result(patient_res)

    cdef int set_res
    filename_string = filename.encode('UTF-8')
    comment_string = comment.encode('UTF-8')
    set_res = cbsdk_file_config(<uint32_t>instance, <const char *>filename_string, <char *>comment_string, start, options)

    
    return set_res


def time(int instance=0, unit='samples'):
    """Instrument time.
    Inputs:
       unit - time unit, string can be one the following
                'samples': (default) sample number integer
                'seconds' or 's': seconds calculated from samples
                'milliseconds' or 'ms': milliseconds calculated from samples
       instance - (optional) library instance number
    Outputs:
       time - time passed since last reset
    """


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
    handle_result(res)

    time = float(cbtime) / factor
    
    return <int>res, time


def analog_out(channel_out, channel_mon, track_last=True, spike_only=False, int instance=0):
    """
    Monitor a channel.
    Inputs:
    channel_out - integer, analog output channel number (1-based)
                  On NSP, should be >= MIN_CHANS_ANALOG_OUT (273) && <= MAX_CHANS_AUDIO (278)
    channel_mon - integer, channel to monitor (1-based)
    track_last - (optional) If True, track last channel clicked on in raster plot or hardware config window.
    spike_only - (optional) If True, only play spikes. If False, play continuous.
    """
    cdef cbSdkResult res
    cdef cbSdkAoutMon mon
    if channel_out < 273:
            channel_out += 128  # Recent NSP firmware upgrade added 128 more analog channels.
    if channel_mon is None:
        res = cbSdkSetAnalogOutput(<uint32_t>instance, <uint16_t>channel_out, NULL, NULL)
    else:
        mon.chan = <uint16_t>channel_mon
        mon.bTrack = track_last
        mon.bSpike = spike_only
        res = cbSdkSetAnalogOutput(<uint32_t>instance, <uint16_t>channel_out, NULL, &mon)
    handle_result(res)
    return <int>res


def digital_out(int channel, int instance=0, value='low'):
    """Digital output command.
    Inputs:
    channel - integer, digital output channel number (1-based)
               On NSP, 153 (dout1), 154 (dout2), 155 (dout3), 156 (dout4)
    value - (optional), depends on the command
           for command of 'set_value':
               string, can be 'high' or 'low' (default)
    instance - (optional) library instance number
    """
    
    values = ['low', 'high']
    if value not in values:
        raise RuntimeError("Invalid value %s" % value)

    cdef cbSdkResult res
    cdef uint16_t int_val = values.index(value)
    res = cbSdkSetDigitalOutput(<uint32_t>instance, <uint16_t>channel, int_val)
    handle_result(res)
    return <int>res


def get_channel_config(int channel, int instance=0, encoding='utf-8'):
    """ Get channel configuration.
    Inputs:
        - channel: 1-based channel number 
        - instance: (optional) library instance number
        - encoding: (optional) encoding for string fields, default = 'utf-8'
    Outputs:
        -chaninfo = A Python dictionary with the following fields:
        'time': system clock timestamp,
        'chid': 0x8000,
        'type': cbPKTTYPE_AINP*,
        'dlen': cbPKT_DLENCHANINFO,
        'chan': actual channel id of the channel being configured,
        'proc': the address of the processor on which the channel resides,
        'bank': the address of the bank on which the channel resides,
        'term': the terminal number of the channel within it's bank,
        'chancaps': general channel capablities (given by cbCHAN_* flags),
        'doutcaps': digital output capablities (composed of cbDOUT_* flags),
        'dinpcaps': digital input capablities (composed of cbDINP_* flags),
        'aoutcaps': analog output capablities (composed of cbAOUT_* flags),
        'ainpcaps': analog input capablities (composed of cbAINP_* flags),
        'spkcaps': spike processing capabilities,
        'label': Label of the channel (null terminated if <16 characters),
        'userflags': User flags for the channel state,
        'doutopts': digital output options (composed of cbDOUT_* flags),
        'dinpopts': digital input options (composed of cbDINP_* flags),
        'aoutopts': analog output options,
        'eopchar': digital input capablities (given by cbDINP_* flags),
        'ainpopts': analog input options (composed of cbAINP* flags),
        'smpfilter': continuous-time pathway filter id,
        'smpgroup': continuous-time pathway sample group,
        'smpdispmin': continuous-time pathway display factor,
        'smpdispmax': continuous-time pathway display factor,
        'trigtype': trigger type (see cbDOUT_TRIGGER_*),
        'trigchan': trigger channel,
        'trigval': trigger value,
        'lncrate': line noise cancellation filter adaptation rate,
        'spkfilter': spike pathway filter id,
        'spkdispmax': spike pathway display factor,
        'lncdispmax': Line Noise pathway display factor,
        'spkopts': spike processing options,
        'spkthrlevel': spike threshold level,
        'spkthrlimit': ,
        'spkgroup': NTrodeGroup this electrode belongs to - 0 is single unit, non-0 indicates a multi-trode grouping,
        'amplrejpos': Amplitude rejection positive value,
        'amplrejneg': Amplitude rejection negative value,
        'refelecchan': Software reference electrode channel,
    """
    cdef cbSdkResult res
    cdef cbPKT_CHANINFO cb_chaninfo
    res = cbSdkGetChannelConfig(<uint32_t>instance, <uint16_t>channel, &cb_chaninfo)
    handle_result(res)
    if res != 0:
        return <int>res, {}

    chaninfo = {
        'time': cb_chaninfo.time,
        'chid': cb_chaninfo.chid,
        'type': cb_chaninfo.type,  # cbPKTTYPE_AINP*
        'dlen': cb_chaninfo.dlen,  # cbPKT_DLENCHANINFO
        'chan': cb_chaninfo.chan,
        'proc': cb_chaninfo.proc,
        'bank': cb_chaninfo.bank,
        'term': cb_chaninfo.term,
        'chancaps': cb_chaninfo.chancaps,
        'doutcaps': cb_chaninfo.doutcaps,
        'dinpcaps': cb_chaninfo.dinpcaps,
        'aoutcaps': cb_chaninfo.aoutcaps,
        'ainpcaps': cb_chaninfo.ainpcaps,
        'spkcaps': cb_chaninfo.spkcaps,
        'label': cb_chaninfo.label.decode(encoding),
        'userflags': cb_chaninfo.userflags,
        'doutopts': cb_chaninfo.doutopts,
        'dinpopts': cb_chaninfo.dinpopts,
        'monsource': cb_chaninfo.monsource,
        'outvalue': cb_chaninfo.outvalue,
        'aoutopts': cb_chaninfo.aoutopts,
        'eopchar': cb_chaninfo.eopchar,
        'ainpopts': cb_chaninfo.ainpopts,
        'smpfilter': cb_chaninfo.smpfilter,
        'smpgroup': cb_chaninfo.smpgroup,
        'smpdispmin': cb_chaninfo.smpdispmin,
        'smpdispmax': cb_chaninfo.smpdispmax,
        'trigtype': cb_chaninfo.trigtype,
        'trigchan': cb_chaninfo.trigchan,
        'lncrate': cb_chaninfo.lncrate,
        'spkfilter': cb_chaninfo.spkfilter,
        'spkdispmax': cb_chaninfo.spkdispmax,
        'lncdispmax': cb_chaninfo.lncdispmax,
        'spkopts': cb_chaninfo.spkopts,
        'spkthrlevel': cb_chaninfo.spkthrlevel,
        'spkthrlimit': cb_chaninfo.spkthrlimit,
        'spkgroup': cb_chaninfo.spkgroup,
        'amplrejpos': cb_chaninfo.amplrejpos,
        'amplrejneg': cb_chaninfo.amplrejneg,
        'refelecchan': cb_chaninfo.refelecchan
    }

        # TODO:
        #cbSCALING           physcalin               # physical channel scaling information
        #cbFILTDESC          phyfiltin               # physical channel filter definition
        #cbSCALING           physcalout              # physical channel scaling information
        #cbFILTDESC          phyfiltout              # physical channel filter definition
        #int32_t             position[4]             # reserved for future position information
        #cbSCALING           scalin                  # user-defined scaling information for AINP
        #cbSCALING           scalout                 # user-defined scaling information for AOUT
        #uint32_t              monsource
        #int32_t               outvalue       # output value
        #uint16_t              lowsamples     # address of channel to monitor
        #uint16_t              highsamples    # address of channel to monitor
        #int32_t               offset
        #cbMANUALUNITMAPPING unitmapping[cbMAXUNITS+0]             # manual unit mapping
        #cbHOOP              spkhoops[cbMAXUNITS+0][cbMAXHOOPS+0]    # spike hoop sorting set

    return <int>res, chaninfo


def set_channel_config(int channel, chaninfo={}, int instance=0, encoding='utf-8'):
    """ Set channel configuration.
    Inputs:
        - channel: 1-based channel number 
        - chaninfo: A Python dict. See fields descriptions in get_channel_config. All fields are optional.
        - instance: (optional) library instance number
        - encoding: (optional) encoding for string fields, default = 'utf-8'
    """
    cdef cbSdkResult res
    cdef cbPKT_CHANINFO cb_chaninfo
    res = cbSdkGetChannelConfig(<uint32_t>instance, <uint16_t>channel, &cb_chaninfo)
    handle_result(res)

    if 'label' in chaninfo:
        new_label = chaninfo['label']
        if not isinstance(new_label, bytes):
            new_label = new_label.encode(encoding)
        strncpy(cb_chaninfo.label, new_label, sizeof(new_label))

    if 'smpgroup' in chaninfo:
        cb_chaninfo.smpgroup = chaninfo['smpgroup']

    if 'spkthrlevel' in chaninfo:
        cb_chaninfo.spkthrlevel = chaninfo['spkthrlevel']

    res = cbSdkSetChannelConfig(<uint32_t>instance, <uint16_t>channel, &cb_chaninfo)
    handle_result(res)
    return <int>res


def get_sample_group(int group_ix, int instance=0, encoding='utf-8'):
    """
    """
    cdef cbSdkResult res
    cdef uint32_t proc = 1
    cdef uint32_t nChansInGroup
    res = cbSdkGetSampleGroupList(<uint32_t>instance, proc, group_ix, &nChansInGroup, NULL)
    handle_result(res)
    if (nChansInGroup <= 0):
        return <int> res, []

    cdef uint16_t pGroupList[cbNUM_ANALOG_CHANS+0]
    res = cbSdkGetSampleGroupList(<uint32_t>instance, proc, <uint32_t>group_ix, &nChansInGroup, pGroupList)
    handle_result(res)

    cdef cbPKT_CHANINFO chanInfo
    channels_info = []
    for chan_ix in range(nChansInGroup):
        chan_info = {}
        res = cbSdkGetChannelConfig(<uint32_t>instance, pGroupList[chan_ix], &chanInfo)
        handle_result(<cbSdkResult>res)
        anaRange = chanInfo.physcalin.anamax - chanInfo.physcalin.anamin
        digRange = chanInfo.physcalin.digmax - chanInfo.physcalin.digmin
        chan_info['chid'] = chanInfo.chid
        chan_info['chan'] = chanInfo.chan
        chan_info['proc'] = chanInfo.proc
        chan_info['bank'] = chanInfo.bank
        chan_info['term'] = chanInfo.term
        chan_info['gain'] = anaRange / digRange
        chan_info['label'] = chanInfo.label.decode(encoding)
        chan_info['unit'] = chanInfo.physcalin.anaunit.decode(encoding)
        channels_info.append(chan_info)

    return <int>res, channels_info


def set_comment(comment_string, rgba_tuple=(0, 0, 0, 255), int instance=0):
    """rgba_tuple is actually t_bgr: transparency, blue, green, red. Default: no-transparency & red."""
    cdef cbSdkResult res
    cdef uint32_t t_bgr = (rgba_tuple[0] << 24) + (rgba_tuple[1] << 16) + (rgba_tuple[2] << 8) + rgba_tuple[3]
    cdef uint8_t charset = 0  # Character set (0 - ANSI, 1 - UTF16, 255 - NeuroMotive ANSI)
    cdef bytes py_bytes = comment_string.encode()
    cdef const char* comment = py_bytes
    res = cbSdkSetComment(<uint32_t> instance, t_bgr, charset, comment)


def get_sys_config(int instance=0):
    cdef cbSdkResult res
    cdef uint32_t spklength
    cdef uint32_t spkpretrig
    cdef uint32_t sysfreq
    res = cbSdkGetSysConfig(<uint32_t> instance, &spklength, &spkpretrig, &sysfreq)
    handle_result(res)
    return {'spklength': spklength, 'spkpretrig': spkpretrig, 'sysfreq': sysfreq}


def set_spike_config(int spklength=48, int spkpretrig=10, int instance=0):
    cdef cbSdkResult res
    res = cbSdkSetSpikeConfig(<uint32_t> instance, <uint32_t> spklength, <uint32_t> spkpretrig)
    handle_result(res)


cdef extern from "numpy/arrayobject.h":
    void PyArray_ENABLEFLAGS(np.ndarray arr, int flags)


cdef class SpikeCache:
    cdef readonly int inst, chan, n_samples, n_pretrig
    cdef cbSPKCACHE *cache
    cdef int last_valid
    # cache
    # .chid: ID of the Channel
    # .pktcnt: number of packets that can be saved
    # .pktsize: size of an individual packet
    # .head: Where (0 based index) in the circular buffer to place the NEXT packet
    # .valid: How many packets have come in since the last configuration
    # .spkpkt: Circular buffer of the cached spikes

    def __cinit__(self, int channel=1, int instance=0):
        self.inst = instance
        self.chan = channel
        cdef cbSPKCACHE ignoreme  # Just so self.cache is not NULL... but this won't be used by anything
        self.cache = &ignoreme    # because cbSdkGetSpkCache changes what self.cache is pointing to.
        self.reset_cache()
        sys_config_dict = get_sys_config(instance)
        self.n_samples = sys_config_dict['spklength']
        self.n_pretrig = sys_config_dict['spkpretrig']

    def reset_cache(self):
        cdef cbSdkResult res = cbSdkGetSpkCache(self.inst, self.chan, &self.cache)
        handle_result(res)
        self.last_valid = self.cache.valid

    @cython.boundscheck(False) # turn off bounds-checking for entire function
    def get_new_waveforms(self):
        cdef int new_valid = self.cache.valid
        cdef int new_head = self.cache.head
        cdef int n_new = min(max(new_valid - self.last_valid, 0), 400)
        cdef np.ndarray[np.int16_t, ndim=2, mode="c"] np_waveforms = np.empty((n_new, self.n_samples), dtype=np.int16)
        cdef np.ndarray[np.uint8_t, ndim=1] np_unit_ids = np.empty(n_new, dtype=np.uint8)
        cdef int wf_ix, pkt_ix, samp_ix
        for wf_ix in range(n_new):
            pkt_ix = (new_head - 2 - n_new + wf_ix) % 400
            np_unit_ids[wf_ix] = self.cache.spkpkt[pkt_ix].unit
            # Instead of per-sample copy, we could copy the pointer for the whole wave to the buffer of a 1-d np array,
            # then use memory view copying from 1-d array into our 2d matrix. But below is pure-C so should be fast too.
            for samp_ix in range(self.n_samples):
                np_waveforms[wf_ix, samp_ix] = self.cache.spkpkt[pkt_ix].wave[samp_ix]
        #unit_ids_out = [<int>unit_ids[wf_ix] for wf_ix in range(n_new)]
        PyArray_ENABLEFLAGS(np_waveforms, np.NPY_OWNDATA)
        self.last_valid = new_valid
        return np_waveforms, np_unit_ids


cdef cbSdkResult handle_result(cbSdkResult res):
    if res == CBSDKRESULT_WARNCLOSED:
        print("Library is already closed.")
    if res < 0:
        errtext = "No associated error string. See cbsdk.h"
        if res == CBSDKRESULT_ERROFFLINE:
            errtext = "Instrument is offline."
        elif res == CBSDKRESULT_CLOSED:
            errtext = "Interface is closed; cannot do this operation."
        elif res == CBSDKRESULT_ERRCONFIG:
            errtext = "Trying to run an unconfigured method."
        elif res == CBSDKRESULT_NULLPTR:
            errtext = "Null pointer."
        elif res == CBSDKRESULT_INVALIDCHANNEL:
            errtext = "Invalid channel number."

        raise RuntimeError(("%d, " + errtext) % res)
    return res
