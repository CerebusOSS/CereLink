///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   protocol_detector.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Protocol version detection for device communication
///
/// Detects the protocol version used by a device by sending a probe packet and analyzing
/// the response format (e.g., 32-bit vs 64-bit timestamps).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_PROTOCOL_DETECTOR_H
#define CBDEV_PROTOCOL_DETECTOR_H

#include <cbdev/result.h>
#include <cstdint>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Protocol version enumeration
///
enum class ProtocolVersion {
    UNKNOWN,          ///< Unknown or undetected protocol
    PROTOCOL_311,     ///< Legacy cbproto_311 (32-bit timestamps, deprecated)
    PROTOCOL_400,     ///< Legacy cbproto_400 (64-bit timestamps, deprecated)
    PROTOCOL_CURRENT  ///< Current protocol (64-bit timestamps)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Convert protocol version to string for logging
/// @param version Protocol version
/// @return Human-readable string
///
const char* protocolVersionToString(ProtocolVersion version);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Detect protocol version by probing the device
///
/// Sends a probe packet to the device and analyzes the response to determine protocol version.
/// This is a blocking operation that may take up to a few hundred milliseconds.
///
/// @param device_addr Device IP address
/// @param send_port Device receives config packets on this port
/// @param client_addr Client IP address (for binding)
/// @param recv_port Client UDP port (for binding)
/// @param timeout_ms Timeout in milliseconds (default: 500ms)
/// @return Detected protocol version, or error
///
/// @note Creates temporary socket for probing, then closes it
/// @note Returns UNKNOWN if device doesn't respond within timeout
///
Result<ProtocolVersion> detectProtocol(const char* device_addr, uint16_t send_port,
                                       const char* client_addr, uint16_t recv_port,
                                       uint32_t timeout_ms = 500);

} // namespace cbdev

#endif // CBDEV_PROTOCOL_DETECTOR_H
