#pragma once
#include <vector>
#include <array>
#include <string>

// Incomplete types. Come from cbsdk.h
struct my_cbSdkTrialEvent;
struct my_cbSdkTrialCont;

class CbSdkNative {
public:

	struct CbSdkConfigParam
	{
		uint32_t bActive;      // Start if true, stop otherwise
		uint16_t Begchan;      // Start channel
		uint32_t Begmask;      // Mask for start value
		uint32_t Begval;       // Start value
		uint16_t Endchan;      // Last channel
		uint32_t Endmask;      // Mask for end value
		uint32_t Endval;       // End value
		bool bDouble;         // If data array is double precision
		uint32_t uWaveforms;   // Number of spike waveform to buffer
		uint32_t uConts;       // Number of continuous data to buffer
		uint32_t uEvents;      // Number of events to buffer
		uint32_t uComments;    // Number of comments to buffer
		uint32_t uTrackings;   // Number of tracking data to buffer
		bool bAbsolute;       // If event timing is absolute or relative to trial
	};

	bool isOnline = false;

	CbSdkNative(uint32_t nInstance, int inPort= 51002, int outPort= 51001, int bufsize= 8 * 1024 * 1024,
		std::string inIP="", std::string outIP="", bool use_double=false);
	~CbSdkNative();
	bool GetIsDouble();
	void PrefetchData(uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers);
	void TransferData(void* buff, int chan_idx, uint32_t* timestamp = NULL);
	
private:
	int32_t m_instance;
	CbSdkConfigParam cfg;
	// forward declarations of structures from cbsdk.h
	std::unique_ptr<my_cbSdkTrialEvent> p_trialEvent;
	std::unique_ptr<my_cbSdkTrialCont> p_trialCont;
	// Convenient references to data vectors, shared with p_trialCont->samples.
	std::array<std::vector<double>, 256 + 16> dblData;
	std::array<std::vector<int16_t>, 256 + 16> intData;

	CbSdkConfigParam FetchTrialConfig();
	void PushTrialConfig(CbSdkConfigParam config);
	void AllocateBuffers();
	void DeallocateBuffers();
};