// https://github.com/hepcatjk/CsharpCLICplusplusDLLExample
#include "cbsdk_cli.h"
#include <vector>
#include <msclr\marshal_cppstd.h>

constexpr auto DEFAULT_BUF_SAMPS = 3000;

using namespace CereLink;
using namespace System;
using namespace System::Runtime::InteropServices;	// needed for Marshal
using namespace msclr::interop;


CbSdkCLI::CbSdkCLI(UInt32 nInstance, int inPort, int outPort, int bufsize, String^ inIP, String^ outIP, Boolean use_double)
{
	// Good defaults for initialization parameters:
	// inPort: 51002
	// outPort: 51001
	// bufsize: 8 * 1024 * 1024, or 6 * 1024 * 1024 on Mac
	// inIP: ""
	// outIP: ""
	// use_double: false
	std::string inip_std = marshal_as<std::string>(inIP);
	std::string outip_std = marshal_as<std::string>(outIP);
	cbsdkNative = new CbSdkNative((uint32_t)nInstance, inPort, outPort, bufsize, inip_std, outip_std, use_double);
	PreallocateCont();
}

CbSdkCLI::~CbSdkCLI()
{
	delete cbsdkNative;
}

void CbSdkCLI::PreallocateCont()
{
	if (cbsdkNative->GetIsDouble())
	{
		contSamplesDbl = gcnew array< array< double> ^>(256 + 16);
		for (int ch_idx = 0; ch_idx < contSamplesDbl->Length; ch_idx++)
		{
			contSamplesDbl[ch_idx] = gcnew array< double >(DEFAULT_BUF_SAMPS);
		}
	}
	else
	{
		contSamplesInt = gcnew array< array< int16_t> ^>(256 + 16);
		for (int ch_idx = 0; ch_idx < contSamplesInt->Length; ch_idx++)
		{
			contSamplesInt[ch_idx] = gcnew array< int16_t >(DEFAULT_BUF_SAMPS);
		}
	}
	for (int ch_idx = 0; ch_idx < (256 + 16); ch_idx++)
	{
		samps_per_chan[ch_idx] = 0;
		chan_numbers[ch_idx] = 0;
	}
}

bool CbSdkCLI::GetIsOnline()
{
	return cbsdkNative->isOnline;
}

bool CbSdkCLI::GetIsDouble()
{
	return cbsdkNative->GetIsDouble();
}

uint16_t CbSdkCLI::Fetch()
{
	//array<uint32_t>^ samps_per_chan = gcnew array<uint32_t>(256 + 16);
	//array<uint16_t>^ chan_numbers = gcnew array< uint16_t>(256 + 16);
	//Console::WriteLine("CLI - samps_per_chan.Length: {0}; chan_numbers.Length: {1]", samps_per_chan->Length, chan_numbers->Length);
	pin_ptr<uint32_t> pspc = &samps_per_chan[0];
	pin_ptr<uint16_t> pcn = &chan_numbers[0];
	uint16_t chan_count;
	cbsdkNative->PrefetchData(chan_count, pspc, pcn);
	Console::WriteLine("Managed - PrefetchData returned {0} channels.", chan_count);
	if (chan_count > 0)
	{
		// Now that we know how many samples are waiting, make sure our
		// buffers are large enough and get a collection of their pinned pointers.
		array<GCHandle>^ handles = gcnew array<GCHandle>(chan_count);
		if (cbsdkNative->GetIsDouble())
		{
			for (int ch_idx = 0; ch_idx < chan_count; ch_idx++)
			{
				Console::WriteLine("Managed - Fetch chan {0} allocating {1} double samples.", ch_idx, samps_per_chan[ch_idx]);
				contSamplesDbl[ch_idx] = gcnew array< double >(samps_per_chan[ch_idx]);
				handles[ch_idx] = GCHandle::Alloc(contSamplesDbl[ch_idx][0], GCHandleType::Pinned);
			}
		}
		else
		{
			for (int ch_idx = 0; ch_idx < chan_count; ch_idx++)
			{
				Console::WriteLine("Managed - Fetch chan {0} allocating {1} int16 samples.", ch_idx, samps_per_chan[ch_idx]);
				contSamplesInt[ch_idx] = gcnew array< int16_t >(samps_per_chan[ch_idx]);
				handles[ch_idx] = GCHandle::Alloc(contSamplesInt[ch_idx][0], GCHandleType::Pinned);
			}
		}

		for (int ch_idx = 0; ch_idx < chan_count; ch_idx++)
		{
			cbsdkNative->TransferData(handles[ch_idx].AddrOfPinnedObject().ToPointer(), ch_idx);
		}
		
		Console::WriteLine("Managed - Sample 0,0: {0}", contSamplesInt[0][0]);

		// Free the pinned handles.
		for (int ch_idx = 0; ch_idx < chan_count; ch_idx++)
		{
			handles[ch_idx].Free();
		}
	}
	return chan_count;
}

/*
array<double>^ CbSdkCLI::GetDataDbl(UInt16 channelIndex)
{
	return contSamplesDbl[channelIndex];
}

array<Int16>^ CbSdkCLI::GetDataInt(UInt16 channelIndex)
{
	return contSamplesInt[channelIndex];
}
*/