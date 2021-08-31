// =STS=> InstNetwork.cpp[2732].aa08   open     SMID:8 
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
//
// $Workfile: InstNetwork.cpp $
// $Archive: /common/InstNetwork.cpp $
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
#include "StdAfx.h"
#include <algorithm>  // Use C++ default min and max implementation.
#include "InstNetwork.h"
#ifndef WIN32
    #include <semaphore.h>
#endif

const uint32_t InstNetwork::MAX_NUM_OF_PACKETS_TO_PROCESS_PER_PASS = 5000;  // This is not exported correctly unless redefined here.

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: Constructor for instrument networking thread
InstNetwork::InstNetwork(STARTUP_OPTIONS startupOption) :
    QThread(),
    m_enLOC(LOC_LOW),
    m_nStartupOptionsFlags(startupOption),
    m_timerTicks(0),
    m_timerId(0),
    m_bDone(false),
    m_nRecentPacketCount(0),
    m_dataCounter(0),
    m_nLastNumberOfPacketsReceived(0),
    m_runlevel(cbRUNLEVEL_SHUTDOWN),
    m_bStandAlone(true),
    m_instInfo(0),
    m_nInstance(0),
    m_nIdx(0),
    m_nInPort(NSP_IN_PORT),
    m_nOutPort(NSP_OUT_PORT),
    m_bBroadcast(false),
    m_bDontRoute(true),
    m_bNonBlocking(true),
    m_nRecBufSize(NSP_REC_BUF_SIZE),
    m_strInIP(NSP_IN_ADDRESS),
    m_strOutIP(NSP_OUT_ADDRESS)
{

    qRegisterMetaType<NetEventType>("NetEventType"); // For QT connect to recognize this type
    qRegisterMetaType<NetCommandType>("NetCommandType"); // For QT connect to recognize this type
    // This should be the last
    moveToThread(this); // The object could not be moved if it had a parent
}

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: Open the instrument network
// Inputs:
//   listener - the instrument listener of packets and events
void InstNetwork::Open(Listener * listener)
{
    if (listener)
        m_listener += listener;
}

// Author & Date: Ehsan Azar       23 Sept 2010
// Purpose: Shut down the instrument network
void InstNetwork::ShutDown()
{
    if (m_bStandAlone)
        m_icInstrument.Shutdown();
    else
        cbSetSystemRunLevel(cbRUNLEVEL_SHUTDOWN, 0, 0, m_nInstance);
}

// Author & Date: Ehsan Azar       23 Sept 2010
// Purpose: Standby the instrument network
void InstNetwork::StandBy()
{
    if (m_bStandAlone)
        m_icInstrument.Standby();
    else
        cbSetSystemRunLevel(cbRUNLEVEL_HARDRESET, 0, 0, m_nInstance);
}

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: Close the instrument network
void InstNetwork::Close()
{
    // Signal thread to finish
    m_bDone = true;
    // Wait for thread to finish
    if (QThread::currentThread() != thread())
        wait();
}

// Author & Date: Ehsan Azar       14 Feb 2012
// Purpose: Specific network commands to handle as Qt slots
// Inputs:
//  cmd      - network command to perform
//  code     - additional argument for the command
void InstNetwork::OnNetCommand(NetCommandType cmd, unsigned int /*code*/)
{
    switch (cmd)
    {
    case NET_COMMAND_NONE:
        // Do nothing
        break;
    case NET_COMMAND_OPEN:
        start(QThread::HighPriority);
        break;
    case NET_COMMAND_CLOSE:
        Close();
        break;
    case NET_COMMAND_STANDBY:
        StandBy();
        break;
    case NET_COMMAND_SHUTDOWN:
        ShutDown();
        break;
    }
}

