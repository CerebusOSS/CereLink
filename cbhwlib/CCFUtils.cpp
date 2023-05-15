// =STS=> CCFUtils.cpp[1691].aa28   open     SMID:29 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003-2008 Cyberkinetics, Inc.
// (c) Copyright 2008-2021 Blackrock Microsystems
//
// $Workfile: CCFUtils.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtils.cpp $
// $Revision: 2 $
// $Date: 4/26/05 2:56p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCFUtils.h"
#include "CCFUtilsBinary.h"
#include "CCFUtilsXml.h"
#include "CCFUtilsConcurrent.h"
#include "cbhwlib.h"
#include "cbHwlibHi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF base constructor
CCFUtils::CCFUtils(bool bSend, bool bThreaded, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance) :
    m_pImpl(NULL),
    m_bSend(bSend),
    m_pCallbackFn(pCallbackFn),
    m_bThreaded(bThreaded),
    m_pCCF(pCCF),
    m_nInternalVersion(0),
    m_nInternalOriginalVersion(0),
    m_nInstance(nInstance),
    m_szFileName(NULL),
    m_bAutoSort(FALSE),
    m_bBinaryOriginal(FALSE)
{
    // Initial copy
    if (pCCF)
    {
        m_pImpl = new ccfXml();
        m_nInternalVersion = m_pImpl->GetInternalVersion();
        // We rely on the fact that last version uses cbCCF
        dynamic_cast<ccfXml *>(m_pImpl)->m_data = *pCCF;
        m_pImpl->m_nInstance = m_nInstance;
    }
}

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF base destructor
CCFUtils::~CCFUtils()
{
    if (m_pImpl)
    {
        delete m_pImpl;
        m_pImpl = NULL;
    }
    m_nInternalVersion = 0;
    m_nInternalOriginalVersion = 0;
}

// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Convert from older version
CCFUtils * CCFUtils::Convert(CCFUtils * pOldConfig)
{
    // Nothing to convert
    return NULL;
}

