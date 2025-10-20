//////////////////////////////////////////////////////////////////////
/**
* \file EventData.h
* \brief Event data class for spike/event storage (internal use)
*
* This header contains the refactored event data class that stores
* spike and digital input event data as a flat time-series of events.
*
* This class is internal to the SDK implementation and is not part of
* the public API.
*/

#ifndef EVENTDATA_H_INCLUDED
#define EVENTDATA_H_INCLUDED

#include "../../include/cerelink/cbhwlib.h"
#include <mutex>


/// Class to store event data (spikes, digital inputs) as a time-series
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
    /// \param buffer_size Total number of events to buffer (across all channels)
    /// \return true if allocation succeeded
    [[nodiscard]] bool allocate(uint32_t buffer_size);

    /// Write an event to the ring buffer
    /// \param channel Channel number (1-based)
    /// \param timestamp Timestamp for this event
    /// \param unit Unit classification (0-5 for units, 255 for noise) or digital data
    /// \return true if buffer overflowed (oldest data was overwritten)
    [[nodiscard]] bool writeEvent(uint16_t channel, PROCTIME timestamp, uint16_t unit);

    /// Reset ring buffer indices and zero data (preserves allocation)
    void reset();

    /// Cleanup - deallocates all memory and resets to constructor state
    void cleanup();

    // Getters for read access
    [[nodiscard]] uint32_t getSize() const { return m_size > 0 ? m_size - 1 : 0; }  // Return usable capacity
    [[nodiscard]] uint32_t getWriteIndex() const { return m_write_index; }
    [[nodiscard]] uint32_t getWriteStartIndex() const { return m_write_start_index; }
    [[nodiscard]] uint32_t getNumEvents() const;  // Number of events currently in buffer

    [[nodiscard]] const PROCTIME* getTimestamps() const { return m_timestamps; }
    [[nodiscard]] const uint16_t* getChannels() const { return m_channels; }
    [[nodiscard]] const uint16_t* getUnits() const { return m_units; }

    [[nodiscard]] int16_t* getWaveformData() { return m_waveform_data; }
    [[nodiscard]] const int16_t* getWaveformData() const { return m_waveform_data; }
    [[nodiscard]] bool isAllocated() const { return m_timestamps != nullptr; }

    // Setters for write index management
    void setWriteStartIndex(uint32_t index);
    void setWriteIndex(uint32_t index);

    /// Read all events from the ring buffer
    /// \param output_timestamps Output array for timestamps
    /// \param output_channels Output array for channel IDs
    /// \param output_units Output array for units/digital data
    /// \param max_events Maximum number of events to read
    /// \param bSeek If true, advance read position
    /// \return Actual number of events read
    [[nodiscard]] uint32_t readEvents(PROCTIME* output_timestamps,
                                      uint16_t* output_channels,
                                      uint16_t* output_units,
                                      uint32_t max_events,
                                      bool bSeek);

    // Public member for external access control
    mutable std::mutex m_mutex;  ///< Mutex for thread-safe access

private:
    // Buffer configuration
    uint32_t m_size;  ///< Total buffer capacity (events across all channels)

    // Flat time-series storage
    PROCTIME* m_timestamps;  ///< [size] - timestamp for each event
    uint16_t* m_channels;    ///< [size] - channel ID (1-based) for each event
    uint16_t* m_units;       ///< [size] - unit classification or digital data for each event

    // Shared waveform buffer (allocated on demand, managed externally)
    int16_t* m_waveform_data;  ///< Buffer with maximum size [size][cbMAX_PNTS]

    // Ring buffer management (single buffer for all channels)
    uint32_t m_write_index;        ///< Next index location to write data
    uint32_t m_write_start_index;  ///< Index location that reading can begin
};

#endif // EVENTDATA_H_INCLUDED
