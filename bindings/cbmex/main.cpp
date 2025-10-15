// =STS=> main.cpp[2461].aa17   open     SMID:18 

///////////////////////////////////////////////////////////////////////
//
// Cerebus SDK
//
// (c) Copyright 2012-2013 Blackrock Microsystems
//
// $Workfile: main.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/main.cpp $
// $Revision: 1 $
// $Date: 04/29/12 11:06a $
// $Author: Ehsan $
//
// Purpose:
//  Produce SDK version information
//
//

#include "StdAfx.h"
#include "../CentralCommon/BmiVersion.h"

// Author & Date: Ehsan Azar       29 April 2012
// Purpose: Print out the compatible library version
int main(int argc, char *argv[])
{
    int major = BMI_VERSION_MAJOR;
    int minor = BMI_VERSION_MINOR;
    int release = BMI_VERSION_RELEASE;

    // We assume the beta releases all share the same ABI
    printf("%d.%02d.%02d", major, minor, release);

    return 0;
}
