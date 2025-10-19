//////////////////////////////////////////////////////////////////////
/**
* \file EventData.h
* \brief Event data class for spike/event storage (internal use)
*
* This header contains the refactored event data class that stores
* spike and digital input event data with per-channel ring buffers.
*
* This class is internal to the SDK implementation and is not part of
* the public API.
*/

#ifndef EVENTDATA_H_INCLUDED
#define EVENTDATA_H_INCLUDED

#include "../../include/cerelink/cbhwlib.h"
#include <mutex>


/// Class to store event data (spikes, digital inputs) for all channels
class EventData
{
public:
    /// Constructor - initializes all fields to safe defaults
    EventData();

    /// Destructor
    ~EventData();

    // Disable copy (to avoid accidental deep copy issues)
    EventData(const EventData&) = delete;
    EventData& operator=(const EventData&) = delete;

    /// Allocate or reallocate buffers for event storage
    /// \param buffer_size Number of events to buffer per channel
    /// \return true if allocation succeeded
    [[nodiscard]] bool allocate(uint32_t buffer_size);

    /// Write an event to the ring buffer for a specific channel
    /// \param channel Channel number (1-based)
    /// \param timestamp Timestamp for this event
    /// \param unit Unit classification (0-5 for units, 255 for noise, etc.)
    /// \return true if buffer overflowed (oldest data was overwritten)
    [[nodiscard]] bool writeEvent(uint16_t channel, PROCTIME timestamp, uint16_t unit);

    /// Reset ring buffer indices and zero data (preserves allocation)
    void reset();

    /// Cleanup - deallocates all memory and resets to constructor state
    void cleanup();

    // Getters for read access
    [[nodiscard]] uint32_t getSize() const { return m_size; }
    [[nodiscard]] const PROCTIME* getTimestamps(uint16_t channel) const;
    [[nodiscard]] const uint16_t* getUnits(uint16_t channel) const;
    [[nodiscard]] uint32_t getWriteIndex(uint16_t channel) const;
    [[nodiscard]] uint32_t getWriteStartIndex(uint16_t channel) const;
    [[nodiscard]] int16_t* getWaveformData() { return m_waveform_data; }
    [[nodiscard]] const int16_t* getWaveformData() const { return m_waveform_data; }
    [[nodiscard]] bool isAllocated() const { return m_timestamps[0] != nullptr; }

    // Setters for write index management (used by SdkGetTrialData)
    void setWriteStartIndex(uint16_t channel, uint32_t index);
    void setWriteIndex(uint16_t channel, uint32_t index);

    // Reading methods
    /// Get number of events available to read for a channel
    /// \param channel Channel number (1-based)
    /// \return Number of available events, or 0 if invalid/not allocated
    [[nodiscard]] uint32_t getAvailableSamples(uint16_t channel) const;

    /// Count number of samples per unit for a channel (without reading)
    /// \param channel Channel number (1-based)
    /// \param num_samples_per_unit Output: count of samples per unit
    /// \param is_digital_or_serial True if channel is digital or serial
    void countSamplesPerUnit(uint16_t channel,
                            uint32_t num_samples_per_unit[cbMAXUNITS + 1],
                            bool is_digital_or_serial) const;

    /// Read events from a channel, separating by unit classification
    /// \param channel Channel number (1-based)
    /// \param max_samples_per_unit Array of max samples to read per unit
    /// \param timestamps Array of output timestamp pointers (per unit), can be nullptr
    /// \param digital_data Output for digital/serial data, can be nullptr
    /// \param num_samples_per_unit Output: actual samples read per unit
    /// \param is_digital_or_serial True if channel is digital or serial
    /// \param bSeek If true, advance read position
    /// \param final_read_index Output: final read position after reading
    void readChannelEvents(uint16_t channel,
                          const uint32_t max_samples_per_unit[cbMAXUNITS + 1],
                          PROCTIME* timestamps[cbMAXUNITS + 1],
                          uint16_t* digital_data,
                          uint32_t num_samples_per_unit[cbMAXUNITS + 1],
                          bool is_digital_or_serial,
                          bool bSeek,
                          uint32_t& final_read_index);

    // Public member for external access control
    mutable std::mutex m_mutex;  ///< Mutex for thread-safe access

private:
    // Buffer configuration
    uint32_t m_size;  ///< Buffer size per channel (events)

    // We use cbMAXCHANS to size the arrays,
    // even though that's more than the analog + digin + serial channels that are required,
    // simply so we can index into these arrays using the channel number (-1).
    // The alternative is to map between channel number and array index, but
    // this is problematic with the recent change to 2-NSP support.
    // Later we may add an m_ChIdxInType or m_ChIdxInBuff for such a map.

    // Per-channel data storage
    PROCTIME* m_timestamps[cbMAXCHANS];  ///< [cbMAXCHANS][size] - timestamps per channel
    uint16_t* m_units[cbMAXCHANS];       ///< [cbMAXCHANS][size] - unit classifications per channel

    // Shared waveform buffer (allocated on demand, managed externally)
    int16_t* m_waveform_data;  ///< Buffer with maximum size [cbNUM_ANALOG_CHANS][size][cbMAX_PNTS]

    // Ring buffer management (per channel)
    uint32_t m_write_index[cbMAXCHANS];        ///< Next index location to write data
    uint32_t m_write_start_index[cbMAXCHANS];  ///< Index location that writing began
};

#endif // EVENTDATA_H_INCLUDED
