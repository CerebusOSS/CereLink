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
}

CbSdkCLI::~CbSdkCLI()
{
	delete cbsdkNative;
}

bool CbSdkCLI::GetIsOnline()
{
	return cbsdkNative->isOnline;
}

bool CbSdkCLI::GetIsDouble()
{
	return cbsdkNative->GetIsDouble();
}

UInt16 CbSdkCLI::Fetch()
{
	pin_ptr<uint32_t> pspc = &samps_per_chan[0];
	pin_ptr<UInt16> pcn = &chan_numbers[0];
	UInt16 chan_count;
	cbsdkNative->PrefetchData(chan_count, pspc, pcn);
	
	if (chan_count > 0)
	{
		// Now that we know how many samples are waiting (chan_count and in samps_per_chan),
		// I would like to allocate managed memory and send a pointer to that memory to the unmanaged code
		// to use as a buffer to back the data transfer.
		// GCHandle handle = GCHandle::Alloc(managed_array_for_channel, GCHandleType::Pinned)
		// cbsdkNative->AssignBufferForChan(handle.AddrOfPinnedObject().ToPointer(), ch_idx);
		// cbsdkNative->TransferData(&timestamp);
		// handle.Free()
		// Then we can use managed_array_for_channel. At least that's the idea. I never got it to work.

		// Instead we let the native code manage its own memory then we relay that memory on-demand.
		cbsdkNative->TransferData();
	}
	return chan_count;
}

array<double>^ CbSdkCLI::GetDataDbl(UInt16 channelIndex)
{
	array< double >^ managed_array = gcnew array< double >(samps_per_chan[channelIndex]);
	pin_ptr<double>pp = &managed_array[0];
	cbsdkNative->GetData(pp, channelIndex);
	return managed_array;
}

array<Int16>^ CbSdkCLI::GetDataInt(UInt16 channelIndex)
{
	array< Int16 >^ managed_array = gcnew array< Int16 >(samps_per_chan[channelIndex]);
	pin_ptr<int16_t>pp = &managed_array[0];
	cbsdkNative->GetData(pp, channelIndex);
	return managed_array;
}