/*
 * Cerebus Python
 *
 * Cython wrapper for cbsdk.h
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef CBPY_H
#define CBPY_H

#include "cbsdk.h"

int cbpy_version(int nInstance, cbSdkVersion * ver);
int cbpy_open(int nInstance, cbSdkConnectionType conType, cbSdkConnection con);
int cbpy_gettype(int nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType);

typedef struct _cbSdkConfigParam {
    UINT32 bActive;
    UINT16 Begchan;
    UINT32 Begmask;
    UINT32 Begval;
    UINT16 Endchan;
    UINT32 Endmask;
    UINT32 Endval;
    bool bDouble;
    UINT32 uWaveforms;
    UINT32 uConts;
    UINT32 uEvents;
    UINT32 uComments;
    UINT32 uTrackings;
    bool bAbsolute;
} cbSdkConfigParam;

#define cbSdk_CONTINUOUS_DATA_SAMPLES 102400 // multiple of 4096
/// The default number of events that will be stored per channel in the trial buffer
#define cbSdk_EVENT_DATA_SAMPLES (2 * 8192) // multiple of 4096

int cbpy_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param);
int cbpy_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param);

#endif // include guard
