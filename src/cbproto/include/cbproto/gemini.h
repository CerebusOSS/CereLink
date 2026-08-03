///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   gemini.h
/// @author CereLink Development Team
///
/// @brief  Single source of truth for classifying a device as a Gemini system.
///
/// Gemini processors report a processor @c ident containing "gemini"
/// (case-insensitive) and timestamp their packets in nanoseconds from a shared
/// PTP clock. All other devices (legacy NSP, nPlay, custom) send sample-count
/// timestamps derived from an independent per-device clock. That distinction
/// drives two things: which timestamp unit conversion to apply, and whether a
/// device participates in cross-device PTP clock consensus.
///
/// Both the direct-to-device path (cbdev DeviceSession) and the shared-memory
/// config mirror (cbsdk SdkSession, which publishes the result into the native
/// status buffer for CLIENT readers) key off this helper so the two never drift
/// apart.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_GEMINI_H
#define CBPROTO_GEMINI_H

#include "types.h"

#ifdef __cplusplus

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>

namespace cbproto {

/// @brief Determine whether a processor ident denotes a Gemini system.
///
/// @param ident      Processor ident buffer (may be non-null-terminated).
/// @param ident_size Size of the ident buffer in bytes.
/// @return true if the ident contains "gemini" (case-insensitive).
inline bool identIsGemini(const char* ident, const std::size_t ident_size) {
    if (ident == nullptr || ident_size == 0) {
        return false;
    }
    const std::size_t len = strnlen(ident, ident_size);
    static constexpr char needle[] = "gemini";
    return std::search(
               ident, ident + len,
               std::begin(needle), std::end(needle) - 1,  // exclude null terminator
               [](const char a, const char b) {
                   return std::tolower(static_cast<unsigned char>(a)) == b;
               }) != ident + len;
}

/// @brief Determine whether a PROCINFO packet denotes a Gemini system.
///
/// @param procinfo Processor info packet (as received in a PROCREP).
/// @return true if the processor ident contains "gemini" (case-insensitive).
inline bool procInfoIsGemini(const cbPKT_PROCINFO& procinfo) {
    return identIsGemini(procinfo.ident, sizeof(procinfo.ident));
}

}  // namespace cbproto

#endif  // __cplusplus

#endif  // CBPROTO_GEMINI_H
