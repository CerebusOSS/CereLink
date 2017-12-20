/*
 * Implementation of helper API for easy Cython wrapping.
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include <string.h>

#include "cbsdk_helper.h"

cbSdkResult cbsdk_get_trial_config(uint32_t nInstance, cbSdkConfigParam * pcfg_param)
{
    cbSdkResult sdkres = cbSdkGetTrialConfig(nInstance, &pcfg_param->bActive,
            &pcfg_param->Begchan, &pcfg_param->Begmask, &pcfg_param->Begval,
            &pcfg_param->Endchan, &pcfg_param->Endmask, &pcfg_param->Endval,
            &pcfg_param->bDouble, &pcfg_param->uWaveforms,
            &pcfg_param->uConts, &pcfg_param->uEvents, &pcfg_param->uComments,
            &pcfg_param->uTrackings,
            &pcfg_param->bAbsolute);

    return sdkres;
}

cbSdkResult cbsdk_set_trial_config(uint32_t nInstance, const cbSdkConfigParam * pcfg_param)
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


cbSdkResult cbsdk_init_trial_event(uint32_t nInstance, int reset, cbSdkTrialEvent * trialevent)
{
    memset(trialevent, 0, sizeof(*trialevent));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, trialevent, 0, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_event(uint32_t nInstance, int reset, cbSdkTrialEvent * trialevent)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, trialevent, 0, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_init_trial_cont(uint32_t nInstance, int reset, cbSdkTrialCont * trialcont)
{
    memset(trialcont, 0, sizeof(*trialcont));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, 0, trialcont, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_cont(uint32_t nInstance, int reset, cbSdkTrialCont * trialcont)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, 0, trialcont, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_init_trial_data(uint32_t nInstance, int reset, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont)
{
	memset(trialevent, 0, sizeof(*trialevent));
	memset(trialcont, 0, sizeof(*trialcont));
	cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, trialevent, trialcont, 0, 0);

	return sdkres;
}

cbSdkResult cbsdk_get_trial_data(uint32_t nInstance, int reset, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont)
{
	cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, trialevent, trialcont, 0, 0);

	return sdkres;
}

cbSdkResult cbsdk_init_trial_comment(uint32_t nInstance, int reset, cbSdkTrialComment * trialcomm)
{
    memset(trialcomm, 0, sizeof(*trialcomm));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, 0, 0, trialcomm, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_comment(uint32_t nInstance, int reset, cbSdkTrialComment * trialcomm)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, 0, 0, trialcomm, 0);

    return sdkres;
}

cbSdkResult cbsdk_file_config(uint32_t instance, const char * filename, const char * comment, int start, unsigned int options)
{
    cbSdkResult sdkres = cbSdkSetFileConfig(instance, filename == NULL ? "" : filename, comment == NULL ? "" : comment, start, options);
    return sdkres;
}