// Author & Date: Ehsan Azar       24 June 2010
// Purpose: Some packets coming from the stand-alone instrument network need
//           to be processed for any listener application.
//           then ask the network listener for more process.
// Inputs:
//  pPkt      - pointer to the packet
void InstNetwork::ProcessIncomingPacket(const cbPKT_GENERIC * const pPkt)
{
    // -------- Process some incoming packet here -----------
    // check for configuration class packets
    if (pPkt->chid & 0x8000)
    {
        // Check for configuration packets
        if (pPkt->chid == 0x8000)
        {
            if ((pPkt->type & 0xF0) == cbPKTTYPE_CHANREP)
            {
                if (m_bStandAlone)
                {
                    const cbPKT_CHANINFO * pNew = reinterpret_cast<const cbPKT_CHANINFO *>(pPkt);
                    uint32_t chan = pNew->chan;
                    if (chan > 0 && chan <= cbMAXCHANS)
                    {
                        memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->chaninfo[chan - 1]), pPkt, sizeof(cbPKT_CHANINFO));
                        // Invalidate the cache
                        if ((pPkt->type == cbPKTTYPE_CHANREP) && (m_ChannelType[chan - 1] == cbCHANTYPE_ANAIN))
                            cb_spk_buffer_ptr[m_nIdx]->cache[chan - 1].valid = 0;
                    }
                }
            }
            else if ((pPkt->type & 0xF0) == cbPKTTYPE_SYSREP)
            {
                const cbPKT_SYSINFO * pNew = reinterpret_cast<const cbPKT_SYSINFO *>(pPkt);
                if (m_bStandAlone)
                {
                    cbPKT_SYSINFO & rOld = cb_cfg_buffer_ptr[m_nIdx]->sysinfo;
                    // replace our copy with this one
                    rOld = *pNew;
                }
                // Rely on the fact that sysrep must be the last config packet sent via NSP6.04 and upwards
                if (pPkt->type == cbPKTTYPE_SYSREP)
                {
                    // Any change to the instrument will be reported here, including initial connection as stand-alone
                    uint32_t instInfo;
                    cbGetInstInfo(&instInfo, m_nInstance);
                    // If instrument connection state has changed
                    if (instInfo != m_instInfo)
                    {
                        m_instInfo = instInfo;
                        InstNetworkEvent(NET_EVENT_INSTINFO, instInfo);
                    }
                }
                else if (pPkt->type == cbPKTTYPE_SYSREPRUNLEV)
                {
                    if (pNew->runlevel == cbRUNLEVEL_HARDRESET)
                    {
                        // If any app did a hard reset which is not the initial reset
                        //  Application should decide what to do on reset
                        if (!m_bStandAlone || (m_bStandAlone && pPkt->time > 500))
                            InstNetworkEvent(NET_EVENT_RESET);
                    }
                    else if (pNew->runlevel == cbRUNLEVEL_RUNNING)
                    {
                        if (pNew->runflags & cbRUNFLAGS_LOCK)
                        {
                            InstNetworkEvent(NET_EVENT_LOCKEDRESET);
                        }
                    }
                }
            }
            else if (pPkt->type == cbPKTTYPE_GROUPREP)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->groupinfo[0][((cbPKT_GROUPINFO*)pPkt)->group-1]), pPkt, sizeof(cbPKT_GROUPINFO));
            }
            else if (pPkt->type == cbPKTTYPE_FILTREP)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->filtinfo[0][((cbPKT_FILTINFO*)pPkt)->filt-1]), pPkt, sizeof(cbPKT_FILTINFO));
            }
            else if (pPkt->type == cbPKTTYPE_PROCREP)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->procinfo[0]), pPkt, sizeof(cbPKT_PROCINFO));
            }
            else if (pPkt->type == cbPKTTYPE_BANKREP)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->bankinfo[0][((cbPKT_BANKINFO*)pPkt)->bank-1]), pPkt, sizeof(cbPKT_BANKINFO));
            }
            else if (pPkt->type == cbPKTTYPE_ADAPTFILTREP)
            {
                if (m_bStandAlone)
                    cb_cfg_buffer_ptr[m_nIdx]->adaptinfo = *reinterpret_cast<const cbPKT_ADAPTFILTINFO *>(pPkt);
            }
            else if (pPkt->type == cbPKTTYPE_REFELECFILTREP)
            {
                if (m_bStandAlone)
                    cb_cfg_buffer_ptr[m_nIdx]->refelecinfo = *reinterpret_cast<const cbPKT_REFELECFILTINFO *>(pPkt);
            }
            else if (pPkt->type == cbPKTTYPE_SS_MODELREP)
            {
                if (m_bStandAlone)
                {
                    cbPKT_SS_MODELSET rNew = *reinterpret_cast<const cbPKT_SS_MODELSET*>(pPkt);
                    UpdateSortModel(rNew);
                }
            }
            else if (pPkt->type == cbPKTTYPE_SS_STATUSREP)
            {
                if (m_bStandAlone)
                {
                    cbPKT_SS_STATUS rNew = *reinterpret_cast<const cbPKT_SS_STATUS*>(pPkt);
                    cbPKT_SS_STATUS & rOld = cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.pktStatus;
                    rOld = rNew;
                }
            }
            else if (pPkt->type == cbPKTTYPE_SS_DETECTREP)
            {
                if (m_bStandAlone)
                {
                    // replace our copy with this one
                    cbPKT_SS_DETECT rNew = *reinterpret_cast<const cbPKT_SS_DETECT*>(pPkt);
                    cbPKT_SS_DETECT & rOld = cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.pktDetect;
                    rOld = rNew;
                }
            }
            else if (pPkt->type == cbPKTTYPE_SS_ARTIF_REJECTREP)
            {
                if (m_bStandAlone)
                {
                    // replace our copy with this one
                    cbPKT_SS_ARTIF_REJECT rNew = *reinterpret_cast<const cbPKT_SS_ARTIF_REJECT*>(pPkt);
                    cbPKT_SS_ARTIF_REJECT & rOld = cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.pktArtifReject;
                    rOld = rNew;
                }
            }
            else if (pPkt->type == cbPKTTYPE_SS_NOISE_BOUNDARYREP)
            {
                if (m_bStandAlone)
                {
                    // replace our copy with this one
                    cbPKT_SS_NOISE_BOUNDARY rNew = *reinterpret_cast<const cbPKT_SS_NOISE_BOUNDARY*>(pPkt);
                    cbPKT_SS_NOISE_BOUNDARY & rOld = cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.pktNoiseBoundary[rNew.chan - 1];
                    rOld = rNew;
                }
            }
            else if (pPkt->type == cbPKTTYPE_SS_STATISTICSREP)
            {
                if (m_bStandAlone)
                {
                    // replace our copy with this one
                    cbPKT_SS_STATISTICS rNew = *reinterpret_cast<const cbPKT_SS_STATISTICS*>(pPkt);
                    cbPKT_SS_STATISTICS & rOld = cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.pktStatistics;
                    rOld = rNew;
                }
            }
            else if (pPkt->type == cbPKTTYPE_FS_BASISREP)
            {
                if (m_bStandAlone)
                {
                    cbPKT_FS_BASIS rPkt = *reinterpret_cast<const cbPKT_FS_BASIS*>(pPkt);
                    UpdateBasisModel(rPkt);
                }
            }
            else if (pPkt->type == cbPKTTYPE_LNCREP)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->isLnc), pPkt, sizeof(cbPKT_LNC));
                // For 6.03 and before, use this packet instead of sysrep for instinfo event
            }
            else if (pPkt->type == cbPKTTYPE_REPFILECFG)
            {
                if (m_bStandAlone)
                {
                    const cbPKT_FILECFG * pPktFileCfg = reinterpret_cast<const cbPKT_FILECFG*>(pPkt);
                    if (pPktFileCfg->options == cbFILECFG_OPT_REC || pPktFileCfg->options == cbFILECFG_OPT_STOP)
                    {
                        cb_cfg_buffer_ptr[m_nIdx]->fileinfo = * reinterpret_cast<const cbPKT_FILECFG *>(pPkt);
                    }
                }
            }
            else if (pPkt->type == cbPKTTYPE_REPNTRODEINFO)
            {
                if (m_bStandAlone)
                    memcpy(&(cb_cfg_buffer_ptr[m_nIdx]->isLnc), pPkt, sizeof(cbPKT_LNC));
            }
            else if (pPkt->type == cbPKTTYPE_NMREP)
            {
                if (m_bStandAlone)
                {
                    const cbPKT_NM * pPktNm = reinterpret_cast<const cbPKT_NM *>(pPkt);
                    // video source to go to the file header
                    if (pPktNm->mode == cbNM_MODE_SETVIDEOSOURCE)
                    {
                        if (pPktNm->flags > 0 && pPktNm->flags <= cbMAXVIDEOSOURCE)
                        {
                            memcpy(cb_cfg_buffer_ptr[m_nIdx]->isVideoSource[pPktNm->flags - 1].name, pPktNm->name, cbLEN_STR_LABEL);
                            cb_cfg_buffer_ptr[m_nIdx]->isVideoSource[pPktNm->flags - 1].fps = ((float)pPktNm->value) / 1000;
                            // fps>0 means valid video source
                        }
                    }
                    // trackable object to go to the file header
                    else if (pPktNm->mode == cbNM_MODE_SETTRACKABLE)
                    {
                        if (pPktNm->flags > 0 && pPktNm->flags <= cbMAXTRACKOBJ)
                        {
                            memcpy(cb_cfg_buffer_ptr[m_nIdx]->isTrackObj[pPktNm->flags - 1].name, pPktNm->name, cbLEN_STR_LABEL);
                            cb_cfg_buffer_ptr[m_nIdx]->isTrackObj[pPktNm->flags - 1].type = (uint16_t)(pPktNm->value & 0xff);
                            cb_cfg_buffer_ptr[m_nIdx]->isTrackObj[pPktNm->flags - 1].pointCount = (uint16_t)((pPktNm->value >> 16) & 0xff);
                            // type>0 means valid trackable
                        }
                    }
                    // nullify all tracking upon NM exit
                    else if (pPktNm->mode == cbNM_MODE_STATUS && pPktNm->value == cbNM_STATUS_EXIT)
                    {
                        memset(cb_cfg_buffer_ptr[m_nIdx]->isTrackObj, 0, sizeof(cb_cfg_buffer_ptr[m_nIdx]->isTrackObj));
                        memset(cb_cfg_buffer_ptr[m_nIdx]->isVideoSource, 0, sizeof(cb_cfg_buffer_ptr[m_nIdx]->isVideoSource));
                    }
                }
            }
            else if (pPkt->type == cbPKTTYPE_WAVEFORMREP)
            {
                if (m_bStandAlone)
                {
                    const cbPKT_AOUT_WAVEFORM * pPktAoutWave = reinterpret_cast<const cbPKT_AOUT_WAVEFORM *>(pPkt);
                    uint16_t nChan = pPktAoutWave->chan;
                    if ((m_ChannelType[nChan-1] == cbCHANTYPE_ANAOUT) || (m_ChannelType[nChan - 1] == cbCHANTYPE_AUDOUT))
                    {
                        uint8_t trigNum = pPktAoutWave->trigNum;
                        if (trigNum < cbMAX_AOUT_TRIGGER)
                            cb_cfg_buffer_ptr[m_nIdx]->isWaveform[nChan][trigNum] = *pPktAoutWave;
                    }
                }
            }
            else if (pPkt->type == cbPKTTYPE_NPLAYREP)
            {
                if (m_bStandAlone)
                {
                    const cbPKT_NPLAY * pNew = reinterpret_cast<const cbPKT_NPLAY *>(pPkt);
                    // Only store the main config packet in stand-alone mode
                    if (pNew->flags == cbNPLAY_FLAG_MAIN)
                    {
                        cbPKT_NPLAY & rOld = cb_cfg_buffer_ptr[m_nIdx]->isNPlay;
                        // replace our copy with this one
                        rOld = *pNew;
                    }
                }
            }
        } // end if (pPkt->chid==0x8000
    } // end if (pPkt->chid & 0x8000
    else if ( (pPkt->chid > 0) && (pPkt->chid < cbPKT_SPKCACHELINECNT) )
    {
        if (m_bStandAlone)
        {
            // post the packet to the cache buffer
            memcpy( &(cb_spk_buffer_ptr[m_nIdx]->cache[pPkt->chid - 1].spkpkt[cb_spk_buffer_ptr[m_nIdx]->cache[pPkt->chid - 1].head]),
                pPkt, (pPkt->dlen + cbPKT_HEADER_32SIZE) * 4);

            // increment the valid pointer
            cb_spk_buffer_ptr[m_nIdx]->cache[pPkt->chid - 1].valid++;

            // increment the head pointer of the packet and check for wraparound
            uint32_t head = cb_spk_buffer_ptr[m_nIdx]->cache[(pPkt->chid)-1].head + 1;
            if (head >= cbPKT_SPKCACHEPKTCNT)
                head = 0;
            cb_spk_buffer_ptr[m_nIdx]->cache[pPkt->chid - 1].head = head;
        }
    }

    // -- Process the incoming packet inside the listeners --
    for (int i = 0; i < m_listener.count(); ++i)
        m_listener[i]->ProcessIncomingPacket(pPkt);
}

