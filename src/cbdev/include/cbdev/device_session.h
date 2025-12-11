///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session.h
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

#include <chrono>
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <cbproto/config.h>
#include <cstddef>
#include <functional>
#include <vector>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Callback Types for Receive Thread
/// @{

/// Callback invoked for each received packet
/// @param pkt The received packet (reference valid only during callback invocation)
/// @note Callbacks run on the receive thread - keep them fast to avoid packet loss!
using ReceiveCallback = std::function<void(const cbPKT_GENERIC& pkt)>;

/// Callback invoked after all packets in a UDP datagram have been processed
/// @note Use this for batch operations like signaling shared memory
using DatagramCompleteCallback = std::function<void()>;

/// Handle for callback registration (used for unregistration)
using CallbackHandle = uint32_t;

/// @}

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

    /// Send multiple packets to device, coalesced into minimal UDP datagrams
    /// @param pkts Vector of packets to send
    /// @return Success or error
    /// @note Packets are batched into datagrams up to MTU size to reduce packet loss
    virtual Result<void> sendPackets(const std::vector<cbPKT_GENERIC>& pkts) = 0;

    /// Send raw bytes to device (for protocol translation)
    /// @param buffer Buffer containing raw bytes to send
    /// @param size Number of bytes to send
    /// @return Success or error
    /// @note This is primarily for use by protocol wrapper implementations
    virtual Result<void> sendRaw(const void* buffer, size_t size) = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Device Control
    /// @{

    /// Set device system runlevel (pure virtual - must be implemented)
    /// Sends cbPKTYPE_SETRUNLEVEL to change device operating state.
    /// Does NOT wait for response - caller must handle SYSREP monitoring.
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @return Success or error
    virtual Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) = 0;

    /// Convenience overload with default resetque
    Result<void> setSystemRunLevel(const uint32_t runlevel, const uint32_t resetque) {
        return setSystemRunLevel(runlevel, resetque, 0);
    }

    /// Convenience overload with default resetque and runflags
    Result<void> setSystemRunLevel(const uint32_t runlevel) {
        return setSystemRunLevel(runlevel, 0, 0);
    }

    /// Set device system runlevel (synchronous)
    /// Sends cbPKTYPE_SETRUNLEVEL and waits for cbPKTTYPE_SYSREPRUNLEV response.
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @param timeout Maximum time to wait for response
    /// @return Success if response received, error on send failure or timeout
    virtual Result<void> setSystemRunLevelSync(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                                std::chrono::milliseconds timeout) = 0;

    /// Request all configuration from the device (asynchronous)
    /// Sends cbPKTTYPE_REQCONFIGALL which triggers the device to send all config packets.
    /// Does NOT wait for response - caller must handle config flood and final SYSREP.
    /// @return Success or error
    virtual Result<void> requestConfiguration() = 0;

    /// Request all configuration from the device (synchronous)
    /// Sends cbPKTTYPE_REQCONFIGALL and waits for cbPKTTYPE_SYSREP response.
    /// @param timeout Maximum time to wait for configuration
    /// @return Success if config received, error on send failure or timeout
    virtual Result<void> requestConfigurationSync(std::chrono::milliseconds timeout) = 0;

    /// Perform complete device handshake sequence (synchronous)
    /// Attempts to bring device to RUNNING state through the following sequence:
    /// 1. Try to set RUNNING directly
    /// 2. If not RUNNING, perform HARDRESET (→ STANDBY)
    /// 3. Request configuration
    /// 4. If still not RUNNING, perform RESET (→ RUNNING)
    /// @param timeout Maximum total time for entire handshake sequence
    /// @return Success if device reaches RUNNING state, error otherwise
    virtual Result<void> performHandshakeSync(std::chrono::milliseconds timeout) = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name State
    /// @{

    /// Check if device connection is active
    /// @return true if socket is open and connected
    [[nodiscard]] virtual bool isConnected() const = 0;

    /// Get device configuration
    /// @return Current device configuration
    [[nodiscard]] virtual const ConnectionParams& getConnectionParams() const = 0;

    /// Get protocol version of this session
    /// @return Protocol version being used by this session
    /// @note For auto-detected sessions, returns the detected version
    [[nodiscard]] virtual ProtocolVersion getProtocolVersion() const = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Configuration Access
    /// @{

    /// Get full device configuration
    /// @return Reference to device configuration buffer
    /// @note Configuration is updated automatically when config packets are received
    [[nodiscard]] virtual const cbproto::DeviceConfig& getDeviceConfig() const = 0;

    /// Get system information
    /// @return Reference to system info packet
    [[nodiscard]] virtual const cbPKT_SYSINFO& getSysInfo() const = 0;

    /// Get channel information for specific channel
    /// @param chan_id Channel ID (1-based, 1 to cbMAXCHANS)
    /// @return Pointer to channel info, or nullptr if invalid channel ID
    [[nodiscard]] virtual const cbPKT_CHANINFO* getChanInfo(uint32_t chan_id) const = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Channel Configuration
    /// @{

    /// Count channels matching a specific type
    /// @param chanType Channel type filter (e.g., ChannelType::FRONTEND)
    /// @param maxCount Maximum number to count (use cbMAXCHANS for all)
    /// @return Number of channels matching the type criteria
    [[nodiscard]] virtual size_t countChannelsOfType(ChannelType chanType, size_t maxCount = cbMAXCHANS) const = 0;

    /// Set sampling group for first N channels of a specific type
    /// Groups 1-4 disable groups 1-5 but not 6. Group 5 disables all others. Group 6 disables 5 but no others.
    /// Group 0 disables all groups including raw.
    /// @param nChans Number of channels to configure (use cbMAXCHANS for all channels of type)
    /// @param chanType Channel type filter (e.g., ChannelType::FRONTEND)
    /// @param group_id Group ID (0-6)
    /// @param disableOthers Whether channels not in the first nChans of type chanType should have their group set to 0
    /// @return Success or error
    virtual Result<void> setChannelsGroupByType(size_t nChans, ChannelType chanType, DeviceRate group_id, bool disableOthers) = 0;

    virtual Result<void> setChannelsGroupSync(size_t nChans, ChannelType chanType, DeviceRate group_id, std::chrono::milliseconds timeout) = 0;

    /// Set AC input coupling (offset correction) for first N channels of a specific type (asynchronous)
    /// @param nChans Number of channels to configure (use cbMAXCHANS for all channels of type)
    /// @param chanType Channel type filter
    /// @param enabled true to enable AC coupling, false to disable
    /// @return Success or error
    virtual Result<void> setChannelsACInputCouplingByType(size_t nChans, ChannelType chanType, bool enabled) = 0;

    /// Set AC input coupling synchronously (blocks until CHANREP received)
    /// @param nChans Number of channels to configure
    /// @param chanType Channel type filter
    /// @param enabled true to enable AC coupling, false to disable
    /// @param timeout Maximum time to wait for response
    /// @return Success if response received, error on send failure or timeout
    virtual Result<void> setChannelsACInputCouplingSync(size_t nChans, ChannelType chanType, bool enabled, std::chrono::milliseconds timeout) = 0;

    /// Set spike sorting options for first N channels
    /// @param nChans Number of channels to configure
    /// @param chanType Channel type filter
    /// @param sortOptions Spike sorting options (cbAINPSPK_* flags)
    /// @return Success or error
    virtual Result<void> setChannelsSpikeSortingByType(size_t nChans, ChannelType chanType, uint32_t sortOptions) = 0;

    virtual Result<void> setChannelsSpikeSortingSync(size_t nChans, ChannelType chanType, uint32_t sortOptions, std::chrono::milliseconds timeout) = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Receive Thread and Callbacks
    /// @{

    /// Register a callback to be invoked for each received packet
    /// @param callback Function to call for each packet
    /// @return Handle for unregistration, or 0 on failure
    /// @note Callbacks run on the receive thread - keep them fast to avoid packet loss!
    /// @note Use the cbsdk API to leverage shared memory and queueing for slower callbacks.
    /// @note Multiple callbacks can be registered and will be called in registration order
    virtual CallbackHandle registerReceiveCallback(ReceiveCallback callback) = 0;

    /// Register a callback to be invoked after all packets in a datagram are processed
    /// @param callback Function to call after datagram processing
    /// @return Handle for unregistration, or 0 on failure
    /// @note Use this for batch operations like signaling shared memory
    virtual CallbackHandle registerDatagramCompleteCallback(DatagramCompleteCallback callback) = 0;

    /// Unregister a previously registered callback
    /// @param handle Handle returned by registerReceiveCallback or registerDatagramCompleteCallback
    virtual void unregisterCallback(CallbackHandle handle) = 0;

    /// Start the receive thread
    /// @return Success or error if thread cannot be started
    /// @note Thread calls receivePackets() in a loop and invokes registered callbacks
    virtual Result<void> startReceiveThread() = 0;

    /// Stop the receive thread
    /// @note Blocks until thread terminates
    virtual void stopReceiveThread() = 0;

    /// Check if receive thread is running
    /// @return true if thread is active
    [[nodiscard]] virtual bool isReceiveThreadRunning() const = 0;

    /// @}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_INTERFACE_H
