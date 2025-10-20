/*
 * Implementation of helper API for easy Cython wrapping.
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include <string.h>

#include "cbsdk_helper.h"

cbSdkResult cbsdk_get_trial_config(const uint32_t nInstance, cbSdkConfigParam * pcfg_param)
{
    const cbSdkResult sdk_result = cbSdkGetTrialConfig(nInstance, &pcfg_param->bActive,
            &pcfg_param->Begchan, &pcfg_param->Begmask, &pcfg_param->Begval,
            &pcfg_param->Endchan, &pcfg_param->Endmask, &pcfg_param->Endval,
            &pcfg_param->uWaveforms,
            &pcfg_param->uConts, &pcfg_param->uEvents, &pcfg_param->uComments,
            &pcfg_param->uTrackings);

    return sdk_result;
}

cbSdkResult cbsdk_set_trial_config(const uint32_t nInstance, const cbSdkConfigParam * pcfg_param)
{
    const cbSdkResult sdk_result = cbSdkSetTrialConfig(nInstance, pcfg_param->bActive,
            pcfg_param->Begchan,pcfg_param->Begmask, pcfg_param->Begval,
            pcfg_param->Endchan, pcfg_param->Endmask, pcfg_param->Endval,
            pcfg_param->uWaveforms,
            pcfg_param->uConts, pcfg_param->uEvents, pcfg_param->uComments,
            pcfg_param->uTrackings);

    return sdk_result;
}


cbSdkResult cbsdk_init_trial_event(const uint32_t nInstance, const int clock_reset, cbSdkTrialEvent * trialevent)
{
    memset(trialevent, 0, sizeof(*trialevent));
    const cbSdkResult sdk_result = cbSdkInitTrialData(nInstance, clock_reset, trialevent, nullptr, nullptr, nullptr);

    return sdk_result;
}

cbSdkResult cbsdk_get_trial_event(const uint32_t nInstance, const int seek, cbSdkTrialEvent * trialevent)
{
    const cbSdkResult sdk_result = cbSdkGetTrialData(nInstance, seek, trialevent, nullptr, nullptr, nullptr);

    return sdk_result;
}

cbSdkResult cbsdk_init_trial_cont(const uint32_t nInstance, const int clock_reset, cbSdkTrialCont * trialcont)
{
    memset(trialcont, 0, sizeof(*trialcont));
    const cbSdkResult sdk_result = cbSdkInitTrialData(nInstance, clock_reset, nullptr, trialcont, nullptr, nullptr);

    return sdk_result;
}

cbSdkResult cbsdk_get_trial_cont(const uint32_t nInstance, const int seek, cbSdkTrialCont * trialcont)
{
    const cbSdkResult sdk_result = cbSdkGetTrialData(nInstance, seek, nullptr, trialcont, nullptr, nullptr);

    return sdk_result;
}

cbSdkResult cbsdk_init_trial_data(const uint32_t nInstance, const int clock_reset, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont, cbSdkTrialComment * trialcomm, unsigned long wait_for_comment_msec)
{
    if(trialevent)
    {
        memset(trialevent, 0, sizeof(*trialevent));
    }
	if (trialcont)
	{
	    memset(trialcont, 0, sizeof(*trialcont));
	}
	if (trialcomm)
	{
	    memset(trialcomm, 0, sizeof(*trialcomm));
	}
	const cbSdkResult sdk_result = cbSdkInitTrialData(nInstance, clock_reset, trialevent, trialcont, trialcomm, nullptr, wait_for_comment_msec);

	return sdk_result;
}

cbSdkResult cbsdk_get_trial_data(const uint32_t nInstance, const int seek, cbSdkTrialEvent * trialevent, cbSdkTrialCont * trialcont, cbSdkTrialComment * trialcomm)
{
	const cbSdkResult sdk_result = cbSdkGetTrialData(nInstance, seek, trialevent, trialcont, trialcomm, nullptr);

	return sdk_result;
}

cbSdkResult cbsdk_init_trial_comment(const uint32_t nInstance, const int clock_reset, cbSdkTrialComment * trialcomm, const unsigned long wait_for_comment_msec)
{
    memset(trialcomm, 0, sizeof(*trialcomm));
    const cbSdkResult sdk_result = cbSdkInitTrialData(nInstance, clock_reset, nullptr, nullptr, trialcomm, nullptr, wait_for_comment_msec);

    return sdk_result;
}

cbSdkResult cbsdk_get_trial_comment(const uint32_t nInstance, const int seek, cbSdkTrialComment * trialcomm)
{
    const cbSdkResult sdk_result = cbSdkGetTrialData(nInstance, seek, nullptr, nullptr, trialcomm, nullptr);

    return sdk_result;
}

cbSdkResult cbsdk_file_config(const uint32_t instance, const char * filename, const char * comment, const int start, const unsigned int options)
{
    const cbSdkResult sdk_result = cbSdkSetFileConfig(instance, filename == nullptr ? "" : filename, comment == nullptr ? "" : comment, start, options);
    return sdk_result;
}
