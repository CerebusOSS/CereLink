// =STS=> SDKSampleDlg.cpp[4274].aa05   open     SMID:5 
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2011 - 2012 Blackrock Microsystems
//
// $Workfile: SDKSampleDlg.cpp $
// $Archive: /Cerebus/Human/WindowsApps/SDKSample/SDKSampleDlg.cpp $
// $Revision: 1 $
// $Date: 3/29/11 9:23a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
/////////////////////////////////////////////////////////////////////////////
//
// PURPOSE: Cerebus SDK example dialog
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SDKSample.h"
#include "SDKSampleDlg.h"
#include <conio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Author & Date:   Almut Branner    12 May 2003
// Purpose: Replace this static control with this window
// Inputs:
//  rcParentWindow - the window this is going to be placed into
//  rcNewWindow - the window that is to be created
//  nControlID - the ID of the control (from the resource editor)
void ReplaceWindowControl(const CWnd &rcParentWindow, CWnd &rcNewWindow, int nControlID)
{
    CWnd *pStatic = rcParentWindow.GetDlgItem(nControlID);

    // For debug mode
    ASSERT(pStatic != 0);

    // For released code
    if (pStatic == 0)
        return;

    CRect rctWindowSize;

    DWORD frmstyle   = pStatic->GetStyle();
    DWORD frmexstyle = pStatic->GetExStyle();

    pStatic->GetWindowRect(rctWindowSize);      // Get window coord.
    rcParentWindow.ScreenToClient(rctWindowSize);              // change to client coord.
    pStatic->DestroyWindow();

    CWnd *pParent = const_cast<CWnd *>(&rcParentWindow);
    rcNewWindow.CreateEx(frmexstyle, NULL, NULL, frmstyle, rctWindowSize, pParent, nControlID);

    // Use for debugging
    // AllocConsole();
}


CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Show version information in about dialog
BOOL CAboutDlg::OnInitDialog()
{
    CString strOld, strNew;
	CDialog::OnInitDialog();
    cbSdkVersion ver;
    cbSdkResult res = cbSdkGetVersion(0, &ver);

    GetDlgItemText(IDC_STATIC_APP_VERSION, strOld);
    strNew.Format("%s v%u.%02u.%02u.%02u", strOld, ver.major, ver.minor, ver.release, ver.beta);
    SetDlgItemText(IDC_STATIC_APP_VERSION, strNew);

    GetDlgItemText(IDC_STATIC_LIB_VERSION, strOld);
    strNew.Format("%s v%u.%02u", strOld, ver.majorp, ver.minorp);
    SetDlgItemText(IDC_STATIC_LIB_VERSION, strNew);

    GetDlgItemText(IDC_STATIC_NSP_APP_VERSION, strOld);

    cbSdkConnectionType conType;
    cbSdkInstrumentType instType;
    // Return the actual openned connection
    if (res == CBSDKRESULT_SUCCESS)
    {
        res = cbSdkGetType(0, &conType, &instType);
        if (res == CBSDKRESULT_SUCCESS)
        {
            if (instType != CBSDKINSTRUMENT_LOCALNSP && instType != CBSDKINSTRUMENT_NSP)
                strOld = "nPlay";
            if (conType == CBSDKCONNECTION_CENTRAL)
                strOld += "(Central)";
        }
        strNew.Format("%s v%u.%02u.%02u.%02u", strOld, ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta);
        SetDlgItemText(IDC_STATIC_NSP_APP_VERSION, strNew);

        GetDlgItemText(IDC_STATIC_NSP_LIB_VERSION, strOld);
        strNew.Format("%s v%u.%02u", strOld, ver.nspmajorp, ver.nspminorp);
        SetDlgItemText(IDC_STATIC_NSP_LIB_VERSION, strNew);
    } else {
        SetDlgItemText(IDC_STATIC_NSP_APP_VERSION, strOld + " not connected");
        SetDlgItemText(IDC_STATIC_NSP_LIB_VERSION, "");
    }

    return TRUE;
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// Author & Date:   Ehsan Azar     29 March 2011
// CSDKSampleDlg dialog constructor
CSDKSampleDlg::CSDKSampleDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSDKSampleDlg::IDD, pParent),
    m_channel(1), m_threshold(-65 * 4)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSDKSampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC_STATUS, m_status);
    DDX_Control(pDX, IDC_CHIDCOMBO, m_cboChannel);
    DDX_Control(pDX, IDC_THRESHOLD_SLIDER, m_sldThresh);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Show error message
