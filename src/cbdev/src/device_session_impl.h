///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_impl.h
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

#ifndef CBDEV_DEVICE_SESSION_IMPL_H
#define CBDEV_DEVICE_SESSION_IMPL_H

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#ifdef _WIN32
    typedef SOCKET SocketHandle;
#else
    typedef int SocketHandle;
#endif

#include <cbdev/device_session.h>
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <functional>
#include <chrono>
#include <memory>
#include <cstdint>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Minimal UDP socket wrapper for device communication (current protocol)
///
/// Provides synchronous send/receive operations, with optional receive thread for
/// callback-based packet handling. Implements IDeviceSession for protocol abstraction.
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

    /// Receive packets from device and update configuration
    /// High-level receive that automatically updates device config from received packets.
    /// @param buffer Destination buffer for received data
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received, or error
    /// @note Returns 0 if no data available (EWOULDBLOCK)
    Result<int> receivePackets(void* buffer, size_t buffer_size) override;

    /// Receive packets without updating configuration (used by protocol wrappers)
    /// Low-level socket receive that does not parse or update config.
    /// Protocol wrappers use this to receive untranslated data.
    /// @param buffer Destination buffer for received data
    /// @param buffer_size Maximum bytes to receive
    /// @return Number of bytes received, or error
    /// @note Returns 0 if no data available (EWOULDBLOCK)
    Result<int> receivePacketsRaw(void* buffer, size_t buffer_size);

    /// Send single packet to device
    /// @param pkt Packet to send
    /// @return Success or error
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override;

    /// Send multiple packets to device, coalesced into minimal UDP datagrams
    /// @param pkts Vector of packets to send
    /// @return Success or error
    /// @note Packets are batched into datagrams up to MTU size to reduce packet loss
    Result<void> sendPackets(const std::vector<cbPKT_GENERIC>& pkts) override;

    /// Send raw bytes to device
    /// @param buffer Buffer containing raw bytes
    /// @param size Number of bytes to send
    /// @return Success or error
    Result<void> sendRaw(const void* buffer, size_t size) override;

    /// Check if socket is open
    /// @return true if connected
    [[nodiscard]] bool isConnected() const override;

    /// Get device configuration
    /// @return Configuration reference
    [[nodiscard]] const ConnectionParams& getConnectionParams() const override;

    /// Get protocol version
    /// @return Protocol version (PROTOCOL_CURRENT for this session)
    [[nodiscard]] ProtocolVersion getProtocolVersion() const override;

    /// Get full device configuration
    [[nodiscard]] const cbproto::DeviceConfig& getDeviceConfig() const override;

    /// Get system information
    [[nodiscard]] const cbPKT_SYSINFO& getSysInfo() const override;

    /// Get channel information for specific channel
    [[nodiscard]] const cbPKT_CHANINFO* getChanInfo(uint32_t chan_id) const override;

    /// @}

    /// Close socket (also called by destructor)
    void close();

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Protocol Commands
    /// @{

    /// Set device system runlevel (asynchronous)
    /// Sends cbPKTYPE_SETRUNLEVEL to change device operating state.
    /// Does NOT wait for response - caller must handle SYSREP monitoring.
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @return Success or error
    /// @note For synchronous version, use setSystemRunLevelSync()
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) override;

    /// Request all configuration from the device (asynchronous)
    /// Sends cbPKTTYPE_REQCONFIGALL which triggers the device to send all config packets.
    /// Does NOT wait for response - caller must handle config flood and final SYSREP.
    /// @return Success or error
    /// @note For synchronous version, use requestConfigurationSync()
    Result<void> requestConfiguration() override;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Channel Configuration
    /// @{

    /// Count channels matching a specific type
    [[nodiscard]] size_t countChannelsOfType(ChannelType chanType, size_t maxCount) const override;

    /// Set sampling group for first N channels of a specific type
    Result<void> setChannelsGroupByType(size_t nChans, ChannelType chanType, DeviceRate group_id, bool disableOthers) override;

    Result<void> setChannelsGroupSync(size_t nChans, ChannelType chanType, DeviceRate group_id, std::chrono::milliseconds timeout) override;

    /// Set AC input coupling for first N channels of a specific type
    Result<void> setChannelsACInputCouplingByType(size_t nChans, ChannelType chanType, bool enabled) override;

    /// Set spike sorting options for first N channels
    Result<void> setChannelsSpikeSortingByType(size_t nChans, ChannelType chanType, uint32_t sortOptions) override;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Response Waiting (General Mechanism)
    /// @{

    /// RAII helper for waiting on packet responses
    /// Automatically registers/unregisters matcher with device session
    class ResponseWaiter {
    public:
        /// Wait for the response packet to arrive
        /// @param timeout Maximum time to wait
        /// @return Success if packet received, error on timeout
        Result<void> wait(std::chrono::milliseconds timeout);

        /// Destructor - automatically unregisters from device session
        ~ResponseWaiter();

        // Non-copyable, movable
        ResponseWaiter(const ResponseWaiter&) = delete;
        ResponseWaiter& operator=(const ResponseWaiter&) = delete;
        ResponseWaiter(ResponseWaiter&&) noexcept;
        ResponseWaiter& operator=(ResponseWaiter&&) noexcept;

    private:
        friend class DeviceSession;

        struct Impl;  // Forward declaration - defined in .cpp
        ResponseWaiter(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> m_impl;
    };

    /// Register a response waiter that will be notified when a matching packet arrives
    /// @param matcher Function that returns true when the desired packet is received
    /// @param count Number of matching packets to wait for (default: 1)
    /// @return ResponseWaiter object - call wait() to block until packet arrives
    /// @note The matcher is checked against all configuration packets in updateConfigFromBuffer
    ResponseWaiter registerResponseWaiter(std::function<bool(const cbPKT_HEADER*)> matcher, size_t count = 1);

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Synchronous Protocol Commands
    /// @{

    /// Request configuration synchronously (blocks until SYSREP received)
    /// @param timeout Maximum time to wait
    /// @return Success if config received, error on timeout or send failure
    Result<void> requestConfigurationSync(std::chrono::milliseconds timeout) override;

    /// Set system runlevel synchronously (blocks until SYSREPRUNLEV received)
    /// @param runlevel Desired runlevel
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @param timeout Maximum time to wait
    /// @return Success if response received, error on timeout or send failure
    Result<void> setSystemRunLevelSync(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                       std::chrono::milliseconds timeout) override;

    /// Set AC input coupling synchronously (blocks until CHANREP received)
    /// @param nChans Number of channels to configure
    /// @param chanType Channel type filter
    /// @param enabled true to enable AC coupling, false to disable
    /// @param timeout Maximum time to wait
    /// @return Success if response received, error on timeout or send failure
    Result<void> setChannelsACInputCouplingSync(size_t nChans, ChannelType chanType, bool enabled,
                                                 std::chrono::milliseconds timeout) override;

    Result<void> setChannelsSpikeSortingSync(size_t nChans, ChannelType chanType, uint32_t sortOptions, std::chrono::milliseconds timeout) override;

    /// Perform complete device handshake sequence (synchronous)
    /// @param timeout Maximum total time for entire handshake sequence
    /// @return Success if device reaches RUNNING state, error otherwise
    Result<void> performHandshakeSync(std::chrono::milliseconds timeout) override;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Configuration Management
    /// @{

    /// Update device configuration from received packet buffer
    /// Parses the buffer for configuration packets and updates internal config accordingly.
    /// This should be called after receiving packets (and after protocol translation for wrappers).
    /// @param buffer Buffer containing packets in current protocol format
    /// @param bytes Number of bytes in buffer
    void updateConfigFromBuffer(const void* buffer, size_t bytes);

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Receive Thread and Callbacks
    /// @{

    /// Register a callback to be invoked for each received packet
    /// @param callback Function to call for each packet
    /// @return Handle for unregistration, or 0 on failure
    /// @note Callbacks run on the receive thread - keep them fast to avoid packet loss!
    /// @note Multiple callbacks can be registered and will be called in registration order
    CallbackHandle registerReceiveCallback(ReceiveCallback callback) override;

    /// Register a callback to be invoked after all packets in a datagram are processed
    /// @param callback Function to call after datagram processing
    /// @return Handle for unregistration, or 0 on failure
    /// @note Use this for batch operations like signaling shared memory
    CallbackHandle registerDatagramCompleteCallback(DatagramCompleteCallback callback) override;

    /// Unregister a previously registered callback
    /// @param handle Handle returned by registerReceiveCallback or registerDatagramCompleteCallback
    void unregisterCallback(CallbackHandle handle) override;

    /// Start the receive thread
    /// @return Success or error if thread cannot be started
    /// @note Thread calls receivePackets() in a loop and invokes registered callbacks
    Result<void> startReceiveThread() override;

    /// Stop the receive thread
    /// @note Blocks until thread terminates
    void stopReceiveThread() override;

    /// Check if receive thread is running
    /// @return true if thread is active
    [[nodiscard]] bool isReceiveThreadRunning() const override;

    /// @}

private:
    /// Private constructor (use create() factory)
    DeviceSession() = default;

    /// Helper method to check if a channel matches a specific type based on capabilities
    /// @param chaninfo Channel information packet
    /// @param chanType Channel type to check
    /// @return true if channel matches the type
    static bool channelMatchesType(const cbPKT_CHANINFO& chaninfo, ChannelType chanType);

    /// Helper for synchronous send-and-wait pattern
    /// @param sender Function that sends the request packet
    /// @param matcher Function that identifies the response packet
    /// @param timeout Maximum time to wait for response
    /// @param count Number of matching packets to wait for (default: 1)
    /// @return Success if response received, error on timeout or send failure
    Result<void> sendAndWait(
        const std::function<Result<void>()>& sender,
        std::function<bool(const cbPKT_HEADER*)> matcher,
        std::chrono::milliseconds timeout,
        size_t count = 1
    );

    /// Implementation details (pImpl pattern)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_IMPL_H
