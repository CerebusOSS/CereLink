///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_version.h
/// @author Caden Shmookler
/// @date   2026-06-25
///
/// @brief  Protocol version detection for Central
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_VERSION_H
#define CBSHM_CENTRAL_VERSION_H 

#include <stdint.h>
#include <cbutil/result.h>
#include <cbproto/connection.h>

namespace cbshm {

enum class CentralVersion {
    UNKNOWN,
    V7_0,
    V7_5,
    V7_6,
    V7_7,
    CURRENT, // V7_8
};

/// @brief Detect the protocol version from Central's binary.
cbutil::Result<CentralVersion> detectCentralVersion();

/// @brief Convert the Central version to the protocol version.
cbproto_protocol_version_t getProtocolVersion(CentralVersion version);

} // namespace cbshm

#endif // CBSHM_CENTRAL_VERSION_H 
