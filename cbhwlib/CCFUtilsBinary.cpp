// =STS=> CCFUtilsBinary.cpp[4874].aa02   open     SMID:2 
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsBinary.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsBinary.cpp $
// $Revision: 1 $
// $Date: 4/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCFUtilsBinary.h"
#include <stddef.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

const char CCFUtilsBinary::m_szConfigFileHeader[7] = "cbCCF ";
const char CCFUtilsBinary::m_szConfigFileVersion[11] = "1234567890"; // room for 9

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF binary constructor
CCFUtilsBinary::CCFUtilsBinary()
{
    memset(&m_data, 0, sizeof(m_data));
    // This is the internal version of the format we binary is stored
    m_nInternalVersion = CCFUTILSBINARY_LASTVERSION;
}

// Author & Date:   Ehsan Azar   10 June 2012
// Purpose: Read binary CCF file version
// Inputs:
//   szFileName - the name of the file to read
ccfResult CCFUtilsBinary::ReadVersion(LPCSTR szFileName)
{
    FILE * hSettingsFile = 0;
    ccfResult res = CCFRESULT_SUCCESS;
    m_szFileName = szFileName;
    // Open the file
    hSettingsFile = fopen(szFileName, "rb");
    if (hSettingsFile == NULL)
        return CCFRESULT_ERR_OPENFAILEDREAD;

    // Check header and version in file
    // Initializing strings in a way that will set elements to zero
    char szID[sizeof(m_szConfigFileHeader)] = {0};
    char szVersion[sizeof(m_szConfigFileVersion)] = {0};

    fread(&szID, sizeof(szID) - 1, 1, hSettingsFile);               // -1 'cuz of NULL termination
    fread(&szVersion, sizeof(szVersion) - 1, 1, hSettingsFile);     // -1 'cuz of NULL termination

    // check for previous supported configuration files to convert from
    if ((strcmp("cb2003", szID) == 0) && (strncmp("1.0", szVersion, 3) == 0))
        m_nInternalOriginalVersion = 1;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("2.5", szVersion) == 0))
        m_nInternalOriginalVersion = 2;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("3.0", szVersion) == 0))
        m_nInternalOriginalVersion = 3;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("3.1", szVersion) == 0))
        m_nInternalOriginalVersion = 4;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("3.2", szVersion) == 0))
        m_nInternalOriginalVersion = 5;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("3.3", szVersion) == 0))
        m_nInternalOriginalVersion = 6;
    else if ((strcmp("cb2005", szID) == 0) && (strcmp("3.4", szVersion) == 0))
        m_nInternalOriginalVersion = 7;
    else if ((strcmp("cbCCF ", szID) == 0) && (strcmp("3.5", szVersion) == 0))
        m_nInternalOriginalVersion = 8;
    else if ((strcmp("cbCCF ", szID) == 0) && (strcmp("3.6", szVersion) == 0))
        m_nInternalOriginalVersion = 9;
    else if ((strcmp("cbCCF ", szID) == 0) && (strcmp("3.7", szVersion) == 0))
        m_nInternalOriginalVersion = 10;
    else if ((strcmp("cbCCF ", szID) == 0) && (strcmp("3.8", szVersion) == 0))
        m_nInternalOriginalVersion = 11;
    else if ((strcmp("cbCCF ", szID) == 0) && (strcmp("3.9", szVersion) == 0))
        m_nInternalOriginalVersion = 11;
    else
        m_nInternalOriginalVersion = 0;

    fclose(hSettingsFile);

    if (m_nInternalOriginalVersion == 0)
        res = CCFRESULT_ERR_FORMAT;
    else
        m_bBinaryOriginal = TRUE;
    return res;
}

