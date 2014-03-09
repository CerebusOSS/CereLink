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
             nsp_beta - beta NSP firmware version (0 if non-beta));
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


