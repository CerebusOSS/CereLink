//////////////////////////////////////////////////////////////////////
/**
* \file ContinuousData.h
* \brief Continuous data classes for per-group allocation (internal use)
*
* This header contains the refactored continuous data classes that use
* per-group allocation instead of per-channel allocation. This reduces memory
* usage and improves cache locality.
*
* These classes are internal to the SDK implementation and are not part of
* the public API.
*/

#ifndef CONTINUOUSDATA_H_INCLUDED
#define CONTINUOUSDATA_H_INCLUDED

#include "../../include/cerelink/cbhwlib.h"


/// Class to store continuous data for a single sample group
class GroupContinuousData
{
public:
    /// Constructor - initializes all fields to safe defaults
    GroupContinuousData();

    /// Destructor
    ~GroupContinuousData();

    // Disable copy (to avoid accidental deep copy issues)
    GroupContinuousData(const GroupContinuousData&) = delete;
    GroupContinuousData& operator=(const GroupContinuousData&) = delete;

    /// Check if reallocation is needed for new channel configuration
    /// \param chan_ids Array of channel IDs from packet
    /// \param n_chans Number of channels in packet
    /// \return true if reallocation/update needed
    bool needsReallocation(const uint16_t* chan_ids, uint32_t n_chans) const;

    /// Allocate or reallocate buffers for this group
    /// \param buffer_size Number of samples to buffer
    /// \param n_chans Number of channels
    /// \param chan_ids Array of channel IDs (1-based)
    /// \param rate Sample rate for this group
    /// \return true if allocation succeeded
    bool allocate(uint32_t buffer_size, uint32_t n_chans, const uint16_t* chan_ids, uint16_t rate);

    /// Write a sample to the ring buffer
    /// \param timestamp Timestamp for this sample
    /// \param data Pointer to channel data (nChans elements)
    /// \param n_chans Number of channels in data
    /// \return true if buffer overflowed (oldest data was overwritten)
    bool writeSample(PROCTIME timestamp, const int16_t* data, uint32_t n_chans);

    /// Reset ring buffer indices and zero data (preserves allocation)
    void reset();

    /// Cleanup - deallocates all memory and resets to constructor state
    void cleanup();

    // Getters for read access
    uint32_t getSize() const { return m_size; }
    uint16_t getSampleRate() const { return m_sample_rate; }
    uint32_t getWriteIndex() const { return m_write_index; }
    uint32_t getWriteStartIndex() const { return m_write_start_index; }
    uint32_t getReadEndIndex() const { return m_read_end_index; }
    uint32_t getNumChannels() const { return m_num_channels; }
    uint32_t getCapacity() const { return m_capacity; }
    const uint16_t* getChannelIds() const { return m_channel_ids; }
    const PROCTIME* getTimestamps() const { return m_timestamps; }
    const int16_t* const* getChannelData() const { return m_channel_data; }
    bool isAllocated() const { return m_channel_data != nullptr; }

    // Setters for write index management (used by SdkGetTrialData)
    void setWriteStartIndex(uint32_t index) { m_write_start_index = index; }
    void setReadEndIndex(uint32_t index) { m_read_end_index = index; }
    void setWriteIndex(uint32_t index) { m_write_index = index; }

private:
    // Buffer configuration
    uint32_t m_size;                    ///< Buffer size for this group (samples)
    uint16_t m_sample_rate;             ///< Sample rate for this group (Hz)

    // Ring buffer management
    uint32_t m_write_index;             ///< Next write position in ring buffer
    uint32_t m_write_start_index;       ///< Where reading starts (oldest unread sample)
    uint32_t m_read_end_index;          ///< Last safe read position (snapshot for readers)

    // Dynamic channel management
    uint32_t m_num_channels;            ///< Number of channels actually in this group
    uint32_t m_capacity;                ///< Allocated capacity (may exceed num_channels for headroom)
    uint16_t* m_channel_ids;            ///< Array of channel IDs (1-based, size = capacity)

    // Data storage (cache-friendly layout: [samples][channels])
    // This layout allows memcpy from packet data directly, as packets contain
    // all channels for a single timestamp.
    PROCTIME* m_timestamps;             ///< [size] - timestamp for each sample
    int16_t** m_channel_data;           ///< [size][capacity] - data indexed by [sample][channel]
};


/// Class to store all continuous data organized by sample groups
class ContinuousData
{
public:
    ContinuousData() : default_size(0) {}

    uint32_t default_size;                      ///< Default buffer size (cbSdk_CONTINUOUS_DATA_SAMPLES)
    GroupContinuousData groups[cbMAXGROUPS];    ///< One group per sample rate (0-7)

    /// Helper: Find channel index within a group
    /// \param group_idx Group index (0-7)
    /// \param channel_id Channel ID to find (1-based)
    /// \return Channel index within group (0-based), or -1 if not found
    int32_t findChannelInGroup(uint32_t group_idx, uint16_t channel_id) const
    {
        if (group_idx >= cbMAXGROUPS)
            return -1;

        const auto& grp = groups[group_idx];
        const uint16_t* chan_ids = grp.getChannelIds();
        if (!chan_ids)
            return -1;

        for (uint32_t i = 0; i < grp.getNumChannels(); ++i)
        {
            if (chan_ids[i] == channel_id)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    /// Reset all groups (preserves allocations)
    void reset()
    {
        for (auto& grp : groups)
            grp.reset();
    }

    /// Cleanup all groups (deallocates all memory)
    void cleanup()
    {
        for (auto& grp : groups)
            grp.cleanup();
    }
};

#endif // CONTINUOUSDATA_H_INCLUDED