// Author & Date:   Hyrum L. Sessions   10 July 2006
// Purpose: Read in a binary CCF file. Check for the correct version of the file
//          and convert if necessary and possible
// Inputs:
//   szFileName - the name of the file to read
//   bConvert   - if convertion can happen
ccfResult CCFUtilsBinary::ReadCCF(LPCSTR szFileName, bool bConvert)
{
    ccfResult res = CCFRESULT_SUCCESS;
    // First read the version
    res = ReadVersion(szFileName);
    if (res)
        return res;
    m_szFileName = szFileName;
    // Open the file
    FILE * hSettingsFile = fopen(szFileName, "rb");
    if (hSettingsFile == NULL)
        return CCFRESULT_ERR_OPENFAILEDREAD;

    // Check header and version in file
    // Initializing strings in a way that will set elements to zero
    char szID[sizeof(m_szConfigFileHeader)] = {0};
    char szVersion[sizeof(m_szConfigFileVersion)] = {0};

    fread(&szID, sizeof(szID) - 1, 1, hSettingsFile);               // -1 'cuz of NULL termination
    fread(&szVersion, sizeof(szVersion) - 1, 1, hSettingsFile);     // -1 'cuz of NULL termination


    // These versions require conversion to 3.7
    if (m_nInternalOriginalVersion >= 1 && m_nInternalOriginalVersion <= 9)
    {
        if (!bConvert)
        {
            res = CCFRESULT_WARN_CONVERT;
            fclose(hSettingsFile);
            return res;
        }
    }

    // Now read and convert
    switch (m_nInternalOriginalVersion)
    {
    case 0:
        res = CCFRESULT_ERR_FORMAT;
        break;
    case 1:
        res = ReadCCFData_cb2003_10(hSettingsFile);
        break;
    case 2:
        res = ReadCCFData_cb2005_25(hSettingsFile);
        break;
    case 3:
        res = ReadCCFData_cb2005_30(hSettingsFile);
        break;
    case 4:
        res = ReadCCFData_cb2005_31(hSettingsFile);       // Unit override was added
        break;
    case 5:
        res = ReadCCFData_cb2005_31(hSettingsFile);       // chaninfo packets are the same as 3.1, so call it...
        break;
    case 6:
        res = ReadCCFData_cb2005_31(hSettingsFile);       // chaninfo packets are the same as 3.1, so call it...
        break;
    case 7:
        res = ReadCCFData_cb2005_34(hSettingsFile);       // unit override changed to ellipse
        break;
    case 8:
        res = ReadCCFData_cb2005_35(hSettingsFile);       // added lncdispmax & refelecchan
        break;
    case 9:
        res = ReadCCFData_cb2005_36(hSettingsFile);       // manual unit mapping dimension changed to 3
        break;
    case 10:
        res = ReadCCFData_cb2005_37(hSettingsFile);       // manual unit mapping changed from float to int16
        break;
    case 11:
        res = ReadCCFData_cb2005_37(hSettingsFile);       // chaninfo packets are the same as 3.7, so call it...
        break;
    default:
        res = CCFRESULT_ERR_FORMAT;
        break;
    }
    fclose(hSettingsFile);
    return res;
}


// Author & Date: Ehsan Azar       12 April 2012
// Purpose: Convert from older version
CCFUtils * CCFUtilsBinary::Convert(CCFUtils * pOldConfig)
{
    // Nothing to convert
    return NULL;
}

// Author & Date:   Hyrum L. Sessions   9 June 2008
// Purpose: load the default channel information
// Inputs:  isChan - structure to fill with the default information
void CCFUtilsBinary::SetChannelDefaults(cbPKT_CHANINFO_CB2005_37 &isChan)
{
    memset(&isChan, 0, sizeof(isChan));

    isChan.time             = 0;
    isChan.chid             = 0x8000;
    isChan.type             = cbPKTTYPE_CHANSET;
    isChan.dlen             = cbPKTDLEN_CHANINFO_CB2005_37;
    isChan.lncdispmax       = 511;
    isChan.refelecchan      = 0;

    // set unit overrides to default values
    for (int i = 0; i < cbLEGACY_MAXUNITS; ++i)
        isChan.unitmapping[i].nOverride = i;
}

// Author & Date:   Hyrum L. Sessions   11 Jul 2006
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v2.5 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2003_10(FILE * hFile)
{
    // there were 2 different versions of cb2003 v1.0  When anagain was added to the
    // protocol, the version wasn't changed as it should have been.  We can differenciate
    // between the 2 by the file size.  I've called the first version A and the second one B

    // position file pointer to the end of the file and get it's position
    fseek(hFile, 0, SEEK_END);
    long lFileSize = ftell(hFile);

    // position file pointer to just after the header info.  For cb2003 v1.0, the
    // version string was only 3 chars wide
    fseek(hFile, sizeof(m_szConfigFileHeader) - 1 + 3, SEEK_SET);

    // check for "A" type file based on size
    if ((lFileSize - m_iPreviousHeaderSize - sizeof(cbPKT_ADAPTFILTINFO) == sizeof(cbPKT_CHANINFO_CB2003_10A) * cbLEGACY_MAXCHANS) ||
        (lFileSize - m_iPreviousHeaderSize == sizeof(cbPKT_CHANINFO_CB2003_10A) * cbLEGACY_MAXCHANS))
        return ReadCCFData_cb2003_10_a(hFile);
    // check for "B" type file based on size -- it is the same chaninfo as cb2005 3.0
    else if (lFileSize - m_iPreviousHeaderSize - sizeof(cbPKT_ADAPTFILTINFO) == sizeof(cbPKT_CHANINFO_CB2005_30) * cbLEGACY_MAXCHANS)
        return ReadCCFData_cb2003_10_b(hFile);
    else
        return CCFRESULT_ERR_FORMAT;
}


