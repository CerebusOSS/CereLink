///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_410.h
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 4.10 wrapper for DeviceSession
///
/// DeviceSession_410 wraps a standard DeviceSession and translates packets between protocol 4.10
/// format and the current protocol format (4.2+). This allows 4.10 devices to work seamlessly
/// with the modern SDK.
///
/// Protocol 4.10 header differences (compared to 4.2+):
/// - 64-bit timestamp (same as 4.2+)
/// - 8-bit type field (vs 16-bit in 4.2+)  <-- KEY DIFFERENCE
/// - 16-bit dlen field (same as 4.2+)
/// - 8-bit instrument field (same as 4.2+)
/// - 16-bit reserved field (vs 8-bit in 4.2+)  <-- Different size
/// - Header size: 16 bytes (same as 4.2+, but different internal layout)
///
/// Byte offset differences:
/// - 4.10:  time(0-7) chid(8-9) type(10) dlen(11-12) instrument(13) reserved(14-15)
/// - 4.2+: time(0-7) chid(8-9) type(10-11) dlen(12-13) instrument(14) reserved(15)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_410_H
#define CBDEV_DEVICE_SESSION_410_H

#include <cbdev/device_session.h>
#include "device_session_impl.h"
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <memory>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Protocol 4.10 wrapper for device communication
///
/// Translates packets between protocol 4.10 format and current format (4.2+).
/// All actual socket I/O is delegated to the wrapped DeviceSession.
///
class DeviceSession_410 : public IDeviceSession {
public:
    /// Create protocol 4.10 wrapper around a device session
    /// @param config Device configuration (IP addresses, ports, device type)
    /// @return DeviceSession_410 on success, error on failure
    static Result<DeviceSession_410> create(const ConnectionParams& config);

    /// Move constructor
    DeviceSession_410(DeviceSession_410&&) noexcept = default;

    /// Move assignment
    DeviceSession_410& operator=(DeviceSession_410&&) noexcept = default;

    /// Destructor
    ~DeviceSession_410() override = default;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name IDeviceSession Implementation
    /// @{

    /// Receive packets from device and translate from 4.10 to current format
    /// @param buffer Destination buffer for received data (current format)
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received (in current format), or error
    /// @note Translation happens automatically: 4.10 -> current
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Send packet to device, translating from current to 4.10 format
    /// @param pkt Packet to send (in current format)
    /// @return Success or error
    /// @note Translation happens automatically: current -> 4.10
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override;

    /// Send multiple packets to device, translating each one
    /// @param pkts Array of packets (in current format)
    /// @param count Number of packets
    /// @return Success or error
    Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count) override;

    /// Send raw bytes (pass-through to underlying device)
    /// @param buffer Buffer containing raw bytes
    /// @param size Number of bytes
    /// @return Success or error
    Result<void> sendRaw(const void* buffer, size_t size) override;

    /// Set device system runlevel (delegated to wrapped device)
    /// @param runlevel Desired runlevel
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @return Success or error
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) override;

    /// Request configuration from device (delegated to wrapped device)
    /// @return Success or error
    Result<void> requestConfiguration() override;

    /// Check if underlying device connection is active
    /// @return true if connected
    [[nodiscard]] bool isConnected() const override;

    /// Get device configuration
    /// @return Configuration reference
    [[nodiscard]] const ConnectionParams& getConnectionParams() const override;

    /// Get protocol version
    /// @return PROTOCOL_410
    [[nodiscard]] ProtocolVersion getProtocolVersion() const override;

    /// @}

private:
    /// Private constructor taking a DeviceSession
    explicit DeviceSession_410(DeviceSession&& device)
        : m_device(std::move(device)) {}

    /// Wrapped device session for actual I/O
    DeviceSession m_device;
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_410_H
