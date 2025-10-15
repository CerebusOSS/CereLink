/* =STS=> SDKSampleDlg.h[4275].aa02   open     SMID:2 */
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2011 - 2012 Blackrock Microsystems
//
// $Workfile: SDKSampleDlg.h $
// $Archive: /Cerebus/Human/WindowsApps/SDKSample/SDKSampleDlg.h $
// $Revision: 1 $
// $Date: 3/29/11 9:23a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
/////////////////////////////////////////////////////////////////////////////
//
// PURPOSE: Cerebus SDK sample dialog
//
/////////////////////////////////////////////////////////////////////////////

#ifndef SDKSAMPLEDLG_H_INCLUDED
#define SDKSAMPLEDLG_H_INCLUDED

#if _MSC_VER > 1000
#pragma once

#endif

#include "cbsdk.h"
#include "SpkDisp.h"

// CSDKSampleDlg dialog
class CSDKSampleDlg : public CDialog
{
// Construction
public:
	CSDKSampleDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_EXAMPLE_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
    void PrintError(cbSdkResult res);
    void SetStatusWindow(const char * statusmsg);
    void SetChannel(UINT16 channel);
    void AddSpike(cbPKT_SPK & spk);
    void SetThreshold(int pos);
    static void SpikesCallback(UINT32 nInstance, const cbSdkPktType type, const void* pEventData, void* pCallbackData);

protected:
	HICON m_hIcon;
    CStatic m_status;
    CSpkDisp m_spkPict;
    CComboBox   m_cboChannel;
    CSliderCtrl m_sldThresh;

protected:
    UINT16 m_channel;
    INT32 m_threshold; // digital threshold

	// event handling functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnBtnConnect();
    afx_msg void OnBtnClose();
    afx_msg void OnBtnSpikes();
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnSelchangeComboChannel();
	DECLARE_MESSAGE_MAP()
};

// CAboutDlg dialog
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

#endif //SDKSAMPLEDLG_H_INCLUDED