// Author & Date:   Hyrum L. Sessions   11 Jul 2006
// Purpose: load the channel configuration from the file
//          convert the data from cb2003 v1.0 type A to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2003_10_a(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2003_10A isChanFile;

    // Read configuration
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            cpyScaling(isChan.physcalin, isChanFile.physcalin);
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            cpyScaling(isChan.physcalout, isChanFile.physcalout);
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            cpyScaling(isChan.scalin, isChanFile.scalin);
            cpyScaling(isChan.scalout, isChanFile.scalout);
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            isChan.spkgroup    = isChanFile.spkgroup;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   11 Jul 2006
// Purpose: load the channel configuration from the file
//          convert the data from cb2003 v1.0 type B to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2003_10_b(FILE * hFile)
{
    // data is the same as cb2005 3.0, so just load it...
    return ReadCCFData_cb2005_30(hFile);
}


// Author & Date:   Hyrum L. Sessions   11 Jul 2006
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v2.5 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_25(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_25 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            cpyScaling(isChan.physcalin, isChanFile.physcalin);
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            cpyScaling(isChan.physcalout, isChanFile.physcalout);
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            cpyScaling(isChan.scalin, isChanFile.scalin);
            cpyScaling(isChan.scalout, isChanFile.scalout);
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   15 Sept 2006
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v3.0 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_30(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_30 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            memcpy(&isChan.physcalin, &isChanFile.physcalin, sizeof(isChan.physcalin));
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            memcpy(&isChan.physcalout, &isChanFile.physcalout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            memcpy(&isChan.scalin, &isChanFile.scalin, sizeof(isChan.scalin));
            memcpy(&isChan.scalout, &isChanFile.scalout, sizeof(isChan.scalout));
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   4 Jun 2008
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v3.1 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_31(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_31 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            memcpy(&isChan.physcalin, &isChanFile.physcalin, sizeof(isChan.physcalin));
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            memcpy(&isChan.physcalout, &isChanFile.physcalout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            memcpy(&isChan.scalin, &isChanFile.scalin, sizeof(isChan.scalin));
            memcpy(&isChan.scalout, &isChanFile.scalout, sizeof(isChan.scalout));
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));

            for (int unit = 0; unit < cbLEGACY_MAXUNITS; ++unit)
            {
                float radius;
                isChan.unitmapping[unit].nOverride   = isChanFile.unitmapping[unit].nOverride;
                isChan.unitmapping[unit].afOrigin[0] = isChanFile.unitmapping[unit].xOrigin;
                isChan.unitmapping[unit].afOrigin[1] = isChanFile.unitmapping[unit].yOrigin;
                isChan.unitmapping[unit].afOrigin[2] = 0;
                radius = float(isChanFile.unitmapping[unit].radius);
                if (radius != 0)
                {
                    isChan.unitmapping[unit].bValid = 1;
                    isChan.unitmapping[unit].afShape[0][0] = INT16(1 / (radius * radius));
                    isChan.unitmapping[unit].afShape[1][1] = INT16(1 / (radius * radius));
                    isChan.unitmapping[unit].afShape[2][2] = INT16(1 / (radius * radius));
                }
            }
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   4 Jun 2008
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v3.4 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_34(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_34 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            memcpy(&isChan.physcalin, &isChanFile.physcalin, sizeof(isChan.physcalin));
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            memcpy(&isChan.physcalout, &isChanFile.physcalout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            memcpy(&isChan.scalin, &isChanFile.scalin, sizeof(isChan.scalin));
            memcpy(&isChan.scalout, &isChanFile.scalout, sizeof(isChan.scalout));
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));

            for (int unit = 0; unit < cbLEGACY_MAXUNITS; ++unit)
            {
                isChan.unitmapping[unit].nOverride   = isChanFile.unitmapping[unit].nOverride;
                isChan.unitmapping[unit].afOrigin[0] = INT16(isChanFile.unitmapping[unit].afOrigin[0]);
                isChan.unitmapping[unit].afOrigin[1] = INT16(isChanFile.unitmapping[unit].afOrigin[1]);
                isChan.unitmapping[unit].afOrigin[2] = 0;
                isChan.unitmapping[unit].afShape[0][0] = INT16(isChanFile.unitmapping[unit].afShape[0][0]);
                isChan.unitmapping[unit].afShape[0][1] = INT16(isChanFile.unitmapping[unit].afShape[0][1]);
                isChan.unitmapping[unit].afShape[1][0] = INT16(isChanFile.unitmapping[unit].afShape[1][0]);
                isChan.unitmapping[unit].afShape[1][1] = INT16(isChanFile.unitmapping[unit].afShape[1][1]);
                isChan.unitmapping[unit].afShape[2][0] = INT16(isChanFile.unitmapping[unit].afShape[1][0]);
                isChan.unitmapping[unit].afShape[2][1] = INT16(isChanFile.unitmapping[unit].afShape[1][1]);
                isChan.unitmapping[unit].aPhi        = INT16(isChanFile.unitmapping[unit].aPhi);
                isChan.unitmapping[unit].bValid      = INT16(isChanFile.unitmapping[unit].bValid);
            }
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   13 Nov 2009
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v3.5 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_35(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_35 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            memcpy(&isChan.physcalin, &isChanFile.physcalin, sizeof(isChan.physcalin));
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            memcpy(&isChan.physcalout, &isChanFile.physcalout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            memcpy(&isChan.scalin, &isChanFile.scalin, sizeof(isChan.scalin));
            memcpy(&isChan.scalout, &isChanFile.scalout, sizeof(isChan.scalout));
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.lncmode;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));

            for (int unit = 0; unit < cbLEGACY_MAXUNITS; ++unit)
            {
                isChan.unitmapping[unit].nOverride   = isChanFile.unitmapping[unit].nOverride;
                isChan.unitmapping[unit].afOrigin[0] = INT16(isChanFile.unitmapping[unit].afOrigin[0]);
                isChan.unitmapping[unit].afOrigin[1] = INT16(isChanFile.unitmapping[unit].afOrigin[1]);
                isChan.unitmapping[unit].afOrigin[2] = 0;
                isChan.unitmapping[unit].afShape[0][0] = INT16(isChanFile.unitmapping[unit].afShape[0][0]);
                isChan.unitmapping[unit].afShape[0][1] = INT16(isChanFile.unitmapping[unit].afShape[0][1]);
                isChan.unitmapping[unit].afShape[1][0] = INT16(isChanFile.unitmapping[unit].afShape[1][0]);
                isChan.unitmapping[unit].afShape[1][1] = INT16(isChanFile.unitmapping[unit].afShape[1][1]);
                isChan.unitmapping[unit].afShape[2][0] = INT16(isChanFile.unitmapping[unit].afShape[1][0]);
                isChan.unitmapping[unit].afShape[2][1] = INT16(isChanFile.unitmapping[unit].afShape[1][1]);
                isChan.unitmapping[unit].aPhi        = INT16(isChanFile.unitmapping[unit].aPhi);
                isChan.unitmapping[unit].bValid      = INT16(isChanFile.unitmapping[unit].bValid);
            }
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    if (m_bAutoSort)
    {
        for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
        {
            cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
            if (isChan.chan)
            {
                isChan.smpfilter   = TranslateAutoFilter(isChanFile.smpfilter);
                isChan.spkfilter   = 2;
            }
        }
    }
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   29 Nov 2010
// Purpose: load the channel configuration from the file
//          convert the data from cb2005 v3.6 to the current
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_36(FILE * hFile)
{
    // Copy data into buffer
    cbPKT_CHANINFO_CB2005_36 isChanFile;

    // Read configuration to Cerebus
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChan = m_data.isChan[i];
        SetChannelDefaults(isChan);
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);

        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // copy the data from the structure read from the file to the current chaninfo structure
            isChan.chan         = isChanFile.chan;
            isChan.proc         = isChanFile.proc;
            isChan.bank         = isChanFile.bank;
            isChan.term         = isChanFile.term;
            isChan.chancaps     = isChanFile.chancaps;
            isChan.doutcaps     = isChanFile.doutcaps;
            isChan.dinpcaps     = isChanFile.dinpcaps;
            isChan.aoutcaps     = isChanFile.aoutcaps;
            isChan.ainpcaps     = isChanFile.ainpcaps;
            isChan.spkcaps      = isChanFile.spkcaps;
            memcpy(&isChan.physcalin, &isChanFile.physcalin, sizeof(isChan.physcalin));
            memcpy(&isChan.phyfiltin, &isChanFile.phyfiltin, sizeof(isChan.phyfiltin));
            memcpy(&isChan.physcalout, &isChanFile.physcalout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.phyfiltout, &isChanFile.phyfiltout, sizeof(isChan.phyfiltout));
            memcpy(&isChan.label, &isChanFile.label, sizeof(isChan.label));
            isChan.userflags    = isChanFile.userflags;
            memcpy(isChan.position, isChanFile.position, sizeof(isChan.position));
            memcpy(&isChan.scalin, &isChanFile.scalin, sizeof(isChan.scalin));
            memcpy(&isChan.scalout, &isChanFile.scalout, sizeof(isChan.scalout));
            isChan.doutopts    = isChanFile.doutopts;
            isChan.dinpopts    = isChanFile.dinpopts;
            isChan.aoutopts    = isChanFile.aoutopts;
            isChan.eopchar     = isChanFile.eopchar;
            isChan.monsource   = isChanFile.monsource;
            isChan.outvalue    = isChanFile.outvalue;
            isChan.ainpopts    = isChanFile.ainpopts;
            isChan.lncrate     = isChanFile.lncrate;
            isChan.smpfilter   = isChanFile.smpfilter;
            isChan.smpgroup    = isChanFile.smpgroup;
            isChan.smpdispmin  = isChanFile.smpdispmin;
            isChan.smpdispmax  = isChanFile.smpdispmax;
            isChan.spkfilter   = isChanFile.spkfilter;
            isChan.spkdispmax  = isChanFile.spkdispmax;
            isChan.spkopts     = isChanFile.spkopts;
            isChan.spkthrlevel = isChanFile.spkthrlevel;
            isChan.spkthrlimit = isChanFile.spkthrlimit;
            memcpy(isChan.spkhoops, isChanFile.spkhoops, sizeof(isChan.spkhoops));

            for (int unit = 0; unit < cbLEGACY_MAXUNITS; ++unit)
            {
                isChan.unitmapping[unit].nOverride   = isChanFile.unitmapping[unit].nOverride;
                isChan.unitmapping[unit].afOrigin[0] = INT16(isChanFile.unitmapping[unit].afOrigin[0]);
                isChan.unitmapping[unit].afOrigin[1] = INT16(isChanFile.unitmapping[unit].afOrigin[1]);
                isChan.unitmapping[unit].afOrigin[2] = INT16(isChanFile.unitmapping[unit].afOrigin[2]);
                isChan.unitmapping[unit].afShape[0][0] = INT16(isChanFile.unitmapping[unit].afShape[0][0]);
                isChan.unitmapping[unit].afShape[0][1] = INT16(isChanFile.unitmapping[unit].afShape[0][1]);
                isChan.unitmapping[unit].afShape[0][2] = INT16(isChanFile.unitmapping[unit].afShape[0][2]);
                isChan.unitmapping[unit].afShape[1][0] = INT16(isChanFile.unitmapping[unit].afShape[1][0]);
                isChan.unitmapping[unit].afShape[1][1] = INT16(isChanFile.unitmapping[unit].afShape[1][1]);
                isChan.unitmapping[unit].afShape[1][2] = INT16(isChanFile.unitmapping[unit].afShape[1][2]);
                isChan.unitmapping[unit].afShape[2][0] = INT16(isChanFile.unitmapping[unit].afShape[2][0]);
                isChan.unitmapping[unit].afShape[2][1] = INT16(isChanFile.unitmapping[unit].afShape[2][1]);
                isChan.unitmapping[unit].afShape[2][2] = INT16(isChanFile.unitmapping[unit].afShape[2][2]);
                isChan.unitmapping[unit].aPhi        = INT16(isChanFile.unitmapping[unit].aPhi);
                isChan.unitmapping[unit].bValid      = INT16(isChanFile.unitmapping[unit].bValid);
            }
        }
    }
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    return CCFRESULT_SUCCESS;
}


