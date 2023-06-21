// =STS=> CCFUtilsXml.cpp[4876].aa03   open     SMID:3 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsXml.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsXml.cpp $
// $Revision: 1 $
// $Date: 4/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCFUtilsXml.h"
#include "CCFUtilsXmlItemsGenerate.h"
#include "CCFUtilsXmlItemsParse.h"
#include "debugmacs.h"
#include "../CentralCommon/BmiVersion.h"
#include <iostream>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF XML v1 constructor
CCFUtilsXml_v1::CCFUtilsXml_v1()
{
    memset(&m_data, 0, sizeof(m_data));
    memset(&m_procInfo, 0, sizeof(m_procInfo));
    // Same internal version because m_data has not changed from binary
    m_nInternalVersion = CCFUTILSBINARY_LASTVERSION + 1;
}


ccfResult CCFUtilsXml_v1::SetProcInfo(const cbPROCINFO& isInfo) {
    m_procInfo = isInfo;
    return ccf::CCFRESULT_SUCCESS;
}


// Author & Date:   Ehsan Azar   12 April 2012
// Purpose: Read in an XML CCF file of this version.
// Inputs:
//   szFileName - the name of the file to read
//   bConvert   - if convertion can happen
ccfResult CCFUtilsXml_v1::ReadCCF(LPCSTR szFileName, bool bConvert)
{
    ccfResult res = CCFRESULT_SUCCESS;
    m_szFileName = szFileName;
    QString strFilename = QString(szFileName);
    XmlFile xml(strFilename, true);
    xml.beginGroup("CCF");
    {
        if (!bConvert)
        {
            // Make sure right version is parsed
            QString strVersion = xml.attribute("Version");
            if (strVersion.toInt() != m_nInternalVersion)
                res = CCFRESULT_WARN_CONVERT;
            if (!res)
            {
                // Also check for protocol version
                xml.beginGroup("Session/Version/Protocol");
                QString strProtocolVersion = xml.value().toString().trimmed();
                xml.endGroup();
                // Although internal versioning has not changed,
                //  any protocol difference also requires customer permission to proceed
                if (strProtocolVersion != QString("%1.%2").arg(cbVERSION_MAJOR).arg(cbVERSION_MINOR))
                    res = CCFRESULT_WARN_VERSION;
            }
        }
        if (!res)
        {
            ccf::ReadItem(&xml, m_data.filtinfo, cbNUM_DIGITAL_FILTERS, "FilterInfo");
            ccf::ReadItem(&xml, m_data.isChan, cbMAXCHANS, "ChanInfo");
            if (!xml.beginGroup("Sorting"))
            {
                ccf::ReadItem(&xml, m_data.isSS_Statistics, "Statistics");
                ccf::ReadItem(&xml, m_data.isSS_NoiseBoundary, cbNUM_ANALOG_CHANS, "NoiseBoundary");
                ccf::ReadItem(&xml, m_data.isSS_Detect, "Detect");
                ccf::ReadItem(&xml, m_data.isSS_ArtifactReject, "ArtifactReject");
                ccf::ReadItem(&xml, m_data.isSS_Status, "Status");
            }
            xml.endGroup(); // Sorting
            ccf::ReadItem(&xml, m_data.isSysInfo, "SysInfo");
            ccf::ReadItem(&xml, m_data.isLnc, "LNC");
            ccf::ReadItem(&xml, m_data.isWaveform, AOUT_NUM_GAIN_CHANS, cbMAX_AOUT_TRIGGER, "AnalogOutput/Waveforms");
            ccf::ReadItem(&xml, m_data.isNTrodeInfo, cbMAXNTRODES, "NTrodeInfo");
            ccf::ReadItem(&xml, m_data.isAdaptInfo, "AdaptInfo");
        } // end if (!res
    }
    xml.endGroup(false); // CCF
    return res;
}

// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Convert from older version
CCFUtils * CCFUtilsXml_v1::Convert(CCFUtils * pOldConfig)
{
    // If it is my own version, great just copy
    CCFUtilsXml_v1 * pConfig_v1 = dynamic_cast<CCFUtilsXml_v1 *>(pOldConfig);
    if (pConfig_v1 != NULL)
    {
        m_data = pConfig_v1->m_data;
        return this;
    }
    // Every version can only convert from its direct parent
    CCFUtilsBinary * pConfig = dynamic_cast<CCFUtilsBinary *>(pOldConfig);
    // If not my direct parent
    if (pConfig == NULL)
    {
        // Ask my parent to convert it (this will trigger a chain of conversions)
        CCFUtils * pOlderConfig = CCFUtilsBinary::Convert(pOldConfig);
        // If my parent canot convert it either, then
        //  it is most likely a new bug (new because previous version was working)
        ASSERT(pOlderConfig);
        // Anything that my parent returns is of its kind
        pConfig = dynamic_cast<CCFUtilsBinary *>(pOlderConfig);
    }
    if (pConfig != NULL)
    {
        // Do the actual conversion here
        Convert(pConfig->m_data);
    }
    // Return my kind to my descendant
    return this;
}

// Author & Date: Ehsan Azar       13 April 2012
// Purpose: Write XML v1 CCF file
// Inputs:
//  szFileName - the name of the file to write
ccfResult CCFUtilsXml_v1::WriteCCFNoPrompt(LPCSTR szFileName)
{
    ccfResult res = CCFRESULT_SUCCESS;
    QString strFilename = QString(szFileName);
    QVariantList lst;
    m_szFileName = szFileName;
    // Digital filters
    lst += GetCCFXmlItem(m_data.filtinfo, cbNUM_DIGITAL_FILTERS, "FilterInfo");
    // Chaninfo
    lst += GetCCFXmlItem(m_data.isChan, cbMAXCHANS, "ChanInfo");
    // Sorting
    {
        lst += GetCCFXmlItem(m_data.isSS_Statistics, "Sorting/Statistics");
        lst += GetCCFXmlItem(m_data.isSS_NoiseBoundary, cbNUM_ANALOG_CHANS, "Sorting/NoiseBoundary");
        lst += GetCCFXmlItem(m_data.isSS_Detect, "Sorting/Detect");
        lst += GetCCFXmlItem(m_data.isSS_ArtifactReject, "Sorting/ArtifactReject");
        lst += GetCCFXmlItem(m_data.isSS_Status, "Sorting/Status");
    }
    // SysInfo
    lst += GetCCFXmlItem(m_data.isSysInfo, "SysInfo");
    // Line Noise Cancellation
    lst += GetCCFXmlItem(m_data.isLnc, "LNC");
    // Analog output waveforms
    lst += GetCCFXmlItem(m_data.isWaveform, AOUT_NUM_GAIN_CHANS, cbMAX_AOUT_TRIGGER, "AnalogOutput/Waveforms", "Waveform");
    // NTrode
    lst += GetCCFXmlItem(m_data.isNTrodeInfo, cbMAXNTRODES, "NTrodeInfo");
    // Adaptive filter
    lst += GetCCFXmlItem(m_data.isAdaptInfo, "AdaptInfo");

    // get computer name
    char szUsername[256];
#ifdef WIN32
    DWORD cchBuff = sizeof(szUsername);
    GetComputerNameA(szUsername, &cchBuff) ; // get the computer name with NeuroMotive running
#else
    strncpy(szUsername, getenv("HOSTNAME"), sizeof(szUsername));
#endif

    XmlFile xml(strFilename, false);
    xml.beginGroup("CCF", "Version", m_nInternalVersion, lst);
    {
        // Session information
        xml.beginGroup("Session");
        {
            xml.beginGroup("Version");
            {
                xml.addGroup("Protocol", "", 0, QString("%1.%2").arg(cbVERSION_MAJOR).arg(cbVERSION_MINOR));
                xml.addGroup("Cerebus", "", 0, QString(BMI_VERSION_STR));

                int nspmajor   = (m_procInfo.idcode & 0x000000ff);
                int nspminor   = (m_procInfo.idcode & 0x0000ff00) >> 8;
                int nsprelease = (m_procInfo.idcode & 0x00ff0000) >> 16;
                int nspbeta    = (m_procInfo.idcode & 0xff000000) >> 24;
                QString strBeta = ".";
                if (nspbeta)
                    strBeta = " Beta ";
                xml.addGroup("NSP", "", 0, QString("%1.%2.%3" + strBeta + "%4")
                    .arg(nspmajor)
                    .arg(nspminor, 2, 10, QLatin1Char('0'))
                    .arg(nsprelease, 2, 10, QLatin1Char('0'))
                    .arg(nspbeta, 2, 10, QLatin1Char('0')));
                xml.addGroup("ID", "", 0, QString(m_procInfo.ident));

                xml.addGroup("Original", "", 0, m_nInternalOriginalVersion);
            }
            xml.endGroup(); // Version
            xml.addGroup("Author", "", 0, QString(szUsername));
            xml.addGroup("Date", "", 0, QDateTime::currentDateTime());
        }
        xml.endGroup(); // Session
    }
    bool bRes = xml.endGroup(true); // CCF
    if (bRes)
        res = CCFRESULT_ERR_OPENFAILEDWRITE;
    return res;
}

// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Convert from older version of data format to my format
void CCFUtilsXml_v1::Convert(ccf::ccfBinaryData & data)
{
    // TODO: This function (v1 data convert) is where should be changed to support binary CCF with 256 channel systems
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        m_data.isChan[i] = *reinterpret_cast<cbPKT_CHANINFO *>(&data.isChan[i]);
    m_data.isAdaptInfo = *reinterpret_cast<cbPKT_ADAPTFILTINFO *>(&data.isAdaptInfo);
    m_data.isSS_Detect = *reinterpret_cast<cbPKT_SS_DETECT *>(&data.isSS_Detect);
    m_data.isSS_ArtifactReject = *reinterpret_cast<cbPKT_SS_ARTIF_REJECT *>(&data.isSS_ArtifactReject);
    for (int i = 0; i < cbNUM_ANALOG_CHANS; ++i)
        m_data.isSS_NoiseBoundary[i] = *reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY *>(&data.isSS_NoiseBoundary[i]);
    m_data.isSS_Statistics = *reinterpret_cast<cbPKT_SS_STATISTICS *>(&data.isSS_Statistics);
    m_data.isSS_Status = *reinterpret_cast<cbPKT_SS_STATUS *>(&data.isSS_Status);
    m_data.isSysInfo = *reinterpret_cast<cbPKT_SYSINFO *>(&data.isSysInfo);
    for (int i = 0; i < cbMAXNTRODES; ++i)
        m_data.isNTrodeInfo[i] = *reinterpret_cast<cbPKT_NTRODEINFO *>(&data.isNTrodeInfo[i]);
}

// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Read XML version of original document (for well-formed XML)
//          e.g.  <CCF Version = "1">
//
//           Note: New XML formats should keep using this function
//
// Outputs:
//  Sets original XML version if valid CCF XML file, or zero fon non-XML
//  Returns error code
ccfResult CCFUtilsXml_v1::ReadVersion(LPCSTR szFileName)
{
    ccfResult res = CCFRESULT_SUCCESS;
    QString fname = szFileName;
    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly))
        return CCFRESULT_ERR_OPENFAILEDREAD;

    m_nInternalOriginalVersion = 0;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd())
    {
        xml.readNext();
        // The very first element must be CCF
        if (xml.isStartElement())
        {
            QXmlStreamAttributes attrs = xml.attributes();
            for (auto attr : attrs) {
                QString attr_name = attr.name().toString();
                QString attr_value = attr.value().toString();
                if (attr_name == "Version")
                    m_nInternalOriginalVersion = attr.value().toString().toInt();
            }
            break;
        }
    }
    // invalid XML
    if (xml.hasError())
        res = CCFRESULT_ERR_FORMAT;
    file.close();
    // If not a valid XML ask parent to read version
    if (res)
        res = CCFUtilsBinary::ReadVersion(szFileName);
    return res;
}

