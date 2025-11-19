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

#include <cbdev/device_session_interface.h>
#include <cbdev/device_session.h>
#include <cbdev/device_conn_params.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <memory>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Protocol 3.11 wrapper for device communication
///
/// Translates packets between protocol 3.11 format and current format (4.1+).
/// All actual socket I/O is delegated to the wrapped DeviceSession.
///
class DeviceSession_311 : public IDeviceSession {
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
    /// @name IDeviceSession Implementation
    /// @{

    /// Receive packets from device and translate from 3.11 to current format
    /// @param buffer Destination buffer for received data (current format)
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received (in current format), or error
    /// @note Translation happens automatically: 3.11 -> current
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Send packet to device, translating from current to 3.11 format
    /// @param pkt Packet to send (in current format)
    /// @return Success or error
    /// @note Translation happens automatically: current -> 3.11
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

    /// Check if underlying device connection is active
    /// @return true if connected
    bool isConnected() const override;

    /// Get device configuration
    /// @return Configuration reference
    const ConnectionParams& getConfig() const override;

    /// @}

private:
    /// Private constructor taking a DeviceSession
    explicit DeviceSession_311(DeviceSession&& device)
        : m_device(std::move(device)) {}

    /// Wrapped device session for actual I/O
    DeviceSession m_device;
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_311_H
