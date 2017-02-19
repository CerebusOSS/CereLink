/*
 * API to add to cbsdk. This wraps some main API functions in simpler (C-only)
 * code. This in turn can be more easily wrapped (e.g. Cython)
 *
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef CBHELPER_H
#define CBHELPER_H

#include "cbsdk.h"

/* The following are already defined in cbsdk.h
 // #define cbSdk_CONTINUOUS_DATA_SAMPLES 102400 // multiple of 4096
 /// The default number of events that will be stored per channel in the trial buffer
 // #define cbSdk_EVENT_DATA_SAMPLES (2 * 8192) // multiple of 4096
 */

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

int cbsdk_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param);
int cbsdk_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param);

int cbsdk_init_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent);
int cbsdk_get_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent);

int cbsdk_init_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont);
int cbsdk_get_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont);

int cbsdk_file_config(int instance,  const char * filename, const char * comment, int start, unsigned int options);

#endif // include guard
