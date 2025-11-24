///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_wrapper.h
/// @author CereLink Development Team
/// @date   2025-01-24
///
/// @brief  Base class for protocol wrappers - eliminates boilerplate delegation
///
/// DeviceSessionWrapper provides a base class for protocol-specific wrappers that handles
/// all delegation to the wrapped DeviceSession. Subclasses only need to override:
///   - receivePackets() - for protocol → current translation
///   - sendPacket() - for current → protocol translation
///   - getProtocolVersion() - to return the protocol version
///
/// All other IDeviceSession methods are automatically delegated to the wrapped device.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_WRAPPER_H
#define CBDEV_DEVICE_SESSION_WRAPPER_H

#include <cbdev/device_session.h>
#include "device_session_impl.h"
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for protocol wrappers - handles delegation to wrapped DeviceSession
///
/// Protocol wrappers only need to override receivePackets() and sendPacket() for translation.
/// All other methods are automatically delegated to the wrapped device.
///
class DeviceSessionWrapper : public IDeviceSession {
protected:
    /// Wrapped device session for actual I/O and logic
    DeviceSession m_device;

    /// Protected constructor - only subclasses can create
    explicit DeviceSessionWrapper(DeviceSession&& device)
        : m_device(std::move(device)) {}

public:
    virtual ~DeviceSessionWrapper() = default;

    // Non-copyable, movable
    DeviceSessionWrapper(const DeviceSessionWrapper&) = delete;
    DeviceSessionWrapper& operator=(const DeviceSessionWrapper&) = delete;
    DeviceSessionWrapper(DeviceSessionWrapper&&) noexcept = default;
    DeviceSessionWrapper& operator=(DeviceSessionWrapper&&) noexcept = default;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Methods That Must Be Overridden (Protocol-Specific)
    /// @{

    /// Receive packets with protocol translation
    /// Subclasses MUST override to translate from protocol format → current format
    Result<int> receivePackets(void* buffer, size_t buffer_size) override = 0;

    /// Send packet with protocol translation
    /// Subclasses MUST override to translate from current format → protocol format
    Result<void> sendPacket(const cbPKT_GENERIC& pkt) override = 0;

    /// Get protocol version
    /// Subclasses MUST override to return their specific protocol version
    [[nodiscard]] ProtocolVersion getProtocolVersion() const override = 0;

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Auto-Delegated Methods (Same for All Protocols)
    /// @{

    /// Send multiple packets (delegated to wrapped device)
    Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count) override {
        return m_device.sendPackets(pkts, count);
    }

    /// Send raw bytes (delegated to wrapped device)
    Result<void> sendRaw(const void* buffer, size_t size) override {
        return m_device.sendRaw(buffer, size);
    }

    /// Check if connected (delegated to wrapped device)
    [[nodiscard]] bool isConnected() const override {
        return m_device.isConnected();
    }

    /// Get connection parameters (delegated to wrapped device)
    [[nodiscard]] const ConnectionParams& getConnectionParams() const override {
        return m_device.getConnectionParams();
    }

    /// Get device configuration (delegated to wrapped device)
    [[nodiscard]] const cbproto::DeviceConfig& getDeviceConfig() const override {
        return m_device.getDeviceConfig();
    }

    /// Get system information (delegated to wrapped device)
    [[nodiscard]] const cbPKT_SYSINFO& getSysInfo() const override {
        return m_device.getSysInfo();
    }

    /// Get channel information (delegated to wrapped device)
    [[nodiscard]] const cbPKT_CHANINFO* getChanInfo(uint32_t chan_id) const override {
        return m_device.getChanInfo(chan_id);
    }

    /// Set system runlevel - async (delegated to wrapped device)
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags) override {
        return m_device.setSystemRunLevel(runlevel, resetque, runflags);
    }

    /// Request configuration - async (delegated to wrapped device)
    Result<void> requestConfiguration() override {
        return m_device.requestConfiguration();
    }

    /// Request configuration - sync (delegated to wrapped device)
    Result<void> requestConfigurationSync(std::chrono::milliseconds timeout) override {
        return m_device.requestConfigurationSync(timeout);
    }

    /// Set system runlevel - sync (delegated to wrapped device)
    Result<void> setSystemRunLevelSync(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                       std::chrono::milliseconds timeout) override {
        return m_device.setSystemRunLevelSync(runlevel, resetque, runflags, timeout);
    }

    /// Perform complete device handshake sequence - sync (delegated to wrapped device)
    Result<void> performHandshakeSync(std::chrono::milliseconds timeout) override {
        return m_device.performHandshakeSync(timeout);
    }

    /// Set sampling group for channels by type (delegated to wrapped device)
    Result<void> setChannelsGroupByType(size_t nChans, ChannelType chanType, uint32_t group_id) override {
        return m_device.setChannelsGroupByType(nChans, chanType, group_id);
    }

    /// Set AC input coupling for channels by type (delegated to wrapped device)
    Result<void> setChannelsACInputCouplingByType(size_t nChans, ChannelType chanType, bool enabled) override {
        return m_device.setChannelsACInputCouplingByType(nChans, chanType, enabled);
    }

    Result<void> setChannelsACInputCouplingSync(size_t nChans, ChannelType chanType, bool enabled,
                                                std::chrono::milliseconds timeout) override {
        return m_device.setChannelsACInputCouplingSync(nChans, chanType, enabled, timeout);
    }

    /// Set spike sorting options (delegated to wrapped device)
    Result<void> setChannelsSpikeSorting(size_t nChans, uint32_t sortOptions) override {
        return m_device.setChannelsSpikeSorting(nChans, sortOptions);
    }

    /// @}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_WRAPPER_H
