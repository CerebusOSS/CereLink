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
#include <mutex>


/// Class to store continuous data for a single sample group
class GroupContinuousData
{
public:
    /// Constructor - initializes all fields to safe defaults
    GroupContinuousData();

    // Allow ContinuousData to access private mutex for multi-group operations
    friend class ContinuousData;

    /// Destructor
    ~GroupContinuousData();

    // Disable copy (to avoid accidental deep copy issues)
    GroupContinuousData(const GroupContinuousData&) = delete;
    GroupContinuousData& operator=(const GroupContinuousData&) = delete;

    /// Check if reallocation is needed for new channel configuration
    /// \param chan_ids Array of channel IDs from packet
    /// \param n_chans Number of channels in packet
    /// \return true if reallocation/update needed
    [[nodiscard]] bool needsReallocation(const uint16_t* chan_ids, uint32_t n_chans) const;

    /// Allocate or reallocate buffers for this group
    /// \param buffer_size Number of samples to buffer
    /// \param n_chans Number of channels
    /// \param chan_ids Array of channel IDs (1-based)
    /// \return true if allocation succeeded
    [[nodiscard]] bool allocate(uint32_t buffer_size, uint32_t n_chans, const uint16_t* chan_ids);

    /// Write a sample to the ring buffer
    /// \param timestamp Timestamp for this sample
    /// \param data Pointer to channel data (nChans elements)
    /// \param n_chans Number of channels in data
    /// \return true if buffer overflowed (oldest data was overwritten)
    [[nodiscard]] bool writeSample(PROCTIME timestamp, const int16_t* data, uint32_t n_chans);

    /// Reset ring buffer indices and zero data (preserves allocation)
    void reset();

    /// Cleanup - deallocates all memory and resets to constructor state
    void cleanup();

    // Getters for read access
    [[nodiscard]] uint32_t getSize() const { return m_size; }
    [[nodiscard]] uint32_t getWriteIndex() const { return m_write_index; }
    [[nodiscard]] uint32_t getWriteStartIndex() const { return m_write_start_index; }
    [[nodiscard]] uint32_t getReadEndIndex() const { return m_read_end_index; }
    [[nodiscard]] uint32_t getNumChannels() const { return m_num_channels; }
    [[nodiscard]] const uint16_t* getChannelIds() const { return m_channel_ids; }
    [[nodiscard]] const PROCTIME* getTimestamps() const { return m_timestamps; }
    [[nodiscard]] const int16_t* getChannelData() const { return m_channel_data; }
    [[nodiscard]] bool isAllocated() const { return m_channel_data != nullptr; }

    // Setters for write index management (used by SdkGetTrialData)
    void setWriteStartIndex(const uint32_t index) { m_write_start_index = index; }
    void setReadEndIndex(const uint32_t index) { m_read_end_index = index; }
    void setWriteIndex(const uint32_t index) { m_write_index = index; }

private:
    // Buffer configuration
    uint32_t m_size;                    ///< Buffer size for this group (samples)

    // Ring buffer management
    uint32_t m_write_index;             ///< Next write position in ring buffer
    uint32_t m_write_start_index;       ///< Where reading starts (oldest unread sample)
    uint32_t m_read_end_index;          ///< Last safe read position (snapshot for readers)

    // Dynamic channel management
    uint32_t m_num_channels;            ///< Number of channels in this group
    uint16_t* m_channel_ids;            ///< Array of channel IDs (1-based, size = num_channels)

    // Data storage (contiguous layout: [samples * channels])
    // Single contiguous allocation for [size][num_channels] layout enables bulk memcpy.
    // Access: m_channel_data[sample_idx * m_num_channels + channel_idx]
    PROCTIME* m_timestamps;             ///< [size] - timestamp for each sample
    int16_t* m_channel_data;            ///< [size * num_channels] - contiguous data block

    mutable std::mutex m_mutex;         ///< Mutex for thread-safe access to this group
};


/// Structure to hold snapshot of group state for reading
struct GroupSnapshot
{
    uint32_t read_start_index;    ///< Start of available data
    uint32_t read_end_index;      ///< End of available data
    uint32_t num_samples;         ///< Number of samples available
    uint32_t num_channels;        ///< Number of channels in group
    uint32_t buffer_size;         ///< Total buffer size
    bool is_allocated;            ///< Whether group is allocated
};

/// Class to store all continuous data organized by sample groups
class ContinuousData
{
public:
    ContinuousData() : default_size(0) {}

    uint32_t default_size;                      ///< Default buffer size (cbSdk_CONTINUOUS_DATA_SAMPLES)
    GroupContinuousData groups[cbMAXGROUPS];    ///< One group per sample rate (0-7)

    /// Thread-safe write of a sample to a group with automatic reallocation
    /// \param group_idx Group index (0-based, 0-7)
    /// \param timestamp Timestamp for this sample
    /// \param data Pointer to channel data
    /// \param n_chans Number of channels in data
    /// \param chan_ids Array of channel IDs (1-based)
    /// \return true if buffer overflowed (oldest data was overwritten)
    [[nodiscard]] bool writeSampleThreadSafe(uint32_t group_idx, PROCTIME timestamp,
                                              const int16_t* data, uint32_t n_chans,
                                              const uint16_t* chan_ids);

    /// Thread-safe snapshot of group state for reading
    /// \param group_idx Group index (0-based, 0-7)
    /// \param snapshot Output structure to fill with snapshot data
    /// \return true if successful, false if group_idx invalid
    [[nodiscard]] bool snapshotForReading(uint32_t group_idx, GroupSnapshot& snapshot);

    /// Thread-safe read of samples from a group
    /// \param group_idx Group index (0-based, 0-7)
    /// \param output_samples Output buffer for sample data [num_samples * num_channels]
    /// \param output_timestamps Output buffer for timestamps [num_samples]
    /// \param num_samples Number of samples to read (in/out - updated with actual read count)
    /// \param bSeek If true, advance read pointer; if false, just peek at data
    /// \return true if successful, false if group not allocated or invalid parameters
    [[nodiscard]] bool readSamples(uint32_t group_idx, int16_t* output_samples,
                                    PROCTIME* output_timestamps, uint32_t& num_samples,
                                    bool bSeek);

    /// Helper: Find channel index within a group
    /// \param group_idx Group index (0-7)
    /// \param channel_id Channel ID to find (1-based)
    /// \return Channel index within group (0-based), or -1 if not found
    [[nodiscard]] int32_t findChannelInGroup(const uint32_t group_idx, const uint16_t channel_id) const
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
        {
            std::lock_guard<std::mutex> lock(grp.m_mutex);
            grp.reset();
        }
    }

    /// Cleanup all groups (deallocates all memory)
    void cleanup()
    {
        for (auto& grp : groups)
        {
            std::lock_guard<std::mutex> lock(grp.m_mutex);
            grp.cleanup();
        }
    }
};

#endif // CONTINUOUSDATA_H_INCLUDED