// Author & Date:   Hyrum L. Sessions   25 Aug 2011
// Purpose: load the channel configuration from the file
//          v3.7 is the last binary CCF
// Inputs:  hFile - the file, positioned at the beginning of the channel data
ccfResult CCFUtilsBinary::ReadCCFData_cb2005_37(FILE * hFile)
{
    // Copy data into buffer
    for (int i = 0; i < cbLEGACY_MAXCHANS; ++i)
    {
        cbPKT_CHANINFO_CB2005_37 & isChanFile = m_data.isChan[i];
        memset(&isChanFile, 0, sizeof(isChanFile));
        fread(&isChanFile, 1, sizeof(isChanFile), hFile);
        // We only need to read packets for valid channels. There could be an
        // error reading the file, or we could be past the end of valid chans
        if (isChanFile.chan != 0)
        {
            // We might as well use this RAM to set the packet types.
            //  The space is already allocated, and won't be used later
            isChanFile.chid = 0x8000;
            isChanFile.type = cbPKTTYPE_CHANSET;
            isChanFile.dlen = cbPKTDLEN_CHANINFO_CB2005_37;
        }
    } // end for(int i = 0
    // now read the rest of the file as individual packets and transmit it to the NSP
    ReadAsPackets(hFile);
    return CCFRESULT_SUCCESS;
}

