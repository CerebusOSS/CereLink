//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2010 - 2023 Blackrock Microsystems
//
// $Workfile: BmiVersion.h $
// $Archive: /common/BmiVersion.h $
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

#define BMI_VERSION_MAJOR    7
#define BMI_VERSION_MINOR    5
#define BMI_VERSION_RELEASE  1
#define BMI_VERSION_BETA     3

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

#define BMI_COPYRIGHT_STR     "Copyright (C) 2012-2023 Blackrock Neurotech"
#define BMI_VERSION_STR       STRINGIFY(BMI_VERSION_MAJOR) "." BMI_VERSION_MINOR_FIXED STRINGIFY(BMI_VERSION_MINOR) "." \
    BMI_VERSION_RELEASE_FIXED STRINGIFY(BMI_VERSION_RELEASE) BMI_BETA_PREFIX BMI_VERSION_BETA_FIXED STRINGIFY(BMI_VERSION_BETA)

#define BMI_VERSION_APP_STR       STRINGIFY(BMI_VERSION_MAJOR) "." STRINGIFY(BMI_VERSION_MINOR) \
    " Build " STRINGIFY(BMI_VERSION_BUILD)
#endif // include guard