// Author & Date:   Kirk Korver     25 Apr 2005
// Purpose: update our sorting model
// Inputs:
//  rUnitModel - the unit model packet of interest
inline void InstNetwork::UpdateSortModel(const cbPKT_SS_MODELSET & rUnitModel)
{
    uint32_t nChan = rUnitModel.chan;
    uint32_t nUnit = rUnitModel.unit_number;

    // Unit 255 == noise, put it into the last slot
    if (nUnit == 255)
        nUnit = ARRAY_SIZE(cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.asSortModel[0]) - 1;

    if (cb_library_initialized[m_nIdx] && cb_cfg_buffer_ptr[m_nIdx])
        cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.asSortModel[nChan][nUnit] = rUnitModel;
}

// Author & Date:   Hyrum L. Sessions   21 Apr 2009
// Purpose: update our PCA basis model
// Inputs:
//  rBasisModel - the basis model packet of interest
inline void InstNetwork::UpdateBasisModel(const cbPKT_FS_BASIS & rBasisModel)
{
    if (0 == rBasisModel.chan)
        return;         // special packet request to get all basis, don't save it

    uint32_t nChan = rBasisModel.chan - 1;

    if (cb_library_initialized[m_nIdx] && cb_cfg_buffer_ptr[m_nIdx])
        cb_cfg_buffer_ptr[m_nIdx]->isSortingOptions.asBasis[nChan] = rBasisModel;
}

