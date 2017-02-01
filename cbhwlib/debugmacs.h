/* =STS=> debugmacs.h[1694].aa08   open     SMID:9 */
//////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003-2008 Cyberkinetics, Inc.
// (c) Copyright 2008-2011 Blackrock Microsystems
//
// $Workfile: debugmacs.h $
// $Archive: /Cerebus/WindowsApps/ConfigDlgs/debugmacs.h $
// $Revision: 1 $
// $Date: 9/30/03 3:19p $
// $Author: Awang $
//
// $NoKeywords: $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DEBUGMACS_H_INCLUDED       // include guards
#define DEBUGMACS_H_INCLUDED


// Macro Definitions -------------------------------------------
//

#ifdef DEBUG
    #ifndef _DEBUG
        #define _DEBUG
    #endif
#endif

// If you have VERBOSE turned on, then turn on all of the Debugs as well
#ifdef VERBOSE_DEBUG
    #ifndef _DEBUG
        #define _DEBUG
    #endif
#endif

// A simple example of how to use these
// DEBUG_PRINTF("This is an int %i", myInt);
//
// The above code will then only be written if the DEBUG or VERBOSE_DEBUG macros
// are turned on

#ifdef _DEBUG
    #ifdef WIN32
        #ifndef _CRT_SECURE_NO_DEPRECATE
            #define _CRT_SECURE_NO_DEPRECATE
        #endif
        #ifndef _CRT_SECURE_NO_WARNINGS
            #define _CRT_SECURE_NO_WARNINGS
        #endif
        #ifdef NO_AFX
            #include <winsock2.h>
            #include <windows.h>
        #endif
        #include <conio.h>
        #ifdef TRACE
            #undef TRACE
        #endif
        #ifdef _CONSOLE
            #define TRACE printf
        #else
            #define TRACE _cprintf
        #endif
    #else
        #define TRACE printf
    #endif
    #ifndef ASSERT
        #include <assert.h>
        #define ASSERT assert
    #endif
    // Ok.. We want debugging output
    #define DEBUG_CODE(X)   X

    #ifdef __KERNEL__
        #define DEBUG_PRINTF(FMT, ARGS...) printk(FMT, ## ARGS)


        #ifdef VERBOSE_DEBUG
            #define VERBOSE_DEBUG_PRINTF(FMT, ARGS...) printk(FMT, ## ARGS)
        #else
            #define VERBOSE_DEBUG_PRINTF(FMT, ARGS...)
        #endif


    #endif
#else
    #define DEBUG_CODE(X)
    #ifndef TRACE
        #ifndef _MSC_VER
            #define TRACE(FMT, ARGS...)
        #else
            #define TRACE(FMT, ...)
        #endif
    #endif
    #ifndef ASSERT
        #define ASSERT(X)
    #endif
    #ifndef _ASSERT
        #define _ASSERT(X)
    #endif
#endif

#ifndef WIN32
#define _cprintf printf
#endif

#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF TRACE
#endif

#endif  // include guard
