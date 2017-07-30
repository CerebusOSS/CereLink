// =STS=> Instrument.cpp[1728].aa15   open     SMID:15 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003-2008 Cyberkinetics, Inc.
// (c) Copyright 2008-2011 Blackrock Microsystems
//
// $Workfile: Instrument.cpp $
// $Archive: /Cerebus/Human/WindowsApps/Central/Instrument.cpp $
// $Revision: 5 $
// $Date: 4/26/05 2:57p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <algorithm>  // Use C++ default min and max implementation.
#include "debugmacs.h"
#include "Instrument.h"

// table of starting ip addresses for each NSP (128 channels)
//	indexed by NSP number:
#define RANGESIZE 16

const char g_NSP_IP[cbMAXOPEN][16] =
{ NSP_IN_ADDRESS,
  "192.168.137.17",
  "192.168.137.33",
  "192.168.137.49"
};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Instrument::Instrument() :
    m_nInPort(NSP_IN_PORT), m_nOutPort(NSP_OUT_PORT),
    m_szInIP(NSP_IN_ADDRESS), m_szOutIP(NSP_OUT_ADDRESS)
{
}

Instrument::~Instrument()
{

}

// Author & Date:   Ehsan Azar     15 May 2012
// Purpose: Set networking parameters to overwrite default addresses
//           Note: must be called before Open for parameters to work
// Inputs:
//  nInPort              - the input port through which instrument connects to me
//  nOutPort             - the output port to connect to in order to connect to the instrument
//  szInIP               - Cerebus IP address
//  szOutIP              - Instrument IP address
void Instrument::SetNetwork(int nInPort, int nOutPort, LPCSTR szInIP, LPCSTR szOutIP)
{
    m_nInPort = nInPort;
    m_nOutPort = nOutPort;
    m_szInIP = szInIP;
    m_szOutIP = szOutIP;
}

// Author & Date:   Tom Richins     2 May 2011
// Purpose: Open a network socket to the NSP instrument allowing for multiple NSPs
//            If NSP is not detected, based on nStartupOptionsFlags, other addresses are tried
// Inputs:
//  nStartupOptionsFlags - the network startup option
//  nNSPnum              - index of nsp
// Outputs:
//  Returns the error code (0 means success)
cbRESULT Instrument::OpenNSP(STARTUP_OPTIONS nStartupOptionsFlags, uint16_t nNSPnum)
{
    m_szInIP = g_NSP_IP[nNSPnum < 4 ? nNSPnum : 0];
    return Open(nStartupOptionsFlags);
}


// Author & Date:   Ehsan Azar     12 March 2010
// Purpose: Open a network socket to the NSP instrument
//            If NSP is not detected, based on nStartupOptionsFlags, other addresses are tried
// Inputs:
//  nStartupOptionsFlags - the network startup option
//  bBroadcast           - establish a broadcast socket
//  bDontRoute           - establish a direct socket with no routing or gateway
//  bNonBlocking         - establish a non-blocking socket
//  nRecBufSize          - the system receive-buffer size allocated for this socket
// Outputs:
//  Returns the error code (0 means success)
cbRESULT Instrument::Open(STARTUP_OPTIONS nStartupOptionsFlags, bool bBroadcast, bool bDontRoute, bool bNonBlocking, int nRecBufSize)
{
    int nRange= RANGESIZE;
    int nInPort = m_nInPort;
    int nOutPort = m_nOutPort;
    LPCSTR szInIP = m_szInIP;
    LPCSTR szOutIP = m_szOutIP;

    bool bVerbose = false;
    if ((OPT_ANY_IP == nStartupOptionsFlags) || (OPT_LOOPBACK == nStartupOptionsFlags))
    {
        bVerbose = true; // We want to see the transactions
        nRange = 130;
#ifdef WIN32
        AllocConsole();
#else
        // Do nothing. _cprintf is defined as printf
#endif
    }
    else if (OPT_LOCAL == nStartupOptionsFlags)
    {
        // If local instrument then connect to it
        szInIP  = LOOPBACK_ADDRESS;
        szOutIP = LOOPBACK_BROADCAST;
        nInPort = NSP_IN_PORT;
        nOutPort = NSP_OUT_PORT;
    }
    return m_icUDP.Open(nStartupOptionsFlags, nRange, bVerbose, szInIP, szOutIP,
            bBroadcast, bDontRoute, bNonBlocking, nRecBufSize, nInPort, nOutPort, cbCER_UDP_SIZE_MAX);
}

