using System;
using System.Runtime.InteropServices;	// needed for Marshal

namespace CereLink
{
    static class Constants
    {
        /// The default number of continuous samples that will be stored per channel in the trial buffer
        public const UInt32 cbSdk_CONTINUOUS_DATA_SAMPLES = 102400;  // multiple of 4096

        /// The default number of events that will be stored per channel in the trial buffer
        public const UInt32 cbSdk_EVENT_DATA_SAMPLES = (2 * 8192);  // multiple of 4096

        /// Maximum file size (in bytes) that is allowed to upload to NSP
        public const UInt32 cbSdk_MAX_UPOLOAD_SIZE = (1024 * 1024 * 1024);

        /// \todo these should become functions as we may introduce different instruments
        public const double cbSdk_TICKS_PER_SECOND = 30000.0;

        /// The number of seconds corresponding to one cb clock tick
        public const double cbSdk_SECONDS_PER_TICK = (1.0 / cbSdk_TICKS_PER_SECOND);

        public const UInt16 cbNUM_FE_CHANS = 256;
        public const UInt16 cbNUM_ANAIN_CHANS = 16;
        public const UInt16 cbNUM_ANALOG_CHANS = (cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS);
        public const UInt16 cbNUM_ANAOUT_CHANS = 4;
        public const UInt16 cbNUM_AUDOUT_CHANS = 2;
        public const UInt16 cbNUM_ANALOGOUT_CHANS = (cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS);
        public const UInt16 cbNUM_DIGIN_CHANS = 1;
        public const UInt16 cbNUM_SERIAL_CHANS = 1;
        public const UInt16 cbNUM_DIGOUT_CHANS = 4;
        public const UInt32 cbMAXCHANS = (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS + cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS + cbNUM_DIGOUT_CHANS);
        public const UInt16 cbMAXUNITS = 5;
        // MAX_CHANS_DIGITAL_IN = (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS + cbNUM_DIGIN_CHANS)
        // MAX_CHANS_SERIAL = (MAX_CHANS_DIGITAL_IN + cbNUM_SERIAL_CHANS)
        public const UInt16 cbMAXTRACKOBJ = 20; // maximum number of trackable objects
        public const UInt16 cbMAXHOOPS = 4;
        public const UInt16 cbPKT_SPKCACHEPKTCNT = 400;
        public const UInt16 cbMAX_PNTS = 128;  // make large enough to track longest possible spike width in samples
    }

    public class CereLinkConnection
    {
        [DllImport("cbsdk_ext.dll")]
        private static extern IntPtr CbSdkNative_Create(UInt32 nInstance, int inPort, int outPort, int bufsize, String inIP, String outIP, bool use_double);

        [DllImport("cbsdk_ext.dll")]
        private static extern bool CbSdkNative_GetIsDouble(IntPtr pCbSdk);

        [DllImport("cbsdk_ext.dll")]
        private static extern bool CbSdkNative_GetIsOnline(IntPtr pCbSdk);

        [DllImport("cbsdk_ext.dll")]
        private static extern void CbSdkNative_PrefetchData(IntPtr pCbSdk, ref UInt16 chan_count, UInt32[] samps_per_chan, UInt16[] chan_numbers);

        [DllImport("cbsdk_ext.dll")]
        private static extern void CbSdkNative_TransferData(IntPtr pCbSdk, ref UInt32 timestamp);

        [DllImport("cbsdk_ext.dll")]
        private static extern void CbSdkNative_GetDataInt(IntPtr pCbSdk, Int16[] buffer, int chan_idx);

        [DllImport("cbsdk_ext.dll")]
        private static extern void CbSdkNative_GetDataDouble(IntPtr pCbSdk, double[] buffer, int chan_idx);

        [DllImport("cbsdk_ext.dll")]
        private static extern void CbSdkNative_Delete(IntPtr value);

        private IntPtr pNative;

        public CereLinkConnection(UInt32 nInstance, int inPort, int outPort, int bufsize, String inIP, String outIP)
        {
            pNative = CbSdkNative_Create(nInstance, inPort, outPort, bufsize, inIP, outIP, false);
        }

        public bool IsOnline()
        {
            return CbSdkNative_GetIsOnline(pNative);
        }

        public Int16[][] FetchData()
        {
            UInt16 chan_count = 0;
            UInt32[] samps_per_chan = new UInt32[Constants.cbNUM_ANALOG_CHANS];
            UInt16[] chan_numbers = new UInt16[Constants.cbNUM_ANALOG_CHANS];
            CbSdkNative_PrefetchData(pNative, ref chan_count, samps_per_chan, chan_numbers);
            UInt32 timestamp = 0;
            CbSdkNative_TransferData(pNative, ref timestamp);

            Int16[][] out_arr = new short[chan_count][];

            for (int chan_ix = 0; chan_ix < chan_count; chan_ix++)
            {
                out_arr[chan_ix] = new short[samps_per_chan[chan_ix]];
                CbSdkNative_GetDataInt(pNative, out_arr[chan_ix], chan_ix);
            }
            return out_arr;
        }

        ~CereLinkConnection()
        {
            CbSdkNative_Delete(pNative);
            pNative = IntPtr.Zero;
        }
    }
}