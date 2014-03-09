'''
Created on March 9, 2013

@author: dashesy

Purpose: Python wrapper for cbsdk  

'''

from cbpy cimport *
import numpy as np
cimport numpy as np
cimport cython

def version(instance = 0):
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
    '''

    cdef int res
    cdef cbSdkVersion ver
    
    res = cbpy_version(<int>instance, &ver)
    
    if res < 0:
        # Make this raise error classes
        raise RuntimeError("error %d" % res)
    
    ver_dict = {'major':ver.major, 'minor':ver.minor, 'release':ver.release, 'beta':ver.beta,
                'protocol_major':ver.majorp, 'protocol_minor':ver.majorp,
                'nsp_major':ver.nspmajor, 'nsp_minor':ver.nspminor, 'nsp_release':ver.nsprelease, 'nsp_beta':ver.nspbeta
                }
    return res, ver_dict


def open(instance = 0, connection='default', parameter={}):
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
        Same as "connection" command output
    '''
    
    cdef int res
    
    wconType = {'default': CBSDKCONNECTION_DEFAULT, 'slave': CBSDKCONNECTION_CENTRAL, 'master': CBSDKCONNECTION_UDP} 
    if not connection in wconType.keys():
        raise RuntimeError("invalid connection %S" % connection)
     
    cdef cbSdkConnectionType conType = wconType[connection]
    cdef cbSdkConnection con
    
    cdef bytes szOutIP = parameter.get('inst-addr', '').encode()
    cdef bytes szInIP  = parameter.get('client-addr', '').encode()
    
    con.szOutIP = szOutIP
    con.nOutPort = parameter.get('inst-port', 51001)
    con.szInIP = szInIP
    con.nInPort = parameter.get('client-port', 51002)
    con.nRecBufSize = parameter.get('receive-buffer-size', 0)
    
    res = cbpy_open(<int>instance, conType, con)

    if res < 0:
        # Make this raise error classes
        raise RuntimeError("error %d" % res)
    
    return res, connection(instance=instance)
    
def connection(instance = 0):
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
    
    cdef int res
    
    cdef cbSdkConnectionType conType
    cdef cbSdkInstrumentType instType
    
    res = cbpy_gettype(<int>instance, &conType, &instType)

    if res < 0:
        # Make this raise error classes
        raise RuntimeError("error %d" % res)
    
    connections = ["Default", "Slave", "Master", "Closed", "Unknown"]
    instruments = ["NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"]

    con_idx = conType
    if con_idx < 0 or con_idx >= len(connections):
        con_idx = len(connections) - 1
    inst_idx = instType 
    if inst_idx < 0 or inst_idx >= len(instruments):
        inst_idx = len(instruments) - 1
        
    return {'connection':connections[con_idx],'instrument':instruments[inst_idx]}
    