// Author & Date:   Hyrum L. Sessions   16 Dec 2009
// Purpose: Autosorting filters didn't line up with the manual sorting filters.
//           The latest binary format is the same as the manual sorting so
//           if this file was saved with autosorting, translate to the new filter number
// Inputs:
//   nFilter - filter number saved in the configuration file as autosort filter
// Outputs: returns the new filter number
UINT32 CCFUtilsBinary::TranslateAutoFilter(UINT32 nFilter)
{
    UINT32 nReturn = 0;
    if (1 == nFilter) nReturn = 6;      // LFP Wide
    if (2 == nFilter) nReturn = 7;      // LFP XWide
    if (3 == nFilter) nReturn = 2;      // Spike Medium
    if (4 == nFilter) nReturn = 9;      // EMG
    if (5 == nFilter) nReturn = 8;      // EEG
    if (6 == nFilter) nReturn = 10;     // Activity
    return nReturn;
}


// Author & Date:   Kirk Korver     28 Nov 2005
// Purpose: Given this file, read each "packet"
// Inputs:
//  hFile - the file is positioned at the beginning of a packet.
// Outputs:
//  settings  - binary settings
void CCFUtilsBinary::ReadAsPackets(FILE * hFile)
{
    BYTE abyData[4096]= {0};
    cbPKT_GENERIC_CB2003_10 * pPkt = reinterpret_cast<cbPKT_GENERIC_CB2003_10 *>(&abyData[0]);
    const UINT32 cbHdrSize = (UINT32)offsetof(cbPKT_GENERIC_CB2003_10, data[0]);
    BYTE * pbyPayload = abyData + cbHdrSize;        // point to first byte of payload
    m_bAutoSort = false;

    // Read in the header
    while (1 == fread(pPkt, cbHdrSize, 1, hFile))
    {
        memset(pbyPayload, 0, sizeof(abyData) - cbHdrSize);
        UINT32 cbPayload = pPkt->dlen * 4;
        if (cbPayload != fread(pbyPayload, 1, cbPayload, hFile))
            return; // actually an error

        // Also an error, but don't read
        if (pPkt->type == 0)
            return;

        // Convert from    NSP->PC  to  PC->NSP
        pPkt->type |= 0x80;

        if (0xD0 == (pPkt->type & 0xD8))    // sorting packets processed separately
        {
            m_bAutoSort = true;
            ReadSpikeSortingPackets(pPkt);
        }
        else if (cbPKTTYPE_SYSSET == (pPkt->type & 0xF0))
        {   // only set spike len and pre trigger len
            pPkt->type = cbPKTTYPE_SYSSETSPKLEN;
            cbPKT_SYSINFO_CB2005_37 * pPktSysInfo = reinterpret_cast<cbPKT_SYSINFO_CB2005_37 *>(pPkt);
            m_data.isSysInfo = *pPktSysInfo;
        }
        else if (cbPKTTYPE_SETNTRODEINFO == pPkt->type)
        {
            cbPKT_NTRODEINFO_CB2005_37 * pPktNTrode = reinterpret_cast<cbPKT_NTRODEINFO_CB2005_37 *>(pPkt);
            if (pPktNTrode->ntrode > 0 && pPktNTrode->ntrode <= cbLEGACY_MAXNTRODES)
            {
                int idx = pPktNTrode->ntrode - 1;
                m_data.isNTrodeInfo[idx] = *pPktNTrode;
            }
        }
        else if (cbPKTTYPE_ADAPTFILTSET == pPkt->type)
        {
            cbPKT_ADAPTFILTINFO_CB2005_37 * pPktAdaptInfo= reinterpret_cast<cbPKT_ADAPTFILTINFO_CB2005_37 *>(pPkt);
            m_data.isAdaptInfo = * pPktAdaptInfo;
        }
    }
    if ((m_nInternalOriginalVersion < 8) && !m_bAutoSort)
    {
        m_data.isSS_Status.type = cbPKTTYPE_SS_STATUSSET;
        m_data.isSS_Status.dlen = cbPKTDLEN_SS_STATUS;
        m_data.isSS_Status.cntlNumUnits.fElapsedMinutes = 99;
        m_data.isSS_Status.cntlUnitStats.fElapsedMinutes = 99;
    }
}


