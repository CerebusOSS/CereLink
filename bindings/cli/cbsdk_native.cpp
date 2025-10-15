#include "cbsdk_native.h"
#include <cerelink/cbsdk.h>
#include <iostream>


struct my_cbSdkTrialEvent : public cbSdkTrialEvent {};
struct my_cbSdkTrialCont : public cbSdkTrialCont {};
struct my_cbSdkTrialComment : public cbSdkTrialComment {};


CbSdkNative::CbSdkNative(uint32_t nInstance, int inPort, int outPort, int bufsize, std::string inIP, std::string outIP, bool use_double)
	:p_trialEvent(new my_cbSdkTrialEvent()), p_trialCont(new my_cbSdkTrialCont), p_trialComment(new my_cbSdkTrialComment)
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

void CbSdkNative::SetComment(std::string comment, uint32_t t_bgr, uint8_t charset)
{
    //uint32_t t_bgr = (transparency << 24) + (blue << 16) + (green << 8) + red;
    //uint8_t charset = 0  // Character set(0 - ANSI, 1 - UTF16, 255 - NeuroMotive ANSI);
    cbSdkResult res = cbSdkSetComment(m_instance, t_bgr, charset, comment.c_str());
}

/*
bool CbSdkNative::GetComment()
{
    cbSdkResult sdkres = cbSdkInitTrialData(m_instance, 1, 0, 0, p_trialComment.get(), 0);
    return (sdkres == CBSDKRESULT_SUCCESS);
}
*/

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

// Receives a pointer to an array of pointers from C# to copy data to.
// We just need to pass the pointer to p_trialCont->samples.
// Whether the data are double or int16 will be determined in C#.
void CbSdkNative::TransferData(void** buffer, PROCTIME* timestamp)
{
	if (timestamp != NULL) *timestamp = p_trialCont->time;
	if (p_trialCont->count > 0)
	{
		for (int chan_ix = 0; chan_ix < p_trialCont->count; chan_ix++)
		{
			if (cfg.bDouble)
			{
				p_trialCont->samples[chan_ix] = (double*)buffer[chan_ix];
			}
			else
			{
				p_trialCont->samples[chan_ix] = (int16_t*)buffer[chan_ix];
			}
		}
		cbSdkResult sdkres = cbSdkGetTrialData(m_instance, 1, 0, p_trialCont.get(), 0, 0); // p_trialEvent.get()
	}
}

// Added by GD 2019-08-20. 
bool CbSdkNative::SetFileStorage(char* file_name, char* file_comment, bool* bStart)
{
	// For some reason the script accepts an integer instead of a boolean. 
	uint32_t iStart = bStart?1:0; 

	cbSdkResult res = cbSdkSetFileConfig(m_instance, file_name, file_comment, iStart);
	return (res == CBSDKRESULT_SUCCESS);
}

void CbSdkNative::SetPatientInfo(char* ID, char* f_name, char* l_name, uint32_t DOB_month, uint32_t DOB_day, uint32_t DOB_year )
{
	cbSdkResult res = cbSdkSetPatientInfo(m_instance, ID, f_name, l_name, DOB_month, DOB_day, DOB_year);
}

bool CbSdkNative::GetIsRecording()
{
	bool bIsRecording = false;
	char filename[256]; // Could be added to the return. 
	char username[256];
	cbSdkResult res = cbSdkGetFileConfig(m_instance, filename, username, &bIsRecording);
	if (res == CBSDKRESULT_SUCCESS)
		return  bIsRecording;
	else
		return false; 
}



CbSdkNative* CbSdkNative_Create(uint32_t nInstance, int inPort, int outPort, int bufsize, const char* inIP, const char* outIP, bool use_double)
{
    //inIP and outIP get auto-cast to std::string
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

//rgba: (red << 24) + (green << 16) + (blue << 8) + alpha
//charset: 0 - ANSI, 1 - UTF16, 255 - NeuroMotive ANSI
void CbSdkNative_SetComment(CbSdkNative* pCbSdk, const char* comment, uint8_t red, uint8_t green, uint8_t blue, uint8_t charset)
{
    uint32_t t_bgr = (blue << 16) + (green << 8) + (red);
    if (pCbSdk != NULL)
        pCbSdk->SetComment(comment, t_bgr, charset);
}

/*
bool CbSdkNative_GetComment(CbSdkNative* pCbSdk)
{
    if (pCbSdk != NULL)
        return pCbSdk->GetComment();
    else
        return false;
}
*/

void CbSdkNative_PrefetchData(CbSdkNative* pCbSdk, uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers)
{
	if (pCbSdk != NULL)
	{
		pCbSdk->PrefetchData(chan_count, samps_per_chan, chan_numbers);
		std::cout << "pCbSdk->PrefetchData(chan_count, samps_per_chan, chan_numbers) returned." << std::endl;
	}
		
}

void CbSdkNative_TransferData(CbSdkNative* pCbSdk, void** buffer, PROCTIME* timestamp)
{
	if (pCbSdk != NULL)
		pCbSdk->TransferData(buffer, timestamp);
}

void CbSdkNative_Delete(CbSdkNative* pCbSdk) {
	if (pCbSdk != NULL)
	{
		delete pCbSdk;
		pCbSdk = NULL;
	}
}


// To set the File Storage settings of the Central suite. 
// return true if OK; False if error; 
bool CbSdkNative_SetFileStorage(CbSdkNative* pCbSdk, char* file_name, char* file_comment, bool* bStart)
{
	if (pCbSdk != NULL)
		return pCbSdk->SetFileStorage(file_name, file_comment, bStart);
	else
		return false;

}

void CbSdkNative_SetPatientInfo(CbSdkNative* pCbSdk, char* patient_ID, char* first_name, char* last_name, uint32_t DOB_month, uint32_t DOB_day, uint32_t DOB_year)
{
	if (pCbSdk != NULL)
		pCbSdk->SetPatientInfo(patient_ID, first_name, last_name, DOB_month, DOB_day, DOB_year);

}

bool CbSdkNative_GetIsRecording(CbSdkNative* pCbSdk)
{
	if (pCbSdk != NULL)
		return pCbSdk->GetIsRecording();
	else
		return false; 
}

