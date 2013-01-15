//////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: WCFUtils.h $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/WCFUtils.h $
// $Revision: 1 $
// $Date: 9/10/12 4:0p $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////

#ifndef WCFUTILS_H_INCLUDED
#define WCFUTILS_H_INCLUDED

#include "cbhwlib.h"

//
// Make sure not to include (directly or indirectly) any
//  implementation details such as Qt
//

class WCFUtils
{
public:
    WCFUtils(bool bThreaded = true, UINT32 nInstance = 0);
    virtual ~WCFUtils();

public:
    int SendWCF(LPCSTR szFileName, UINT16 channel);
    int WriteWCFNoPrompt(LPCSTR szFileName, UINT16 channel);

private:
    bool m_bThreaded;
    bool m_nInstance;
    enum {WCF_XML_VERSION = 1}; // Own version

protected:
    LPCSTR m_szFileName; // filename
};

#endif // include guard
