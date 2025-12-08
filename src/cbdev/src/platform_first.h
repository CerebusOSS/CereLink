///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   platform_first.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Platform headers that MUST be included before cbproto headers
///
/// IMPORTANT: This header must be included FIRST in any source file that uses both
/// Windows headers and cbproto headers.
///
/// Reason: cbproto uses #pragma pack(1) for network structures. Windows SDK headers
/// (specifically winnt.h) have a static_assert that fails if packing is not at the
/// default setting when they are included. This header ensures Windows headers are
/// processed before any pack directives are active.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_PLATFORM_FIRST_H
#define CBDEV_PLATFORM_FIRST_H

#ifdef _WIN32
    // Windows headers must be included before cbproto headers due to packing requirements
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>
#endif

#endif // CBDEV_PLATFORM_FIRST_H
