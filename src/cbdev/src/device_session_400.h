///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_400.h
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 4.0 wrapper for DeviceSession
///
/// DeviceSession_400 wraps a standard DeviceSession and translates packets between protocol 4.0
/// format and the current protocol format (4.1+). This allows 4.0 devices to work seamlessly
/// with the modern SDK.
///
/// Protocol 4.0 header differences (compared to 4.1+):
/// - 64-bit timestamp (same as 4.1+)
/// - 8-bit type field (vs 16-bit in 4.1+)  <-- KEY DIFFERENCE
/// - 16-bit dlen field (same as 4.1+)
/// - 8-bit instrument field (same as 4.1+)
/// - 16-bit reserved field (vs 8-bit in 4.1+)  <-- Different size
/// - Header size: 16 bytes (same as 4.1+, but different internal layout)
///
/// Byte offset differences:
/// - 4.0:  time(0-7) chid(8-9) type(10) dlen(11-12) instrument(13) reserved(14-15)
/// - 4.1+: time(0-7) chid(8-9) type(10-11) dlen(12-13) instrument(14) reserved(15)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_400_H
#define CBDEV_DEVICE_SESSION_400_H

#include "device_session_wrapper.h"
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Protocol 4.0 wrapper for device communication
///
/// Translates packets between protocol 4.0 format and current format (4.1+).
/// Inherits from DeviceSessionWrapper which handles all delegation automatically.
///
class DeviceSession_400 : public DeviceSessionWrapper {
public:
    /// Create protocol 4.0 wrapper around a device session
    /// @param config Device configuration (IP addresses, ports, device type)
    /// @return DeviceSession_400 on success, error on failure
    static Result<DeviceSession_400> create(const ConnectionParams& config);

    /// Move constructor
    DeviceSession_400(DeviceSession_400&&) noexcept = default;

    /// Move assignment
    DeviceSession_400& operator=(DeviceSession_400&&) noexcept = default;

    /// Destructor
    ~DeviceSession_400() override = default;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Protocol-Specific Overrides
    /// @{

    /// Receive packets from device and translate from 4.0 to current format
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Send packet to device, translating from current to 4.0 format
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override;

    /// Send multiple packets to device, translating each one
    Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count) override;

    /// Send raw bytes (pass-through to underlying device)
    Result<void> sendRaw(const void* buffer, size_t size) override;

    /// Get protocol version
    [[nodiscard]] ProtocolVersion getProtocolVersion() const override;

    /// @}

private:
    /// Private constructor taking a DeviceSession
    explicit DeviceSession_400(DeviceSession&& device)
        : DeviceSessionWrapper(std::move(device)) {}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_400_H