void CCFUtilsBinary::ReadSpikeSortingPackets(cbPKT_GENERIC_CB2003_10 *pPkt)
{
    cbPKT_SS_STATISTICS_CB2005_37 & isSSStatistics = m_data.isSS_Statistics;
    switch (pPkt->type)
    {
    case cbPKTTYPE_SS_STATISTICSSET:            // handle statistics packet changes
        switch (m_nInternalOriginalVersion)
        {
        case 1:
        case 2:
        case 3:
            {
            cbPKT_SS_STATISTICS_CB2005_30 * pPktSSStatistics = reinterpret_cast<cbPKT_SS_STATISTICS_CB2005_30 *>(pPkt);

            isSSStatistics.set(pPktSSStatistics->nUpdateSpikes, cbAUTOALG_HIST_PEAK_COUNT_MAJ, cbAUTOALG_MODE_APPLY,
                     pPktSSStatistics->fMinClusterSpreadFactor, pPktSSStatistics->fMaxSubclusterSpreadFactor,
                     0.80f, 0.94f,
                     0.50f, 0.50f, 0.016f,
                     250, 0);
            }
            break;
        case 4:
            {
            cbPKT_SS_STATISTICS_CB2005_31 * pPktSSStatistics = reinterpret_cast<cbPKT_SS_STATISTICS_CB2005_31 *>(pPkt);

            isSSStatistics.set(pPktSSStatistics->nUpdateSpikes, pPktSSStatistics->nAutoalg, cbAUTOALG_MODE_APPLY,
                     pPktSSStatistics->fMinClusterPairSpreadFactor, pPktSSStatistics->fMaxSubclusterSpreadFactor,
                     pPktSSStatistics->fMinClusterHistCorrMajMeasure, pPktSSStatistics->fMaxClusterPairHistCorrMajMeasure,
                     pPktSSStatistics->fClusterHistMajValleyPercentage, pPktSSStatistics->fClusterHistMajPeakPercentage,
                     0.016f, 250, 0);
            }
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            {
            cbPKT_SS_STATISTICS_CB2005_32 * pPktSSStatistics = reinterpret_cast<cbPKT_SS_STATISTICS_CB2005_32 *>(pPkt);

            isSSStatistics.set(pPktSSStatistics->nUpdateSpikes, pPktSSStatistics->nAutoalg, cbAUTOALG_MODE_APPLY,
                     pPktSSStatistics->fMinClusterPairSpreadFactor, pPktSSStatistics->fMaxSubclusterSpreadFactor,
                     pPktSSStatistics->fMinClusterHistCorrMajMeasure, pPktSSStatistics->fMaxClusterPairHistCorrMajMeasure,
                     pPktSSStatistics->fClusterHistValleyPercentage, pPktSSStatistics->fClusterHistClosePeakPercentage,
                     0.016f, 250, 0);
            }
            break;
        case 9:
        case 10:
        case 11:
            cbPKT_SS_STATISTICS_CB2005_37 * pPktSSStatistics = reinterpret_cast<cbPKT_SS_STATISTICS_CB2005_37 *>(pPkt);
            isSSStatistics = *pPktSSStatistics;
            break;
        }
        break;
    case cbPKTTYPE_SS_NOISE_BOUNDARYSET:            // handle noise boundary changes
        UINT32 nChan;

        switch (m_nInternalOriginalVersion)
        {
        case 1:     // previously noise line which doesn't translate to boundry, so use default
        case 2:
        case 3:
            for(nChan = 1; nChan <= cbLEGACY_NUM_ANALOG_CHANS; nChan++)        // channels are 1 based
            {
                cbPKT_SS_NOISE_BOUNDARY_CB2005_37 & isNoiseBoundary = m_data.isSS_NoiseBoundary[nChan - 1];
                isNoiseBoundary.set(nChan, float(0.0), float(0.0),
                    float(1.0/(300*300)), float(0.0),
                    float(0.0), float(1.0/(300*300)), float(0.0));
            }
            break;
        case 4:     // theta was added
        case 5:
            {
                cbPKT_SS_NOISE_BOUNDARY_CB2005_31 * pPktSSNoiseBoundary = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY_CB2005_31 *>(pPkt);

                for(nChan = 1; nChan <= cbLEGACY_NUM_ANALOG_CHANS; nChan++)        // channels are 1 based
                {
                    cbPKT_SS_NOISE_BOUNDARY_CB2005_37 & isNoiseBoundary = m_data.isSS_NoiseBoundary[nChan - 1];
                    isNoiseBoundary.set(nChan, pPktSSNoiseBoundary->afc[0], pPktSSNoiseBoundary->afc[1],
                        pPktSSNoiseBoundary->afS[0][0], pPktSSNoiseBoundary->afS[0][1],
                        pPktSSNoiseBoundary->afS[1][0], pPktSSNoiseBoundary->afS[1][1], 0);
                }
            }
            break;
        case 6:     // added boundary per channel
            {
                cbPKT_SS_NOISE_BOUNDARY_CB2005_33 * pPktSSNoiseBoundary = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY_CB2005_33 *>(pPkt);

                for(nChan = 1; nChan <= cbLEGACY_NUM_ANALOG_CHANS; nChan++)        // channels are 1 based
                {
                    cbPKT_SS_NOISE_BOUNDARY_CB2005_37 & isNoiseBoundary = m_data.isSS_NoiseBoundary[nChan - 1];
                    isNoiseBoundary.set(nChan, pPktSSNoiseBoundary->afc[0], pPktSSNoiseBoundary->afc[1],
                        pPktSSNoiseBoundary->afS[0][0], pPktSSNoiseBoundary->afS[0][1],
                        pPktSSNoiseBoundary->afS[1][0], pPktSSNoiseBoundary->afS[1][1],
                        pPktSSNoiseBoundary->aTheta);
                }
            }
            break;
        case 7:     // added 3rd dimension
        case 8:
            {
                cbPKT_SS_NOISE_BOUNDARY_CB2005_34 * pPktSSNoiseBoundary = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY_CB2005_34 *>(pPkt);
                nChan = pPktSSNoiseBoundary->chan;
                if (nChan > 0 && nChan <= cbLEGACY_NUM_ANALOG_CHANS)
                {
                    cbPKT_SS_NOISE_BOUNDARY_CB2005_37 & isNoiseBoundary = m_data.isSS_NoiseBoundary[nChan - 1];
                    isNoiseBoundary.set(nChan, pPktSSNoiseBoundary->afc[0], pPktSSNoiseBoundary->afc[1],
                        pPktSSNoiseBoundary->afS[0][0], pPktSSNoiseBoundary->afS[0][1],
                        pPktSSNoiseBoundary->afS[1][0], pPktSSNoiseBoundary->afS[1][1],
                        pPktSSNoiseBoundary->aTheta);
                }
            }
            break;
        case 9:
        case 10:
        case 11:
            cbPKT_SS_NOISE_BOUNDARY_CB2005_37 * pPktSSNoiseBoundary = reinterpret_cast<cbPKT_SS_NOISE_BOUNDARY_CB2005_37 *>(pPkt);
            nChan = pPktSSNoiseBoundary->chan;
            if (nChan > 0 && nChan <= cbLEGACY_NUM_ANALOG_CHANS)
            {
                cbPKT_SS_NOISE_BOUNDARY_CB2005_37 & isNoiseBoundary = m_data.isSS_NoiseBoundary[nChan - 1];
                isNoiseBoundary = * pPktSSNoiseBoundary;
            }
            break;
        }
        break;
    default:
        // if we got here, we didn't process the packet so we'll do some global processing
        // block autosorting restarting
        if (cbPKTTYPE_SS_STATUSSET == pPkt->type)
        {
            cbPKT_SS_STATUS_CB2005_37 * pPktSSStatus = reinterpret_cast<cbPKT_SS_STATUS_CB2005_37 *>(pPkt);
            pPktSSStatus->cntlNumUnits.fElapsedMinutes = 99;
            pPktSSStatus->cntlUnitStats.fElapsedMinutes = 99;
            m_data.isSS_Status = * pPktSSStatus;
        }
        else if (cbPKTTYPE_SS_DETECTSET == pPkt->type)
        {
            cbPKT_SS_DETECT_CB2005_37 * pPktSSStatus = reinterpret_cast<cbPKT_SS_DETECT_CB2005_37 *>(pPkt);
            m_data.isSS_Detect = * pPktSSStatus;
        }
        else if (cbPKTTYPE_SS_ARTIF_REJECTSET == pPkt->type)
        {
            cbPKT_SS_ARTIF_REJECT_CB2005_37 * pPktSSStatus = reinterpret_cast<cbPKT_SS_ARTIF_REJECT_CB2005_37 *>(pPkt);
            m_data.isSS_ArtifactReject = * pPktSSStatus;
        }
    }
}

