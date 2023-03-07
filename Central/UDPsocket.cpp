// =STS=> UDPsocket.cpp[1732].aa11   open     SMID:11 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 - 2008 Cyberkinetics, Inc.
// (c) Copyright 2008 - 2017 Blackrock Microsystems, LLC
//
// $Workfile: UDPsocket.cpp $
// $Archive: /Cerebus/WindowsApps/Central/UDPsocket.cpp $
// $Revision: 1 $
// $Date: 1/05/04 4:29p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "debugmacs.h"
#include "UDPsocket.h"
#ifdef WIN32
#include <conio.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
typedef struct sockaddr SOCKADDR;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define FAR
#define SD_BOTH SHUT_RDWR
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

UDPSocket::UDPSocket() :
    m_nStartupOptionsFlags(OPT_NONE), m_bVerbose(false), m_TCPconnected(false)
{
    inst_sock = INVALID_SOCKET;
}

UDPSocket::~UDPSocket()
{
    if (inst_sock != INVALID_SOCKET)
        Close();
}

// Author & Date:   Ehsan Azar     12 March 2010
// Purpose: Open a network UDP socket
// Inputs:
//  nStartupOptionsFlags - the network startup option
//  nRange               - the maximum allowed increments to the given IP address of the instrument to automatically connect to
//  bVerbose             - verbose mode
//  szInIP               - the IP of the instrument through which connects to me
//  szOutIP              - the IP of the instrument to connect to (it could be a subnet)
//  bBroadcast           - establish a broadcast socket
//  bDontRoute           - establish a direct socket with no routing or gateway
//  bNonBlocking         - establish a non-blocking socket
//  nRecBufSize          - the system receive-buffer size allocated for this socket
//  nInPort              - the input port through which instrument connects to me
//  nOutPort             - the output port to connect to in order to connect to the instrument
//  nPacketSize          - the maximum packet size that we receive
// Outputs:
//  Returns the error code (0 means success)
cbRESULT UDPSocket::OpenUDP(STARTUP_OPTIONS nStartupOptionsFlags, int nRange, bool bVerbose, LPCSTR szInIP,
          LPCSTR szOutIP, bool bBroadcast, bool bDontRoute, bool bNonBlocking,
          int nRecBufSize, int nInPort, int nOutPort, int nPacketSize)
{
    m_bVerbose = bVerbose;
    m_nPacketSize = nPacketSize;
    m_nStartupOptionsFlags = nStartupOptionsFlags;

#ifdef WIN32
    // Initialize Winsock 2.2
    WSADATA data;
    if (WSAStartup (MAKEWORD(2,0), &data) != 0)
        return cbRESULT_SOCKERR;
#endif

    // Create Socket for Receiving the Data Stream
#ifdef WIN32
    inst_sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
#else
    inst_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
    if (inst_sock == INVALID_SOCKET)
    {
        Close();
        return cbRESULT_SOCKERR;
    }

    BOOL opt_val = TRUE;
    socklen_t  opt_len = sizeof(BOOL);

    if (bBroadcast)
    {
        if (setsockopt(inst_sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt_val, opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
    }

    if (bDontRoute)
    {
        if (setsockopt(inst_sock, SOL_SOCKET, SO_DONTROUTE, (char*)&opt_val, opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
    }

    if (OPT_REUSE == nStartupOptionsFlags)
    {
        if (setsockopt(inst_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt_val, opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
    }

    if (nRecBufSize > 0)
    {
        // Set the data stream input buffer size
        opt_len = sizeof(int);
        int data_buff_size = nRecBufSize;
        if (setsockopt(inst_sock, SOL_SOCKET, SO_RCVBUF, (char*)&data_buff_size, opt_len) != 0)
        {
            Close();
#ifdef __APPLE__
            return cbRESULT_SOCKMEMERR;
#else
            return cbRESULT_SOCKOPTERR;
#endif
        }
        if (getsockopt(inst_sock, SOL_SOCKET, SO_RCVBUF, (char *)&data_buff_size, &opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
#ifdef __linux__
        // Linux returns double the requested size up to twice the rmem_max
        data_buff_size /= 2;
#endif
        if (data_buff_size < nRecBufSize)
        {
            // to increase buffer
            // sysctl -w net.core.rmem_max=8388608
            //  or
            // nvram boot-args="ncl=65536"
            // sysctl -w kern.ipc.maxsockbuf=8388608
            Close();
            return cbRESULT_SOCKMEMERR;
        }
    }

    if (bNonBlocking)
    {
        // Set the data socket to non-blocking operation
#ifdef WIN32
        u_long arg_val = 1;
        if (ioctlsocket(inst_sock, FIONBIO, &arg_val) == SOCKET_ERROR)
#else
        if (fcntl(inst_sock, F_SETFL, O_NONBLOCK))
#endif
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
    }

    // Attempt to bind Data Stream Socket to lowest address in range 192.168.137.1 to XXX.16
    BOOL socketbound = FALSE;
    SOCKADDR_IN inst_sockaddr;
    memset(&inst_sockaddr, 0, sizeof(inst_sockaddr));

    inst_sockaddr.sin_family      = AF_INET;
    inst_sockaddr.sin_port        = htons(nInPort);    // Neuroflow Data Port
#ifdef __APPLE__
    inst_sockaddr.sin_len = sizeof(inst_sockaddr);
#endif
    if (szInIP == 0)
        inst_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        inst_sockaddr.sin_addr.s_addr = inet_addr(szInIP);

    int nCount = 1;
    do
    {
        if (bind(inst_sock, (struct sockaddr FAR *)&inst_sockaddr, sizeof(inst_sockaddr)) == 0)
            socketbound = TRUE;
        else
        {
            // increment the last ip number
            inst_sockaddr.sin_addr.s_addr = htonl(ntohl(inst_sockaddr.sin_addr.s_addr) + 1);
        }
        nCount++;
    } while( (!socketbound) && (nCount <= nRange));

    if (socketbound)
    {
        // Set up transmission target address
        dest_sockaddr.sin_family      = AF_INET;
        dest_sockaddr.sin_port        = htons(nOutPort);	// Neuroflow Control Port
        dest_sockaddr.sin_addr.s_addr = inet_addr(szOutIP);	// Subnet Broadcast
    }
    else
    {
        if (OPT_NONE == nStartupOptionsFlags)
        {
            // if no valid address was found to bind the socket, shut her down and return error
            Close();
            return cbRESULT_SOCKBIND;
        }
        else
        {
            // if no valid address was found to bind the socket, just connect on any available
            // interface
            if (m_bVerbose)
                _cprintf("Warning: could not bind to socket on the subnet...\n");

            opt_len = sizeof(BOOL);
            if (setsockopt(inst_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt_val, opt_len) != 0)
            {
                if(m_bVerbose)
                    _cprintf("Error enabling address re-used\n");
                Close();
                return cbRESULT_SOCKOPTERR;
            }

            // Bind to the broadcast address
            if (OPT_LOOPBACK == nStartupOptionsFlags)
                inst_sockaddr.sin_addr.s_addr = inet_addr(LOOPBACK_ADDRESS);
            else
                inst_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

            int result = bind(inst_sock, (struct sockaddr FAR *)&inst_sockaddr, sizeof(inst_sockaddr));

            if (result == 0)
            {
                // Set up transmission target address
                //
                // Assume there's no cerebus subnet, so control packets should go to
                // broadcast
                dest_sockaddr.sin_family      = AF_INET;
                dest_sockaddr.sin_port        = htons(nOutPort); // Neuroflow Control Port

                if (OPT_LOOPBACK == nStartupOptionsFlags)
                    dest_sockaddr.sin_addr.s_addr = inet_addr(LOOPBACK_BROADCAST);
                else
                    dest_sockaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            } else {      // if we still can't bind, it's an error
                Close();
                return cbRESULT_SOCKBIND;
            }
        }
    }
    if(m_bVerbose) {
        _cprintf("Successfully initialized network socket, bound to %s:%d\n",
        inet_ntoa(inst_sockaddr.sin_addr),(int)(ntohs(inst_sockaddr.sin_port)));

        _cprintf("Sending control packets to %s:%d\n",
        inet_ntoa(dest_sockaddr.sin_addr),(int)(ntohs(dest_sockaddr.sin_port)));
    }
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar     13 Aug 2010
// Purpose: Change destination port number.
// Inputs:
//   nOutPort - new port number
void UDPSocket::OutPort(int nOutPort)
{
    dest_sockaddr.sin_port        = htons(nOutPort);
}


cbRESULT UDPSocket::OpenTCP(STARTUP_OPTIONS nStartupOptionsFlags, int nRange, bool bVerbose, LPCSTR szInIP,
    LPCSTR szOutIP, bool bBroadcast, bool bDontRoute, bool bNonBlocking,
    int nRecBufSize, int nInPort, int nOutPort, int nPacketSize)
{
    m_bVerbose = bVerbose;
    m_nPacketSize = nPacketSize;
    m_nStartupOptionsFlags = nStartupOptionsFlags;

#ifdef WIN32
    // Initialize Winsock 2.2
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 0), &data) != 0)
        return cbRESULT_SOCKERR;
#endif

    // Create Socket
#ifdef WIN32
    inst_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, 0);
#else
    inst_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
#endif
    if (inst_sock == INVALID_SOCKET)
    {
        Close();
        return cbRESULT_SOCKERR;
    }

    BOOL opt_val = TRUE;
    socklen_t  opt_len = sizeof(BOOL);

    if (nRecBufSize > 0)
    {
        // Set the data stream input buffer size
        opt_len = sizeof(int);
        int data_buff_size = nRecBufSize;
        if (setsockopt(inst_sock, SOL_SOCKET, SO_RCVBUF, (char*)&data_buff_size, opt_len) != 0)
        {
            Close();
#ifdef __APPLE__
            return cbRESULT_SOCKMEMERR;
#else
            return cbRESULT_SOCKOPTERR;
#endif
        }
        if (getsockopt(inst_sock, SOL_SOCKET, SO_RCVBUF, (char*)&data_buff_size, &opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
        else
        {
            TRACE("GOT RCVBUF %i\n", data_buff_size);
        }
#ifdef __linux__
        // Linux returns double the requested size up to twice the rmem_max
        data_buff_size /= 2;
#endif
        if (data_buff_size < nRecBufSize)
        {
            // to increase buffer
            // sysctl -w net.core.rmem_max=8388608
            //  or
            // nvram boot-args="ncl=65536"
            // sysctl -w kern.ipc.maxsockbuf=8388608
            Close();
            return cbRESULT_SOCKMEMERR;
        }
    }

    int opt_sndbuf = nRecBufSize;
    if (setsockopt(inst_sock, SOL_SOCKET, SO_SNDBUF, (char*)&opt_sndbuf, sizeof(int)) != 0)
    {
        Close();
        return cbRESULT_SOCKOPTERR;
    }

    //    if (bDontRoute)
    {
        if (setsockopt(inst_sock, SOL_SOCKET, SO_DONTROUTE, (char*)&opt_val, opt_len) != 0)
        {
            Close();
            return cbRESULT_SOCKOPTERR;
        }
    }

    //    if (bNonBlocking)
    //    {
            // Set the data socket to non-blocking operation
#ifdef WIN32
    u_long arg_val = 1;
    if (ioctlsocket(inst_sock, FIONBIO, &arg_val) == SOCKET_ERROR)
#else
    if (fcntl(inst_sock, F_SETFL, O_NONBLOCK))
#endif
    {
        Close();
        return cbRESULT_SOCKOPTERR;
    }
    //    }

        // Attempt to connect to an existing Gemini server
    BOOL socketbound = FALSE;

    SOCKADDR_IN inst_sockaddr;
    memset(&inst_sockaddr, 0, sizeof(inst_sockaddr));

    inst_sockaddr.sin_family = AF_INET;
    inst_sockaddr.sin_port = htons(nInPort);    // Neuroflow Data Port
    inst_sockaddr.sin_addr.s_addr = inet_addr(szInIP);
#ifdef __APPLE__
    inst_sockaddr.sin_len = sizeof(inst_sockaddr);
#endif

    int err = connect(inst_sock, (struct sockaddr FAR*) & inst_sockaddr, sizeof(inst_sockaddr));

    if (err == 0)
    {
        Close();
        return cbRESULT_SOCKOPTERR;
    }

    if (m_bVerbose) {
        _cprintf("Successfully initialized TCP network socket, bound to %s:%d\n",
            inet_ntoa(inst_sockaddr.sin_addr), (int)(ntohs(inst_sockaddr.sin_port)));

        _cprintf("Sending control packets to %s:%d\n",
            inet_ntoa(dest_sockaddr.sin_addr), (int)(ntohs(dest_sockaddr.sin_port)));
    }

    m_TCPconnected = true;

    return cbRESULT_OK;
}


void UDPSocket::Close()
{
    m_TCPconnected = false;
    shutdown(inst_sock, SD_BOTH); // shutdown communication
#ifdef WIN32
    closesocket(inst_sock);
    WSACleanup();
#else
    close(inst_sock);
#endif
    inst_sock = INVALID_SOCKET;
}

int UDPSocket::RecvUDP(void * packet) const
{
    int ret = recv(inst_sock, (char*)packet, m_nPacketSize, 0);

    if (ret != SOCKET_ERROR)
        return ret; // This is actual size returned
    else
    {
        int err = 0;
#ifdef WIN32
        err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return 0;
        TRACE("Socket Recv error was %i\n", err);
#else
        err = errno;
        if (err == EAGAIN)
            return 0;
        TRACE("Socket Recv error was %i\n", err);
#endif
        return 0;
    }
}


int UDPSocket::RecvTCP(void* packet) const
{
    while (1)
    {
        int err = 0;
        int ret = recv(inst_sock, (char*)packet, m_nPacketSize, 0);
#ifdef WIN32
        err = ::WSAGetLastError();
#else
        err = errno;
#endif

        if (ret != SOCKET_ERROR)
        {
            TRACE("TCP rcv - packet read %i, (error) %i\n", ret, err);
            //   return ret; // This is actual size returned
        }
        else
        {
            //return 0;
        }
    }

    return 0;

    //    static unsigned __int16 pkt_length = 0xFFFF;
    //	int ret, err;
    //
    //	if(pkt_length == 0xFFFF)
    //	{
    //		ret = recv(inst_sock, (char*)&pkt_length, sizeof(pkt_length), 0);
    //		err = ::WSAGetLastError();
    //
    ////		TRACE("TCP rcv pkt size rcv - ret %i, pkt length %i\n",ret, pkt_length);
    //
    //		return 0;
    ////		if (ret == SOCKET_ERROR) // Identify if we received data or if the socket is in error
    ////		{
    ////
    //////			TRACE("TCP rcv - error getting length %i\n", err); // The socket was in error - return the error code
    ////			if (err == WSAEWOULDBLOCK) // The socket was empty - return
    ////				return 0;
    ////
    ////			return -1;
    ////		}
    ////
    ////		if (ret != sizeof(pkt_length) || pkt_length > m_nPacketSize) // Do a second level check to understand if the packet size is legit
    ////		{
    //////			TRACE("TCP Socket Recv error %i\n", ret);
    ////			return -1;
    ////		}
    //	}
    //
    //	ret = recv(inst_sock, (char*)packet, pkt_length, 0);
    //	err = ::WSAGetLastError();
    //
    ////	TRACE("TCP rcv - packet received %i\n", ret);
    //
    //	if(ret == SOCKET_ERROR)
    //	{
    ////		TRACE("TCP rcv - packet read %i, (error) %i\n", ret, err);
    //
    //		if (err == WSAEWOULDBLOCK) // The socket was empty - return
    //			return 0;
    //
    //		return -1;
    //	}
    //
    //    if (ret == pkt_length)
    //	{
    //	//	TRACE("TCP rcv - packet properly received %i\n", ret);
    //		pkt_length = 0xFFFF;
    //		return ret; // This is actual size returned
    //	}
    //    else
    //    {
    //		TRACE("TCP rcv - packet received does not have the right length\n");
    //		return -1;
    //	}
}


int UDPSocket::SendUDP(void *ppkt, int cbBytes) const
{
    int sendRet = sendto(inst_sock, (const char *)ppkt, cbBytes, 0,
                         (SOCKADDR*)&dest_sockaddr, sizeof(dest_sockaddr));
#ifdef WIN32
    DEBUG_CODE
    (
        if (sendRet == SOCKET_ERROR) {
            TRACE("Socket Send error was %i\n", ::WSAGetLastError());
        }
    )
#else
    DEBUG_CODE
    (
        if (sendRet == SOCKET_ERROR) {
            TRACE("Socket Send error was %i\n", errno);
        }
    )
#endif

    return sendRet;

}


int UDPSocket::SendTCP(void* ppkt, int cbBytes) const
{

    TRACE("Send TCP pkt type: 0x%02X\n", ((cbPKT_GENERIC*)ppkt)->cbpkt_header.type);

    int sendRet = send(inst_sock, (const char*)ppkt, cbBytes, 0);
    //#ifdef WIN32
    //    DEBUG_CODE
    //    (
    if (sendRet == SOCKET_ERROR) {
        //            TRACE("Socket Send error was %i\n", ::WSAGetLastError());
    }
    //    )
    //#else
    //    DEBUG_CODE
    //    (
    //        if (sendRet == SOCKET_ERROR) {
    //            TRACE("Socket Send error was %i\n", errno);
    //        }
    //    )
    //#endif

    return sendRet;
}
