using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CereLink;

namespace SimpleClient
{

    class Program
    {
        static void Main(string[] args)
        {
            UInt32 nInstance = 0;
            int inPort = 51002;
            int outPort = 51001;
            int bufsize = 8 * 1024 * 1024;
            string inIP = "127.0.0.1";
            string outIP = "";
            bool use_double = false;

            CbSdkNative managed = new CbSdkNative(nInstance, inPort, outPort, bufsize, inIP, outIP, use_double);
            Console.WriteLine("C# - managed.GetIsOnline(): {0}", managed.GetIsOnline());
            if (managed.GetIsOnline())
            {
                for (int i = 0; i < 5; i++)
                {
                    UInt16 nChans = managed.Fetch();
                    Console.WriteLine("C# - managed.Fetch() {0} - fetched {1} channels.", i, nChans);
                    System.Threading.Thread.Sleep(11);
                    if (managed.GetIsDouble())
                    {
                        for (UInt16 chan_idx = 0; chan_idx < nChans; chan_idx++)
                        {
                            Double[] arr = managed.GetDataDbl(chan_idx);
                            Console.WriteLine("C# - Channel {0} ({1} samples): [{2} ... {3}]", chan_idx, arr.Length, arr[0], arr[arr.Length - 1]);
                        }
                    }
                    else
                    {
                        for (UInt16 chan_idx = 0; chan_idx < nChans; chan_idx++)
                        {
                            Int16[] arr = managed.GetDataInt(chan_idx);
                            Console.WriteLine("C# - Channel {0} ({1} samples): [{2} ... {3}]", chan_idx, arr.Length, arr[0], arr[arr.Length - 1]);
                        }
                    }
                }
                
            }
        }
    }
}