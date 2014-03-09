'''
@date March 9, 2014

@author: dashesy

Purpose: C interface for cbpy wrapper 

'''

from libc.stdint cimport uint32_t

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

    