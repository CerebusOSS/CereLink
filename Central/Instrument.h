/* =STS=> Instrument.h[1729].aa09   open     SMID:10 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 Cyberkinetics, Inc.
//
// $Workfile: Instrument.h $
// $Archive: /Cerebus/WindowsApps/Central/Instrument.h $
// $Revision: 4 $
// $Date: 1/05/04 4:36p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#ifndef INSTRUMENT_H_INCLUDED
#define INSTRUMENT_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "cbhwlib.h"
#include "UDPsocket.h"
#include "cki_common.h"

#define NSP_IN_ADDRESS      cbNET_UDP_ADDR_INST  // NSP default address
#define NSP_OUT_ADDRESS     cbNET_UDP_ADDR_CNT   // NSP subnet default address
#define NSP_IN_PORT         cbNET_UDP_PORT_BCAST // Neuroflow Data Port
#define NSP_OUT_PORT        cbNET_UDP_PORT_CNT   // Neuroflow Control Port
#define NSP_REC_BUF_SIZE (4096 * 2048)  // Receiving system buffer size (multiple of 4096)

class Instrument
{
public:
    Instrument();
    virtual ~Instrument();

    // Open the NSP instrument
    cbRESULT OpenNSP(STARTUP_OPTIONS nStartupOptionsFlags, uint16_t nNSPnum);
    cbRESULT Open(STARTUP_OPTIONS nStartupOptionsFlags, bool bBroadcast = false, bool bDontRoute = true,
        bool bNonBlocking = true, int nRecBufSize = NSP_REC_BUF_SIZE);
    void SetNetwork(int nInPort, int nOutPort, LPCSTR szInIP, LPCSTR szOutIP);

    void Close();
    void Standby();
    void Shutdown();
    void TestForReply(void * pPacket);

    enum { TICK_COUNT = 15 };        // Default number of ticks to wait for a response ( 10ms per tick)
    enum { RESEND_COUNT = 10 };     // Default number of times to "resend" a packet before erroring out.
    void Reset(int nMaxTickCount = TICK_COUNT, int nMaxRetryCount = RESEND_COUNT);


    // Called every 10 ms...use for "resending"
    // Outputs:
    //  TRUE if any instrument error has happened; FALSE otherwise
    bool Tick(void);


    enum ModeType
    {
        MT_OK_TO_SEND,              // It is OK to send out a packet
        MT_WAITING_FOR_REPLY,       // We are waiting for the "response"
    };
    ModeType GetSendMode();     // What is our current send mode?



    int Recv(void * packet);

    int Send(void *ppkt);       // Send this packet out

    // Is it OK to send a new packet out?
    bool OkToSend();


    // Purpose: receive a buffer from the "loopback" area. In other words, get it from our
    //  local buffer. It wasn't really sent
    // Inputs:
    //  pBuffer - Where to stuff the packet
    // Outputs:
    //  the number of bytes read, or 0 if no data was found
    int LoopbackRecv(void * pBuffer, uint32_t nInstance = 0)
    {
        uint32_t nIdx = cb_library_index[nInstance];

        // The logic here is quite complicated. Data is filled in from other processes
        // in a 2 pass mode. First they fill all except they skip the first 4 bytes.
        // The final step in the process is to convert the 1st dword from "0" to some other number.
        // This step is done in a thread-safe manner
        // Consequently, all packets can not have "0" as the first DWORD. At the time of writing,
        // We were looking at the "time" value of a packet.
        if (cb_xmt_local_buffer_ptr[nIdx]->buffer[cb_xmt_local_buffer_ptr[nIdx]->tailindex] != 0)
        {
            cbPKT_GENERIC * pPacket = (cbPKT_GENERIC*)&(cb_xmt_local_buffer_ptr[nIdx]->buffer[cb_xmt_local_buffer_ptr[nIdx]->tailindex]);
            return LoopbackRecvLow(pBuffer, pPacket, nInstance);
        }
        return 0;
    }

protected:
    UDPSocket m_icUDP;          // Socket to deal with the sending of the UDP packets

    // Purpose: receive a buffer from the "loopback" area. In other words, get it from our
    //  local buffer. It wasn't really sent. We assume that there is enough memory for
    //  all of the memory copies that take place
    // Inputs:
    //  pBuffer - Where to stuff the packet
    //  pPacketData - the packet put put in this location
    // Outputs:
    //  the number of bytes read, or 0 if no data was found
    int LoopbackRecvLow(void * pBuffer, void * pPacketData, uint32_t nInstance = 0);


    class CachedPacket
    {
    public:
        CachedPacket();
        ModeType GetMode() { return m_enSendMode; }

        void Reset(int nMaxTickCount = TICK_COUNT, int nMaxRetryCount = RESEND_COUNT); // Stop trying to send packets and restart
        bool AddPacket(void * pPacket, int cbBytes);         // Save this packet, of this size
        void CheckForReply(void * pPacket);     // A packet came in, compare to see if necessary
        bool OkToSend() { return m_enSendMode == MT_OK_TO_SEND; }


        // Called every 10 ms...use for "resending"
        // Outputs:
        //  TRUE if any instrument error has happened; FALSE otherwise
        bool Tick(const UDPSocket & rcUDP);

    protected:

        ModeType m_enSendMode;          // What is our current send mode?

        int m_nTickCount;               // How many ticks have passed since we sent?
        int m_nRetryCount;              // How many times have we re-sent this packet?
        int m_nMaxTickCount;            // How many ticks to wait for a response ( 10ms per tick)
        int m_nMaxRetryCount;           // How many times to "resend" a packet before erroring out.

        // This array MUST be larger than the largest data packet
        char m_abyPacket[2048];         // This is the packet that was sent out.
        int m_cbPacketBytes;            // The size of the most recent packet addition

    };


    enum { NUM_OF_PACKETS_CACHED = 6 };
    CachedPacket m_aicCache[NUM_OF_PACKETS_CACHED];

private:
    int m_nInPort;
    int m_nOutPort;
    LPCSTR m_szInIP;
    LPCSTR m_szOutIP;
};



extern Instrument g_icInstrument;   // The one and only instrument



#endif // include guard
