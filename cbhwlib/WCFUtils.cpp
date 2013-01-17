//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: WCFUtils.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/WCFUtils.cpp $
// $Revision: 1 $
// $Date: 9/10/12 4:0p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "WCFUtils.h"
#include "debugmacs.h"
#include "CCFUtilsXmlItems.h"
#include "CCFUtilsXmlItemsGenerate.h"
#include "CCFUtilsXmlItemsParse.h"
#include "../CentralCommon/BmiVersion.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

// Author & Date: Ehsan Azar       10 Sept 2012
// Purpose: WCF base constructor
WCFUtils::WCFUtils(bool bThreaded, UINT32 nInstance) : 
    m_bThreaded(bThreaded), m_nInstance(nInstance)
{
}

// Author & Date: Ehsan Azar       10 Sept 2012
// Purpose: CCF base destructor
WCFUtils::~WCFUtils()
{
}

// Author & Date:   Ehsan Azar     10 Sept 2012
// Purpose: Read and send the given configuration to NSP
// Inputs:
//  szFileName - the name of the file to write
//  channel    - the channel apply waveform agaisnt
int WCFUtils::SendWCF(LPCSTR szFileName, UINT16 channel)
{
    ccfResult res = CCFRESULT_SUCCESS;
    m_szFileName = szFileName;
    QString strFilename = QString(szFileName);
    XmlFile xml(strFilename, true);
    xml.beginGroup("WCF");
    {
        // TODO: check protocol version
        {
            cbPKT_AOUT_WAVEFORM isWaveform[cbMAX_AOUT_TRIGGER];
            memset(isWaveform, 0, sizeof(isWaveform));
            ccf::ReadItem(&xml, isWaveform, cbMAX_AOUT_TRIGGER, "Waveforms/Waveform");
            bool bAnyWave = false;
            for (int i = 0; i < cbMAX_AOUT_TRIGGER; ++i)
            {
                if (isWaveform[i].chan)
                {
                    bAnyWave = true;
                    isWaveform[i].type |= 0x80;
                    // Overwrite the written channel with channel we want to apply waveorm to
                    isWaveform[i].chan = channel;
                    cbSendPacket(&isWaveform[i]);
                }
            }
            if (bAnyWave)
            {
                UINT32 dwOptions;
                UINT32 nMonitoredChan;
                cbRESULT cbres = cbGetAoutOptions(channel, &dwOptions, &nMonitoredChan, NULL, m_nInstance);
                // Also make sure channel is to output waveform
                dwOptions &= ~(cbAOUT_MONITORSMP | cbAOUT_MONITORSPK);
                dwOptions |= cbAOUT_WAVEFORM;
                // Set monitoring option
                cbres = cbSetAoutOptions(channel, dwOptions, nMonitoredChan, 0, m_nInstance);
            }
        }
    }
    xml.endGroup(false); // CCF
    return 0;
}

// Author & Date:   Ehsan Azar     10 Sept 2012
// Purpose: Write out a WCF file. Overwrite any other file by this name.
//  Do NOT prompt for the file to write out.
// Inputs:
//  szFileName - the name of the file to write
//  channel    - the channel read waveform from
int WCFUtils::WriteWCFNoPrompt(LPCSTR szFileName, UINT16 channel)
{
/*
    QString strFilename = QString(szFileName);
    m_szFileName = szFileName;
    QVariantList lst;

    if (channel <= cbFIRST_ANAOUT_CHAN)
        return -1;
    channel -= (cbFIRST_ANAOUT_CHAN + 1); // make it 0-based
    if (channel >= AOUT_NUM_GAIN_CHANS)
        return -1;

    cbPKT_AOUT_WAVEFORM isWaveform[cbMAX_AOUT_TRIGGER];
    UINT32 nIdx = cb_library_index[m_nInstance];
    memcpy(isWaveform, cb_cfg_buffer_ptr[nIdx]->isWaveform[channel], sizeof(isWaveform));
    // Unset triggered state, so that when loading it does not start generating waveform
    for (int i = 0; i < cbMAX_AOUT_TRIGGER; ++i)
        isWaveform[i].active = 0;
    lst += GetCCFXmlItem(isWaveform, cbMAX_AOUT_TRIGGER, "Waveforms/Waveform");
    // get computer name
    char szUsername[256];
#ifdef WIN32
    DWORD cchBuff = sizeof(szUsername);
    GetComputerNameA(szUsername, &cchBuff) ; // get the computer name with NeuroMotive running
#else
    strncpy(szUsername, getenv("HOSTNAME"), sizeof(szUsername));
#endif
    XmlFile xml(strFilename, false);
    xml.beginGroup("WCF", "Version", WCF_XML_VERSION, lst);
    {
        // Session information
        xml.beginGroup("Session");
        {
            xml.beginGroup("Version");
            {
                xml.addGroup("Protocol", "", 0, QString("%1.%2").arg(cbVERSION_MAJOR).arg(cbVERSION_MINOR));
                xml.addGroup("Cerebus", "", 0, QString(BMI_VERSION_STR));
                cbPROCINFO isInfo;
                cbRESULT cbRet = cbGetProcInfo(cbNSP1, &isInfo, m_nInstance);
                if (cbRet == cbRESULT_OK)
                {
                    int nspmajor   = (isInfo.idcode & 0x000000ff);
                    int nspminor   = (isInfo.idcode & 0x0000ff00) >> 8;
                    int nsprelease = (isInfo.idcode & 0x00ff0000) >> 16;
                    int nspbeta    = (isInfo.idcode & 0xff000000) >> 24;
                    QString strBeta = ".";
                    if (nspbeta)
                        strBeta = " Beta ";
                    xml.addGroup("NSP", "", 0, QString("%1.%2.%3" + strBeta + "%4")
                        .arg(nspmajor)
                        .arg(nspminor, 2, 10, QLatin1Char('0'))
                        .arg(nsprelease, 2, 10, QLatin1Char('0'))
                        .arg(nspbeta, 2, 10, QLatin1Char('0')));
                    xml.addGroup("ID", "", 0, QString(isInfo.ident));
                }
            }
            xml.endGroup(); // Version
            xml.addGroup("Author", "", 0, szUsername);
            xml.addGroup("Date", "", 0, QDateTime::currentDateTime());
        }
        xml.endGroup(); // Session
    }
    bool bRes = xml.endGroup(true);
    if (bRes)
        return -1;
*/
    return 0;
}

