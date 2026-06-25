///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   shmem_session.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Shared memory session management for Cerebus protocol
///
/// This module provides cross-platform shared memory management for configuration and data
/// buffers used by Central and cbsdk clients.
///
/// Key Design Principles:
/// - Uses Central-compatible buffer layout (cbMAXPROCS=4, not 1)
/// - Mode-independent indexing (always uses packet.instrument)
/// - Thread-safe for concurrent access
/// - Platform-abstracted (Windows/macOS/Linux)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_SHMEM_SESSION_H
#define CBSHM_SHMEM_SESSION_H

// Include Central-compatible types which bring in protocol definitions
#include <cbshm/central_current.h>
#include <cbshm/native_types.h>
#include <cbproto/connection.h>
#include <cbutil/result.h>
#include <memory>
#include <string>
#include <cstdint>

namespace cbshm {

template<typename T>
using Result = cbutil::Result<T>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Operating mode for shared memory session
///
enum class Mode {
    STANDALONE,  ///< First client, creates shared memory (owns device)
    CLIENT       ///< Subsequent client, attaches to existing shared memory
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Buffer layout for shared memory session
///
/// Controls buffer sizes, struct types, and bounds checking.
///
enum class ShmemLayout {
    CENTRAL,         ///< Central's actual binary layout (cbCFGBUFF)
    NATIVE           ///< Native single-instrument layout (NativeConfigBuffer)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Shared memory session for Cerebus configuration and data buffers
///
/// Manages lifecycle of shared memory buffers that are compatible with Central's layout.
/// Implements correct indexing for multi-instrument systems.
///
/// CRITICAL: Even in STANDALONE mode, uses Central-compatible layout (cbMAXPROCS=4)
/// so that subsequent CLIENT connections work correctly.
///
class ShmemSession {
public:
    ///////////////////////////////////////////////////////////////////////////
    /// @name Lifecycle
    /// @{

    /// @brief Create a new shared memory session
    ///
    /// The seven segment names are synthesized internally from @p layout and
    /// @p name_qualifier, whose meaning depends on the layout:
    /// - CENTRAL: Central's fixed, well-known names (e.g., "cbCFGbuffer"), with
    ///   @p name_qualifier appended as the Central *instance* suffix ("" selects
    ///   the primary instance, "1" selects cbCFGbuffer1, etc.).
    /// - NATIVE: per-device names of the form "cbshm_<name_qualifier>_<segment>",
    ///   where @p name_qualifier is the device token (e.g., "hub1").
    ///
    /// @param mode Operating mode (STANDALONE or CLIENT)
    /// @param layout Buffer layout (CENTRAL or NATIVE)
    /// @param name_qualifier Layout-specific naming discriminator: the Central
    ///                       instance suffix for CENTRAL, or the device token for
    ///                       NATIVE
    /// @param id Instrument ID (1-based)
    /// @return Result containing ShmemSession on success, error message on failure
    static Result<ShmemSession> create(Mode mode, ShmemLayout layout,
                                        const std::string& name_qualifier, cbproto::InstrumentId id);

    /// @brief Destructor - closes shared memory and releases resources
    ~ShmemSession();

    // Move-only type (no copying of shared memory sessions)
    ShmemSession(ShmemSession&& other) noexcept;
    ShmemSession& operator=(ShmemSession&& other) noexcept;
    ShmemSession(const ShmemSession&) = delete;
    ShmemSession& operator=(const ShmemSession&) = delete;

    /// @}
    
    ///////////////////////////////////////////////////////////////////////////
    /// @name Status
    /// @{

    /// @brief Check if session is open and ready
    /// @return true if session is open, false otherwise
    bool isOpen() const;

    /// @brief Get the current operating mode
    /// @return STANDALONE or CLIENT
    Mode getMode() const;

    /// @brief Get the current buffer layout
    /// @return CENTRAL or NATIVE
    ShmemLayout getLayout() const;

    /// @brief Get the maximum number of instruments
    ///
    /// With the NATIVE layout, always returns 1.
    /// With the CENTRAL layout, returns the value of cbMAXPROCS.
    ///
    /// @return the maximum instrument count
    uint32_t getMaxProcs() const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Instrument Status
    /// @{

    /// @brief Get instrument active status
    ///
    /// Always returns true if layout is CENTRAL.
    ///
    /// @return true if instrument is active in shared memory
    Result<bool> isInstrumentActive() const;

