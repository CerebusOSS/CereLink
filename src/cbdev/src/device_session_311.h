///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_311.h
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 3.11 wrapper for DeviceSession
///
/// DeviceSession_311 wraps a standard DeviceSession and translates packets between protocol 3.11
/// format and the current protocol format (4.1+). This allows older devices to work seamlessly
/// with the modern SDK.
///
/// Protocol 3.11 header differences:
/// - 32-bit timestamp (vs 64-bit in 4.1+)
/// - 8-bit type field (vs 16-bit in 4.1+)
/// - 8-bit dlen field (vs 16-bit in 4.1+)
/// - No instrument/reserved fields
/// - Header size: 8 bytes (vs 16 bytes in 4.1+)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_311_H
#define CBDEV_DEVICE_SESSION_311_H

#include "device_session_wrapper.h"
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Protocol 3.11 wrapper for device communication
///
/// Translates packets between protocol 3.11 format and current format (4.1+).
/// Inherits from DeviceSessionWrapper which handles all delegation automatically.
///
class DeviceSession_311 : public DeviceSessionWrapper {
public:
    /// Create protocol 3.11 wrapper around a device session
    /// @param config Device configuration (IP addresses, ports, device type)
    /// @return DeviceSession_311 on success, error on failure
    static Result<DeviceSession_311> create(const ConnectionParams& config);

    /// Move constructor
    DeviceSession_311(DeviceSession_311&&) noexcept = default;

    /// Move assignment
    DeviceSession_311& operator=(DeviceSession_311&&) noexcept = default;

    /// Destructor
    ~DeviceSession_311() override = default;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Protocol-Specific Overrides
    /// @{

    /// Receive packets from device and translate from 3.11 to current format
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Send packet to device, translating from current to 3.11 format
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override;

    /// Send raw bytes (pass-through to underlying device)
    Result<void> sendRaw(const void* buffer, size_t size) override;

    /// Get protocol version
    [[nodiscard]] ProtocolVersion getProtocolVersion() const override;

    /// @}

private:
    /// Private constructor taking a DeviceSession
    explicit DeviceSession_311(DeviceSession&& device)
        : DeviceSessionWrapper(std::move(device)) {}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_311_H
