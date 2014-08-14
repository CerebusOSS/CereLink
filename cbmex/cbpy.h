//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright Blackrock Microsystems
// (c) Copyright Ehsan Azarnasab
//
// $Workfile: cbpy.cpp $
// $Archive: /Cerebus/Human/LinuxApps/cbmex/cbpy.cpp $
// $Revision: 1 $
// $Date: 04/29/12 11:06a $
// $Author: Ehsan $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Cerebus Python (cbpy) interface for cbsdk
//

#ifndef CBPY_H_INCLUDED
#define CBPY_H_INCLUDED

#include <Python.h>
#include "cbsdk.h"

// cbpy errors from sdk results
void cbPySetErrorFromSdkError(cbSdkResult sdkres, const char * szErr = NULL);


#endif /* CBPY_H_INCLUDED */
