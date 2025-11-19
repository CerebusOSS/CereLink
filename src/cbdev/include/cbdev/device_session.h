///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Minimal UDP socket wrapper for Cerebus device communication
///
/// DeviceSession is a thin wrapper around UDP sockets for communicating with Cerebus devices.
/// It provides only socket operations - no threads, no callbacks, no statistics, no parsing.
/// All orchestration logic (threads, statistics, callbacks, parsing) is handled by SdkSession.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_H
#define CBDEV_DEVICE_SESSION_H

#include <cbdev/device_session_interface.h>
#include <cbdev/device_conn_params.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET SocketHandle;
#else
    typedef int SocketHandle;
#endif

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Minimal UDP socket wrapper for device communication (current protocol)
///
/// Provides synchronous send/receive operations only. No threads, callbacks, or state management.
/// Implements IDeviceSession for protocol abstraction.
///
class DeviceSession : public IDeviceSession {
public:
    /// Move constructor
    DeviceSession(DeviceSession&&) noexcept;

    /// Move assignment
    DeviceSession& operator=(DeviceSession&&) noexcept;

    /// Destructor - closes socket
    ~DeviceSession() override;

    /// Create and initialize device session
    /// @param config Device configuration (IP addresses, ports, device type)
    /// @return DeviceSession on success, error on failure
    static Result<DeviceSession> create(const ConnectionParams& config);

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name IDeviceSession Implementation
    /// @{

    /// Receive UDP datagram from device (non-blocking)
    /// @param buffer Destination buffer for received data
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received, or error
    /// @note Returns 0 if no data available (EWOULDBLOCK)
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Send single packet to device
    /// @param pkt Packet to send
    /// @return Success or error
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override;

    /// Send multiple packets to device
    /// @param pkts Array of packets
    /// @param count Number of packets
    /// @return Success or error
    Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count) override;

    /// Send raw bytes to device
    /// @param buffer Buffer containing raw bytes
    /// @param size Number of bytes to send
    /// @return Success or error
    Result<void> sendRaw(const void* buffer, size_t size) override;

    /// Check if socket is open
    /// @return true if connected
    bool isConnected() const override;

    /// Get device configuration
    /// @return Configuration reference
    const ConnectionParams& getConfig() const override;

    /// @}

    /// Close socket (also called by destructor)
    void close();

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Protocol Commands
    /// @{

    /// Send a runlevel command packet to the device
    /// Creates cbPKT_SYSINFO with specified parameters and sends it.
    /// Does NOT wait for response - caller must handle SYSREP monitoring.
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @return Success or error
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque = 0, uint32_t runflags = 0);

    /// Request all configuration from the device
    /// Sends cbPKTTYPE_REQCONFIGALL which triggers the device to send all config packets.
    /// Does NOT wait for response - caller must handle config flood and final SYSREP.
    /// @return Success or error
    Result<void> requestConfiguration();

    /// @}

private:
    /// Private constructor (use create() factory)
    DeviceSession() = default;

    /// Implementation details (pImpl pattern)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_H