// Author & Date:   Ehsan Azar     10 April 2012
// Purpose: Send the latest configuration to NSP
ccfResult CCFUtils::SendCCF()
{
    if (m_pImpl == NULL)
        return CCFRESULT_ERR_NULL;
    m_nInternalVersion = m_pImpl->GetInternalVersion();
    cbCCF & data = dynamic_cast<ccfXml *>(m_pImpl)->m_data;
    ccfResult res = CCFRESULT_SUCCESS;

    if (m_pCallbackFn)
        m_pCallbackFn(m_nInstance, res, m_szFileName, CCFSTATE_SEND, 0);

    // Custom digital filters
    for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
    {
        if (data.filtinfo[i].filt)
        {
            data.filtinfo[i].cbpkt_header.type = cbPKTTYPE_FILTSET;
            cbSendPacket(&data.filtinfo[i], m_nInstance);
        }
    }
    // Chaninfo
    int nAinChan = 1;
    int nAoutChan = 1;
    int nDinChan = 1;
    int nSerialChan = 1;
    int nDoutChan = 1;
    uint32_t nChannelNumber = 0;
    for (int i = 0; i < cbMAXCHANS; ++i)
    {
        if (data.isChan[i].chan)
        {
            // this function is supposed to line up channels based on channel capabilities.  It doesn't
            // work with the multiple NSP setup.  TODO look into this at a future time
            nChannelNumber = 0;
            switch (data.isChan[i].chancaps)
            {
            case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_ISOLATED | cbCHAN_AINP:  // FE channels
                nChannelNumber = cbGetExpandedChannelNumber(data.isChan[i].cbpkt_header.instrument + 1, data.isChan[i].chan);
                break;
            case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_AINP:  // Analog input channels
                nChannelNumber = GetAIAnalogInChanNumber(nAinChan++);
                break;
            case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_AOUT:  // Analog & Audio output channels
                nChannelNumber = GetAnalogOrAudioOutChanNumber(nAoutChan++);
                break;
            case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_DINP:  // digital & serial input channels
                if (data.isChan[i].dinpcaps & cbDINP_SERIALMASK)
                {
                    nChannelNumber = GetSerialChanNumber(nSerialChan++);
                }
                else
                {
                    nChannelNumber = GetDiginChanNumber(nDinChan++);
                }
                break;
            case cbCHAN_EXISTS | cbCHAN_CONNECTED | cbCHAN_DOUT:  // digital output channels
                nChannelNumber = GetDigoutChanNumber(nDoutChan++);
                break;
            default:
                nChannelNumber = 0;
            }
            // send it if it's a valid channel number
            if ((0 != nChannelNumber))  // && (data.isChan[i].chan))
            {
                data.isChan[i].chan = nChannelNumber;
                data.isChan[i].cbpkt_header.type = cbPKTTYPE_CHANSET;
                data.isChan[i].cbpkt_header.instrument = data.isChan[i].proc - 1;   // send to the correct instrument
                cbSendPacket(&data.isChan[i], m_nInstance);
            }
        }
    }
    // Sorting
    {
        if (data.isSS_Statistics.cbpkt_header.type)
        {
            data.isSS_Statistics.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSSET;
            cbSendPacket(&data.isSS_Statistics, m_nInstance);
        }
        for (uint32_t nChan = 0; nChan < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans(); ++nChan)
        {
            if (data.isSS_NoiseBoundary[nChan].chan)
            {
                data.isSS_NoiseBoundary[nChan].cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYSET;
                cbSendPacket(&data.isSS_NoiseBoundary[nChan], m_nInstance);
            }
        }
        if (data.isSS_Detect.cbpkt_header.type)
        {
            data.isSS_Detect.cbpkt_header.type  = cbPKTTYPE_SS_DETECTSET;
            cbSendPacket(&data.isSS_Detect, m_nInstance);
        }
        if (data.isSS_ArtifactReject.cbpkt_header.type)
        {
            data.isSS_ArtifactReject.cbpkt_header.type = cbPKTTYPE_SS_ARTIF_REJECTSET;
            cbSendPacket(&data.isSS_ArtifactReject, m_nInstance);
        }
    }
    // Sysinfo
    if (data.isSysInfo.cbpkt_header.type)
    {
        data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
        cbSendPacket(&data.isSysInfo, m_nInstance);
    }
    // LNC
    if (data.isLnc.cbpkt_header.type)
    {
        data.isLnc.cbpkt_header.type = cbPKTTYPE_LNCSET;
        cbSendPacket(&data.isLnc, m_nInstance);
    }
    // Analog output waveforms
    for (uint32_t nChan = 0; nChan < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogoutChans(); ++nChan)
    {
        for (int nTrigger = 0; nTrigger < cbMAX_AOUT_TRIGGER; ++nTrigger)
        {
            if (data.isWaveform[nChan][nTrigger].chan)
            {
                data.isWaveform[nChan][nTrigger].chan = GetAnalogOutChanNumber(nChan + 1);
                data.isWaveform[nChan][nTrigger].cbpkt_header.instrument = cbGetChanInstrument(data.isWaveform[nChan][nTrigger].chan);
                data.isWaveform[nChan][nTrigger].cbpkt_header.type = cbPKTTYPE_WAVEFORMSET;
                cbSendPacket(&data.isWaveform[nChan][nTrigger], m_nInstance);
            }
        }
    }
    // NTrode
    for (int nNTrode = 0; nNTrode < cbMAXNTRODES; ++nNTrode)
    {
        char szNTrodeLabel[cbLEN_STR_LABEL + 1];     // leave space for trailing null
        memset(szNTrodeLabel, 0, sizeof(szNTrodeLabel));
        cbGetNTrodeInfo(nNTrode + 1, szNTrodeLabel, NULL, NULL, NULL, NULL);

        if ((0 != strlen(szNTrodeLabel)) && (cbGetNTrodeInstrument(nNTrode + 1) == data.isNTrodeInfo->cbpkt_header.instrument + 1))
        {
            if (data.isNTrodeInfo[nNTrode].ntrode)
            {
                data.isNTrodeInfo[nNTrode].cbpkt_header.type = cbPKTTYPE_SETNTRODEINFO;
                cbSendPacket(&data.isNTrodeInfo[nNTrode], m_nInstance);
            }
        }
    }
    // Adaptive filter
    if (data.isAdaptInfo.cbpkt_header.type)
    {
        data.isAdaptInfo.cbpkt_header.type = cbPKTTYPE_ADAPTFILTSET;
        cbSendPacket(&data.isAdaptInfo, m_nInstance);
    }
    // if any spike sorting packets were read and the protocol is before the combined firmware,
    // set all the channels to autosorting
    if ((m_nInternalVersion < 8) && !m_bAutoSort)
    {
        cbPKT_SS_STATISTICS isSSStatistics;

        isSSStatistics.set(300, cbAUTOALG_HOOPS, cbAUTOALG_MODE_APPLY,
                 9, 125,
                 0.80f, 0.94f,
                 0.50f, 0.50f, 0.016f,
                 250, 0);
        cbSendPacket(&isSSStatistics, m_nInstance);

    }
    if (data.isSS_Status.cbpkt_header.type)
    {
        data.isSS_Status.cbpkt_header.type = cbPKTTYPE_SS_STATUSSET;
        data.isSS_Status.cntlNumUnits.nMode = ADAPT_NEVER; // Prevent rebuilding spike sorting when loading ccf.
        cbSendPacket(&data.isSS_Status, m_nInstance);
    }

    if (m_pCallbackFn)
        m_pCallbackFn(m_nInstance, res, m_szFileName, CCFSTATE_SEND, 100);
    return res;
}

