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
		if (res == CBSDKRESULT_SUCCESS)
		{
		}

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

		//AllocateBuffers();
	}
}

CbSdkNative::~CbSdkNative()
{
	cbSdkResult res = cbSdkClose(m_instance);
	//DeallocateBuffers();
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
	if (res != CBSDKRESULT_SUCCESS)
	{
	}
	return config;
}

void CbSdkNative::AllocateBuffers()
{
	// For trial cont.
	for (size_t chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; chan_ix++)
	{
		// void * samples[cbNUM_ANALOG_CHANS];
		if (cfg.bDouble)
		{
			dblData[chan_ix] = std::vector<double>(0);
		}
		else
		{
			intData[chan_ix] = std::vector<int16_t>(0);
		}
	}
}

void CbSdkNative::DeallocateBuffers()
{
	for (size_t chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; chan_ix++)
	{
		if (cfg.bDouble)
		{
			//delete (double *)p_trialCont->samples[chan_ix];
		}
		else
		{
			//delete (int16_t *)p_trialCont->samples[chan_ix];
		}
	}
}

void CbSdkNative::PrefetchData(uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers)
{
	// zero-out p_trialCont. Actually this is probably unnecessary.
	//memset(p_trialCont.get(), 0, sizeof(cbSdkTrialCont));
	//memset(p_trialEvent.get(), 0, sizeof(cbSdkTrialEvent));
	// Determine the current number of samples.
	cbSdkResult sdkres = cbSdkInitTrialData(m_instance, 1, 0, p_trialCont.get(), 0, 0);  // Put p_trialEvent.get() back
	chan_count = p_trialCont->count;
	std::cout << "Native - PrefetchData - cbSdkInitTrialData. Data for N channels: " << chan_count << std::endl;
	for (int chan_ix = 0; chan_ix < chan_count; chan_ix++)
	{
		samps_per_chan[chan_ix] = p_trialCont->num_samples[chan_ix];
		chan_numbers[chan_ix] = p_trialCont->chan[chan_ix];
		if (cfg.bDouble)
		{
			dblData[chan_ix] = std::vector<double>(chan_numbers[chan_ix]);
			p_trialCont->samples[chan_ix] = dblData[chan_ix].data();
		}
		else
		{
			intData[chan_ix] = std::vector<int16_t>(chan_numbers[chan_ix]);
			p_trialCont->samples[chan_ix] = intData[chan_ix].data();
		}
	}

	sdkres = cbSdkGetTrialData(m_instance, 1, 0, p_trialCont.get(), 0, 0); // p_trialEvent.get()
}


void CbSdkNative::TransferData(void* buff, int chan_idx, uint32_t* timestamp)
{
	if (timestamp != NULL) *timestamp = p_trialCont->time;
	if (chan_idx < p_trialCont->count)
	{
		for (uint32_t samp_ix = 0; samp_ix < p_trialCont->num_samples[chan_idx]; samp_ix++)
		{
			if (cfg.bDouble)
			{
				((double*)buff)[samp_ix] = dblData[chan_idx][samp_ix];
			}
			else
			{
				((int16_t*)buff)[samp_ix] = intData[chan_idx][samp_ix];
			}
		}
	}
}
