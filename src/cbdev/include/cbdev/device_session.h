///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Device transport layer for CereLink
///
/// This module provides clean UDP socket communication with Cerebus devices (NSP, Gemini NSP, Hub).
/// It abstracts platform-specific networking details and provides a callback-based receive system.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_SESSION_H
#define CBDEV_DEVICE_SESSION_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include <optional>

#include <cbproto/cbproto.h>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Result Template (matching cbshmem pattern)
///////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class Result {
public:
    static Result<T> ok(T value) {
        Result<T> r;
        r.m_ok = true;
        r.m_value = std::move(value);
        return r;
    }

    static Result<T> error(const std::string& msg) {
        Result<T> r;
        r.m_ok = false;
        r.m_error = msg;
        return r;
    }

    [[nodiscard]] bool isOk() const { return m_ok; }
    [[nodiscard]] bool isError() const { return !m_ok; }

    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    bool m_ok = false;
    std::optional<T> m_value;
    std::string m_error;
};

// Specialization for Result<void>
template<>
class Result<void> {
public:
    static Result<void> ok() {
        Result<void> r;
        r.m_ok = true;
        return r;
    }

    static Result<void> error(const std::string& msg) {
        Result<void> r;
        r.m_ok = false;
        r.m_error = msg;
        return r;
    }

    [[nodiscard]] bool isOk() const { return m_ok; }
    [[nodiscard]] bool isError() const { return !m_ok; }
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    bool m_ok = false;
    std::string m_error;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Device Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Device type enumeration
enum class DeviceType {
    NSP,        ///< Neural Signal Processor (legacy)
    GEMINI_NSP,     ///< Gemini NSP
    HUB1,       ///< Hub 1 (legacy addressing)
    HUB2,       ///< Hub 2 (legacy addressing)
    HUB3,       ///< Hub 3 (legacy addressing)
    NPLAY,      ///< nPlayServer
    CUSTOM      ///< Custom IP/port configuration
};

/// Device configuration structure
struct DeviceConfig {
    DeviceType type = DeviceType::NSP;

    // Network addresses
    std::string device_address;     ///< Device IP address (where to send packets)
    std::string client_address;     ///< Client IP address (where to bind receive socket)

    // Ports
    uint16_t recv_port = 51001;     ///< Port to receive packets on (client side)
    uint16_t send_port = 51002;     ///< Port to send packets to (device side)

    // Socket options
    bool broadcast = false;         ///< Enable broadcast mode
    bool non_blocking = false;      ///< Non-blocking socket (false = blocking, better for dedicated receive thread)
    int recv_buffer_size = 6000000; ///< Receive buffer size (6MB default)

    /// Create configuration for a known device type
    static DeviceConfig forDevice(DeviceType type);

    /// Create custom configuration with explicit addresses
    static DeviceConfig custom(const std::string& device_addr,
                              const std::string& client_addr = "0.0.0.0",
                              uint16_t recv_port = 51001,
                              uint16_t send_port = 51002);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Statistics for monitoring device communication
struct DeviceStats {
    uint64_t packets_sent = 0;          ///< Total packets sent to device
    uint64_t packets_received = 0;      ///< Total packets received from device
    uint64_t bytes_sent = 0;            ///< Total bytes sent
    uint64_t bytes_received = 0;        ///< Total bytes received
    uint64_t send_errors = 0;           ///< Send operation failures
    uint64_t recv_errors = 0;           ///< Receive operation failures

