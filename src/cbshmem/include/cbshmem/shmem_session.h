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

#ifndef CBSHMEM_SHMEM_SESSION_H
#define CBSHMEM_SHMEM_SESSION_H

// Include Central-compatible types which bring in protocol definitions
#include <cbshmem/central_types.h>
#include <memory>
#include <string>
#include <optional>
#include <cstdint>

namespace cbshmem {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Result type for operations that can fail
///
/// Provides simple error handling without exceptions
///
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

// Specialization for void (operations with no return value)
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
/// @brief Operating mode for shared memory session
///
enum class Mode {
    STANDALONE,  ///< First client, creates shared memory (owns device)
    CLIENT       ///< Subsequent client, attaches to existing shared memory
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
    /// @param cfg_name Config buffer shared memory name (e.g., "cbCFGbuffer")
    /// @param rec_name Receive buffer shared memory name (e.g., "cbRECbuffer")
    /// @param xmt_name Transmit buffer shared memory name (e.g., "XmtGlobal")
    /// @param xmt_local_name Local transmit buffer shared memory name (e.g., "XmtLocal")
    /// @param status_name PC status buffer shared memory name (e.g., "cbSTATUSbuffer")
    /// @param spk_name Spike cache buffer shared memory name (e.g., "cbSPKbuffer")
    /// @param signal_event_name Signal event name (e.g., "cbSIGNALevent")
    /// @param mode Operating mode (STANDALONE or CLIENT)
    /// @return Result containing ShmemSession on success, error message on failure
    static Result<ShmemSession> create(const std::string& cfg_name, const std::string& rec_name,
                                        const std::string& xmt_name, const std::string& xmt_local_name,
                                        const std::string& status_name, const std::string& spk_name,
                                        const std::string& signal_event_name, Mode mode);

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

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Instrument Status (CRITICAL for multi-instrument)
    /// @{

    /// @brief Get instrument active status
    /// @param id Instrument ID (1-based)
    /// @return true if instrument is active in shared memory
    Result<bool> isInstrumentActive(cbproto::InstrumentId id) const;

    /// @brief Set instrument active status
    /// @param id Instrument ID (1-based)
    /// @param active true to mark active, false to mark inactive
    /// @return Result indicating success or failure
    Result<void> setInstrumentActive(cbproto::InstrumentId id, bool active);

    /// @brief Get first active instrument ID
    /// @return InstrumentId of first active instrument, or error if none active
    Result<cbproto::InstrumentId> getFirstActiveInstrument() const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Configuration Read Operations
    /// @{

    /// @brief Get processor information for specified instrument
    /// @param id Instrument ID (1-based, e.g., cbNSP1)
    /// @return cbPKT_PROCINFO structure on success
    Result<cbPKT_PROCINFO> getProcInfo(cbproto::InstrumentId id) const;

    /// @brief Get bank information
    /// @param id Instrument ID (1-based)
    /// @param bank Bank number (0-based)
    /// @return cbPKT_BANKINFO structure on success
    Result<cbPKT_BANKINFO> getBankInfo(cbproto::InstrumentId id, uint32_t bank) const;

    /// @brief Get filter information
    /// @param id Instrument ID (1-based)
    /// @param filter Filter number (0-based)
    /// @return cbPKT_FILTINFO structure on success
    Result<cbPKT_FILTINFO> getFilterInfo(cbproto::InstrumentId id, uint32_t filter) const;

    /// @brief Get channel information
    /// @param channel Channel number (0-based, global across all instruments)
    /// @return cbPKT_CHANINFO structure on success
    Result<cbPKT_CHANINFO> getChanInfo(uint32_t channel) const;

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Configuration Write Operations
    /// @{

    /// @brief Set processor information for specified instrument
    /// @param id Instrument ID (1-based)
    /// @param info cbPKT_PROCINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setProcInfo(cbproto::InstrumentId id, const cbPKT_PROCINFO& info);

    /// @brief Set bank information
    /// @param id Instrument ID (1-based)
    /// @param bank Bank number (0-based)
    /// @param info cbPKT_BANKINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setBankInfo(cbproto::InstrumentId id, uint32_t bank, const cbPKT_BANKINFO& info);

    /// @brief Set filter information
    /// @param id Instrument ID (1-based)
    /// @param filter Filter number (0-based)
    /// @param info cbPKT_FILTINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setFilterInfo(cbproto::InstrumentId id, uint32_t filter, const cbPKT_FILTINFO& info);

    /// @brief Set channel information
    /// @param channel Channel number (0-based)
    /// @param info cbPKT_CHANINFO structure to write
    /// @return Result indicating success or failure
    Result<void> setChanInfo(uint32_t channel, const cbPKT_CHANINFO& info);

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

    /// @brief Get NSP status for specified instrument
    /// @param id Instrument ID (1-based)
    /// @return NSP status (INIT, NOIPADDR, NOREPLY, FOUND, INVALID)
    Result<NSPStatus> getNspStatus(cbproto::InstrumentId id) const;

    /// @brief Set NSP status for specified instrument
    /// @param id Instrument ID (1-based)
    /// @param status NSP status to set
    /// @return Result indicating success or failure
    Result<void> setNspStatus(cbproto::InstrumentId id, NSPStatus status);

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
    Result<void> getSpikeCache(uint32_t channel, CentralSpikeCache& cache) const;

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

    /// @}

private:
    /// @brief Private constructor (use create() factory method)
    ShmemSession();

    /// @brief Platform-specific implementation (pimpl idiom)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cbshmem

#endif // CBSHMEM_SHMEM_SESSION_H
