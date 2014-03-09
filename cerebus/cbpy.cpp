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

