///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   sdk_session.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  SDK session that orchestrates cbdev + cbshm
///
/// This is the main SDK implementation that combines device communication (cbdev) with
/// shared memory management (cbshm), providing a clean API for receiving packets from
/// Cerebus devices with user callbacks.
///
/// Architecture:
///   Device → cbdev receive thread → cbshm (fast!) → queue → callback thread → user callback
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_V2_SDK_SESSION_H
#define CBSDK_V2_SDK_SESSION_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include <optional>
#include <atomic>
#include <array>

// Protocol types (from upstream)
#include <cbproto/cbproto.h>

namespace cbsdk {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Result Template (consistent with cbshm/cbdev)
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

    bool isOk() const { return m_ok; }
    bool isError() const { return !m_ok; }

    const T& value() const { return m_value.value(); }
    T& value() { return m_value.value(); }
    const std::string& error() const { return m_error; }

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

    bool isOk() const { return m_ok; }
    bool isError() const { return !m_ok; }
    const std::string& error() const { return m_error; }

private:
    bool m_ok = false;
    std::string m_error;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Lock-Free SPSC Queue (Single Producer, Single Consumer)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Lock-free ring buffer for passing packets from receive thread to callback thread
/// Uses atomic operations for wait-free enqueue/dequeue
template<typename T, size_t CAPACITY>
class SPSCQueue {
public:
    SPSCQueue() : m_head(0), m_tail(0) {}

