///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_interface.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Device session interface for protocol abstraction
///
/// Defines the interface for device communication, allowing different protocol implementations
/// (e.g., current protocol vs legacy cbproto_311) without affecting SDK code.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_INTERFACE_H
#define CBDEV_DEVICE_SESSION_INTERFACE_H

#include <cbdev/device_conn_params.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <cstddef>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Abstract interface for device communication
///
/// Implementations handle protocol-specific details (e.g., packet format translation for legacy
/// devices) while presenting a uniform interface to SDK.
///
class IDeviceSession {
public:
    virtual ~IDeviceSession() = default;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Packet Reception
    /// @{

    /// Receive UDP datagram from device into provided buffer
    /// @param buffer Destination buffer for received data
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received, or error
    /// @note Non-blocking. Returns 0 if no data available.
    /// @note For protocol-translating implementations, translation happens before returning
    virtual Result<int> receivePackets(void* buffer, size_t buffer_size) = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Packet Transmission
    /// @{

    /// Send single packet to device
    /// @param pkt Packet to send (in current protocol format)
    /// @return Success or error
    /// @note For protocol-translating implementations, translation happens before sending
    virtual Result<void> sendPacket(const cbPKT_GENERIC& pkt) = 0;

    /// Send multiple packets to device
    /// @param pkts Array of packets to send
    /// @param count Number of packets in array
    /// @return Success or error
    virtual Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count) = 0;

    /// Send raw bytes to device (for protocol translation)
    /// @param buffer Buffer containing raw bytes to send
    /// @param size Number of bytes to send
    /// @return Success or error
    /// @note This is primarily for use by protocol wrapper implementations
    virtual Result<void> sendRaw(const void* buffer, size_t size) = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name State
    /// @{

    /// Check if device connection is active
    /// @return true if socket is open and connected
    virtual bool isConnected() const = 0;

    /// Get device configuration
    /// @return Current device configuration
    virtual const ConnectionParams& getConfig() const = 0;

    /// @}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_INTERFACE_H
