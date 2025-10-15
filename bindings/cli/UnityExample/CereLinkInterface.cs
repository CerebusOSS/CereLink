using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using CereLink;

public class CereLinkInterface : MonoBehaviour {

    public uint nInstance = 0;
    public int inPort = 51002;
    public int outPort = 51001;
    public int bufsize = 8 * 1024 * 1024;
    public string inIP = "127.0.0.1";
    public string outIP = "";
    public bool useDouble = false;

    private CereLinkConnection conn;

    // Use this for initialization
    void Start () {
        conn = new CereLinkConnection(nInstance, inPort, outPort, bufsize, inIP, outIP, useDouble);

    }
	
	// Update is called once per frame
	void Update () {
        if (conn.IsOnline())
        {
            conn.FetchData(out short[][] result);
            Debug.Log(string.Format("Returned {0} chans.", result.Length));
            for (int chan_ix = 0; chan_ix < result.Length; chan_ix++)
            {
                Debug.Log(string.Format("Chan {0} has {1} samples: [{2} ... {3}]",
                    chan_ix, result[chan_ix].Length, result[chan_ix][0], result[chan_ix][result[chan_ix].Length - 1]));
            }   
        }
    }
}
