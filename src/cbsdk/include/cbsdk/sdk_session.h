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
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <cstdint>
#include <optional>
#include <atomic>
#include <array>

// Protocol types (from upstream)
#include <cbproto/cbproto.h>
#include <cbutil/result.h>

namespace cbsdk {

template<typename T>
using Result = cbutil::Result<T>;

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
    NPLAY        ///< NPlay loopback (127.0.0.1, ports 51001/51002)
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
// Channel Type (for typed event callbacks)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Channel type classification for event callback filtering
enum class ChannelType {
    ANY,          ///< Matches all event channels (catch-all)
    FRONTEND,     ///< Front-end electrode channels (1..cbNUM_FE_CHANS)
    ANALOG_IN,    ///< Analog input channels
    ANALOG_OUT,   ///< Analog output channels
    AUDIO,        ///< Audio output channels
    DIGITAL_IN,   ///< Digital input channels
    SERIAL,       ///< Serial input channels
    DIGITAL_OUT,  ///< Digital output channels
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Sample Rate (maps to Cerebus sampling groups)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Sampling rate enumeration — wraps Cerebus group IDs with human-readable names.
///
/// Each value corresponds to a hardware sampling group:
///   NONE=0 (disabled), SR_500=1, SR_1kHz=2, SR_2kHz=3,
///   SR_10kHz=4, SR_30kHz=5, SR_RAW=6.
enum class SampleRate : uint32_t {
    NONE    = 0,  ///< Sampling disabled
    SR_500  = 1,  ///< 500 Hz
    SR_1kHz = 2,  ///< 1 kHz
    SR_2kHz = 3,  ///< 2 kHz
    SR_10kHz = 4, ///< 10 kHz
    SR_30kHz = 5, ///< 30 kHz
    SR_RAW  = 6   ///< 30 kHz raw (unfiltered)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Info Field (for bulk getters)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Selects a numeric field from cbPKT_CHANINFO for bulk extraction.
enum class ChanInfoField : uint32_t {
    SMPGROUP,       ///< uint32_t — sampling group (0=disabled, 1–6)
    SMPFILTER,      ///< uint32_t — continuous-time filter ID
    SPKFILTER,      ///< uint32_t — spike pathway filter ID
    AINPOPTS,       ///< uint32_t — analog input option flags (cbAINP_*)
    SPKOPTS,        ///< uint32_t — spike processing option flags
    SPKTHRLEVEL,    ///< int32_t  — spike threshold level
    LNCRATE,        ///< uint32_t — LNC adaptation rate
    REFELECCHAN,    ///< uint32_t — reference electrode channel
    AMPLREJPOS,     ///< int16_t  — positive amplitude rejection
    AMPLREJNEG,     ///< int16_t  — negative amplitude rejection
    CHANCAPS,       ///< uint32_t — channel capability flags
    BANK,           ///< uint32_t — bank index
    TERM,           ///< uint32_t — terminal index within bank
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Types
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Callback handle for unregistering callbacks
using CallbackHandle = uint32_t;

/// Generic callback for all packets (runs on callback thread, can be slow)
/// @param pkt The received packet
using PacketCallback = std::function<void(const cbPKT_GENERIC& pkt)>;

/// Event callback for spike/digital/serial event packets (chid = 1..cbMAXCHANS)
/// @param pkt The received event packet
using EventCallback = std::function<void(const cbPKT_GENERIC& pkt)>;

/// Group callback for continuous sample data packets (chid == 0)
/// @param pkt The received group packet (pkt.cbpkt_header.type is the group ID 1-6)
using GroupCallback = std::function<void(const cbPKT_GROUP& pkt)>;

/// Config callback for system/configuration packets (chid & 0x8000)
/// @param pkt The received config packet
using ConfigCallback = std::function<void(const cbPKT_GENERIC& pkt)>;

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
///   config.device_type = DeviceType::HUB1;
///
///   auto result = SdkSession::create(config);
///   if (result.isOk()) {
///       auto& session = result.value();
///
///       // Register typed callbacks (all run on callback thread, off the queue)
///       session.registerEventCallback(ChannelType::FRONTEND, [](const cbPKT_GENERIC& pkt) {
///           // Handle spike events
///       });
///       session.registerGroupCallback(5, [](const cbPKT_GENERIC& pkt) {
///           // Handle 30kHz continuous data
///       });
///
///       session.start();
///       // ... do work ...
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
    /// Callbacks (all run on dedicated callback thread, off the queue — may be slow)
    ///--------------------------------------------------------------------------------------------