    void reset() {
        packets_sent = packets_received = 0;
        bytes_sent = bytes_received = 0;
        send_errors = recv_errors = 0;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceSession - Main API
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Callback function for received packets
/// @param pkts Pointer to array of received packets
/// @param count Number of packets in array
using PacketCallback = std::function<void(const cbPKT_GENERIC* pkts, size_t count)>;

/// Callback function for transmit operations
/// Returns true if a packet was dequeued, false if queue is empty
/// @param pkt Output parameter to receive the packet to transmit
/// @return true if packet was dequeued, false if no packets available
using TransmitCallback = std::function<bool(cbPKT_GENERIC& pkt)>;

/// Device communication session
///
/// This class manages UDP socket communication with Cerebus devices. It handles:
/// - Platform-specific socket creation (Windows/POSIX)
/// - macOS multi-interface routing (IP_BOUND_IF)
/// - Packet send/receive operations
/// - Callback-based receive thread
/// - Statistics and monitoring
///
/// Example usage:
/// @code
///   auto result = DeviceSession::create(DeviceConfig::forDevice(DeviceType::NSP));
///   if (result.isOk()) {
///       auto& session = result.value();
///       session.setPacketCallback([](const cbPKT_GENERIC* pkts, size_t count) {
///           // Handle received packets
///       });
///       session.startReceiveThread();
///
///       // Send packet
///       cbPKT_GENERIC pkt;
///       // ... fill packet
///       session.sendPacket(pkt);
///   }
/// @endcode
class DeviceSession {
public:
    /// Non-copyable (owns socket resources)
    DeviceSession(const DeviceSession&) = delete;
    DeviceSession& operator=(const DeviceSession&) = delete;

    /// Movable
    DeviceSession(DeviceSession&&) noexcept;
    DeviceSession& operator=(DeviceSession&&) noexcept;

    /// Destructor - closes socket if open
    ~DeviceSession();

    /// Create and open a device session
    /// @param config Device configuration
    /// @return Result containing session on success, error message on failure
    static Result<DeviceSession> create(const DeviceConfig& config);

    /// Close the device session
    /// Stops receive thread if running and closes socket
    void close();

    /// Check if session is open and ready for communication
    /// @return true if open and ready
    [[nodiscard]] bool isOpen() const;

    ///--------------------------------------------------------------------------------------------
    /// Packet Operations
    ///--------------------------------------------------------------------------------------------

    /// Send a single packet to the device
    /// @param pkt Packet to send
    /// @return Result indicating success or error
    Result<void> sendPacket(const cbPKT_GENERIC& pkt);

    /// Send multiple packets to the device
    /// @param pkts Pointer to array of packets
    /// @param count Number of packets in array
    /// @return Result indicating success or error
    Result<void> sendPackets(const cbPKT_GENERIC* pkts, size_t count);

    ///--------------------------------------------------------------------------------------------
    /// Callback-Based Receive
    ///--------------------------------------------------------------------------------------------

    /// Set callback function for received packets
    /// Callback will be invoked from receive thread
    /// @param callback Function to call when packets are received
    void setPacketCallback(PacketCallback callback) const;

    /// Start asynchronous receive thread
    /// Packets will be delivered via callback set by setPacketCallback()
    /// @return Result indicating success or error
    Result<void> startReceiveThread();

    /// Stop asynchronous receive thread
    void stopReceiveThread() const;

    /// Check if receive thread is running
    /// @return true if receive thread is active
    [[nodiscard]] bool isReceiveThreadRunning() const;

    ///--------------------------------------------------------------------------------------------
    /// Send Thread (for transmit queue)
    ///--------------------------------------------------------------------------------------------

    /// Set callback function for transmit operations
    /// The send thread will periodically call this to get packets to send
    /// @param callback Function to call to dequeue packets for transmission
    void setTransmitCallback(TransmitCallback callback) const;

    /// Start asynchronous send thread
    /// Thread will periodically call transmit callback to get packets to send
    /// @return Result indicating success or error
    [[nodiscard]] Result<void> startSendThread() const;

    /// Stop asynchronous send thread
    void stopSendThread() const;

    /// Check if send thread is running
    /// @return true if send thread is active
    [[nodiscard]] bool isSendThreadRunning() const;

    ///--------------------------------------------------------------------------------------------
    /// Statistics & Monitoring
    ///--------------------------------------------------------------------------------------------

    /// Get current statistics
    /// @return Copy of current statistics
    [[nodiscard]] DeviceStats getStats() const;

    /// Reset statistics counters to zero
    void resetStats();

    ///--------------------------------------------------------------------------------------------
    /// Configuration Access
    ///--------------------------------------------------------------------------------------------

    /// Get the configuration used to create this session
    /// @return Reference to device configuration
    [[nodiscard]] const DeviceConfig& getConfig() const;

    ///--------------------------------------------------------------------------------------------
    /// Device Startup & Handshake
    ///--------------------------------------------------------------------------------------------

    /// Send a runlevel command packet to the device
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on (default: 0)
    /// @param runflags Lock recording after reset (default: 0)
    /// @return Result indicating success or error
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque = 0, uint32_t runflags = 0);

    /// Request all configuration from the device
    /// Sends cbPKTTYPE_REQCONFIGALL which triggers the device to send all config packets
    /// The device will respond with > 1000 packets (PROCINFO, CHANINFO, etc.)
    /// @return Result indicating success or error
    Result<void> requestConfiguration();

    /// Perform complete device startup handshake sequence
    /// Transitions the device from any state to RUNNING. Call this after create() to start the device.
    ///
    /// Startup sequence:
    /// 1. Quick presence check (100ms) - fails fast if device not reachable
    /// 2. Check if device is already running
    /// 3. If not running, send cbRUNLEVEL_HARDRESET - wait for STANDBY
    /// 4. Send REQCONFIGALL - request all configuration
    /// 5. Send cbRUNLEVEL_RESET - transition to RUNNING
    ///
    /// @param timeout_ms Maximum time to wait for each step (default: 500ms)
    /// @return Result indicating success or error (clear message if device not reachable)
    Result<void> performStartupHandshake(uint32_t timeout_ms = 500);

private:
    /// Private constructor (use create() factory method)
    DeviceSession();

    /// Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Auto-detect local network adapter IP address
/// On macOS with multiple interfaces, returns "0.0.0.0" (recommended for compatibility)
/// On other platforms, attempts to find the adapter on the Cerebus subnet
/// @return IP address string, or "0.0.0.0" if detection fails
std::string detectLocalIP();

/// Get default device addresses (from upstream/cbproto/cbproto.h)
namespace DeviceDefaults {
    // Device addresses
    constexpr const char* NSP_ADDRESS        = "192.168.137.128";  // Legacy NSP (cbNET_UDP_ADDR_CNT)
    constexpr const char* GEMINI_NSP_ADDRESS = "192.168.137.128";  // Gemini NSP (cbNET_UDP_ADDR_GEMINI_NSP)
    constexpr const char* GEMINI_HUB1_ADDRESS = "192.168.137.200"; // Gemini Hub1 (cbNET_UDP_ADDR_GEMINI_HUB)
    constexpr const char* GEMINI_HUB2_ADDRESS = "192.168.137.201"; // Gemini Hub2 (cbNET_UDP_ADDR_GEMINI_HUB2)
    constexpr const char* GEMINI_HUB3_ADDRESS = "192.168.137.202"; // Gemini Hub3 (cbNET_UDP_ADDR_GEMINI_HUB3)
    constexpr const char* NPLAY_ADDRESS      = "127.0.0.1";        // nPlayServer (loopback)

    // Client/Host addresses (empty = auto-detect based on device type and platform)
    constexpr const char* DEFAULT_CLIENT_ADDRESS = "";  // Auto-detect (was 192.168.137.199)

    // Ports
    constexpr uint16_t LEGACY_NSP_RECV_PORT   = 51001;  // cbNET_UDP_PORT_CNT
    constexpr uint16_t LEGACY_NSP_SEND_PORT   = 51002;  // cbNET_UDP_PORT_BCAST
    constexpr uint16_t GEMINI_NSP_PORT        = 51001;  // cbNET_UDP_PORT_GEMINI_NSP (both send & recv)
    constexpr uint16_t GEMINI_HUB1_PORT       = 51002;  // cbNET_UDP_PORT_GEMINI_HUB (both send & recv)
    constexpr uint16_t GEMINI_HUB2_PORT       = 51003;  // cbNET_UDP_PORT_GEMINI_HUB2 (both send & recv)
    constexpr uint16_t GEMINI_HUB3_PORT       = 51004;  // cbNET_UDP_PORT_GEMINI_HUB3 (both send & recv)
    constexpr uint16_t NPLAY_PORT             = 51001;  // nPlayServer port
}

} // namespace cbdev

#endif // CBDEV_DEVICE_SESSION_H
