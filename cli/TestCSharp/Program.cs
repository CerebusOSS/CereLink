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
            // bool use_double = false;

            CereLinkConnection conn = new CereLinkConnection(nInstance, inPort, outPort, bufsize, inIP, outIP);
            if (conn.IsOnline())
            {
                for (int fetch_ix = 0; fetch_ix < 5; fetch_ix++)
                {
                    short[][] result = conn.FetchData();
                    Console.WriteLine("Returned {0} chans.", result.Length);
                    for (int chan_ix = 0; chan_ix < result.Length; chan_ix++)
                    {
                        Console.WriteLine("Chan {0} has {1} samples: [{2} ... {3}]",
                            chan_ix, result[chan_ix].Length, result[chan_ix][0], result[chan_ix][result[chan_ix].Length - 1]);
                    }
                    System.Threading.Thread.Sleep(11);
                }
            }
            else
            {
                Console.WriteLine("Not online.");
            }
        }
    }
}