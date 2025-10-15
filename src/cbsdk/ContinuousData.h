//////////////////////////////////////////////////////////////////////
/**
* \file ContinuousData.h
* \brief Continuous data structures for per-group allocation (internal use)
*
* This header contains the refactored continuous data structures that use
* per-group allocation instead of per-channel allocation. This reduces memory
* usage and improves cache locality.
*
* These structures are internal to the SDK implementation and are not part of
* the public API.
*/

#ifndef CONTINUOUSDATA_H_INCLUDED
#define CONTINUOUSDATA_H_INCLUDED

#include "../../include/cerelink/cbhwlib.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

/// Structure to store continuous data for a single sample group
struct GroupContinuousData
{
    // Buffer configuration
    uint32_t size;                    ///< Buffer size for this group (samples)
    uint16_t sample_rate;             ///< Sample rate for this group (Hz)

    // Ring buffer management
    uint32_t write_index;             ///< Next write position in ring buffer
    uint32_t write_start_index;       ///< Where reading starts (oldest unread sample)
    uint32_t read_end_index;          ///< Last safe read position (snapshot for readers)

    // Dynamic channel management
    uint32_t num_channels;            ///< Number of channels actually in this group
    uint32_t capacity;                ///< Allocated capacity (may exceed num_channels for headroom)
    uint16_t* channel_ids;            ///< Array of channel IDs (1-based, size = capacity)

    // Data storage (contiguous, cache-friendly layout)
    PROCTIME* timestamps;             ///< [size] - timestamp for each sample
    int16_t** channel_data;           ///< [capacity][size] - data for each channel

    /// Constructor - initializes all fields to safe defaults
    GroupContinuousData() :
        size(0), sample_rate(0), write_index(0), write_start_index(0),
        read_end_index(0), num_channels(0), capacity(0),
        channel_ids(nullptr), timestamps(nullptr), channel_data(nullptr) {}

    /// Reset ring buffer indices and zero data (preserves allocation)
    void reset()
    {
        if (size && timestamps)
            std::fill_n(timestamps, size, static_cast<PROCTIME>(0));

        if (channel_data)
        {
            for (uint32_t i = 0; i < num_channels; ++i)
            {
                if (channel_data[i])
                    std::fill_n(channel_data[i], size, static_cast<int16_t>(0));
            }
        }

        write_index = 0;
        write_start_index = 0;
        read_end_index = 0;
    }

    /// Cleanup - deallocates all memory and resets to constructor state
    void cleanup()
    {
        if (timestamps)
        {
            delete[] timestamps;
            timestamps = nullptr;
        }

        if (channel_data)
        {
            for (uint32_t i = 0; i < capacity; ++i)
            {
                if (channel_data[i])
                {
                    delete[] channel_data[i];
                    channel_data[i] = nullptr;
                }
            }
            delete[] channel_data;
            channel_data = nullptr;
        }

        if (channel_ids)
        {
            delete[] channel_ids;
            channel_ids = nullptr;
        }

        num_channels = 0;
        capacity = 0;
    }
};

/// Structure to store all continuous data organized by sample groups
struct ContinuousData
{
    uint32_t default_size;                      ///< Default buffer size (cbSdk_CONTINUOUS_DATA_SAMPLES)
    GroupContinuousData groups[cbMAXGROUPS];    ///< One struct per group (0-7)

    /// Helper: Find channel index within a group
    /// \param group_idx Group index (0-7)
    /// \param channel_id Channel ID to find (1-based)
    /// \return Channel index within group (0-based), or -1 if not found
    int32_t findChannelInGroup(uint32_t group_idx, uint16_t channel_id) const
    {
        if (group_idx >= cbMAXGROUPS)
            return -1;

        const auto& grp = groups[group_idx];
        for (uint32_t i = 0; i < grp.num_channels; ++i)
        {
            if (grp.channel_ids[i] == channel_id)
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
