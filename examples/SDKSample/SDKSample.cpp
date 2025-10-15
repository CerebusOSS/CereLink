// =STS=> SDKSample.cpp[4270].aa00   closed   SMID:1 
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2011 - 2012 Blackrock Microsystems
//
// $Workfile: SDKSample.cpp $
// $Archive: /Cerebus/Human/WindowsApps/SDKSample/SDKSample.cpp $
// $Revision: 1 $
// $Date: 3/29/11 9:23a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
/////////////////////////////////////////////////////////////////////////////
//
// PURPOSE: cbmex SDK example application
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <afxmt.h>
#include "SDKSample.h"
#include "SDKSampleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CSDKSampleApp

BEGIN_MESSAGE_MAP(CSDKSampleApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CSDKSampleApp construction

CSDKSampleApp::CSDKSampleApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CExampleApp object

CSDKSampleApp theApp;


// CSDKSampleApp initialization

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Initialize SDK example application,
//           and make sure at most one instance runs.
// Outputs:
//   returns the error code
BOOL CSDKSampleApp::InitInstance()
{
    CMutex cbSDKSampleAppMutex(TRUE, "cbSDKSampleAppMutex");
    CSingleLock cbSDKSampleLock(&cbSDKSampleAppMutex);

	// We let only one instance of nPlay GUI
    if (!cbSDKSampleLock.Lock(0))
        return FALSE;

	CWinApp::InitInstance();

	CSDKSampleDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
