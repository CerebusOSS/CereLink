#include "cbsdk_native.h"
#include "cbsdk.h"
#include <iostream>


struct my_cbSdkTrialEvent : public cbSdkTrialEvent {};
struct my_cbSdkTrialCont : public cbSdkTrialCont {};


CbSdkNative::CbSdkNative(uint32_t nInstance, int inPort, int outPort, int bufsize, std::string inIP, std::string outIP, bool use_double)
	:p_trialEvent(new my_cbSdkTrialEvent()), p_trialCont(new my_cbSdkTrialCont)
{
	m_instance = nInstance;
	cbSdkConnection con = cbSdkConnection();
	con.nInPort = inPort;
	con.nOutPort = outPort;
	con.nRecBufSize = bufsize;
	con.szInIP = inIP.c_str();
	con.szOutIP = outIP.c_str();
	cbSdkResult res = cbSdkOpen(m_instance, CBSDKCONNECTION_DEFAULT, con);
	isOnline = res == CBSDKRESULT_SUCCESS;
	if (isOnline)
	{
		cbSdkVersion ver = cbSdkVersion();
		cbSdkGetVersion(m_instance, &ver);
		// TODO: Construct version string and store as member variable.

		cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
		cbSdkInstrumentType instType = CBSDKINSTRUMENT_COUNT;
		res = cbSdkGetType(m_instance, &conType, &instType);
		
		cfg = FetchTrialConfig();
		cfg.bActive = 1;
		cfg.bDouble = use_double;
		cfg.uWaveforms = 0;
		cfg.uConts = cbSdk_CONTINUOUS_DATA_SAMPLES;
		cfg.uEvents = 0; // cbSdk_EVENT_DATA_SAMPLES;
		cfg.uComments = 0;
		cfg.uTrackings = 0;
		cfg.bAbsolute = true;
		PushTrialConfig(cfg);
	}
}

CbSdkNative::~CbSdkNative()
{
	cbSdkResult res = cbSdkClose(m_instance);
}

void CbSdkNative::PushTrialConfig(CbSdkConfigParam config)
{
	cfg.bActive = config.bActive;
	cfg.Begchan = config.Begchan;
	cfg.Begmask = config.Begmask;
	cfg.Begval = config.Begval;
	cfg.Endchan = config.Endchan;
	cfg.Endmask = config.Endmask;
	cfg.Endval = config.Endval;
	cfg.bDouble = config.bDouble;
	cfg.uWaveforms = config.uWaveforms;
	cfg.uConts = config.uConts;
	cfg.uEvents = config.uEvents;
	cfg.uComments = config.uComments;
	cfg.uTrackings = config.uTrackings;
	cfg.bAbsolute = config.bAbsolute;
	cbSdkResult res = cbSdkSetTrialConfig(m_instance, cfg.bActive, cfg.Begchan, cfg.Begmask, cfg.Begval,
		cfg.Endchan, cfg.Endmask, cfg.Endval, cfg.bDouble, cfg.uWaveforms, cfg.uConts, cfg.uEvents,
		cfg.uComments, cfg.uTrackings, cfg.bAbsolute);
}

bool CbSdkNative::GetIsDouble()
{
	return cfg.bDouble;
}

CbSdkNative::CbSdkConfigParam CbSdkNative::FetchTrialConfig()
{
	CbSdkConfigParam config;
	cbSdkResult res = cbSdkGetTrialConfig(m_instance, &config.bActive, &config.Begchan, &config.Begmask, &config.Begval,
			&config.Endchan, &config.Endmask, &config.Endval, &config.bDouble, &config.uWaveforms, &config.uConts, &config.uEvents,
			&config.uComments, &config.uTrackings, &config.bAbsolute);
	return config;
}