    /// Register callback for all packets (catch-all)
    /// @param callback Function to call for every received packet
    /// @return Handle for unregistration
    CallbackHandle registerPacketCallback(PacketCallback callback) const;

    /// Register callback for event packets (spikes, digital events, etc.)
    /// @param channel_type Channel type filter (ANY matches all event channels)
    /// @param callback Function to call for matching events
    /// @return Handle for unregistration
    CallbackHandle registerEventCallback(ChannelType channel_type, EventCallback callback) const;

    /// Register callback for continuous sample group packets
    /// @param rate Sample rate to match (SR_500 through SR_RAW)
    /// @param callback Function to call for matching group packets
    /// @return Handle for unregistration
    CallbackHandle registerGroupCallback(SampleRate rate, GroupCallback callback) const;

    /// Register callback for config/system packets
    /// @param packet_type Packet type to match (e.g. cbPKTTYPE_COMMENTREP, cbPKTTYPE_SYSREPRUNLEV)
    /// @param callback Function to call for matching config packets
    /// @return Handle for unregistration
    CallbackHandle registerConfigCallback(uint16_t packet_type, ConfigCallback callback) const;

    /// Unregister a previously registered callback
    /// @param handle Handle returned by any register*Callback method
    void unregisterCallback(CallbackHandle handle) const;

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

    /// Get system information
    /// @return Pointer to system info packet, or nullptr if not available
    const cbPKT_SYSINFO* getSysInfo() const;

    /// Get channel information
    /// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
    /// @return Pointer to channel info, or nullptr if invalid/unavailable
    const cbPKT_CHANINFO* getChanInfo(uint32_t chan_id) const;

    /// Get sample group information
    /// @param group_id Group ID (1-6)
    /// @return Pointer to group info, or nullptr if invalid/unavailable
    const cbPKT_GROUPINFO* getGroupInfo(uint32_t group_id) const;

    /// Get filter information
    /// @param filter_id Filter ID (0 to cbMAXFILTS-1)
    /// @return Pointer to filter info, or nullptr if invalid/unavailable
    const cbPKT_FILTINFO* getFilterInfo(uint32_t filter_id) const;

    /// Get current device run level
    /// @return Current run level (cbRUNLEVEL_*), or 0 if unknown
    uint32_t getRunLevel() const;

    /// Get the protocol version used by this session
    /// @return Protocol version, or UNKNOWN if not available (e.g. CLIENT mode)
    uint32_t getProtocolVersion() const;

    /// Get the processor identification string (from PROCREP packet)
    /// @return ident string (e.g. "Gemini Hub 1"), or empty if unavailable
    std::string getProcIdent() const;

    /// Get the global spike event length (samples per spike waveform)
    /// @return Spike length in samples, or 0 if unavailable
    uint32_t getSpikeLength() const;

    /// Get the global spike pre-trigger length (samples before threshold crossing)
    /// @return Pre-trigger length in samples, or 0 if unavailable
    uint32_t getSpikePretrigger() const;

    /// Set the global spike event length and pre-trigger
    /// @param spikelen Total spike waveform length in samples
    /// @param spikepre Pre-trigger samples (must be < spikelen)
    /// @return Result indicating success or error
    Result<void> setSpikeLength(uint32_t spikelen, uint32_t spikepre);

    /// Get most recent device timestamp from shared memory
    /// On Gemini (protocol 4.0+) this is PTP nanoseconds.
    /// On legacy NSP (protocol 3.x, CBPROTO_311) this is 30kHz ticks.
    /// @return Raw device timestamp, or 0 if not available
    uint64_t getTime() const;

    ///--------------------------------------------------------------------------------------------
    /// Bulk Channel Queries
    ///--------------------------------------------------------------------------------------------

    /// Get any numeric field from a single channel by field selector
    /// @param chanId 1-based channel ID (1 to cbMAXCHANS)
    /// @param field Which field to extract
    /// @return Field value widened to int64_t, or error if channel invalid
    Result<int64_t> getChannelField(uint32_t chanId, ChanInfoField field) const;

    /// Get the 1-based IDs of channels matching a type
    /// @param nChans Max channels to return (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @return Vector of 1-based channel IDs
    Result<std::vector<uint32_t>> getMatchingChannelIds(size_t nChans, ChannelType chanType) const;

