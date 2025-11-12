///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   upstream_protocol.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Wrapper to include upstream protocol definitions
///
/// This header explicitly includes the UPSTREAM cbproto.h (from upstream cbproto.h)
/// to avoid conflicts with the minimal cbproto.h in cbproto.
///
/// TODO: Phase 3 - Remove this file when all packet types are extracted to cbproto
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHMEM_UPSTREAM_PROTOCOL_H
#define CBSHMEM_UPSTREAM_PROTOCOL_H

// Since both cbproto/include/cbproto/ and upstream/cbproto/ are in the include path,
// we need to be explicit about which cbproto.h we want. We want the full upstream one.
//
// First, let's undefine the include guard from cbproto if it was included
#ifdef CBPROTO_TYPES_H
    #undef CBPROTO_TYPES_H
#endif

// Now include the upstream protocol
// upstream/cbproto/cbproto.h already has extern "C" blocks, but we wrap again to be safe
extern "C" {
    #include <cbproto/cbproto.h>
}

// Verify we got the upstream version (it defines cbPKT_PROCINFO, cbproto doesn't)
#ifndef cbPKTTYPE_PROCREP
    #error "Failed to include upstream cbproto.h - packet types not defined. Check include path order."
#endif

// Upstream only defines cbNSP1, but we need cbNSP2-4 for multi-instrument support
// These were added in cbproto, but including that causes typedef conflicts
// So we define them here explicitly
#ifndef cbNSP2
    #define cbNSP2          2   ///< Second instrument ID (1-based)
#endif
#ifndef cbNSP3
    #define cbNSP3          3   ///< Third instrument ID (1-based)
#endif
#ifndef cbNSP4
    #define cbNSP4          4   ///< Fourth instrument ID (1-based)
#endif

#endif // CBSHMEM_UPSTREAM_PROTOCOL_H
