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

// IMPORTANT: device_session_impl.h includes Windows headers FIRST (before cbproto),
// so it must be included before any other headers that might include cbproto.
#include "device_session_impl.h"
#include <cbdev/device_session.h>
#include <cbdev/connection.h>
#include <cbdev/result.h>
#include <cbproto/cbproto.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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
    virtual ~DeviceSessionWrapper() {
        // Stop receive thread before destruction
        stopReceiveThread();
    }

    // Non-copyable, movable (uses pImpl for thread state)
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

    /// Count channels matching type (delegated to wrapped device)
    [[nodiscard]] size_t countChannelsOfType(const ChannelType chanType, const size_t maxCount) const override {
        return m_device.countChannelsOfType(chanType, maxCount);
    }

    /// Set sampling group for channels by type (delegated to wrapped device)
    Result<void> setChannelsGroupByType(const size_t nChans, const ChannelType chanType, const DeviceRate group_id, const bool disableOthers) override {
        return m_device.setChannelsGroupByType(nChans, chanType, group_id, disableOthers);
    }

    Result<void> setChannelsGroupSync(const size_t nChans, const ChannelType chanType, const DeviceRate group_id, const std::chrono::milliseconds timeout) override {
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

    Result<void> setChannelConfig(const cbPKT_CHANINFO& chaninfo) override {
        return m_device.setChannelConfig(chaninfo);
    }

    Result<void> setDigitalOutput(const uint32_t chan_id, const uint16_t value) override {
        return m_device.setDigitalOutput(chan_id, value);
    }

    Result<void> sendComment(const std::string& comment, const uint32_t rgba, const uint8_t charset) override {
        return m_device.sendComment(comment, rgba, charset);
    }

    /// Clock sync delegation (uses m_device's ClockSync which is fed by
    /// receivePacketsRaw and updateConfigFromBuffer on the same call path)
    std::optional<std::chrono::steady_clock::time_point>
        toLocalTime(uint64_t device_time_ns) const override {
        return m_device.toLocalTime(device_time_ns);
    }

    std::optional<uint64_t>
        toDeviceTime(std::chrono::steady_clock::time_point local_time) const override {
        return m_device.toDeviceTime(local_time);
    }

    Result<void> sendClockProbe() override {
        return m_device.sendClockProbe();
    }

    [[nodiscard]] std::optional<int64_t> getOffsetNs() const override {
        return m_device.getOffsetNs();
    }

    [[nodiscard]] std::optional<int64_t> getUncertaintyNs() const override {
        return m_device.getUncertaintyNs();
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Receive Thread and Callbacks (Wrapper-Specific Implementation)
    /// @{

    /// Register a receive callback
    /// @note Uses wrapper's own callback storage (not underlying device's)
    CallbackHandle registerReceiveCallback(ReceiveCallback callback) override {
        if (!callback || !m_thread_state) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(m_thread_state->callback_mutex);
        CallbackHandle handle = m_thread_state->next_callback_handle++;
        m_thread_state->receive_callbacks.push_back({handle, std::move(callback)});
        return handle;
    }

    /// Register a datagram complete callback
    CallbackHandle registerDatagramCompleteCallback(DatagramCompleteCallback callback) override {
        if (!callback || !m_thread_state) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(m_thread_state->callback_mutex);
        CallbackHandle handle = m_thread_state->next_callback_handle++;
        m_thread_state->datagram_callbacks.push_back({handle, std::move(callback)});
        return handle;
    }

    /// Unregister a callback
    void unregisterCallback(CallbackHandle handle) override {
        if (handle == 0 || !m_thread_state) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_thread_state->callback_mutex);

        // Check receive callbacks
        auto& recv_cbs = m_thread_state->receive_callbacks;
        recv_cbs.erase(
            std::remove_if(recv_cbs.begin(), recv_cbs.end(),
                [handle](const CallbackRegistration& reg) { return reg.handle == handle; }),
            recv_cbs.end());

        // Check datagram callbacks
        auto& dg_cbs = m_thread_state->datagram_callbacks;
        dg_cbs.erase(
            std::remove_if(dg_cbs.begin(), dg_cbs.end(),
                [handle](const DatagramCallbackRegistration& reg) { return reg.handle == handle; }),
            dg_cbs.end());
    }

    /// Start the receive thread
    /// @note Thread calls this wrapper's receivePackets() (with translation)
    Result<void> startReceiveThread() override {
        if (!m_thread_state) {
            return Result<void>::error("Thread state not initialized");
        }

        if (m_thread_state->receive_thread_running.load()) {
            return Result<void>::error("Receive thread already running");
        }

        m_thread_state->receive_thread_stop_requested.store(false);
        m_thread_state->receive_thread_running.store(true);

        m_thread_state->receive_thread = std::thread([this]() {
            uint8_t buffer[cbCER_UDP_SIZE_MAX];

            while (!m_thread_state->receive_thread_stop_requested.load()) {
                // Call virtual receivePackets() - handles protocol translation
                auto result = this->receivePackets(buffer, sizeof(buffer));

                if (result.isError()) {
                    continue;
                }

                const int bytes_received = result.value();
                if (bytes_received == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }

                // Parse packets and invoke callbacks
                size_t offset = 0;
                while (offset + cbPKT_HEADER_SIZE <= static_cast<size_t>(bytes_received)) {
                    const auto* pkt = reinterpret_cast<const cbPKT_GENERIC*>(&buffer[offset]);
                    const size_t packet_size = cbPKT_HEADER_SIZE + (pkt->cbpkt_header.dlen * 4);

                    if (offset + packet_size > static_cast<size_t>(bytes_received)) {
                        break;
                    }

                    // Invoke receive callbacks
                    {
                        std::lock_guard<std::mutex> lock(m_thread_state->callback_mutex);
                        for (const auto& reg : m_thread_state->receive_callbacks) {
                            reg.callback(*pkt);
                        }
                    }

                    offset += packet_size;
                }

                // Invoke datagram complete callbacks
                {
                    std::lock_guard<std::mutex> lock(m_thread_state->callback_mutex);
                    for (const auto& reg : m_thread_state->datagram_callbacks) {
                        reg.callback();
                    }
                }
            }

            m_thread_state->receive_thread_running.store(false);
        });

        return Result<void>::ok();
    }

    /// Stop the receive thread
    void stopReceiveThread() override {
        if (m_thread_state) {
            m_thread_state->stop();
        }
    }

    /// Check if receive thread is running
    [[nodiscard]] bool isReceiveThreadRunning() const override {
        return m_thread_state && m_thread_state->receive_thread_running.load();
    }

    /// @}

private:
    // Callback storage (in pImpl for move semantics)
    struct CallbackRegistration {
        CallbackHandle handle;
        ReceiveCallback callback;
    };
    struct DatagramCallbackRegistration {
        CallbackHandle handle;
        DatagramCompleteCallback callback;
    };

    // Thread state pImpl - allows DeviceSessionWrapper to be movable
    struct ThreadState {
        std::vector<CallbackRegistration> receive_callbacks;
        std::vector<DatagramCallbackRegistration> datagram_callbacks;
        std::mutex callback_mutex;
        CallbackHandle next_callback_handle = 1;

        std::thread receive_thread;
        std::atomic<bool> receive_thread_running{false};
        std::atomic<bool> receive_thread_stop_requested{false};

        void stop() {
            if (receive_thread_running.load()) {
                receive_thread_stop_requested.store(true);
                if (receive_thread.joinable()) {
                    receive_thread.join();
                }
                receive_thread_running.store(false);
                receive_thread_stop_requested.store(false);
            }
        }

        ~ThreadState() {
            stop();
        }
    };

    std::unique_ptr<ThreadState> m_thread_state = std::make_unique<ThreadState>();
};

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_WRAPPER_H
