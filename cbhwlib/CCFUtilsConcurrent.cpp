//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: CCFUtilsConcurrent.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/CCFUtilsConcurrent.cpp $
// $Revision: 1 $
// $Date: 6/10/12 1:40p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <QtConcurrentRun>
#include "CCFUtilsConcurrent.h"
#include "debugmacs.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

using namespace ccf;

// Author & Date: Ehsan Azar       10 June 2012
// Purpose: Helper function to read CCF without interaction 
//           (unthreaded and with forced conversion inside a thread)
//           Since it is forced conversion, confirmation should be made beforehand
// Inputs:
//   szFileName  - the name of the file to read (if NULL uses live config)
//   bSend       - if should send upon successful reading
//   pCallbackFn - the progress reporting function
// Outputs:
//   pCCF        - where to take an extra copy of the CCF upon successful reading
void ReadCCFHelper(QString strFileName, bool bSend, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance)
{
    // make sure byte array is constructed, thus we can use the internal pointer to character data
    const QByteArray tmp = strFileName.toLatin1();
    LPCSTR szFileName = tmp;
    if (pCallbackFn)
        pCallbackFn(nInstance, CCFRESULT_SUCCESS, szFileName, CCFSTATE_THREADREAD, 0);
    CCFUtils config(bSend, false, pCCF, pCallbackFn, nInstance);
    ccf::ccfResult res = config.ReadCCF(szFileName, true);
    if (pCallbackFn)
        pCallbackFn(nInstance, res, szFileName, CCFSTATE_THREADREAD, 100);
}

// Author & Date: Ehsan Azar       10 June 2012
// Purpose: Wrapper to run ReadCCFHelper in a thread
void ccf::ConReadCCF(LPCSTR szFileName, bool bSend, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance)
{
    QString strFileName = szFileName == NULL ? "" : szFileName;
    // Parameters are copied before thread starts, originals will go out of scope
    QtConcurrent::run(ReadCCFHelper, strFileName, bSend, pCCF, pCallbackFn, nInstance);
}

// Author & Date: Ehsan Azar       10 June 2012
// Purpose: Helper function to write CCF without interaction 
//           (unthreaded inside a thread)
// Inputs:
//   szFileName  - the name of the file to write to (if NULL sends to NSP)
//   ccf         - initial CCF content
//   pCallbackFn - the progress reporting function
void WriteCCFHelper(QString strFileName, cbCCF ccf, cbCCFCallback pCallbackFn, uint32_t nInstance)
{
    // make sure byte array is constructed, thus we can use the internal pointer to character data
    const QByteArray tmp = strFileName.toLatin1();
    LPCSTR szFileName = tmp;
    QThread::currentThread()->setPriority(QThread::LowestPriority);
    if (pCallbackFn)
        pCallbackFn(nInstance, CCFRESULT_SUCCESS, szFileName, CCFSTATE_THREADWRITE, 0);
    // If valid ccf is passed, use it as initial data, otherwise use NULL to have it read from NSP
    CCFUtils config(false, false, ccf.isChan[0].chid == 0 ? NULL : &ccf, pCallbackFn, nInstance);
    ccf::ccfResult res = config.WriteCCFNoPrompt(szFileName);
    if (pCallbackFn)
        pCallbackFn(nInstance, res, szFileName, CCFSTATE_THREADWRITE, 100);
}

// Author & Date: Ehsan Azar       10 June 2012
// Purpose: Wrapper to run WriteCCFHelper in a thread
void ccf::ConWriteCCF(LPCSTR szFileName, cbCCF * pCCF, cbCCFCallback pCallbackFn, uint32_t nInstance)
{
    QString strFileName = szFileName == NULL ? "" : szFileName;
    cbCCF ccf;
    memset(&ccf, 0, sizeof(cbCCF)); // Invalidate ccf
    if (pCCF != NULL)
        ccf = *pCCF;
    // Parameters are copied before thread starts, originals will go out of scope
    QtConcurrent::run(WriteCCFHelper, strFileName, ccf, pCallbackFn, nInstance);
}