    /// Get a numeric field from all channels matching a type
    /// @param nChans Max channels to query (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @param field Which field to extract
    /// @return Vector of field values (same order as getMatchingChannelIds)
    Result<std::vector<int64_t>> getChannelField(size_t nChans, ChannelType chanType,
                                                  ChanInfoField field) const;

    /// Get labels from all channels matching a type
    /// @param nChans Max channels to query (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @return Vector of label strings (same order as getMatchingChannelIds)
    Result<std::vector<std::string>> getChannelLabels(size_t nChans, ChannelType chanType) const;

    /// Get positions from all channels matching a type
    /// @param nChans Max channels to query (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @return Flat vector of int32_t: [x0,y0,z0,w0, x1,y1,z1,w1, ...]  (4 per channel)
    Result<std::vector<int32_t>> getChannelPositions(size_t nChans, ChannelType chanType) const;

    ///--------------------------------------------------------------------------------------------
    /// Channel Configuration
    ///--------------------------------------------------------------------------------------------

    /// Set sampling rate for channels of a specific type
    /// @param nChans Number of channels to configure (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @param rate Desired sample rate (NONE to disable, SR_500 through SR_RAW)
    /// @param disableOthers Disable sampling on channels not in the first nChans of type
    /// @return Result indicating success or error
    Result<void> setChannelSampleGroup(size_t nChans, ChannelType chanType,
                                       SampleRate rate, bool disableOthers = false);

    /// Set spike sorting options for channels of a specific type
    /// @param nChans Number of channels to configure
    /// @param chanType Channel type filter
    /// @param sortOptions Spike sorting option flags (cbAINPSPK_*)
    /// @return Result indicating success or error
    Result<void> setChannelSpikeSorting(size_t nChans, ChannelType chanType,
                                        uint32_t sortOptions);

    /// Set AC input coupling (offset correction) for channels of a specific type
    /// @param nChans Number of channels to configure (cbMAXCHANS for all)
    /// @param chanType Channel type filter
    /// @param enabled true = AC coupling (offset correction on), false = DC coupling
    /// @return Result indicating success or error
    Result<void> setACInputCoupling(size_t nChans, ChannelType chanType, bool enabled);

    /// Set full channel configuration by packet
    /// @param chaninfo Complete channel info packet to send
    /// @return Result indicating success or error
    Result<void> setChannelConfig(const cbPKT_CHANINFO& chaninfo);

    ///--------------------------------------------------------------------------------------------
    /// Comments
    ///--------------------------------------------------------------------------------------------

    /// Send a comment string to the device (appears in recorded data)
    /// @param comment Comment text (max 127 chars)
    /// @param rgba Color as RGBA uint32_t (default 0 = white)
    /// @param charset Character set (0 = ANSI)
    /// @return Result indicating success or error
    Result<void> sendComment(const std::string& comment, uint32_t rgba = 0, uint8_t charset = 0);

    ///--------------------------------------------------------------------------------------------
    /// File Recording
    ///--------------------------------------------------------------------------------------------

    /// Open Central's File Storage dialog (required before startCentralRecording)
    ///
    /// The old cbsdk/cbpy required a two-step sequence to start recording:
    /// 1. openCentralFileDialog() — sends cbFILECFG_OPT_OPEN
    /// 2. Wait ~250ms for Central to open the dialog
    /// 3. startCentralRecording() — sends recording=1
    ///
    /// @return Result indicating success or error
    Result<void> openCentralFileDialog();

    /// Close Central's File Storage dialog
    /// @return Result indicating success or error
    Result<void> closeCentralFileDialog();

    /// Start file recording on the device (Central recording)
    /// @param filename Base filename (without extension)
    /// @param comment Recording comment
    /// @return Result indicating success or error
    Result<void> startCentralRecording(const std::string& filename, const std::string& comment = "");

    /// Stop file recording on the device (Central recording)
    /// @return Result indicating success or error
    Result<void> stopCentralRecording();

    ///--------------------------------------------------------------------------------------------
    /// Analog/Digital Output
    ///--------------------------------------------------------------------------------------------

    /// Set digital output value
    /// @param chan_id Channel ID (1-based) of a digital output channel
    /// @param value Digital output value (bitmask)
    /// @return Result indicating success or error
    Result<void> setDigitalOutput(uint32_t chan_id, uint16_t value);

