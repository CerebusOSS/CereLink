///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_factory.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Factory for creating protocol-specific device session implementations
///
/// Creates the appropriate DeviceSession implementation based on detected or specified protocol
/// version. Handles protocol auto-detection if version is UNKNOWN.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_FACTORY_H
#define CBDEV_DEVICE_FACTORY_H

#include <cbdev/device_session.h>
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <memory>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Create device session with appropriate protocol implementation
///
/// Factory function that creates the correct DeviceSession implementation based on protocol
/// version. If version is UNKNOWN, automatically detects the device protocol.
///
/// @param config Device configuration (IP addresses, ports, device type)
/// @param version Protocol version (UNKNOWN triggers auto-detection)
/// @return Unique pointer to IDeviceSession implementation, or error
///
/// @note Auto-detection blocks briefly (up to 500ms) to probe device
/// @note For PROTOCOL_CURRENT, creates standard DeviceSession
/// @note For PROTOCOL_311, creates DeviceSession_311 with translation
///
/// @example
/// ```cpp
/// // Auto-detect protocol
/// auto result = createDeviceSession(config, ProtocolVersion::UNKNOWN);
/// if (result.isOk()) {
///     auto device = std::move(result.value());
///     // Use device->receivePackets(), etc.
/// }
///
/// // Force specific protocol
/// auto result = createDeviceSession(config, ProtocolVersion::PROTOCOL_CURRENT);
/// ```
///
Result<std::unique_ptr<IDeviceSession>> createDeviceSession(
    const ConnectionParams& config,
    ProtocolVersion version = ProtocolVersion::UNKNOWN
);

} // namespace cbdev

#endif // CBDEV_DEVICE_FACTORY_H
