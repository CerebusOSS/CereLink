/*
 * Cerebus Python
 *  C wrapper to use cbsdk in Cython
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include <string.h>

#include "cbpy.h"

int cbpy_version(int nInstance, cbSdkVersion *ver)
{
    cbSdkResult sdkres = cbSdkGetVersion(nInstance, ver);

    return sdkres;
}

int cbpy_open(int nInstance, cbSdkConnectionType conType, cbSdkConnection con)
{
    cbSdkResult sdkres = cbSdkOpen(nInstance, conType, con);

    return sdkres;
}

int cbpy_close(int nInstance)
{
    cbSdkResult sdkres = cbSdkClose(nInstance);

    return sdkres;
}

int cbpy_gettype(int nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
{
    cbSdkResult sdkres = cbSdkGetType(nInstance, conType, instType);

    return sdkres;
}

int cbpy_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param)
{
    cbSdkResult sdkres = cbSdkGetTrialConfig(nInstance, &pcfg_param->bActive,
            &pcfg_param->Begchan,&pcfg_param->Begmask, &pcfg_param->Begval,
            &pcfg_param->Endchan, &pcfg_param->Endmask, &pcfg_param->Endval,
            &pcfg_param->bDouble, &pcfg_param->uWaveforms,
            &pcfg_param->uConts, &pcfg_param->uEvents, &pcfg_param->uComments,
            &pcfg_param->uTrackings,
            &pcfg_param->bAbsolute);

    return sdkres;
}

int cbpy_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param)
{
    cbSdkResult sdkres = cbSdkSetTrialConfig(nInstance, pcfg_param->bActive,
            pcfg_param->Begchan,pcfg_param->Begmask, pcfg_param->Begval,
            pcfg_param->Endchan, pcfg_param->Endmask, pcfg_param->Endval,
            pcfg_param->bDouble, pcfg_param->uWaveforms,
            pcfg_param->uConts, pcfg_param->uEvents, pcfg_param->uComments,
            pcfg_param->uTrackings,
            pcfg_param->bAbsolute);

    return sdkres;
}

int cbpy_init_trial_event(int nInstance, cbSdkTrialEvent * trialevent)
{
    memset(trialevent, 0, sizeof(*trialevent));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, trialevent, 0, 0, 0);

    return sdkres;
}

int cbpy_get_trial_event(int nInstance, bool reset, cbSdkTrialEvent * trialevent)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, trialevent, 0, 0, 0);

    return sdkres;
}

int cbpy_init_trial_cont(int nInstance, cbSdkTrialCont * trialcont)
{
    memset(trialcont, 0, sizeof(*trialcont));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, 0, trialcont, 0, 0);

    return sdkres;
}

int cbpy_get_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, 0, trialcont, 0, 0);

    return sdkres;
}

int cbpy_get_file_config(int instance,  char * filename, char * username, int * pbRecording)
{
    bool bRecording;
    cbSdkResult sdkres = cbSdkGetFileConfig(instance, filename, username, &bRecording);
    *pbRecording = bRecording;

    return sdkres;
}

int cbpy_file_config(int instance, const char * filename, const char * comment, int start, unsigned int options)
{
    cbSdkResult sdkres = cbSdkSetFileConfig(instance, filename == NULL ? "" : filename, comment == NULL ? "" : comment, start, options);
    return sdkres;
}


int cbpy_time(int instance, int * pcbtime) {
    sdkres = cbSdkGetTime(nInstance, pcbtime);
    return sdkres;
}