    /// Set analog output monitoring (route a channel's audio to an analog/audio output)
    /// @param aout_chan_id 1-based channel ID of the analog/audio output channel
    /// @param monitor_chan_id 1-based channel ID of the channel to monitor
    /// @param track_last If true, track last channel clicked in raster plot
    /// @param spike_only If true, only play spikes; if false, play continuous
    /// @return Result indicating success or error
    Result<void> setAnalogOutputMonitor(uint32_t aout_chan_id, uint32_t monitor_chan_id,
                                         bool track_last = true, bool spike_only = false);

    ///--------------------------------------------------------------------------------------------
    /// Patient Information
    ///--------------------------------------------------------------------------------------------

    /// Set patient information (embedded in recorded files)
    /// Must be called before starting recording for the info to be included.
    /// @param id Patient identification string (max 127 chars)
    /// @param firstname Patient first name (max 127 chars)
    /// @param lastname Patient last name (max 127 chars)
    /// @param dob_month Birth month (1-12, 0 = unset)
    /// @param dob_day Birth day (1-31, 0 = unset)
    /// @param dob_year Birth year (e.g. 1990, 0 = unset)
    /// @return Result indicating success or error
    Result<void> setPatientInfo(const std::string& id,
                                const std::string& firstname = "",
                                const std::string& lastname = "",
                                uint32_t dob_month = 0, uint32_t dob_day = 0, uint32_t dob_year = 0);

    ///--------------------------------------------------------------------------------------------
    /// Channel Mapping (CMP) Files
    ///--------------------------------------------------------------------------------------------

    /// Load a channel mapping file and apply electrode positions
    ///
    /// CMP files define physical electrode positions on arrays. Because the device does not
    /// persist the position field in chaninfo, this method stores positions locally and
    /// overlays them onto channel info whenever updated config data arrives from the device.
    ///
    /// Can be called multiple times for different ports on a Hub device (each port may
    /// have a different array with its own CMP file). Subsequent calls merge positions
    /// into the existing map.
    ///
    /// @param filepath Path to the .cmp file
    /// @param bank_offset Offset added to CMP bank indices to produce absolute bank numbers.
    ///        CMP bank letter A becomes absolute bank (1 + bank_offset).
    ///        Port 1: offset 0 (A=bank 1). Port 2: offset 4 (A=bank 5), etc.
    /// @return Result indicating success or error
    Result<void> loadChannelMap(const std::string& filepath, uint32_t bank_offset = 0);

    ///--------------------------------------------------------------------------------------------
    /// CCF Configuration Files
    ///--------------------------------------------------------------------------------------------

    /// Save the current device configuration to a CCF (XML) file
    /// @param filename Path to the CCF file to write
    /// @return Result indicating success or error
    Result<void> saveCCF(const std::string& filename);

    /// Load a CCF file and apply its configuration to the device
    /// @param filename Path to the CCF file to read
    /// @return Result indicating success or error
    Result<void> loadCCF(const std::string& filename);

    ///--------------------------------------------------------------------------------------------
    /// Clock Synchronization
    ///--------------------------------------------------------------------------------------------

    /// Convert a device timestamp (nanoseconds) to the host's steady_clock time_point.
    /// @param device_time_ns Device timestamp in nanoseconds
    /// @return Corresponding host time, or nullopt if no sync data available
    std::optional<std::chrono::steady_clock::time_point>
        toLocalTime(uint64_t device_time_ns) const;

    /// Convert a host steady_clock time_point to device timestamp (nanoseconds).
    /// @param local_time Host time
    /// @return Corresponding device timestamp in nanoseconds, or nullopt if no sync data available
    std::optional<uint64_t>
        toDeviceTime(std::chrono::steady_clock::time_point local_time) const;

    /// Send a clock synchronization probe to the device.
    /// @return Result indicating success or error
    Result<void> sendClockProbe();

    /// Current offset estimate: device_ns - steady_clock_ns.
    /// @return Offset in nanoseconds, or nullopt if no sync data available
    std::optional<int64_t> getClockOffsetNs() const;

    /// Uncertainty (half-RTT) from best probe, or INT64_MAX for one-way only.
    /// @return Uncertainty in nanoseconds, or nullopt if no sync data available
    std::optional<int64_t> getClockUncertaintyNs() const;

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

    /// Send a FILECFG packet (shared helper for recording commands)
    Result<void> sendFileCfgPacket(uint32_t options, uint32_t recording,
                                   const std::string& filename, const std::string& comment);

    /// Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbsdk

#endif // CBSDK_V2_SDK_SESSION_H