    /// @brief Set instrument active status
    ///
    /// Does nothing if layout is CENTRAL (instruments assumed to always be active).
    ///
    /// @param active true to mark active, false to mark inactive
    /// @return Result indicating success or failure
    Result<void> setInstrumentActive(bool active);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Configuration Read Operations
    /// @{

    /// @brief Get processor information
    /// @return cbPKT_PROCINFO structure on success
    Result<cbPKT_PROCINFO> getProcInfo() const;

    /// @brief Get bank information
    /// @param bank Bank number (1-based, as used in cbPKT_BANKINFO)
    /// @return cbPKT_BANKINFO structure on success
    Result<cbPKT_BANKINFO> getBankInfo(uint32_t bank) const;

    /// @brief Get filter information
    /// @param filter Filter number (1-based, as used in cbPKT_FILTINFO)
    /// @return cbPKT_FILTINFO structure on success
    Result<cbPKT_FILTINFO> getFilterInfo(uint32_t filter) const;

    /// @brief Get channel information
    /// @param channel Channel number (0-based, global across all instruments)
    /// @return cbPKT_CHANINFO structure on success
    Result<cbPKT_CHANINFO> getChanInfo(uint32_t channel) const;
    
    /// @brief Get system information
    /// @return cbPKT_SYSINFO structure on success
    Result<cbPKT_SYSINFO> getSysInfo() const;
    
    /// @brief Get sample group information
    /// @param group Group number (0-based, 0 to cbMAXGROUPS-1)
    /// @param info cbPKT_GROUPINFO structure to write
    /// @return Result indicating success or failure
    Result<cbPKT_GROUPINFO> getGroupInfo(uint32_t group) const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Configuration Write Operations
    /// @{

    /// @brief Set processor information
    /// @param info cbPKT_PROCINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setProcInfo(const cbPKT_PROCINFO& info);

    /// @brief Set bank information
    /// @param bank Bank number (0-based)
    /// @param info cbPKT_BANKINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setBankInfo(uint32_t bank, const cbPKT_BANKINFO& info);

    /// @brief Set filter information
    /// @param filter Filter number (0-based)
    /// @param info cbPKT_FILTINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setFilterInfo(uint32_t filter, const cbPKT_FILTINFO& info);

    /// @brief Set channel information
    /// @param channel Channel number (0-based)
    /// @param info cbPKT_CHANINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setChanInfo(uint32_t channel, const cbPKT_CHANINFO& info);

    /// @brief Set system information (sysfreq, spike length, etc.)
    /// @param info cbPKT_SYSINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setSysInfo(const cbPKT_SYSINFO& info);

    /// @brief Set sample group information
    /// @param group Group number (0-based, 0 to cbMAXGROUPS-1)
    /// @param info cbPKT_GROUPINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setGroupInfo(uint32_t group, const cbPKT_GROUPINFO& info);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Configuration Buffer Direct Access
    /// @{

    /// @brief Get direct pointer to native configuration buffer
    /// @return Pointer to native configuration buffer, or nullptr if not NATIVE layout
    NativeConfigBuffer* getNativeConfigBuffer();

    /// @brief Get direct pointer to native configuration buffer (const version)
    /// @return Const pointer to native configuration buffer, or nullptr if not NATIVE layout
    const NativeConfigBuffer* getNativeConfigBuffer() const;

    /// @brief Get a translated copy of Central's configuration buffer
    /// @param buf Output parameter to receive the configuration buffer (very large, allocate on the heap!)
    /// @return Result::value containing the configuration buffer, or Result::error if not CENTRAL layout
    Result<void> getLegacyConfigBuffer(NativeConfigBuffer& buf);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Clock Synchronization
    /// @{

    /// @brief Set clock sync offset (called by STANDALONE mode)
    /// @param offset_ns device_ns - steady_clock_ns
    /// @param uncertainty_ns Half-RTT uncertainty
    void setClockSync(int64_t offset_ns, int64_t uncertainty_ns);

    /// @brief Get clock sync offset (readable by CLIENT mode)
    /// @return offset in nanoseconds, or nullopt if no sync data
    std::optional<int64_t> getClockOffsetNs() const;