void CSDKSampleDlg::PrintError(cbSdkResult res)
{
    switch(res)
    {
    case CBSDKRESULT_WARNCLOSED:
        SetStatusWindow("Library is already closed");
        break;
    case CBSDKRESULT_WARNOPEN:
        SetStatusWindow("Library is already opened");
        break;
    case CBSDKRESULT_SUCCESS:
        SetStatusWindow("Success");
        break;
    case CBSDKRESULT_NOTIMPLEMENTED:
        SetStatusWindow("Not implemented");
        break;
    case CBSDKRESULT_INVALIDPARAM:
        SetStatusWindow("Invalid parameter");
        break;
    case CBSDKRESULT_CLOSED:
        SetStatusWindow("Interface is closed cannot do this operation");
        break;
    case CBSDKRESULT_OPEN:
        SetStatusWindow("Interface is open cannot do this operation");
        break;
    case CBSDKRESULT_NULLPTR:
        SetStatusWindow("Null pointer");
        break;
    case CBSDKRESULT_ERROPENCENTRAL:
        SetStatusWindow("NUnable to open Central interface");
        break;
    case CBSDKRESULT_ERROPENUDP:
        SetStatusWindow("Unable to open UDP interface (might happen if default)");
        break;
    case CBSDKRESULT_ERROPENUDPPORT:
        SetStatusWindow("Unable to open UDP port");
        break;
    case CBSDKRESULT_ERRMEMORYTRIAL:
        SetStatusWindow("Unable to allocate RAM for trial cache data");
        break;
    case CBSDKRESULT_ERROPENUDPTHREAD:
        SetStatusWindow("Unable to open UDP timer thread");
        break;
    case CBSDKRESULT_ERROPENCENTRALTHREAD:
        SetStatusWindow("Unable to open Central communication thread");
        break;
    case CBSDKRESULT_INVALIDCHANNEL:
        SetStatusWindow("Invalid channel number");
        break;
    case CBSDKRESULT_INVALIDCOMMENT:
        SetStatusWindow("Comment too long or invalid");
        break;
    case CBSDKRESULT_INVALIDFILENAME:
        SetStatusWindow("Filename too long or invalid");
        break;
    case CBSDKRESULT_INVALIDCALLBACKTYPE:
        SetStatusWindow("Invalid callback type");
        break;
    case CBSDKRESULT_CALLBACKREGFAILED:
        SetStatusWindow("Callback register/unregister failed");
        break;
    case CBSDKRESULT_ERRCONFIG:
        SetStatusWindow("Trying to run an unconfigured method");
        break;
    case CBSDKRESULT_INVALIDTRACKABLE:
        SetStatusWindow("Invalid trackable id, or trackable not present");
        break;
    case CBSDKRESULT_INVALIDVIDEOSRC:
        SetStatusWindow("Invalid video source id, or video source not present");
        break;
    case CBSDKRESULT_UNKNOWN:
        SetStatusWindow("Unknown error");
        break;
    case CBSDKRESULT_ERROPENFILE:
        SetStatusWindow("Cannot open file");
        break;
    case CBSDKRESULT_ERRFORMATFILE:
        SetStatusWindow("Wrong file format");
        break;
    case CBSDKRESULT_OPTERRUDP:
        SetStatusWindow("Socket option error (possibly permission issue)");
        break;
    case CBSDKRESULT_MEMERRUDP:
        SetStatusWindow("Unable to assign UDP interface memory");
        break;
    case CBSDKRESULT_INVALIDINST:
        SetStatusWindow("Invalid range or instrument address");
        break;
    case CBSDKRESULT_ERRMEMORY:
        SetStatusWindow("Memory allocation error");
        break;
    case CBSDKRESULT_ERRINIT:
        SetStatusWindow("Initialization error");
        break;
    case CBSDKRESULT_TIMEOUT:
        SetStatusWindow("Conection timeout error");
        break;
    case CBSDKRESULT_BUSY:
        SetStatusWindow("Resource is busy");
        break;
    default:
        SetStatusWindow("Unexpected error");
    }
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Set last status message
void CSDKSampleDlg::SetStatusWindow(const char * statusmsg)
{
    m_status.SetWindowText(statusmsg);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Set channel to listen to
// Inputs:
//   channel - channel number (1-based)
void CSDKSampleDlg::SetChannel(UINT16 channel)
{
    m_channel = channel;
    cbPKT_CHANINFO chaninfo;
    cbSdkResult res;
    res = cbSdkGetChannelConfig(0, channel, &chaninfo);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    m_threshold = chaninfo.spkthrlevel;
    m_sldThresh.SetPos(-m_threshold / 4);
    UINT32 spklength;
    res = cbSdkGetSysConfig(0, &spklength);
    m_spkPict.m_spklength = spklength;
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
}

// Author & Date:   Ehsan Azar     26 April 2012
// Purpose: Add spike to the the cache to be drawn later
// Inputs:
//   spk - spike packet
void CSDKSampleDlg::AddSpike(cbPKT_SPK & spk)
{
    // Only for one channel draw the spikes
    if (spk.chid == m_channel)
        m_spkPict.AddSpike(spk);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Set threshold based on slider position
// Inputs:
//    pos          - threshold slider position
void CSDKSampleDlg::SetThreshold(int pos)
{
    INT32 newthreshold = pos * 4;
    if (m_threshold == newthreshold)
        return;
    cbPKT_CHANINFO chaninfo;
    cbSdkResult res;
    res = cbSdkGetChannelConfig(0, m_channel, &chaninfo);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    chaninfo.spkthrlevel = newthreshold;
    res = cbSdkSetChannelConfig(0, m_channel, &chaninfo);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    m_threshold = newthreshold;
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Callback to receive spike data
// Inputs:
//    type          - type of the event
//    pEventData    - points to a cbPkt_* structure depending on the type
//    pCallbackData - is what is used to register the callback
void CSDKSampleDlg::SpikesCallback(UINT32 nInstacne, const cbSdkPktType type, const void* pEventData, void* pCallbackData)
{
    CSDKSampleDlg * pDlg = reinterpret_cast<CSDKSampleDlg *>(pCallbackData);
    switch(type)
    {
    case cbSdkPkt_PACKETLOST:
        break;
    case cbSdkPkt_SPIKE:
        if (pDlg && pEventData)
        {
            cbPKT_SPK spk = *reinterpret_cast<const cbPKT_SPK *>(pEventData);
            // Note: Callback should return fast, so it is better to buffer it  here
            //       and use another thread to handle data
            pDlg->AddSpike(spk);
        }
        break;
    default:
        break;
    }
    return;
}

BEGIN_MESSAGE_MAP(CSDKSampleDlg, CDialog)
	ON_WM_SYSCOMMAND()
    ON_WM_VSCROLL()
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CSDKSampleDlg::OnBtnConnect)
    ON_BN_CLICKED(IDC_BTN_DISCONNECT, &CSDKSampleDlg::OnBtnClose)
    ON_BN_CLICKED(IDC_BTN_SPIKES, &CSDKSampleDlg::OnBtnSpikes)
    ON_CBN_SELCHANGE(IDC_CHIDCOMBO, &CSDKSampleDlg::OnSelchangeComboChannel)
END_MESSAGE_MAP()

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Handle vertical slider control message
void CSDKSampleDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    switch (pScrollBar->GetDlgCtrlID())
    {
    case IDC_THRESHOLD_SLIDER:
        if (nSBCode == SB_ENDSCROLL)
            SetThreshold(-m_sldThresh.GetPos());
        break;
    }

    CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

// Author & Date:   Ehsan Azar     30 March 2011
// Purpose: Current channel changed
void CSDKSampleDlg::OnSelchangeComboChannel()
{
    UINT16 newchan = m_cboChannel.GetCurSel() + 1;
    if (newchan != m_channel)
        SetChannel(newchan);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Open and connect to the library
void CSDKSampleDlg::OnBtnConnect()
{
    cbSdkResult res = cbSdkOpen(0);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    cbSdkConnectionType conType;
    cbSdkInstrumentType instType;
    // Return the actual openned connection
    res = cbSdkGetType(0, &conType, &instType);
    if (res != CBSDKRESULT_SUCCESS)
    {
        SetStatusWindow("Unable to determine connection type");
        return;
    }
    cbSdkVersion ver;
    res = cbSdkGetVersion(0, &ver);
    if (res != CBSDKRESULT_SUCCESS)
    {
        SetStatusWindow("Unable to determine nsp version");
        return;
    }

    if (conType < 0 || conType > CBSDKCONNECTION_CLOSED)
        conType = CBSDKCONNECTION_CLOSED;
    if (instType < 0 || instType > CBSDKINSTRUMENT_COUNT)
        instType = CBSDKINSTRUMENT_COUNT;

    char strConnection[CBSDKCONNECTION_CLOSED + 1][8] = {"Default", "Central", "Udp", "Closed"};
    char strInstrument[CBSDKINSTRUMENT_COUNT + 1][13] = {"NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"};
    CString strStatus;
    SetStatusWindow(strStatus);
    strStatus.Format("%s real-time interface to %s (%d.%02d.%02d.%02d) successfully initialized\n", strConnection[conType], strInstrument[instType], ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta);
    SetStatusWindow(strStatus);

    // Slider shows analog threshold
    m_sldThresh.SetRange(-255, 255, TRUE);
    m_sldThresh.SetPageSize(5);

    m_cboChannel.ResetContent();
    char label[cbLEN_STR_LABEL + 1];
    for(UINT16 chan = 1; chan <= cbNUM_ANALOG_CHANS; chan++)
    {
        label[cbLEN_STR_LABEL] = 0;
        res = cbSdkGetChannelLabel(0, chan, NULL, label, NULL, NULL);
        if (res == CBSDKRESULT_SUCCESS)
            m_cboChannel.AddString(label);
    }
    if (m_cboChannel.GetCount() > 0)
        m_cboChannel.SetCurSel(m_channel - 1);
    SetChannel(m_channel);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Close the library
void CSDKSampleDlg::OnBtnClose()
{
    cbSdkResult res = cbSdkClose(0);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    SetStatusWindow("Interface closed successfully");
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Listen to the spike packets
void CSDKSampleDlg::OnBtnSpikes()
{
    cbSdkResult res = cbSdkRegisterCallback(0, CBSDKCALLBACK_SPIKE, SpikesCallback, this);
    if (res != CBSDKRESULT_SUCCESS)
    {
        PrintError(res);
        return;
    }
    SetStatusWindow("Successfully listening to the spikes");
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Initialize the dialog
BOOL CSDKSampleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

    // Use a better window for drawing
    ::ReplaceWindowControl(*this, m_spkPict, IDC_PICT_SPIKES);

    // Initialize the channel combo box and refresh channel display
    m_cboChannel.InitStorage(cbNUM_ANALOG_CHANS, cbLEN_STR_LABEL + 1);


	return TRUE;  // return TRUE  unless you set the focus to a control
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: System menu command
void CSDKSampleDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}