// Author & Date:   Kirk Korver     22 Sep 2004
// Purpose: Write out a CCF file. Overwrite any other file by this name.
//  Do NOT prompt for the file to write out.
// Inputs:
//  szFileName - the name of the file to write
ccfResult CCFUtils::WriteCCFNoPrompt(LPCSTR szFileName)
{
    m_szFileName = szFileName;
    ccfResult res = CCFRESULT_SUCCESS;
    if (m_bThreaded)
    {
        // Perform a threaded write operation
        ccf::ConWriteCCF(szFileName, m_pCCF, m_pCallbackFn, m_nInstance);
    } else {
        // If not read, use live NSP
        if (!m_pImpl)
        {
            // This should create m_pImpl on success
            res = ReadCCF(); // Read from NSP
            if (res)
                return res;
        }
        if (!m_pImpl)
            return CCFRESULT_ERR_UNKNOWN;
        if (m_pCallbackFn)
            m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_WRITE, 0);
        // Ask the actual implementation to write file
        res = m_pImpl->WriteCCFNoPrompt(szFileName);
        if (m_pCallbackFn)
            m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_WRITE, 100);
    }
    return res;
}

// Author & Date:   Ehsan Azar   10 June 2012
// Purpose: Read a CCF version information alone.
// Inputs:
//   szFileName - the name of the file to read (if NULL uses live config)
ccfResult CCFUtils::ReadVersion(LPCSTR szFileName)
{
    ccfResult res = CCFRESULT_SUCCESS;
    m_szFileName = szFileName;
    if (m_pImpl == NULL)
    {
        m_pImpl = new ccfXml();
        m_pImpl->m_nInstance = m_nInstance;
    }
    if (szFileName)
    {
        // read from file
        res = m_pImpl->ReadVersion(szFileName);
        m_nInternalVersion = m_pImpl->GetInternalVersion();
        m_nInternalOriginalVersion = m_pImpl->GetInternalOriginalVersion();
        m_bBinaryOriginal = m_pImpl->IsBinaryOriginal();
    }
    else
    {
        // read from NSP
        m_nInternalVersion = m_pImpl->GetInternalVersion();
        m_pImpl->m_nInternalOriginalVersion = m_nInternalOriginalVersion = m_nInternalVersion;
        m_bBinaryOriginal = FALSE;
    }

    return res;
}

// Author & Date:   Ehsan Azar   10 April 2012
// Purpose: Read in a CCF file.
//           Check for the correct version of the file and convert if necessary and possible
// Inputs:
//   szFileName - the name of the file to read (if NULL uses live config)
//   bConvert   - if convertion can happen (has no effect if reading from NSP)
ccfResult CCFUtils::ReadCCF(LPCSTR szFileName, bool bConvert)
{
    m_szFileName = szFileName;
    ccfResult res = CCFRESULT_SUCCESS;

    // First read version
    res = ReadVersion(szFileName);

    if (res)
        return res;

    if (szFileName == NULL)
    {
        uint32_t nIdx = cb_library_index[m_nInstance];
        if (!cb_library_initialized[nIdx] || cb_cfg_buffer_ptr[nIdx] == NULL || cb_cfg_buffer_ptr[nIdx]->sysinfo.cbpkt_header.chid == 0)
            res = CCFRESULT_ERR_OFFLINE;
        else
            ReadCCFOfNSP(); // Read from NSP
        return res;
    }

    CCFUtils * pConfig = NULL;

    if (m_bBinaryOriginal)
    {
        pConfig = new CCFUtilsBinary();
    }
    else
    {
        switch (m_pImpl->GetInternalOriginalVersion())
        {
        case 12:
            pConfig = new CCFUtilsXml_v1();
            break;
        default:
            // This means a newer unsupported XML
            break;
        }
    }
    if (pConfig == NULL)
        res = CCFRESULT_ERR_FORMAT;

    // If conversion is needed, check permission
    if (res == CCFRESULT_SUCCESS && !bConvert)
    {
        if (m_nInternalVersion != m_nInternalOriginalVersion || m_bBinaryOriginal)
            res = CCFRESULT_WARN_CONVERT;
    }

    if (res == CCFRESULT_SUCCESS)
    {
        if (m_bThreaded)
        {
            // Perform a threaded read (and optional send) operation
            ccf::ConReadCCF(szFileName, m_bSend, m_pCCF, m_pCallbackFn, m_nInstance);
        }
        else
        {
            if (m_pCallbackFn)
                m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_READ, 0);
            // Ask the right version to read its own data
            res = pConfig->ReadCCF(szFileName, bConvert);
            if (m_pCallbackFn)
                m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_READ, 100);

            // Start the conversion chain
            if (res == CCFRESULT_SUCCESS)
            {
                if (m_pCallbackFn)
                    m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_CONVERT, 0);
                m_pImpl->Convert(pConfig);
                if (m_pCallbackFn)
                    m_pCallbackFn(m_nInstance, res, szFileName, CCFSTATE_CONVERT, 100);
                // Take a copy if needed
                if (m_pCCF)
                    *m_pCCF = dynamic_cast<ccfXml *>(m_pImpl)->m_data;
                // Auto send if asked for
                if (m_bSend)
                    res = SendCCF();
            }
        }
    }
    // Cleanup
    if (pConfig)
        delete pConfig;

    return res;
}