void CbSdkNative::PrefetchData(uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers)
{
	// Determine the current number of samples.
	cbSdkResult sdkres = cbSdkInitTrialData(m_instance, 1, 0, p_trialCont.get(), 0, 0);  // Put p_trialEvent.get() back
	chan_count = p_trialCont->count;
	for (int chan_ix = 0; chan_ix < chan_count; chan_ix++)
	{
		samps_per_chan[chan_ix] = p_trialCont->num_samples[chan_ix];
		chan_numbers[chan_ix] = p_trialCont->chan[chan_ix];
	}
}

void CbSdkNative::TransferData(uint32_t* timestamp)
{
	if (timestamp != NULL) *timestamp = p_trialCont->time;
	if (p_trialCont->count > 0)
	{
		for (int chan_ix = 0; chan_ix < p_trialCont->count; chan_ix++)
		{
			// Create new vectors for each channel and copy the vector pointers to p_trialCont.
			if (cfg.bDouble)
			{
				dblData[chan_ix] = std::vector<double>(p_trialCont->num_samples[chan_ix]);
				p_trialCont->samples[chan_ix] = dblData[chan_ix].data();
			}
			else
			{
				intData[chan_ix] = std::vector<int16_t>(p_trialCont->num_samples[chan_ix]);
				p_trialCont->samples[chan_ix] = intData[chan_ix].data();
			}
		}
		cbSdkResult sdkres = cbSdkGetTrialData(m_instance, 1, 0, p_trialCont.get(), 0, 0); // p_trialEvent.get()
	}
}

void CbSdkNative::GetData(int16_t* buffer, int chan_idx)
{
	for (uint32_t samp_ix = 0; samp_ix < p_trialCont->num_samples[chan_idx]; samp_ix++)
	{
		buffer[samp_ix] = intData[chan_idx][samp_ix];
	}
}

void CbSdkNative::GetData(double* buffer, int chan_idx)
{
	std::vector<double> native_data = dblData[chan_idx];
	for (int samp_ix = 0; samp_ix < native_data.size(); samp_ix++)
	{
		buffer[samp_ix] = native_data[samp_ix];
	}
}

CbSdkNative* CbSdkNative_Create(uint32_t nInstance, int inPort, int outPort, int bufsize, const char* inIP, const char* outIP, bool use_double)
{
	return new CbSdkNative(nInstance, inPort, outPort, bufsize, inIP, outIP, use_double);
}

bool CbSdkNative_GetIsDouble(CbSdkNative* pCbSdk) {
	if (pCbSdk != NULL)
		return pCbSdk->GetIsDouble();
	else
		return false;
}

bool CbSdkNative_GetIsOnline(CbSdkNative* pCbSdk)
{
	if (pCbSdk != NULL)
		return pCbSdk->isOnline;
	else
		return false;
}

void CbSdkNative_PrefetchData(CbSdkNative* pCbSdk, uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers)
{
	if (pCbSdk != NULL)
	{
		pCbSdk->PrefetchData(chan_count, samps_per_chan, chan_numbers);
		std::cout << "pCbSdk->PrefetchData(chan_count, samps_per_chan, chan_numbers) returned." << std::endl;
	}
		
}

void CbSdkNative_TransferData(CbSdkNative* pCbSdk, uint32_t* timestamp)
{
	if (pCbSdk != NULL)
		pCbSdk->TransferData(timestamp);
}

void CbSdkNative_GetDataInt(CbSdkNative* pCbSdk, int16_t* buffer, int chan_idx)
{
	if (pCbSdk != NULL)
		pCbSdk->GetData(buffer, chan_idx);
}

void CbSdkNative_GetDataDouble(CbSdkNative* pCbSdk, double* buffer, int chan_idx)
{
	if (pCbSdk != NULL)
		pCbSdk->GetData(buffer, chan_idx);
}

void CbSdkNative_Delete(CbSdkNative* pCbSdk) {
	if (pCbSdk != NULL)
	{
		delete pCbSdk;
		pCbSdk = NULL;
	}
}