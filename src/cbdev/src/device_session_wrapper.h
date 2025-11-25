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
#include <thread>

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

    /// Send multiple packets with translation via virtual sendPacket()
    Result<void> sendPackets(const std::vector<cbPKT_GENERIC>& pkts) override {
        if (pkts.empty()) {
            return Result<void>::error("Empty packet vector");
        }

        // Send each packet as its own datagram with a small delay between packets.
        // Uses virtual sendPacket() so derived classes handle protocol translation.
        for (const auto& pkt : pkts) {
            if (auto result = sendPacket(pkt); result.isError()) {
                return result;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        return Result<void>::ok();
    }

    /// Send raw bytes (delegated to wrapped device)
    Result<void> sendRaw(const void* buffer, const size_t size) override {
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
    [[nodiscard]] const cbPKT_CHANINFO* getChanInfo(const uint32_t chan_id) const override {
        return m_device.getChanInfo(chan_id);
    }

    /// Set system runlevel - async (delegated to wrapped device)
    Result<void> setSystemRunLevel(const uint32_t runlevel, const uint32_t resetque, const uint32_t runflags) override {
        return m_device.setSystemRunLevel(runlevel, resetque, runflags);
    }

    /// Request configuration - async (delegated to wrapped device)
    Result<void> requestConfiguration() override {
        return m_device.requestConfiguration();
    }

    /// Request configuration - sync (delegated to wrapped device)
    Result<void> requestConfigurationSync(const std::chrono::milliseconds timeout) override {
        return m_device.requestConfigurationSync(timeout);
    }

    /// Set system runlevel - sync (delegated to wrapped device)
    Result<void> setSystemRunLevelSync(const uint32_t runlevel, const uint32_t resetque, const uint32_t runflags,
                                       const std::chrono::milliseconds timeout) override {
        return m_device.setSystemRunLevelSync(runlevel, resetque, runflags, timeout);
    }

    /// Perform complete device handshake sequence - sync (delegated to wrapped device)
    Result<void> performHandshakeSync(const std::chrono::milliseconds timeout) override {
        return m_device.performHandshakeSync(timeout);
    }

    /// Set sampling group for channels by type (delegated to wrapped device)
    Result<void> setChannelsGroupByType(const size_t nChans, const ChannelType chanType, const uint32_t group_id, const bool disableOthers) override {
        return m_device.setChannelsGroupByType(nChans, chanType, group_id, disableOthers);
    }

    Result<void> setChannelsGroupSync(const size_t nChans, const ChannelType chanType, const uint32_t group_id, const std::chrono::milliseconds timeout) override {
        return m_device.setChannelsGroupSync(nChans, chanType, group_id, timeout);
    }

    /// Set AC input coupling for channels by type (delegated to wrapped device)
    Result<void> setChannelsACInputCouplingByType(const size_t nChans, const ChannelType chanType, const bool enabled) override {
        return m_device.setChannelsACInputCouplingByType(nChans, chanType, enabled);
    }

    Result<void> setChannelsACInputCouplingSync(const size_t nChans, const ChannelType chanType, const bool enabled,
                                                const std::chrono::milliseconds timeout) override {
        return m_device.setChannelsACInputCouplingSync(nChans, chanType, enabled, timeout);
    }

    /// Set spike sorting options (delegated to wrapped device)
    Result<void> setChannelsSpikeSortingByType(const size_t nChans, const ChannelType chanType, const uint32_t sortOptions) override {
        return m_device.setChannelsSpikeSortingByType(nChans, chanType, sortOptions);
    }

    Result<void> setChannelsSpikeSortingSync(const size_t nChans, const ChannelType chanType, const uint32_t sortOptions, const std::chrono::milliseconds timeout) override {
        return m_device.setChannelsSpikeSortingSync(nChans, chanType, sortOptions, timeout);
    }

    /// @}
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_WRAPPER_H
