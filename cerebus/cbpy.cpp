/*
 * Cerebus Python
 *
 * @date March 9, 2014
 * @author: dashesy
 */

#include "cbpy.h"

int cbpy_version(int nInstance, cbSdkVersion *ver)
{
    cbSdkResult sdkres = cbSdkGetVersion(nInstance, ver);

    return sdkres;
}

int cbpy_open(int nInstance, cbSdkConnectionType conType, cbSdkConnection con)
{
    cbSdkResult sdkres = cbSdkOpen(nInstance, conType, con);

    return sdkres;
}

int cbpy_gettype(int nInstance, cbSdkConnectionType * conType, cbSdkInstrumentType * instType)
{
    cbSdkResult sdkres = cbSdkGetType(nInstance, conType, instType);

    return sdkres;
}