// Author & Date:   Kirk Korver     22 Sep 2004
// Purpose: Write out a binary CCF file. Overwrite any other file by this name.
//           Do NOT prompt for the file to write out.
// Inputs:
//  szFileName - the name of the file to write
ccfResult CCFUtilsBinary::WriteCCFNoPrompt(LPCSTR szFileName)
{
    m_szFileName = szFileName;
    // Open a file
    FILE *hSettingsFile;
    hSettingsFile = fopen(szFileName, "wb");
    if (hSettingsFile == NULL)
        return CCFRESULT_ERR_OPENFAILEDWRITE;

    // Write a header and version to file
    // The version now is the version from the protocol
    fwrite(m_szConfigFileHeader, strlen(m_szConfigFileHeader), 1, hSettingsFile);

    char szVer[sizeof(m_szConfigFileVersion)] = {0};    // ANSI - all 0
    _snprintf(szVer, sizeof(szVer) - 1, "%d.%d", cbVERSION_MAJOR, cbVERSION_MINOR);

    fwrite(szVer, sizeof(szVer) - 1, 1, hSettingsFile);


    // Write channel info
    fwrite(&m_data.isChan[0], 1, sizeof(m_data.isChan), hSettingsFile);

    // Also write out the "adaptive filter" settings
    fwrite(&m_data.isAdaptInfo, sizeof(m_data.isAdaptInfo), 1, hSettingsFile);

    /////////// Now do the spike sorting parameters /////////////////////////////
    // The "model" isn't really stored until it is asked for, so I need to skip it,
    // and the "model", will always be the very 1st item in the sorting options area,
    // so I just have to move over it, before I write it out.
    // Sorting
    {
        fwrite(&m_data.isSS_Detect, sizeof(m_data.isSS_Detect), 1, hSettingsFile);
        fwrite(&m_data.isSS_ArtifactReject, sizeof(m_data.isSS_ArtifactReject), 1, hSettingsFile);
        for (int i = 0; i < cbLEGACY_NUM_ANALOG_CHANS; ++i)
            fwrite(&m_data.isSS_NoiseBoundary[i], sizeof(m_data.isSS_NoiseBoundary[i]), 1, hSettingsFile);
        fwrite(&m_data.isSS_Statistics, sizeof(m_data.isSS_Statistics), 1, hSettingsFile);
        fwrite(&m_data.isSS_Status, sizeof(m_data.isSS_Status), 1, hSettingsFile);
    }

    // Write sys info (spike length and pretrigger length)
    fwrite(&m_data.isSysInfo, sizeof(m_data.isSysInfo), 1, hSettingsFile);

    // Write nTrode info
    fwrite(&m_data.isNTrodeInfo[0], sizeof(m_data.isNTrodeInfo), 1, hSettingsFile);

    // Close the file
    fclose(hSettingsFile);
    return CCFRESULT_SUCCESS;
}