    /// @brief Get clock sync uncertainty
    /// @return uncertainty in nanoseconds, or nullopt if no sync data
    std::optional<int64_t> getClockUncertaintyNs() const;

    /// @brief Check if the STANDALONE owner of these segments is still alive
    ///
    /// For NATIVE CLIENT mode, reads owner_pid from the config buffer and checks
    /// if that process still exists. Returns true in all other cases (STANDALONE mode,
    /// non-NATIVE layout, or unknown PID).
    ///
    /// @return false if the owner process is confirmed dead (stale segments), true otherwise
    bool isOwnerAlive() const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Packet Routing (THE KEY FIX)
    /// @{

    /// @brief Store a packet in shared memory using correct indexing
    ///
    /// CRITICAL FIX: This method ALWAYS uses packet.cbpkt_header.instrument
    /// as the array index, regardless of mode. This ensures:
    /// - Standalone mode: packets go to correct slot for later CLIENT access
    /// - Client mode: packets go to same slot Central would use
    ///
    /// @param pkt Generic packet to store
    /// @return Result indicating success or failure
    Result<void> storePacket(const cbPKT_GENERIC& pkt);

    /// @brief Store multiple packets in batch
    /// @param pkts Array of packets
    /// @param count Number of packets
    /// @return Result indicating success or failure
    Result<void> storePackets(const cbPKT_GENERIC* pkts, size_t count);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Transmit Queue (for sending packets to device)
    /// @{

    /// @brief Enqueue a packet to be sent to the device
    ///
    /// Writes packet to shared memory transmit buffer. In STANDALONE mode,
    /// the device thread will dequeue and send it. In CLIENT mode, the
    /// STANDALONE process will dequeue and send it.
    ///
    /// @param pkt Packet to enqueue for transmission
    /// @return Result indicating success or failure (buffer full returns error)
    Result<void> enqueuePacket(const cbPKT_GENERIC& pkt);

    /// @brief Dequeue a packet from the transmit buffer
    ///
    /// Used by STANDALONE mode to get packets to send to device.
    /// Returns error if queue is empty.
    ///
    /// @param pkt Output parameter to receive dequeued packet
    /// @return Result<bool> - true if packet was dequeued, false if queue empty
    Result<bool> dequeuePacket(cbPKT_GENERIC& pkt);

    /// @brief Check if transmit queue has packets waiting
    /// @return true if queue has packets, false if empty
    bool hasTransmitPackets() const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Local Transmit Queue (IPC-only packets)
    /// @{

    /// @brief Enqueue a packet to local transmit buffer (IPC only, not sent to device)
    ///
    /// Writes packet to XmtLocal buffer for local client communication.
    /// These packets are NOT sent to the device - they're only visible to
    /// local processes via shared memory.
    ///
    /// This is the shared memory equivalent of cbSendLoopbackPacket().
    ///
    /// @param pkt Packet to enqueue for local IPC
    /// @return Result indicating success or failure (buffer full returns error)
    Result<void> enqueueLocalPacket(const cbPKT_GENERIC& pkt);

    /// @brief Dequeue a packet from the local transmit buffer
    ///
    /// Used by local clients to receive IPC-only packets.
    /// Returns error if queue is empty.
    ///
    /// @param pkt Output parameter to receive dequeued packet
    /// @return Result<bool> - true if packet was dequeued, false if queue empty
    Result<bool> dequeueLocalPacket(cbPKT_GENERIC& pkt);

    /// @brief Check if local transmit queue has packets waiting
    /// @return true if queue has packets, false if empty
    bool hasLocalTransmitPackets() const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name PC Status Buffer Access
    /// @{

    /// @brief Get total number of channels in the system
    /// @return Total channel count
    Result<uint32_t> getNumTotalChans() const;

    /// @brief Get NSP status
    /// @return NSP status (INIT, NOIPADDR, NOREPLY, FOUND, INVALID)
    Result<NativeNSPStatus> getNspStatus() const;

    /// @brief Set NSP status
    /// @param status NSP status to set
    /// @return Result indicating success or failure
    Result<void> setNspStatus(NativeNSPStatus status);

    /// @brief Check if system is configured as Gemini
    /// @return true if Gemini system, false otherwise
    Result<bool> isGeminiSystem() const;