    /// Try to push an item (returns false if queue is full)
    bool push(const T& item) {
        size_t current_tail = m_tail.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % CAPACITY;

        if (next_tail == m_head.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

        m_buffer[current_tail] = item;
        m_tail.store(next_tail, std::memory_order_release);
        return true;
    }

    /// Try to pop an item (returns false if queue is empty)
    bool pop(T& item) {
        size_t current_head = m_head.load(std::memory_order_relaxed);

        if (current_head == m_tail.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        item = m_buffer[current_head];
        m_head.store((current_head + 1) % CAPACITY, std::memory_order_release);
        return true;
    }

    /// Get current size (approximate, may be stale)
    size_t size() const {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail >= head) {
            return tail - head;
        } else {
            return CAPACITY - head + tail;
        }
    }

    /// Get capacity
    size_t capacity() const { return CAPACITY - 1; }  // One slot reserved for full detection

    /// Check if empty (approximate)
    bool empty() const {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

private:
    std::array<T, CAPACITY> m_buffer;
    alignas(64) std::atomic<size_t> m_head;  // Cache line alignment
    alignas(64) std::atomic<size_t> m_tail;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Device type for automatic address/port configuration
enum class DeviceType {
    LEGACY_NSP,   ///< Legacy NSP (192.168.137.128, ports 51001/51002)
    NSP,   ///< Gemini NSP (192.168.137.128, port 51001 bidirectional)
    HUB1,  ///< Gemini Hub 1 (192.168.137.200, port 51002 bidirectional)
    HUB2,  ///< Gemini Hub 2 (192.168.137.201, port 51003 bidirectional)
    HUB3,  ///< Gemini Hub 3 (192.168.137.202, port 51004 bidirectional)
    NPLAY         ///< NPlay loopback (127.0.0.1, ports 51001/51002)
};

/// SDK configuration
struct SdkConfig {
    // Device type (automatically maps to correct address/port and shared memory name)
    // Used only when creating new shared memory (STANDALONE mode)
    DeviceType device_type = DeviceType::LEGACY_NSP;

    // Callback thread configuration
    size_t callback_queue_depth = 16384;      ///< Packets to buffer (as discussed)
    bool enable_realtime_priority = false;    ///< Elevated thread priority
    bool drop_on_overflow = true;             ///< Drop oldest on overflow (vs newest)

    // Advanced options
    int recv_buffer_size = 6000000;           ///< UDP receive buffer (6MB)
    bool non_blocking = false;                ///< Non-blocking sockets (false = blocking, better for dedicated receive thread)
    bool autorun = true;                     ///< Automatically start device (full handshake). If false, only requests configuration.

    // Optional custom device configuration (overrides device_type mapping)
    // Used rarely for non-standard network configurations
    std::optional<std::string> custom_device_address;   ///< Override device IP
    std::optional<std::string> custom_client_address;   ///< Override client IP
    std::optional<uint16_t> custom_device_port;         ///< Override device port
    std::optional<uint16_t> custom_client_port;         ///< Override client port
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics
///////////////////////////////////////////////////////////////////////////////////////////////////

/// SDK statistics
struct SdkStats {
    // Device statistics
    uint64_t packets_received_from_device = 0;   ///< Packets from UDP socket
    uint64_t bytes_received_from_device = 0;     ///< Bytes from UDP socket

    // Shared memory statistics
    uint64_t packets_stored_to_shmem = 0;        ///< Packets written to shmem

    // Callback queue statistics
    uint64_t packets_queued_for_callback = 0;    ///< Packets added to queue
    uint64_t packets_delivered_to_callback = 0;  ///< Packets delivered to user
    uint64_t packets_dropped = 0;                ///< Dropped due to queue overflow
    uint64_t queue_current_depth = 0;            ///< Current queue usage
    uint64_t queue_max_depth = 0;                ///< Peak queue usage

    // Transmit statistics (STANDALONE mode only)
    uint64_t packets_sent_to_device = 0;         ///< Packets sent to device

    // Error counters
    uint64_t shmem_store_errors = 0;             ///< Failed to store to shmem
    uint64_t receive_errors = 0;                 ///< Socket receive errors
    uint64_t send_errors = 0;                    ///< Socket send errors

    void reset() {
        packets_received_from_device = 0;
        bytes_received_from_device = 0;
        packets_stored_to_shmem = 0;
        packets_queued_for_callback = 0;
        packets_delivered_to_callback = 0;
        packets_dropped = 0;
        queue_current_depth = 0;
        queue_max_depth = 0;
        packets_sent_to_device = 0;
        shmem_store_errors = 0;
        receive_errors = 0;
        send_errors = 0;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Types
///////////////////////////////////////////////////////////////////////////////////////////////////

/// User callback for received packets (runs on callback thread, can be slow)
/// @param pkts Pointer to array of packets
/// @param count Number of packets in array
using PacketCallback = std::function<void(const cbPKT_GENERIC* pkts, size_t count)>;

/// Error callback for queue overflow and other errors
/// @param error_message Description of the error
using ErrorCallback = std::function<void(const std::string& error_message)>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SdkSession - Main API
///////////////////////////////////////////////////////////////////////////////////////////////////

/// SDK session that orchestrates device communication and shared memory
///
/// This class combines cbdev (device transport) and cbshm (shared memory) into a unified
/// API with a two-stage pipeline:
///   1. Receive thread (cbdev) → stores to cbshm (fast path, microseconds)
///   2. Callback thread → delivers to user callback (can be slow)
///
/// Example usage:
/// @code
///   SdkConfig config;
///   config.device_address = "192.168.137.128";
///
///   auto result = SdkSession::create(config);
///   if (result.isOk()) {
///       auto& session = result.value();
///
///       session.setPacketCallback([](const cbPKT_GENERIC* pkts, size_t count) {
///           // Process packets (can be slow)
///       });
///
///       session.setErrorCallback([](const std::string& error) {
///           std::cerr << "Error: " << error << std::endl;
///       });
///
///       session.start();
///
///       // ... do work ...
///
///       session.stop();
///   }
/// @endcode
class SdkSession {
public:
    /// Non-copyable (owns resources)
    SdkSession(const SdkSession&) = delete;
    SdkSession& operator=(const SdkSession&) = delete;

    /// Movable
    SdkSession(SdkSession&&) noexcept;
    SdkSession& operator=(SdkSession&&) noexcept;

    /// Destructor - stops session and cleans up resources
    ~SdkSession();

    /// Create and initialize an SDK session
    /// @param config SDK configuration
    /// @return Result containing session on success, error message on failure
    static Result<SdkSession> create(const SdkConfig& config);

    ///--------------------------------------------------------------------------------------------
    /// Session Control
    ///--------------------------------------------------------------------------------------------

    /// Start receiving packets from device
    /// Starts both receive thread (cbdev) and callback thread
    /// @return Result indicating success or error
    Result<void> start();

    /// Stop receiving packets
    /// Stops both receive and callback threads, waits for clean shutdown
    void stop();

    /// Check if session is running
    /// @return true if started and receiving packets
    bool isRunning() const;

    ///--------------------------------------------------------------------------------------------
    /// Callbacks
    ///--------------------------------------------------------------------------------------------

    /// Set callback for received packets
    /// Callback runs on dedicated callback thread (can be slow)
    /// @param callback Function to call with received packets
    void setPacketCallback(PacketCallback callback);

    /// Set callback for errors (queue overflow, etc.)
    /// @param callback Function to call when errors occur
    void setErrorCallback(ErrorCallback callback);

    ///--------------------------------------------------------------------------------------------
    /// Statistics & Monitoring
    ///--------------------------------------------------------------------------------------------

    /// Get current statistics
    /// @return Copy of current statistics
    SdkStats getStats() const;

    /// Reset statistics counters to zero
    void resetStats();

    ///--------------------------------------------------------------------------------------------
    /// Configuration Access
    ///--------------------------------------------------------------------------------------------

    /// Get the configuration used to create this session
    /// @return Reference to SDK configuration
    const SdkConfig& getConfig() const;

    ///--------------------------------------------------------------------------------------------
    /// Packet Transmission
    ///--------------------------------------------------------------------------------------------

    /// Send a single packet to the device
    /// Only available in STANDALONE mode (when device_session exists)
    /// @param pkt Packet to send
    /// @return Result indicating success or error
    Result<void> sendPacket(const cbPKT_GENERIC& pkt);

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

    ///--------------------------------------------------------------------------------------------
    /// Device Startup & Handshake
    ///--------------------------------------------------------------------------------------------

    /// Perform complete device startup handshake sequence
    /// Transitions the device from any state to RUNNING. This is automatically called during
    /// create() when config.autorun = true. Users can call this manually after create()
    /// with config.autorun = false to start the device on demand.
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
    SdkSession();

    /// Shared memory receive thread loop (CLIENT mode only - reads from cbRECbuffer)
    void shmemReceiveThreadLoop();

    /// Wait for SYSREP packet (helper for handshaking)
    /// @param timeout_ms Timeout in milliseconds
    /// @param expected_runlevel Expected runlevel (0 = any SYSREP)
    /// @return true if SYSREP received with expected runlevel, false if timeout
    bool waitForSysrep(uint32_t timeout_ms, uint32_t expected_runlevel = 0) const;

    /// Send a runlevel command packet to the device (internal version with wait_for_runlevel)
    /// @param runlevel Desired runlevel (cbRUNLEVEL_*)
    /// @param resetque Channel for reset to queue on
    /// @param runflags Lock recording after reset
    /// @param wait_for_runlevel Runlevel to wait for (0 = any SYSREP)
    /// @param timeout_ms Timeout in milliseconds
    /// @return Result indicating success or error
    Result<void> setSystemRunLevel(uint32_t runlevel, uint32_t resetque, uint32_t runflags,
                                   uint32_t wait_for_runlevel, uint32_t timeout_ms);

    /// Request configuration with custom timeout (internal version)
    /// @param timeout_ms Timeout in milliseconds
    /// @return Result indicating success or error
    Result<void> requestConfiguration(uint32_t timeout_ms);

    /// Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbsdk

#endif // CBSDK_V2_SDK_SESSION_H
