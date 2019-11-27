#pragma once
#include <vector>
#include <array>
#include <string>

// Incomplete types. Come from cbsdk.h
struct my_cbSdkTrialEvent;
struct my_cbSdkTrialCont;
struct my_cbSdkTrialComment;

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
    void SetComment(std::string comment="", uint32_t t_bgr=255, uint8_t charset=0);
    //bool GetComment();
	void PrefetchData(uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers);
	void TransferData(void** buffer, uint32_t* timestamp = NULL);
	bool SetFileStorage(char* file_name, char* file_comment, bool* bStart);
	void SetPatientInfo(char* ID, char* f_name, char* l_name, uint32_t DOB_month, uint32_t DOB_day, uint32_t DOB_year);
	bool GetIsRecording();

private:
	int32_t m_instance;
	CbSdkConfigParam cfg;
	
	// forward declarations of structures from cbsdk.h
	std::unique_ptr<my_cbSdkTrialEvent> p_trialEvent;
	std::unique_ptr<my_cbSdkTrialCont> p_trialCont;
    std::unique_ptr<my_cbSdkTrialComment> p_trialComment;
	
	// Data vectors to store the result of TransferData.
	std::array<std::vector<double>, 256 + 16> dblData;
	std::array<std::vector<int16_t>, 256 + 16> intData;

	CbSdkConfigParam FetchTrialConfig();
	void PushTrialConfig(CbSdkConfigParam config);
};


extern "C"
{
	__declspec(dllexport) CbSdkNative* CbSdkNative_Create(uint32_t nInstance, int inPort, int outPort, int bufsize, const char* inIP, const char* outIP, bool use_double);
	__declspec(dllexport) bool CbSdkNative_GetIsDouble(CbSdkNative* pCbSdk);
	__declspec(dllexport) bool CbSdkNative_GetIsOnline(CbSdkNative* pCbSdk);
    //__declspec(dllexport) bool CbSdkNative_GetComment(CbSdkNative* pCbSdk);
    __declspec(dllexport) void CbSdkNative_SetComment(CbSdkNative* pCbSdk, const char* comment, uint8_t red, uint8_t green, uint8_t blue, uint8_t charset);
	__declspec(dllexport) void CbSdkNative_PrefetchData(CbSdkNative* pCbSdk, uint16_t &chan_count, uint32_t* samps_per_chan, uint16_t* chan_numbers);
	__declspec(dllexport) void CbSdkNative_TransferData(CbSdkNative* pCbSdk, void** buffer, uint32_t* timestamp = NULL);
	__declspec(dllexport) void CbSdkNative_Delete(CbSdkNative* pCbSdk);
	__declspec(dllexport) bool CbSdkNative_SetFileStorage(CbSdkNative* pCbSdk, char* file_name, char* file_comment, bool* bStart);
	__declspec(dllexport) void CbSdkNative_SetPatientInfo(CbSdkNative* pCbSdk, char* ID, char* f_name, char* l_name, uint32_t DOB_month, uint32_t DOB_day, uint32_t DOB_year );
	__declspec(dllexport) bool CbSdkNative_GetIsRecording(CbSdkNative* pCbSdk);
}