// Author & Date:   Ehsan Azar   12 April 2012
// Purpose: Read latest CCF from NSP
void CCFUtils::ReadCCFOfNSP()
{
    uint32_t nIdx = cb_library_index[m_nInstance];

    cbCCF & data = dynamic_cast<ccfXml *>(m_pImpl)->m_data;
    for (int i = 0; i < cbNUM_DIGITAL_FILTERS; ++i)
        data.filtinfo[i] = cb_cfg_buffer_ptr[nIdx]->filtinfo[0][cbFIRST_DIGITAL_FILTER + i - 1];    // First is 1 based, but index is 0 based
    for (int i = 0; i < cbMAXCHANS; ++i)
        data.isChan[i] = cb_cfg_buffer_ptr[nIdx]->chaninfo[i];
    data.isAdaptInfo = cb_cfg_buffer_ptr[nIdx]->adaptinfo[cbNSP1];
    data.isSS_Detect = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktDetect;
    data.isSS_ArtifactReject = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktArtifReject;
    for (uint32_t i = 0; i < cb_pc_status_buffer_ptr[0]->cbGetNumAnalogChans(); ++i)
        data.isSS_NoiseBoundary[i] = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktNoiseBoundary[i];
    data.isSS_Statistics = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatistics;
    {
        data.isSS_Status = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatus;
        data.isSS_Status.cntlNumUnits.fElapsedMinutes = 99;
        data.isSS_Status.cntlUnitStats.fElapsedMinutes = 99;
    }
    {
        data.isSysInfo = cb_cfg_buffer_ptr[nIdx]->sysinfo;
        // only set spike len and pre trigger len
        data.isSysInfo.cbpkt_header.type = cbPKTTYPE_SYSSETSPKLEN;
    }
    for (int i = 0; i < cbMAXNTRODES; ++i)
        data.isNTrodeInfo[i] = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[i];
    data.isLnc = cb_cfg_buffer_ptr[nIdx]->isLnc[cbNSP1];
    for (int i = 0; i < AOUT_NUM_GAIN_CHANS; ++i)
    {
        for (int j = 0; j < cbMAX_AOUT_TRIGGER; ++j)
        {
            data.isWaveform[i][j] = cb_cfg_buffer_ptr[nIdx]->isWaveform[i][j];
            // Unset triggered state, so that when loading it does not start generating waveform
            data.isWaveform[i][j].active = 0;
        }
    }
    // Take a copy if needed
    if (m_pCCF)
        *m_pCCF = dynamic_cast<ccfXml*>(m_pImpl)->m_data;
}

// Author & Date:   Ehsan Azar   12 April 2012
// Purpose: Get current internal version.
// Outputs:
// Returns version (0 is invalid)
int CCFUtils::GetInternalVersion()
{
    return m_nInternalVersion;
}

// Author & Date:   Ehsan Azar   13 April 2012
// Purpose: Get internal version of original data.
// Outputs:
// Returns version (0 is invalid)
int CCFUtils::GetInternalOriginalVersion()
{
    return m_nInternalOriginalVersion;
}

// Author & Date:   Ehsan Azar   30 Nov 2012
// Purpose: If original file is binary file
BOOL CCFUtils::IsBinaryOriginal()
{
    return m_bBinaryOriginal;
}

