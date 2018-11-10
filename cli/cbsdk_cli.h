#pragma once
#include "cbsdk_native.h"
#include <vector>
#include <msclr\marshal_cppstd.h>


namespace CereLink
{
    using namespace System;
	
    public ref class CbSdkCLI
    {
		
	public:
		CbSdkCLI(UInt32 nInstance, int inPort, int outPort, int bufsize, String^ inIP, String^ outIP, Boolean use_double);
		~CbSdkCLI();

		bool GetIsOnline();
		bool GetIsDouble();
		UInt16 Fetch();
		array<double>^ GetDataDbl(UInt16 channelIndex);
		array<Int16>^ GetDataInt(UInt16 channelIndex);

	private:
		CbSdkNative* cbsdkNative;
		array<uint32_t>^ samps_per_chan = gcnew array<uint32_t>(256 + 16);
		array<uint16_t>^ chan_numbers = gcnew array< uint16_t>(256 + 16);
	};
}