int Instrument::Send(void *ppkt)
{
    uint32_t quadlettotal = (((cbPKT_GENERIC*)ppkt)->dlen) + cbPKT_HEADER_32SIZE;
    uint32_t cbSize = quadlettotal << 2;      // number of bytes


    CachedPacket * pEnd = ARRAY_END(m_aicCache);
    CachedPacket * pCache;
    // Find the first location that is open
    for (pCache = m_aicCache; pCache != pEnd; pCache++) {
        if (pCache->OkToSend())
            break;
    }

    if (pCache == pEnd)
        return -1;
    pCache->AddPacket(ppkt, cbSize);
    return m_icUDP.Send(ppkt, cbSize);
}

int Instrument::Recv(void * packet)
{
    return m_icUDP.Recv(packet);
}


// What is our current send mode?
Instrument::ModeType Instrument::GetSendMode()
{
    ModeType ret = static_cast<ModeType>(0);
    for (CachedPacket * pCache = m_aicCache; pCache != ARRAY_END(m_aicCache); ++pCache)
    {
        ret = std::max(ret, pCache->GetMode());
    }
    return ret;
}


// A packet has come in...
void Instrument::TestForReply(void * pPacket)
{
    // Give everyone a chance to see if this is an important packet
    for (CachedPacket * pCache = m_aicCache; pCache != ARRAY_END(m_aicCache); ++pCache)
    {
        pCache->CheckForReply(pPacket);
    }
}


void Instrument::Close()
{
    m_icUDP.Close();
}


void Instrument::Standby()	
{
    cbPKT_SYSINFO pktsysinfo;
    pktsysinfo.time = 0;
    pktsysinfo.chid = 0x8000;
    pktsysinfo.type = cbPKTTYPE_SYSSETRUNLEV;
    pktsysinfo.dlen = cbPKTDLEN_SYSINFO;
    pktsysinfo.runlevel = cbRUNLEVEL_HARDRESET;
    Send(&pktsysinfo);
    return;
}


void Instrument::Shutdown()	
{
    cbPKT_SYSINFO pktsysinfo;
    pktsysinfo.time = 0;
    pktsysinfo.chid = 0x8000;
    pktsysinfo.type = cbPKTTYPE_SYSSETRUNLEV;
    pktsysinfo.dlen = cbPKTDLEN_SYSINFO;
    pktsysinfo.runlevel = cbRUNLEVEL_SHUTDOWN;
    Send(&pktsysinfo);
    return;
}


// Author & Date:       Kirk Korver     17 Jun 2003
// Purpose: receive a buffer from the "loopback" area. In other words, get it from our
//  local buffer. It wasn't really sent
// Inputs:
//  pBuffer - Where to stuff the packet
//  pPacketData - the packet put put in this location
// Outputs:
//  the number of bytes read, or 0 if no data was found
int Instrument::LoopbackRecvLow(void * pBuffer, void * pPacketData, uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    cbPKT_GENERIC * pPacket = static_cast<cbPKT_GENERIC *>(pPacketData);

    // find the length of the packet
    const uint32_t quadlettotal = (pPacket->dlen) + cbPKT_HEADER_32SIZE;
    const int cbSize = quadlettotal << 2;           // How many bytes are there

    // copy the packet
    memcpy(pBuffer, pPacket, cbSize);

    // complete the packet processing by clearing the packet from the xmt buffer
    memset(pPacket, 0, cbSize);
    cb_xmt_local_buffer_ptr[nIdx]->transmitted++;
    cb_xmt_local_buffer_ptr[nIdx]->tailindex += quadlettotal;

    if ((cb_xmt_local_buffer_ptr[nIdx]->tailindex) > (cb_xmt_local_buffer_ptr[nIdx]->bufferlen - (cbCER_UDP_SIZE_MAX / 4)))
    {
        cb_xmt_local_buffer_ptr[nIdx]->tailindex = 0;
    }
    return cbSize;
}


// Called every 10 ms...use for "resending"
// Outputs:
//  TRUE if any instrument error has happened; FALSE otherwise
bool Instrument::Tick()
{
    // What is the end?
    CachedPacket * pEnd = ARRAY_END(m_aicCache);
    CachedPacket * pFound;
    // Find the first case where        {item}.Tick(m_icUDP) == true
    for (pFound = m_aicCache; pFound != pEnd; pFound++)
    {
        if (pFound->Tick(m_icUDP))
            break;
    }

   // If I've reached the end, then none are true
    return pFound != pEnd;
}

void Instrument::Reset(int nMaxTickCount, int nMaxRetryCount)
{
    // Call everybody's reset member function
    CachedPacket * pCache;
    for (pCache = m_aicCache; pCache != ARRAY_END(m_aicCache); pCache++)
    {
        pCache->Reset(nMaxTickCount, nMaxRetryCount);
    }
}


