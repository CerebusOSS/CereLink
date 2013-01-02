/* =STS=> BmiVersion.h[4256].aa03   open     SMID:3 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2011 Blackrock Microsystems
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

#define BMI_VERSION_MAJOR    6
#define BMI_VERSION_MINOR    3
#define BMI_VERSION_RELEASE  0
#define BMI_VERSION_BETA     2

// Take care of the leading zero
#if BMI_VERSION_MINOR < 10
#define BMI_VERSION_MINOR_FIXED  "0"
#else
#define BMI_VERSION_MINOR_FIXED
#endif
#if BMI_VERSION_RELEASE < 10
#define BMI_VERSION_RELEASE_FIXED "0"
#else
#define BMI_VERSION_RELEASE_FIXED
#endif
#if BMI_VERSION_BETA < 10
#define BMI_VERSION_BETA_FIXED  "0"
#else
#define BMI_VERSION_BETA_FIXED
#endif

#define BMI_VERSION           BMI_VERSION_MAJOR,BMI_VERSION_MINOR,BMI_VERSION_RELEASE,BMI_VERSION_BETA
#if BMI_VERSION_BETA
#define BMI_BETA_PREFIX	" Beta "
#else
#define BMI_BETA_PREFIX "."
#endif

#define BMI_COPYRIGHT_STR     "Copyright (C) 2012-2013 Blackrock Microsystems"
#define BMI_VERSION_STR       STRINGIFY(BMI_VERSION_MAJOR) "." BMI_VERSION_MINOR_FIXED STRINGIFY(BMI_VERSION_MINOR) "." \
    BMI_VERSION_RELEASE_FIXED STRINGIFY(BMI_VERSION_RELEASE) BMI_BETA_PREFIX BMI_VERSION_BETA_FIXED STRINGIFY(BMI_VERSION_BETA)

#endif // include guard
