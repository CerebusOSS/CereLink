/* =STS=> CCFUtils.h[1692].aa13   open     SMID:14 */
//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003 - 2008 Cyberkinetics, Inc.
// (c) Copyright 2008 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtils.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtils.h $
// $Revision: 2 $
// $Date: 4/26/05 2:56p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#ifndef CCFUTILS_H_INCLUDED
#define CCFUTILS_H_INCLUDED

//
// Make sure not to include (directly or indirectly) any
//  implementation details such as Qt
//

#include "cbhwlib.h"

// Namespace for Cerebus Config Files
namespace ccf
{

typedef enum _ccfResult
{
    CCFRESULT_WARN_VERSION          = 2,  // Protocol version is different
    CCFRESULT_WARN_CONVERT          = 1,  // Data conversion is needed
    CCFRESULT_SUCCESS               = 0,
    CCFRESULT_ERR_NULL              = -1, // Empty configuration
    CCFRESULT_ERR_OFFLINE           = -2, // Instrument is offline
    CCFRESULT_ERR_FORMAT            = -3, // Wrong format
    CCFRESULT_ERR_OPENFAILEDWRITE   = -4, // Cannot open file for writing
    CCFRESULT_ERR_OPENFAILEDREAD    = -5, // Cannot open file for reading
    CCFRESULT_ERR_UNKNOWN           = -6, // Unknown error
} ccfResult;

};      // namespace ccf

// CCF callback
typedef void (* cbCCFCallback)(UINT32 nInstance, const ccf::ccfResult res, LPCSTR szFileName, const cbStateCCF state, const UINT32 nProgress);

class CCFUtils
{
public:
    CCFUtils(bool bSend = false, bool bThreaded = false, cbCCF * pCCF = NULL, cbCCFCallback pCallbackFn = NULL, UINT32 nInstance = 0);
    virtual ~CCFUtils();

public:
    virtual ccf::ccfResult ReadCCF(LPCSTR szFileName = NULL, bool bConvert = false);
    virtual ccf::ccfResult WriteCCFNoPrompt(LPCSTR szFileName);
    virtual ccf::ccfResult ReadVersion(LPCSTR szFileName);
    int GetInternalVersion();
    int GetInternalOriginalVersion();
    BOOL IsBinaryOriginal();
    ccf::ccfResult SendCCF();

private:
    // Hide actual implementation in this private variable
    CCFUtils * m_pImpl;
    bool m_bSend; // If should send after successful read
    cbCCFCallback m_pCallbackFn; // CCF progress callback
    bool m_bThreaded; // If operation should be threaded with given callback
    cbCCF * m_pCCF; // Extra copy of CCF

private:
    void ReadCCFOfNSP(); // Read CCF from NSP

protected:
    // Convert from old config
    virtual CCFUtils * Convert(CCFUtils * pOldConfig);

protected:
    int m_nInternalVersion;  // internal version number
    int m_nInternalOriginalVersion;  // internal version of original data
    int m_nInstance; // Library instance for CCF operations
    LPCSTR m_szFileName; // filename
    BOOL m_bAutoSort; // Compatibility flag for auto sort
    BOOL m_bBinaryOriginal; // if original file is binary
};

#endif // include guard
