//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsConcurrent.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsConcurrent.h $
// $Revision: 1 $
// $Date: 4/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////
//
// Purpose: Concurrent access to CCF utilities
//

#ifndef CCFUTILSCONCURRENT_H_INCLUDED
#define CCFUTILSCONCURRENT_H_INCLUDED

#include "cbhwlib.h"
#include "CCFUtils.h"

// Namespace for Cerebus Config Files
namespace ccf
{
    void ConReadCCF(LPCSTR szFileName, bool bSend, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance);
    void ConWriteCCF(LPCSTR szFileName, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance);
};


#endif // include guard
