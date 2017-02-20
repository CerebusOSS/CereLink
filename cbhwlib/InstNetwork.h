/* =STS=> InstNetwork.h[2733].aa08   open     SMID:8 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: InstNetwork.h $
// $Archive: /common/InstNetwork.h $
// $Revision: 1 $
// $Date: 3/15/10 12:21a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Common xPlatform instrument network
//

#ifndef INSTNETWORK_H_INCLUDED
#define INSTNETWORK_H_INCLUDED

#include "debugmacs.h"
#include "cbhwlib.h"
#include "Instrument.h"
#include "cki_common.h"
#include <QThread>
#include <QTimer>
#include <QMetaType>
#include <QVector>
#include <QString>

enum NetCommandType
{
    NET_COMMAND_NONE = 0, // invalid command
    NET_COMMAND_OPEN,       // open network
    NET_COMMAND_CLOSE,      // close network
    NET_COMMAND_STANDBY,    // instrument standby
    NET_COMMAND_SHUTDOWN,   // instrument shutdown
};
// Events by network
enum NetEventType
{
    NET_EVENT_NONE = 0,         // Invalid event
    NET_EVENT_INIT,             // Instrument initialization started
    NET_EVENT_LISTENERERR,      // Listener is not set
    NET_EVENT_CBERR,            // cbRESULT error happened
    NET_EVENT_NETOPENERR,       // Error opening the instrument network
    NET_EVENT_NETCLIENT,        // Client network connection
    NET_EVENT_NETSTANDALONE,    // Stand-alone network connection
    NET_EVENT_INSTINFO,         // Instrument information (sent when network is established)
    NET_EVENT_INSTCONNECTING,   // Instrument connecting
    NET_EVENT_INSTHARDRESET,    // Instrument reset hardware
    NET_EVENT_INSTCONFIG,       // Instrument retrieving configuration
    NET_EVENT_INSTRUN,          // Instrument reset software to run
    NET_EVENT_PCTONSPLOST,      // Connection lost
    NET_EVENT_LINKFAILURE,      // Link failure
    NET_EVENT_CRITICAL,         // Critical data catchup
    NET_EVENT_CLOSE,            // Instrument closed
    NET_EVENT_RESET,            // Instrument got reset
    NET_EVENT_LOCKEDRESET,      // Locked reset (for recording)
};

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: Instrument networking thread
class InstNetwork: public QThread
{

    Q_OBJECT

public:
    // Instrument network listener
    class Listener
    {
    public:
        // Callback function to process packets, must be implemented in target class
        virtual void ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt) = 0;
    };

public:
    InstNetwork(STARTUP_OPTIONS startupOption = OPT_NONE);
    void Open(Listener * listener); // Open the network
    void ShutDown(); // Instrument shutdown
    void StandBy();  // Instrument standby
    bool IsStandAlone() {return m_bStandAlone;} // If running in stand-alone
    UINT32 getPacketCounter() {return m_nRecentPacketCount;}
    UINT32 getDataCounter() {return m_dataCounter;}
protected:
    enum { INST_TICK_COUNT = 10 };
    void run();
    void ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt); // Process incoming packets in stand-alone mode
    void timerEvent(QTimerEvent *event); // the QT timer event for stand-alone networking
    void OnWaitEvent(); // Non-stand-alone networking
    inline void CheckForLinkFailure(UINT32 nTicks, UINT32 nCurrentPacketCount); // Check link failure
private:
    void UpdateSortModel(const cbPKT_SS_MODELSET & rUnitModel);
    void UpdateBasisModel(const cbPKT_FS_BASIS & rBasisModel);
private:
    cbLevelOfConcern m_enLOC; // level of concern
    STARTUP_OPTIONS m_nStartupOptionsFlags;
    QVector<Listener *> m_listener;   // instrument network listeners
    UINT32 m_timerTicks;    // network timer ticks
    int m_timerId; // Stand-alone timer ID
    bool m_bDone;// flag to finish networking thread
    UINT32 m_nRecentPacketCount; // number of real packets
    UINT32 m_dataCounter;        // data counter
    UINT32 m_nLastNumberOfPacketsReceived;
    UINT32 m_runlevel; // Last runlevel
protected:
    static const UINT32 MAX_NUM_OF_PACKETS_TO_PROCESS_PER_PASS;
    bool m_bStandAlone;  // If it is stand-alone
    Instrument m_icInstrument;   // The instrument
    UINT32 m_instInfo; // Last instrument state
    UINT32 m_nInstance;  // library instance
    UINT32 m_nIdx;  // library instance index
    int m_nInPort;  // Client port number
    int m_nOutPort; // Instrument port number
    bool m_bBroadcast;
    bool m_bDontRoute;
    bool m_bNonBlocking;
    int m_nRecBufSize;
    QString m_strInIP;  // Client IPv4 address
    QString m_strOutIP; // Instrument IPv4 address

public Q_SLOTS:
    void Close(); // stop timer and close the message loop

private Q_SLOTS:
    void OnNetCommand(NetCommandType cmd, unsigned int code = 0);

    // Heper slots to specialize this class for networking
private Q_SLOTS:
    virtual void OnInstNetworkEvent(NetEventType, unsigned int) {;}
    virtual void OnExec() {;}

Q_SIGNALS:
    void InstNetworkEvent(NetEventType type, unsigned int code = 0);
};

#endif // include guard
