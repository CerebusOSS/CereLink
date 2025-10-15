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
#include "../central/BmiVersion.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "pugixml.hpp"
#ifndef WIN32
#include <unistd.h>
#endif
#include "../include/cerelink/cbhwlib.h"

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
    XmlFile xml(m_szFileName, true);
    xml.beginGroup("CCF");
    {
        if (!bConvert)
        {
            // Make sure right version is parsed
            std::string strVersion = xml.attribute("Version");
            if (std::stoi(strVersion) != m_nInternalVersion)
                res = CCFRESULT_WARN_CONVERT;
            if (!res)
            {
                // Also check for protocol version
                xml.beginGroup("Session/Version/Protocol");
                std::any val = xml.value();
                std::string strProtocolVersion;
                if (val.has_value() && val.type() == typeid(std::string)) {
                    strProtocolVersion = std::any_cast<std::string>(val);
                }
                xml.endGroup();
                // Although internal versioning has not changed,
                //  any protocol difference also requires customer permission to proceed
                std::string key = std::to_string(cbVERSION_MAJOR) + "." + std::to_string(cbVERSION_MINOR);
                if (strProtocolVersion != key)
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
    std::string strFilename = std::string(szFileName);
    std::vector<std::any> lst;
    m_szFileName = szFileName;
    // Digital filters
    lst.push_back(GetCCFXmlItem(m_data.filtinfo, cbNUM_DIGITAL_FILTERS, "FilterInfo"));
    // Chaninfo
    lst.push_back(GetCCFXmlItem(m_data.isChan, cbMAXCHANS, "ChanInfo"));
    // Sorting
    {
        lst.push_back(GetCCFXmlItem(m_data.isSS_Statistics, "Sorting/Statistics"));
        lst.push_back(GetCCFXmlItem(m_data.isSS_NoiseBoundary, cbNUM_ANALOG_CHANS, "Sorting/NoiseBoundary"));
        lst.push_back(GetCCFXmlItem(m_data.isSS_Detect, "Sorting/Detect"));
        lst.push_back(GetCCFXmlItem(m_data.isSS_ArtifactReject, "Sorting/ArtifactReject"));
        lst.push_back(GetCCFXmlItem(m_data.isSS_Status, "Sorting/Status"));
    }
    // SysInfo
    lst.push_back(GetCCFXmlItem(m_data.isSysInfo, "SysInfo"));
    // Line Noise Cancellation
    lst.push_back(GetCCFXmlItem(m_data.isLnc, "LNC"));
    // Analog output waveforms
    lst.push_back(GetCCFXmlItem(m_data.isWaveform, AOUT_NUM_GAIN_CHANS, cbMAX_AOUT_TRIGGER, "AnalogOutput/Waveforms", "Waveform"));
    // NTrode
    lst.push_back(GetCCFXmlItem(m_data.isNTrodeInfo, cbMAXNTRODES, "NTrodeInfo"));
    // Adaptive filter
    lst.push_back(GetCCFXmlItem(m_data.isAdaptInfo, "AdaptInfo"));

    // get computer name
    char szUsername[256];
#ifdef WIN32
    DWORD cchBuff = sizeof(szUsername);
    GetComputerNameA(szUsername, &cchBuff) ; // get the computer name with NeuroMotive running
#else
    const char* envHost = getenv("HOSTNAME");
    if (envHost && *envHost) {
        strncpy(szUsername, envHost, sizeof(szUsername)-1);
        szUsername[sizeof(szUsername)-1] = 0;
    } else {
        char hostbuf[256] = {0};
        if (gethostname(hostbuf, sizeof(hostbuf)) == 0 && hostbuf[0]) {
            strncpy(szUsername, hostbuf, sizeof(szUsername)-1);
            szUsername[sizeof(szUsername)-1] = 0;
        } else {
            strncpy(szUsername, "unknown", sizeof(szUsername)-1);
            szUsername[sizeof(szUsername)-1] = 0;
        }
    }
#endif

    XmlFile xml(strFilename, false);
    xml.beginGroup("CCF", "Version", m_nInternalVersion, lst);
    {
        // Session information
        xml.beginGroup("Session");
        {
            xml.beginGroup("Version");
            {
                xml.addGroup("Protocol", "", 0, std::to_string(cbVERSION_MAJOR) + "." + std::to_string(cbVERSION_MINOR));
                xml.addGroup("Cerebus", "", 0, std::string(BMI_VERSION_STR));

                // Optional fallback: attempt to query device only if enabled
#ifdef CCFUTILS_ENABLE_DEVICE_QUERY
                // Fallback: if caller didn't provide processor info attempt to query device
                if (m_procInfo.idcode == 0) {
                    cbPROCINFO isInfo{};
                    if (cbGetProcInfo(cbNSP1, &isInfo, m_nInstance) == cbRESULT_OK) {
                        m_procInfo = isInfo;
                    }
                }
#endif

                int nspmajor   = (m_procInfo.idcode & 0x000000ff);
                int nspminor   = (m_procInfo.idcode & 0x0000ff00) >> 8;
                int nsprelease = (m_procInfo.idcode & 0x00ff0000) >> 16;
                int nspbeta    = (m_procInfo.idcode & 0xff000000) >> 24;
                std::string strBeta = ".";
                if (nspbeta)
                    strBeta = " Beta ";
                std::string minor_str = std::to_string(nspminor);
                minor_str.insert(0, std::max(0, 2 - (int)minor_str.size()), '0');
                std::string release_str = std::to_string(nsprelease);
                release_str.insert(0, std::max(0, 2 - (int)release_str.size()), '0');
                std::string beta_str = std::to_string(nspbeta);
                beta_str.insert(0, std::max(0, 2 - (int)beta_str.size()), '0');
                std::string ver_str = std::to_string(nspmajor) + "." + minor_str + "." + release_str + strBeta + beta_str;
                xml.addGroup("NSP", "", 0, ver_str);
                xml.addGroup("ID", "", 0, std::string(m_procInfo.ident));

                xml.addGroup("Original", "", 0, m_nInternalOriginalVersion);
            }
            xml.endGroup(); // Version
            xml.addGroup("Author", "", 0, std::string(szUsername));

            auto now = std::chrono::system_clock::now();
            std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
            std::tm dateTime{};
#ifdef _WIN32
            localtime_s(&dateTime, &currentTime);
#else
            localtime_r(&currentTime, &dateTime);
#endif
            char dateBuf[64];
            if (std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", &dateTime)) {
                xml.addGroup("Date", "", 0, std::string(dateBuf));
            } else {
                xml.addGroup("Date", "", 0, "UNKNOWN");
            }
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
    std::string fname = szFileName;
    pugi::xml_document doc;

    if (!doc.load_file(fname.c_str())) {
        return CCFRESULT_ERR_OPENFAILEDREAD;
    }

    m_nInternalOriginalVersion = 0;
    auto root = doc.child("CCF");
    if (root) {
        auto version = root.attribute("Version");
        if (version) {
            m_nInternalOriginalVersion = std::stoi(version.value());
        } else {
            res = CCFRESULT_ERR_FORMAT;
        }
    } else {
        res = CCFRESULT_ERR_FORMAT;
    }

    // If not a valid XML ask parent to read version
    if (res)
        res = CCFUtilsBinary::ReadVersion(szFileName);
    return res;
}
