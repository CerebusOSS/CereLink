///////////////////////////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2012 - 2013 Blackrock Microsystems
//
// $Workfile: compat.h $
// $Archive: /Cerebus/WindowsApps/cbhwlib/compat.h $
// $Revision: 1 $
// $Date: 4/29/12 9:55a $
// $Author: Ehsan $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Purpose: This is a x-platform compatibility header file that needs to be the last to include
//            It is used to override possible clashes
//
//            StdAfx.h must be the first
//            compat.h file must be the last
//
//  This header file never should appear in another header file
//

#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

// Windows.h file may introduce min and max unless NOMINMAX is defined globally
#ifndef WIN32
    #undef max
    #undef min
    #define max(a,b) ((a)>(b)?(a):(b))
    #define min(a,b) ((a)<(b)?(a):(b))
#endif

#endif // include guard