/////////////////////////////////////////////////////////////////////////////
// Author & Date:   Kirk Korver     07 Jan 2003
// Purpose: do any processing necessary to test for link failure (i.e. wire disconnected)
// Inputs:
//  nTicks - the ever growing number of times that the mmtimer has been called
//  rParent - the parent window
//  nCurrentPacketCount - the number of packets that we have ever received
inline void InstNetwork::CheckForLinkFailure(uint32_t nTicks, uint32_t nCurrentPacketCount)
{
    if ((nTicks % 250) == 0) // Check every 2.5 seconds
    {
        if (m_nLastNumberOfPacketsReceived != nCurrentPacketCount)
        {
            m_nLastNumberOfPacketsReceived = nCurrentPacketCount;
        }
        else
        {
            InstNetworkEvent(NET_EVENT_LINKFAILURE);
        }
    }
}

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: Networking timer timeout function
// Inputs:
//   event - QT timer event for networking
void InstNetwork::timerEvent(QTimerEvent * /*event*/)
{
    m_timerTicks++; // number of intervals
    int burstcount = 0;
    int recv_returned = 0;
    if (m_bDone)
    {
        if (m_timerId)
        {
            killTimer(m_timerId);
            m_timerId = 0;
        }
        quit();
        return;
    }
    /////////////////////////////////////////
    // below 5 seconds, call startup routines
    if (m_timerTicks < 500)
    {
        // at time 0 request sysinfo
        if (m_timerTicks == 1)
        {
            InstNetworkEvent(NET_EVENT_INSTCONNECTING);
            cbSetSystemRunLevel(cbRUNLEVEL_RUNNING, 0, 0, m_nInstance);
        }
        // at 0.5 seconds, reset the hardware
        else if (m_timerTicks == 50)
        {
            // get runlevel
            cbGetSystemRunLevel(&m_runlevel, NULL, NULL, m_nInstance);
            // if not running reset
            if (cbRUNLEVEL_RUNNING != m_runlevel)
            {
                InstNetworkEvent(NET_EVENT_INSTHARDRESET);
                cbSetSystemRunLevel(cbRUNLEVEL_HARDRESET, 0, 0, m_nInstance);
            }
        }
        // at 1.0 seconds, retreive the hardware config
        else if (m_timerTicks == 100)
        {
            InstNetworkEvent(NET_EVENT_INSTCONFIG);
            cbPKT_GENERIC pktgeneric;
            pktgeneric.time = 1;
            pktgeneric.chid = 0x8000;
            pktgeneric.type = cbPKTTYPE_REQCONFIGALL;
            pktgeneric.dlen = 0;
            cbSendPacket(&pktgeneric, m_nInstance);
        }
        // at 2.0 seconds, start running
        else if (m_timerTicks == 200)
        {
            InstNetworkEvent(NET_EVENT_INSTRUN); // going to soft reset and run
            // if already running do not reset, otherwise reset
            if (cbRUNLEVEL_RUNNING != m_runlevel)
            {
                cbSetSystemRunLevel(cbRUNLEVEL_RESET, 0, 0, m_nInstance);
            }
        }
    } // end if (m_timerTicks < 500
    if (m_icInstrument.Tick())
    {
        InstNetworkEvent(NET_EVENT_PCTONSPLOST);
        m_bDone = true;
    }

    // Check for link failure because we always have heartbeat packets
    if (!(m_instInfo & cbINSTINFO_NPLAY))
        CheckForLinkFailure(m_timerTicks, cb_rec_buffer_ptr[m_nIdx]->received);

    // Process 1024 remaining packets
    while (burstcount < 1024)
    {
        bool bLoopbackPacket = false;
        burstcount++;
        recv_returned = m_icInstrument.Recv(&(cb_rec_buffer_ptr[m_nIdx]->buffer[cb_rec_buffer_ptr[m_nIdx]->headindex]));
        if (recv_returned <= 0)
        {
            // If the real instrument doesn't work, then try the fake one
            recv_returned = m_icInstrument.Recv(&(cb_rec_buffer_ptr[m_nIdx]->buffer[cb_rec_buffer_ptr[m_nIdx]->headindex]));
            if (recv_returned <= 0)
                break; // No data returned
            bLoopbackPacket = true;
        }

        // get pointer to the first packet in received data block
        cbPKT_GENERIC *pktptr = (cbPKT_GENERIC*) &(cb_rec_buffer_ptr[m_nIdx]->buffer[cb_rec_buffer_ptr[m_nIdx]->headindex]);

        uint32_t bytes_to_process = recv_returned;
        do {
            if (bLoopbackPacket)
            {
                // Put fake packets in-order
                pktptr->time = cb_rec_buffer_ptr[m_nIdx]->lasttime;
            } else {
                ++m_nRecentPacketCount; // only count the "real" packets, not loopback ones
                m_icInstrument.TestForReply(pktptr); // loopbacks won't need a "reply"...they are never sent
            }

            // make sure that the next packet in the data block that we are processing fits.
            uint32_t quadlettotal = (pktptr->dlen) + 2;
            uint32_t packetsize = quadlettotal << 2;
            if (packetsize > bytes_to_process)
            {
                // TODO: complain about bad packet
                break;
            }
            // update time index
            cb_rec_buffer_ptr[m_nIdx]->lasttime = pktptr->time;
            // Do incoming packet process
            ProcessIncomingPacket(pktptr);

            // increment packet pointer and subract out the packetsize from the processing counter
            pktptr = (cbPKT_GENERIC*) (((BYTE*) pktptr) + packetsize);
            bytes_to_process -= packetsize;

            // Increment head index and check for buffer wraparound.
            // If the currently processed packet extends at all within the last 1k of the circular
            // recording buffer, wrap the head pointer around to zero.  This is the same mechanism
            // that the client applications that read the buffer use to update their tail pointers.
            cb_rec_buffer_ptr[m_nIdx]->headindex += quadlettotal;
            if ((cb_rec_buffer_ptr[m_nIdx]->headindex) > (cbRECBUFFLEN - (cbCER_UDP_SIZE_MAX / 4)))
            {
                // rewind the circular buffer head pointer and increment the headwrap count
                cb_rec_buffer_ptr[m_nIdx]->headwrap++;
                cb_rec_buffer_ptr[m_nIdx]->headindex = 0;

                // Since multiple Cerebus packets can be contained within a single UDP packet,
                // a few Cerebus packets may be extended beyond the bound of the currently
                // processed packet.  These need to be copied (wrapped) around to the beginning
                // of the circular buffer so that they are not dropped.
                if (bytes_to_process > 0)
                {
                    // copy the remaining packet bytes
                    memcpy(&(cb_rec_buffer_ptr[m_nIdx]->buffer[0]), pktptr,
                            bytes_to_process);

                    // wrap the internal packet pointer
                    pktptr = (cbPKT_GENERIC*) &(cb_rec_buffer_ptr[m_nIdx]->buffer[0]);
                }
            }

            // increment the packets received and data exchanged counters
            cb_rec_buffer_ptr[m_nIdx]->received++;
            m_dataCounter += quadlettotal;

        } while (bytes_to_process); // end do
    } // end while (burstcount
    // check for receive errors
    if (recv_returned < 0)
    {
        // Complain
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // Check for and process outgoing packets
    //
    // This routine roughly executes every 10ms.  In order to prevent the NSP from being overloaded by
    // floods of configuration packets, throttle the configuration packets bursts per 10ms.
    {
        // UINT nPacketsLeftInBurst = 16;
        //
        // The line above was in use for a while. It appears that sometimes a packet from the PC->NSP
        // will be dropped. By reducing the possible number of packets that can be sent at a time, we
        // appear to not have this problem any more. The real solution is to ensure that packets are sent
        UINT nPacketsLeftInBurst = 4;

        cbPKT_GENERIC
                *xmtpacket =
                        (cbPKT_GENERIC*) &(cb_xmt_global_buffer_ptr[m_nIdx]->buffer[cb_xmt_global_buffer_ptr[m_nIdx]->tailindex]);

        while ((xmtpacket->time) && (nPacketsLeftInBurst--))
        {
            // find the length of the packet
            uint32_t quadlettotal = (xmtpacket->dlen) + 2;

            if (m_icInstrument.OkToSend() == false)
                continue;

            // transmit the packet
            m_icInstrument.Send(xmtpacket);

            // complete the packet processing by clearing the packet from the xmt buffer
            memset(xmtpacket, 0, quadlettotal << 2);
            cb_xmt_global_buffer_ptr[m_nIdx]->transmitted++;
            cb_xmt_global_buffer_ptr[m_nIdx]->tailindex += quadlettotal;
            m_dataCounter += quadlettotal;

            if (cb_xmt_global_buffer_ptr[m_nIdx]->tailindex
                    > cb_xmt_global_buffer_ptr[m_nIdx]->last_valid_index)
            {
                cb_xmt_global_buffer_ptr[m_nIdx]->tailindex = 0;
            }

            // update the local reference pointer
            xmtpacket = (cbPKT_GENERIC*) &(cb_xmt_global_buffer_ptr[m_nIdx]->buffer[cb_xmt_global_buffer_ptr[m_nIdx]->tailindex]);
        }
    }
    // Signal the other apps that new data is available
#ifdef WIN32
    PulseEvent(cb_sig_event_hnd[m_nIdx]);
#else
    sem_post((sem_t *)cb_sig_event_hnd[m_nIdx]);
#endif
}

// Author & Date: Ehsan Azar       15 March 2010
// Purpose: The thread function
void InstNetwork::run()
{
    // No instrument yet
    m_instInfo = 0;
    // Start initializing instrument network
    InstNetworkEvent(NET_EVENT_INIT);

    if (m_listener.count() == 0)
    {
        // If listener not set
        InstNetworkEvent(NET_EVENT_LISTENERERR);
        return;
    }

    m_nIdx = cb_library_index[m_nInstance];
    
    // Open the cbhwlib library and create the shared objects
    cbRESULT cbRet = cbOpen(FALSE, m_nInstance);
    if (cbRet == cbRESULT_OK)
    {
        m_nIdx = cb_library_index[m_nInstance];
        m_bStandAlone = false;
        InstNetworkEvent(NET_EVENT_NETCLIENT); // Client to the Central application
    } else if (cbRet == cbRESULT_NOCENTRALAPP) { // If Central is not running run as stand alone
        m_bStandAlone = true; // Run stand alone without Central
        // Run as stand-alone application
        cbRet = cbOpen(TRUE, m_nInstance);
        if (cbRet)
        {
            InstNetworkEvent(NET_EVENT_CBERR, cbRet); // report cbRESULT
            return;
        }
        m_nIdx = cb_library_index[m_nInstance];
        InstNetworkEvent(NET_EVENT_NETSTANDALONE); // Stand-alone application
    } else {
        // Report error and quit
        InstNetworkEvent(NET_EVENT_CBERR, cbRet);
        return;
    }

    STARTUP_OPTIONS startupOption = m_nStartupOptionsFlags;
    // If non-stand-alone just being local can be detected at this stage
    //  because the network is not yet running.
    cbGetInstInfo(&m_instInfo, m_nInstance);
    if (m_instInfo & cbINSTINFO_LOCAL)
    {
        // if local instrument detected
        startupOption = OPT_LOCAL; // Override startup option
    }
    // If stand-alone network
    if (m_bStandAlone)
    {
        // Give nPlay and Cereplex more time
        bool bHighLatency = (m_instInfo & (cbINSTINFO_NPLAY | cbINSTINFO_CEREPLEX));
        m_icInstrument.Reset(bHighLatency ? (int)INST_TICK_COUNT : (int)Instrument::TICK_COUNT);
        // Set network connection details
        const QByteArray inIP = m_strInIP.toLatin1();
        const QByteArray outIP = m_strOutIP.toLatin1();
        m_icInstrument.SetNetwork(m_nInPort, m_nOutPort, inIP, outIP);
        // Open UDP
        cbRESULT cbres = m_icInstrument.Open(startupOption, m_bBroadcast, m_bDontRoute, m_bNonBlocking, m_nRecBufSize);
        if (cbres)
        {
            // if we can't open NSP, say so
            InstNetworkEvent(NET_EVENT_NETOPENERR, cbres); // report cbRESULT
            cbClose(m_bStandAlone, m_nInstance); // Close library
            return;
        }
    }

    // Reset counters and initial state
    m_timerTicks = 0;
    m_nRecentPacketCount = 0;
    m_dataCounter = 0;
    m_nLastNumberOfPacketsReceived = 0;
    m_runlevel = cbRUNLEVEL_SHUTDOWN;
    m_bDone = false;

    // Determine the channel type for each channel.
    for (uint32_t ch_ix = 0; ch_ix < cbMAXCHANS; ch_ix++)
    {
        uint32_t chancaps;
        cbRESULT res = cbGetChanCaps(ch_ix + 1, &chancaps, m_nInstance);
        if ((chancaps & cbCHAN_AINP) == cbCHAN_AINP)
            m_ChannelType[ch_ix] = cbCHANTYPE_ANAIN;
        else if ((chancaps & cbCHAN_AOUT) == cbCHAN_AOUT)
            m_ChannelType[ch_ix] = cbCHANTYPE_ANAOUT;
        else if ((chancaps & cbCHAN_DINP) == cbCHAN_DINP)
        {
            uint32_t diginCaps;
            res = cbGetDinpCaps(ch_ix + 1, &diginCaps, m_nInstance);
            if (diginCaps & cbDINP_SERIALMASK)
                m_ChannelType[ch_ix] = cbCHANTYPE_SERIAL;
            else
                m_ChannelType[ch_ix] = cbCHANTYPE_DIGIN;
        }
        else if ((chancaps & cbCHAN_DOUT) == cbCHAN_DOUT)
            m_ChannelType[ch_ix] = cbCHANTYPE_DIGOUT;
    }

    // If stand-alone setup network packet handling timer
    if (m_bStandAlone)
    {
        // Start network packet processing timer later in the message loop
#ifdef WIN32
        timeBeginPeriod(1);
#endif
        m_timerId = startTimer(10);
        // Start the message loop
        exec();
    } else { // else wait for central application data
        // Instrument info for non-stand-alone
        InstNetworkEvent(NET_EVENT_INSTINFO, m_instInfo);
        bool bMonitorThreadMessageWaiting = false; // If message is waiting in non stand-alone mode
        UINT missed_messages = 0;
        // Start the network loop
        while (!m_bDone)
        {
            cbRESULT waitresult = cbWaitforData(m_nInstance);   //hls );
            if (waitresult == cbRESULT_NOCENTRALAPP)
            {
                // No instrument anymore
                m_instInfo = 0;
                InstNetworkEvent(NET_EVENT_NETOPENERR, cbRESULT_NOCENTRALAPP);
                m_bDone = true;
            }
            else if ((waitresult == cbRESULT_OK) || (bMonitorThreadMessageWaiting == false))
            {
                missed_messages = 0;
                bMonitorThreadMessageWaiting = true;
                OnWaitEvent(); // Handle incoming packets
            }
            else if ((missed_messages++) > 25)
                bMonitorThreadMessageWaiting = false;
        }
    }
    // No instrument anymore
    m_instInfo = 0;
    InstNetworkEvent(NET_EVENT_CLOSE);
    msleep(500); // Give apps some to flush their work
    if (m_bStandAlone)
    {
        // Close the Data Socket and Winsock Subsystem
        m_icInstrument.Close();
    }
    // Close the library
    cbClose(m_bStandAlone, m_nInstance);
}

// Author & Date: Ehsan Azar       1 June 2010
// Purpose: Network incoming packet handling in non-stand-alone mode
void InstNetwork::OnWaitEvent()
{
    // Look to see how much there is to process
    uint32_t pktstogo;
    cbCheckforData(m_enLOC, &pktstogo, m_nInstance);

    if (m_enLOC == LOC_CRITICAL)
    {
        cbMakePacketReadingBeginNow(m_nInstance);
        InstNetworkEvent(NET_EVENT_CRITICAL);
        return;
    }
    // Limit how many we can look at
    pktstogo = std::min(pktstogo, MAX_NUM_OF_PACKETS_TO_PROCESS_PER_PASS);

    // process any available packets
    for(UINT p = 0; p < pktstogo; ++p)
    {
        cbPKT_GENERIC *pktptr = cbGetNextPacketPtr(m_nInstance);
        if (pktptr == NULL)
        {
            cbMakePacketReadingBeginNow(m_nInstance);
            InstNetworkEvent(NET_EVENT_CRITICAL);
            break;
        } else {
            ProcessIncomingPacket(pktptr);
        }
    }
}
