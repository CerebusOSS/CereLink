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

#include "cbproto.h"
#include <cstddef>

// Latest CCF structure
typedef struct {
    cbPKT_CHANINFO isChan[cbMAXCHANS];
    cbPKT_ADAPTFILTINFO isAdaptInfo;
    cbPKT_SS_DETECT isSS_Detect;
    cbPKT_SS_ARTIF_REJECT isSS_ArtifactReject;
    cbPKT_SS_NOISE_BOUNDARY isSS_NoiseBoundary[cbNUM_ANALOG_CHANS];
    cbPKT_SS_STATISTICS isSS_Statistics;
    cbPKT_SS_STATUS isSS_Status;
    cbPKT_SYSINFO isSysInfo;
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_FILTINFO filtinfo[cbNUM_DIGITAL_FILTERS];
    cbPKT_LNC       isLnc;
} cbCCF;

// CCF processing state
typedef enum _cbStateCCF
{
    CCFSTATE_READ = 0,     // Reading in progress
    CCFSTATE_WRITE,        // Writing in progress
    CCFSTATE_SEND ,        // Sendign in progress
    CCFSTATE_CONVERT,      // Conversion in progress
    CCFSTATE_THREADREAD,   // Total threaded read progress
    CCFSTATE_THREADWRITE,  // Total threaded write progress
    CCFSTATE_UNKNOWN, // (Always the last) unknown state
} cbStateCCF;

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
typedef void (* cbCCFCallback)(uint32_t nInstance, const ccf::ccfResult res, LPCSTR szFileName, const cbStateCCF state, const uint32_t nProgress);

class CCFUtils
{
public:
    // New simplified constructor (no automatic send support)
    CCFUtils(bool bThreaded = false, cbCCF * pCCF = NULL, cbCCFCallback pCallbackFn = NULL, uint32_t nInstance = 0);
    // Backward-compatible constructor signature (bSend ignored). Prefer the simplified one.
    CCFUtils(bool bSend /*ignored*/, bool bThreaded, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance) :
        CCFUtils(bThreaded, pCCF, pCallbackFn, nInstance) {}
    virtual ~CCFUtils();

public:
    virtual ccf::ccfResult ReadCCF(LPCSTR szFileName = NULL, bool bConvert = false);
    virtual ccf::ccfResult WriteCCFNoPrompt(LPCSTR szFileName);
    virtual ccf::ccfResult ReadVersion(LPCSTR szFileName);
    int GetInternalVersion();
    int GetInternalOriginalVersion();
    bool IsBinaryOriginal();
    bool IsAutosort();
    // Deprecated: legacy API placeholder. Automatic sending removed from this refactor.
    virtual ccf::ccfResult SendCCF() { return ccf::CCFRESULT_SUCCESS; }
    virtual ccf::ccfResult SetProcInfo(const cbPROCINFO& isInfo);

private:
    // Hide actual implementation in this private variable
    CCFUtils * m_pImpl;
    cbCCFCallback m_pCallbackFn; // CCF progress callback
    bool m_bThreaded; // If operation should be threaded with given callback
    cbCCF * m_pCCF; // Extra copy of CCF

protected:
    // Convert from old config
    virtual CCFUtils * Convert(CCFUtils * pOldConfig);

protected:
    int m_nInternalVersion;  // internal version number
    int m_nInternalOriginalVersion;  // internal version of original data
    int m_nInstance; // Library instance for CCF operations
    LPCSTR m_szFileName; // filename
    bool m_bAutoSort; // Compatibility flag for auto sort
    bool m_bBinaryOriginal; // if original file is binary
};

#endif // include guard
