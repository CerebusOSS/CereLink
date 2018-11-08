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

            CbSdkCLI managed = new CbSdkCLI(nInstance, inPort, outPort, bufsize, inIP, outIP, use_double);
            Console.WriteLine("C# - managed.GetIsOnline(): {0}", managed.GetIsOnline());
            if (managed.GetIsOnline())
            {
                for (int i = 0; i < 5; i++)
                {
                    Console.WriteLine("C# - managed.Fetch() {0}", i);
                    UInt16 nChans = managed.Fetch();
                    Console.WriteLine("C# - managed.Fetch() {0} - fetched {1} channels.", i, nChans);
                    if (managed.GetIsDouble())
                    {
                        Double[][] arr = managed.contSamplesDbl;
                        for (int chan_idx = 0; chan_idx < nChans; chan_idx++)
                        {
                            Console.WriteLine("C# - Retrieved {0} dbl samples for channel {1}", arr[chan_idx].Length, chan_idx);
                        }
                    }
                    else
                    {
                        Int16[][] arr = managed.contSamplesInt;
                        for (int chan_idx = 0; chan_idx < nChans; chan_idx++)
                        {
                            Console.WriteLine("C# - Retrieved {0} int samples for channel {1}", arr[chan_idx].Length, chan_idx);
                        }
                    }
                }
                
            }
        }
    }
}