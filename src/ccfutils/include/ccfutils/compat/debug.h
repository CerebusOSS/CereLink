///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   debug.h
/// @author CereLink Development Team
/// @brief  Debug macros for CCFUtils
///
/// Minimal replacement for old debugmacs.h - provides ASSERT and TRACE macros
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CCFUTILS_COMPAT_DEBUG_H
#define CCFUTILS_COMPAT_DEBUG_H

#include <cassert>
#include <cstdio>

// ASSERT macro - uses standard assert in debug builds, no-op in release
#ifndef ASSERT
    #ifdef NDEBUG
        #define ASSERT(x) ((void)0)
    #else
        #define ASSERT(x) assert(x)
    #endif
#endif

// TRACE macro - prints to stderr in debug builds, no-op in release
#ifndef TRACE
    #ifdef NDEBUG
        // No-op in release builds
        #ifdef _MSC_VER
            #define TRACE(...) ((void)0)
        #else
            #define TRACE(fmt, ...) ((void)0)
        #endif
    #else
        // Print to stderr in debug builds
        #ifdef _MSC_VER
            #define TRACE(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
        #else
            #define TRACE(fmt, args...) fprintf(stderr, fmt, ##args)
        #endif
    #endif
#endif

// Legacy _DEBUG define (some code checks for this)
#ifdef DEBUG
    #ifndef _DEBUG
        #define _DEBUG
    #endif
#endif

#endif // CCFUTILS_COMPAT_DEBUG_H