// Author & Date:   Kirk Korver     05 Jan 2003
// Purpose: Is it OK to send a new packet out?
// Outputs:
//  TRUE if it is OK to send; FALSE if not
bool Instrument::OkToSend()
{
    CachedPacket * pEnd = ARRAY_END(m_aicCache);
    CachedPacket * pFound;
    // Find the first location that is open
    for (pFound = m_aicCache; pFound != pEnd; pFound++)
    {
        if (pFound->OkToSend())
            break;
    }

    return (pFound != pEnd);
}


Instrument::CachedPacket::CachedPacket()
{
    Reset();
}

// Stop trying to send packets and restart
void Instrument::CachedPacket::Reset(int nMaxTickCount, int nMaxRetryCount)
{
    m_enSendMode = MT_OK_TO_SEND;   // What is our current send mode?
    m_nTickCount = m_nMaxTickCount = nMaxTickCount;    // How many ticks have passed since we sent?
    m_nRetryCount = m_nMaxRetryCount = nMaxRetryCount; // How many times have we re-sent this packet?
}

// Save this packet
bool Instrument::CachedPacket::AddPacket(void * pPacket, int cbBytes)
{
    ASSERT((unsigned int)cbBytes <= sizeof(m_abyPacket));
    memcpy(m_abyPacket, pPacket, cbBytes);
    m_cbPacketBytes = cbBytes;


    m_enSendMode = MT_WAITING_FOR_REPLY;
    m_nTickCount = m_nMaxTickCount;              // How many ticks have passed since we sent?
    m_nRetryCount = m_nMaxRetryCount;           // How many times have we re-sent this packet?

//    TRACE("Outgoing Pkt Type: 0x%2X\n", ((cbPKT_GENERIC*)pPacket)->type);
    return true;
}

// A packet came in, compare to see if necessary
void Instrument::CachedPacket::CheckForReply(void * pPacket)
{
    if (m_enSendMode == MT_WAITING_FOR_REPLY)
    {
        cbPKT_GENERIC * pIn = static_cast<cbPKT_GENERIC *>(pPacket);
        cbPKT_GENERIC * pOut = reinterpret_cast<cbPKT_GENERIC *>(m_abyPacket);

/*
        DEBUG_CODE
            (
            // If it is a "hearbeat" packet, then just don't look at it
            if (pIn->chid == 0x8000 && pIn->type == 0)
                return;

            TRACE("Incoming Pkt chid: 0x%04X type:  0x%02X,   SENT chid: 0x%04X, type: 0x%02X,  chan: %d\n",
                     pIn->chid, pIn->type,
                     pOut->chid, pOut->type,
                     ((cbPKT_CHANINFO *)(pOut))->chan  );

            );
*/

        if (pIn->type != (pOut->type & ~0x80))    // mask off the highest bit
            return;

        if (pIn->chid != pOut->chid)
            return;

        // If this is a "configuration type packet"
        // The logic works because Chanset is 0xC0 and all config packets are 0xC?
        // It will also work out because the 0xD0 family of packets will come in here
        // and their 1st value is channel.
        if ((pOut->type & cbPKTTYPE_CHANSET) == cbPKTTYPE_CHANSET)
        {
            if (pOut->dlen)
            {
                cbPKT_CHANINFO * pChanIn = static_cast<cbPKT_CHANINFO *>(pPacket);
                cbPKT_CHANINFO * pChanOut = reinterpret_cast<cbPKT_CHANINFO *>(m_abyPacket);

                if (pChanIn->chan != pChanOut->chan)
                    return;
            }
        }

        // If we get this far, then we must have a match
        m_enSendMode = MT_OK_TO_SEND;
    }
}


// Called every 10 ms...use for "resending"
// Outputs:
//  TRUE if any instrument error has happened; FALSE otherwise
bool Instrument::CachedPacket::Tick(const UDPSocket & rcUDP)
{
    if (m_enSendMode == MT_WAITING_FOR_REPLY)
    {
        if (--m_nTickCount <= 0)            // If time to resend
        {
            if (--m_nRetryCount > 0)        // If not too many retries
            {
                TRACE("********************Resending packet******************* type: 0x%02X\n", ((cbPKT_GENERIC *)m_abyPacket)->type );
                rcUDP.Send(m_abyPacket, m_cbPacketBytes);
                m_nTickCount = m_nMaxTickCount;
            }
            else
            {
                // true means that we have an error here
                return true;
            }
        }
    }
    return false;
}
