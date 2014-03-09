/*
 * Cerebus Python
 *
 * Cython wrapper for cbsdk.h
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#ifndef CBPY_H
#define CBPY_H

#include "cbsdk.h"

int cbpy_version(int nInstance, cbSdkVersion * ver);
int cbpy_open(int nInstance, cbSdkConnectionType conType, cbSdkConnection con);
int cbpy_gettype(int nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType);

#endif // include guard
