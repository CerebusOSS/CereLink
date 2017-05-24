/*
 * Implementation of helper API for easy Cython wrapping.
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include <string.h>

#include "cbsdk_helper.h"

cbSdkResult cbsdk_get_trial_config(int nInstance, cbSdkConfigParam * pcfg_param)
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

cbSdkResult cbsdk_set_trial_config(int nInstance, const cbSdkConfigParam * pcfg_param)
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


cbSdkResult cbsdk_init_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent)
{
    memset(trialevent, 0, sizeof(*trialevent));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, trialevent, 0, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_event(int nInstance, int reset, cbSdkTrialEvent * trialevent)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, trialevent, 0, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_init_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont)
{
    memset(trialcont, 0, sizeof(*trialcont));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, 0, trialcont, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_cont(int nInstance, int reset, cbSdkTrialCont * trialcont)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, 0, trialcont, 0, 0);

    return sdkres;
}

cbSdkResult cbsdk_init_trial_comment(int nInstance, int reset, cbSdkTrialComment * trialcomm)
{
    memset(trialcomm, 0, sizeof(*trialcomm));
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, reset, 0, 0, trialcomm, 0);

    return sdkres;
}

cbSdkResult cbsdk_get_trial_comment(int nInstance, int reset, cbSdkTrialComment * trialcomm)
{
    cbSdkResult sdkres = cbSdkGetTrialData(nInstance, reset, 0, 0, trialcomm, 0);

    return sdkres;
}

cbSdkResult cbsdk_file_config(int instance, const char * filename, const char * comment, int start, unsigned int options)
{
    cbSdkResult sdkres = cbSdkSetFileConfig(instance, filename == NULL ? "" : filename, comment == NULL ? "" : comment, start, options);
    return sdkres;
}

int cbsdk_get_spikes(int nInstance, int channel, int valid_since, int spike_samples, int16_t * waveforms, uint8_t * unit_ids, int * valid_out)
{
    cbSPKCACHE *cache;
    cbSdkResult sdkres = cbSdkGetSpkCache(nInstance, channel, &cache);
    int n_spikes = 0;
    if (cache->valid > valid_since)
    {
        n_spikes = (cache->valid - valid_since) < (int)cbPKT_SPKCACHEPKTCNT ? (cache->valid - valid_since) : cbPKT_SPKCACHEPKTCNT;
        int start_ix = (cache->head - 1 - n_spikes) % (int)cbPKT_SPKCACHEPKTCNT;
        for (int spike_ix = 0; spike_ix < n_spikes; spike_ix++)
        {
            int buff_ix = (start_ix + spike_ix) % (int)cbPKT_SPKCACHEPKTCNT;
            memcpy(&waveforms[spike_ix * spike_samples * sizeof(int16_t)], cache->spkpkt[buff_ix].wave, spike_samples * sizeof(int16_t));
            unit_ids[spike_ix] = cache->spkpkt[buff_ix].unit;
        }
    }
    *valid_out = cache->valid;
    return n_spikes;
}
