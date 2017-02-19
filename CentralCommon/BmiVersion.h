/* =STS=> BmiVersion.h[4256].aa04   submit   SMID:5 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2014 Blackrock Microsystems
//
// $Workfile: BmiVersion.h $
// $Archive: /CentralCommon/BmiVersion.h $
// $Revision: 1 $
// $Date: 7/19/10 9:52a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////
//
// PURPOSE:
//
// Blackrock Microsystems product version information
//

#ifndef BMIVERSION_H_INCLUDED
#define BMIVERSION_H_INCLUDED

#define STR(s) #s
#define STRINGIFY(s) STR(s)

#define BMI_VERSION_DATE_BUILT      03 October 2016

#define BMI_VERSION_MAJOR       6
#define BMI_VERSION_MINOR       5
#define BMI_VERSION_REVISION    4
#define BMI_VERSION_BUILD       0

#define BMI_VERSION           BMI_VERSION_MAJOR,BMI_VERSION_MINOR,BMI_VERSION_BUILD_NUMBER

#define BMI_COPYRIGHT_S       "Copyright (C) " STRINGIFY(BMI_COPYRIGHT_FROM) " - " STRINGIFY(BMI_COPYRIGHT_TO) " Blackrock Microsystems"
#define BMI_COPYRIGHT_STR       BMI_COPYRIGHT_S

#if BMI_VERSION_BUILD == 0
#define BMI_VERSION_STR       STRINGIFY(BMI_VERSION_MAJOR) "." STRINGIFY(BMI_VERSION_MINOR) \
    "." STRINGIFY(BMI_VERSION_REVISION)
#else
#define BMI_VERSION_STR       STRINGIFY(BMI_VERSION_MAJOR) "." STRINGIFY(BMI_VERSION_MINOR) \
    "." STRINGIFY(BMI_VERSION_REVISION) " Build " STRINGIFY(BMI_VERSION_BUILD)
#endif

#define BMI_VERSION_APP_STR       STRINGIFY(BMI_VERSION_MAJOR) "." STRINGIFY(BMI_VERSION_MINOR) \
    " Build " STRINGIFY(BMI_VERSION_BUILD_NUMBER)

#endif // include guard