    /// @brief Set Gemini system flag
    /// @param is_gemini true for Gemini system, false otherwise
    /// @return Result indicating success or failure
    Result<void> setGeminiSystem(bool is_gemini);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Spike Cache Buffer Access
    /// @{

    /// @brief Get spike cache for a specific channel
    ///
    /// The spike cache stores the most recent spikes for each channel,
    /// allowing tools like Raster to quickly access recent spikes without
    /// scanning the entire receive buffer.
    ///
    /// @param channel Channel number (0-based)
    /// @param cache Output parameter to receive spike cache
    /// @return Result indicating success or failure
    Result<void> getSpikeCache(uint32_t channel, NativeSpikeCache& cache) const;

    /// @brief Get most recent spike packet from cache
    ///
    /// Returns the most recently cached spike for a channel. This is faster
    /// than scanning the receive buffer.
    ///
    /// @param channel Channel number (0-based)
    /// @param spike Output parameter to receive spike packet
    /// @return Result<bool> - true if spike available, false if cache empty
    Result<bool> getRecentSpike(uint32_t channel, cbPKT_SPK& spike) const;

    /// @}

    /// @brief Get detected protocol version for CENTRAL mode
    ///
    /// In CENTRAL mode, Central may store packets in an older protocol format.
    /// This returns the detected protocol version based on Central's executable
    /// file. Returns CBPROTO_PROTOCOL_CURRENT for the NATIVE layout.
    ///
    /// @return Detected protocol version
    cbproto_protocol_version_t getCompatProtocolVersion() const;

    ///////////////////////////////////////////////////////////////////////////
    /// @name Receive Buffer Access (Ring Buffer for Incoming Packets)
    /// @{

    /// @brief Read available packets from receive buffer
    ///
    /// Reads packets that have been written since last read.
    /// Uses ring buffer with wrap-around tracking.
    ///
    /// @param packets Output buffer to receive packets (must be pre-allocated)
    /// @param max_packets Maximum number of packets to read
    /// @param packets_read Output: actual number of packets read
    /// @return Result indicating success or failure
    Result<void> readReceiveBuffer(cbPKT_GENERIC* packets, size_t max_packets, size_t& packets_read);

    /// @brief Get current receive buffer statistics
    ///
    /// Returns information about the receive buffer state for monitoring.
    ///
    /// @param received Total packets received by writer
    /// @param available Packets available to read (not yet consumed)
    /// @return Result indicating success or failure
    Result<void> getReceiveBufferStats(uint32_t& received, uint32_t& available) const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Data Synchronization (Signal Event)
    /// @{

    /// @brief Wait for data availability signal
    ///
    /// Blocks until Central signals new data is available, or timeout occurs.
    /// This is the shared memory equivalent of cbWaitforData().
    ///
    /// @param timeout_ms Timeout in milliseconds (default 250ms)
    /// @return Result<bool> - true if signal received, false if timeout
    Result<bool> waitForData(uint32_t timeout_ms = 250) const;

    /// @brief Signal that new data is available
    ///
    /// Used by STANDALONE mode to notify CLIENT processes that new data
    /// has been written to shared memory buffers.
    ///
    /// On Windows: SetEvent() to signal manual-reset event
    /// On POSIX: sem_post() to increment semaphore
    ///
    /// @return Result indicating success or failure
    Result<void> signalData();

    /// @brief Reset the data signal
    ///
    /// Used by STANDALONE mode after clients have consumed data.
    /// Only applicable to Windows (manual-reset events).
    ///
    /// On Windows: ResetEvent() to clear the event
    /// On POSIX: No-op (semaphore is auto-reset by sem_wait)
    ///
    /// @return Result indicating success or failure
    Result<void> resetSignal();

    /// @brief Get last timestamp from receive buffer (always nanoseconds)
    ///
    /// Returns the most recent packet timestamp written to the receive buffer.
    /// In CENTRAL mode with a non-Gemini device the raw value (clock ticks)
    /// is converted to nanoseconds using sysfreq, so callers always receive a
    /// uniform nanosecond timestamp.
    ///
    /// @return Last timestamp in nanoseconds, or 0 if receive buffer not initialized
    PROCTIME getLastTime() const;

    /// @}

private:
    /// @brief Private constructor (use create() factory method)
    ShmemSession();

    /// @brief Platform-specific implementation (pimpl idiom)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbshm

#endif // CBSHMEM_SHMEM_SESSION_H
