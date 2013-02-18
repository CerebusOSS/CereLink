/* =STS=> UDPsocket.h[1733].aa07   open     SMID:7 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 Cyberkinetics, Inc.
//
// $Workfile: UDPsocket.h $
// $Archive: /Cerebus/WindowsApps/Central/UDPsocket.h $
// $Revision: 1 $
// $Date: 1/05/04 4:29p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#ifndef UDPSOCKET_H_INCLUDED
#define UDPSOCKET_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#ifndef __APPLE__
#include <linux/sockios.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
typedef struct sockaddr_in SOCKADDR_IN;
typedef int SOCKET;
#endif
#include "cbhwlib.h"
#include "cki_common.h"

#define LOOPBACK_ADDRESS   "127.0.0.1"
#define LOOPBACK_BROADCAST "127.0.0.1"

class UDPSocket
{
public:
    UDPSocket();
    virtual ~UDPSocket();

    // Open UDP socket
    cbRESULT Open(STARTUP_OPTIONS nStartupOptionsFlags, int nRange, bool bVerbose, LPCSTR szInIP,
          LPCSTR szOutIP, bool bBroadcast, bool bDontRoute, bool bNonBlocking,
          int nRecBufSize, int nInPort, int nOutPort, int nPacketSize);

    void OutPort(int nOutPort);

    void Close();

    SOCKET GetSocket() {return inst_sock;}

    // Receive one packet if queued
    int Recv(void * packet) const;

    // Send this packet, it has cbBytes of length
    int Send(void *ppkt, int cbBytes) const;

protected:
    SOCKET      inst_sock;     // instrument socket for input
    SOCKADDR_IN dest_sockaddr;
    STARTUP_OPTIONS m_nStartupOptionsFlags;
    int m_nPacketSize; // packet size
    bool m_bVerbose;   // verbose output
};

#endif // include guard
