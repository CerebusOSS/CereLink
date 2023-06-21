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

#include "CCFUtils.h"
#include "CCFUtilsBinary.h"
#include "CCFUtilsXml.h"
#include "CCFUtilsConcurrent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

// Author & Date: Ehsan Azar       11 April 2012
// Purpose: CCF base constructor
CCFUtils::CCFUtils(bool bThreaded, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance) :
    m_pImpl(NULL),
    m_pCallbackFn(pCallbackFn),
    m_bThreaded(bThreaded),
    m_pCCF(pCCF),
    m_nInternalVersion(0),
    m_nInternalOriginalVersion(0),
    m_nInstance(nInstance),
    m_szFileName(NULL),
    m_bAutoSort(false),
    m_bBinaryOriginal(false)
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
        if (!m_pImpl)
            return CCFRESULT_ERR_NULL;

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
        m_bBinaryOriginal = false;
    }

    return res;
}

// Author & Date:   Ehsan Azar   10 April 2012
// Purpose: Read in a CCF file.
//           Check for the correct version of the file and convert if necessary and possible
// Inputs:
//   szFileName - the name of the file to read (if NULL raises error; use SDK calls to fetch from device)
//   bConvert   - if conversion can happen
ccfResult CCFUtils::ReadCCF(LPCSTR szFileName, bool bConvert)
{
    m_szFileName = szFileName;
    ccfResult res = CCFRESULT_SUCCESS;

    // First read version
    res = ReadVersion(szFileName);

    if (res)
        return res;

    if (szFileName == NULL)
        return CCFRESULT_ERR_OPENFAILEDREAD;

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
            // Perform a threaded read operation
            ccf::ConReadCCF(szFileName, m_pCCF, m_pCallbackFn, m_nInstance);
            // TODO: If caller intends to send the config to the device then it may be necessary to delay
            //  the m_pCallbackFn of CCFSTATE_THREADREAD, 100 until after that is complete.
            //  However, we do no such waiting for CCFSTATE_READ below so maybe it's OK.
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
            }
        }
    }
    // Cleanup
    if (pConfig)
        delete pConfig;

    return res;
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
bool CCFUtils::IsBinaryOriginal()
{
    return m_bBinaryOriginal;
}

bool CCFUtils::IsAutosort()
{
    return m_bAutoSort;
}

