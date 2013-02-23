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

// Extend the SDK events
typedef enum _cbPyEventType
{
    cbPy_EVENT_NONE  = cbSdkPkt_COUNT, // Empty event
    cbPy_EVENT_COUNT // Allways the last value
} cbPyEventType;

// cbpy errors from sdk results
void cbPySetErrorFromSdkError(cbSdkResult sdkres, const char * szErr = NULL);

// Add cbpy types to the module
//int cbPyAddTypes(PyObject * m);

// Create an instance of the event type with given data and type
//  (borrows data reference, returns new reference)
//PyObject * cbPyCreateEvent(cbPyEventType type, PyObject * pData);

#endif /* CBPY_H_INCLUDED */
