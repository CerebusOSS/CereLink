///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   platform.h
/// @author CereLink Development Team
/// @brief  Platform compatibility definitions for CCFUtils
///
/// Minimal replacement for old StdAfx.h - provides cross-platform string function compatibility
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CCFUTILS_COMPAT_PLATFORM_H
#define CCFUTILS_COMPAT_PLATFORM_H

#include <cstring>
#include <cstdio>
#include <cstdlib>

// Windows-specific definitions
#ifdef _WIN32
    #ifndef WIN32
        #define WIN32
    #endif

    #include <windows.h>

#endif // _WIN32

#endif // CCFUTILS_COMPAT_PLATFORM_H
