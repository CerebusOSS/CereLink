// =STS=> SpkDisp.cpp[4902].aa01   open     SMID:1 
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: SpkDisp.h $
// $Archive: /Cerebus/WindowsApps/SDKSample/SpkDisp.h $
// $Revision: 15 $
// $Date: 4/26/12 9:50a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
/////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "SpkDisp.h"
#include <conio.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpkDisp

CSpkDisp::CSpkDisp() :
    m_spklength(48)
{
    m_count = 0;
    m_cacheIdxRead = m_cacheIdxWrite = 0;
}

CSpkDisp::~CSpkDisp()
{
}


BEGIN_MESSAGE_MAP(CSpkDisp, CWnd)
	//{{AFX_MSG_MAP(CSpkDisp)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSpkDisp message handlers


// Author & Date:   Ehsan Azar     26 April 2012
// Purpose: Specialized DC for drawing
BOOL CSpkDisp::PreCreateWindow(CREATESTRUCT& cs)
{
	WNDCLASS wc;
	wc.style		= CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc  = AfxWndProc;
	wc.cbClsExtra	= NULL;
	wc.cbWndExtra	= NULL;
	wc.hInstance	= AfxGetInstanceHandle();
	wc.hIcon		= NULL;
	wc.hCursor		= AfxGetApp()->LoadCursor(IDC_ARROW);
	wc.hbrBackground= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= "cbSpikeDisplay";
	if (!AfxRegisterClass( &wc ))
        return FALSE;
	
	cs.lpszClass = "cbSpikeDisplay";
	return CWnd::PreCreateWindow(cs);
}

// Author & Date:   Ehsan Azar     26 April 2012
// Purpose: Specialized DC for drawing
int CSpkDisp::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

    RECT rect_client;
    GetClientRect(&rect_client);
    m_iClientSizeX = rect_client.right;
    m_iClientSizeY = rect_client.bottom;
    // Get picture area
    CRect rectFrame;
    GetClientRect(&rectFrame);
    m_width = rectFrame.Width();
    m_height = rectFrame.Height();

    m_dispunit[0]  = RGB(192,192,192);
    m_dispunit[1]  = RGB(255, 51,153);
    m_dispunit[2]  = RGB(  0,255,255);
    m_dispunit[3]  = RGB(255,255,  0);
    m_dispunit[4]  = RGB(153,  0,204);
    m_dispunit[5]  = RGB(  0,255,  0);

    return 0;
}

BOOL CSpkDisp::OnEraseBkgnd(CDC* pDC)
{
	// clear background
	pDC->FillSolidRect(0, 0, m_iClientSizeX, m_iClientSizeY, RGB(0, 0, 0));

	return TRUE;
}

// Author & Date:   Ehsan Azar     26 April 2012
// Purpose: Add spike to the the cache to be drawn later
// Inputs:
//   spk - spike packet
void CSpkDisp::AddSpike(cbPKT_SPK & spk)
{
    int idx = m_cacheIdxWrite + 1;
    if (idx == 500)
        idx = 0;

    if (idx == m_cacheIdxRead)
        return; // We are behind on drawing!

    m_spkCache[m_cacheIdxWrite] = spk;
    if (m_cacheIdxWrite < 499)
        m_cacheIdxWrite++;
    else
        m_cacheIdxWrite = 0;
    // Begin drawing
    Invalidate(false);
}

// Author & Date:   Ehsan Azar     29 March 2011
// Purpose: Draw spike to the screen
// Inputs:
//   spk - spike packet
void CSpkDisp::DrawSpikes(CDC & rcDC)
{
    // Draw at most these many spikes from cache in one shot
    for (int nSpikes = 0; nSpikes < 100; ++nSpikes)
    {
        if (m_cacheIdxRead == m_cacheIdxWrite)
            return;

        cbPKT_SPK & spk = m_spkCache[m_cacheIdxRead];

        UINT8 unit = spk.unit;
        if (unit > cbMAXUNITS)
            unit = 0;
        if (m_count == 0)
            rcDC.FillSolidRect(0, 0, m_width, m_height, RGB(0, 0, 0));
        if (m_count++ > 100)
            m_count = 0;
        rcDC.SelectObject(GetStockObject(DC_PEN));
        rcDC.SetDCPenColor(m_dispunit[unit]);
        int halfheight = m_height / 2;
        rcDC.MoveTo(0, halfheight);
        for (UINT32 i = 0; i < m_spklength; ++i)
        {
            int x = (int)(i * (float(m_width) / m_spklength));
            int y = halfheight - (int)(spk.wave[i] / (4.0 * 255) * halfheight);
            if (y > m_height)
                y = m_height;
            else if (y < 0)
                y = 0;
            rcDC.LineTo(x, y);
        }
        if (m_cacheIdxRead < 499)
            m_cacheIdxRead++;
        else
            m_cacheIdxRead = 0;
    }
}

// Author & Date:   Ehsan Azar     26 April 2012
// Purpose: PAint spikes from the cache
void CSpkDisp::OnPaint()
{
    CPaintDC dc(this);

    // Draw spike on screen
    DrawSpikes(dc);
}

