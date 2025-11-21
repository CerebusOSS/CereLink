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


#ifndef SPKDISP_H_INCLUDED
#define SPKDISP_H_INCLUDED

#include "cbhwlib.h"

/////////////////////////////////////////////////////////////////////////////
// CSpkDisp window

class CSpkDisp : public CWnd
{
// Construction
public:
	CSpkDisp();
	virtual ~CSpkDisp();

public:
    void AddSpike(cbPKT_SPK & spk);

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

protected:
    void DrawSpikes(CDC & rcDC);

// Attributes
protected:
    COLORREF m_dispunit[cbMAXUNITS + 1];
    int m_width, m_height;
    int  m_iClientSizeX;
    int  m_iClientSizeY;
    int m_count;
    cbPKT_SPK m_spkCache[500];
    int m_cacheIdxWrite;
    int m_cacheIdxRead;
public:
    UINT32 m_spklength;

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};

#endif // include guard
