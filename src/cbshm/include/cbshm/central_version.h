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
    V7_0,
    V7_5,
    V7_6,
    V7_7,
    CURRENT, // V7_8
};

/// @brief Whether a Central.exe process is running on this machine.
///
/// Independent of whether its version is one CereLink supports: a running
/// Central owns the instrument sockets either way.  Always false off Windows.
bool isCentralRunning();

/// @brief Detect the protocol version from Central's binary.
cbutil::Result<CentralVersion> detectCentralVersion();

/// @brief Convert the Central version to the protocol version.
cbproto_protocol_version_t getProtocolVersion(CentralVersion version);

/// @brief Central's zero-based instrument slot for a device type.
///
/// Central assigns slots at compile time (CentralDlg.cpp, GEMSTART == 2),
/// and the assignment changed as Hub support grew:
///
///   | Central | slots | HUB1 | HUB2 | HUB3 | NSP |
///   |---------|-------|------|------|------|-----|
///   | 7.0     | 1     |  -   |  -   |  -   |  -  |
///   | 7.5     | 2     |  1   |  -   |  -   |  0  |
///   | 7.6/7.7 | 3     |  0   |  1   |  -   |  2  |
///   | 7.8     | 4     |  0   |  1   |  2   |  3  |
///
/// A legacy (non-Gemini) NSP is always slot 0.
///
/// @return the slot, or -1 if this Central version never connects to that
///         device type (it then cannot be the device's owner either).
int32_t getCentralInstrumentIndex(CentralVersion version, cbproto_device_type_t device);

} // namespace cbshm

#endif // CBSHM_CENTRAL_VERSION_H 
