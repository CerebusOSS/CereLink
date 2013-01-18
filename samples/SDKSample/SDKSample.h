/* =STS=> SDKSample.h[4271].aa00   closed   SMID:1 */
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2011 - 2012 Blackrock Microsystems
//
// $Workfile: SDKSample.cpp $
// $Archive: /Cerebus/Human/WindowsApps/SDKSample/SDKSample.h $
// $Revision: 1 $
// $Date: 3/29/11 9:23a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
/////////////////////////////////////////////////////////////////////////////
//
// PURPOSE: cbmex SDK Sample application
//
/////////////////////////////////////////////////////////////////////////////


#ifndef SDKSAMPLE_H_INCLUDED
#define SDKSAMPLE_H_INCLUDED

#if _MSC_VER > 1000
#pragma once

#endif

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

class CSDKSampleApp : public CWinApp
{
public:
	CSDKSampleApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

#endif // #ifndef SDKSAMPLE_H_INCLUDED
