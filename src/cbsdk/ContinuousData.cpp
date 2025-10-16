#include "ContinuousData.h"
#include <algorithm>
#include <cstring>

GroupContinuousData::GroupContinuousData() :
    m_size(0), m_sample_rate(0), m_write_index(0), m_write_start_index(0),
    m_read_end_index(0), m_num_channels(0), m_capacity(0),
    m_channel_ids(nullptr), m_timestamps(nullptr), m_channel_data(nullptr)
{
}

GroupContinuousData::~GroupContinuousData()
{
    cleanup();
}

bool GroupContinuousData::needsReallocation(const uint16_t* chan_ids, uint32_t n_chans) const
{
    if (m_channel_data == nullptr)
        return true;  // First packet for this group

    if (m_num_channels != n_chans)
        return true;  // Channel count changed

    // Same count, but check if actual channels changed
    for (uint32_t i = 0; i < n_chans; ++i)
    {
        if (m_channel_ids[i] != chan_ids[i])
            return true;
    }

    return false;  // No change needed
}

bool GroupContinuousData::allocate(uint32_t buffer_size, uint32_t n_chans, const uint16_t* chan_ids, uint16_t rate)
{
    // Determine if we need more capacity
    const uint32_t needed_capacity = n_chans + 4;  // Add headroom to reduce reallocations

    if (m_capacity < n_chans)
    {
        // Need to expand capacity - clean up old allocation
        if (m_channel_data)
        {
            for (uint32_t i = 0; i < m_size; ++i)
            {
                if (m_channel_data[i])
                    delete[] m_channel_data[i];
            }
            delete[] m_channel_data;
            delete[] m_channel_ids;
        }

        // Allocate new with headroom
        try {
            // Allocate [size][capacity] layout for memcpy-friendly writes
            m_channel_data = new int16_t*[buffer_size];
            for (uint32_t i = 0; i < buffer_size; ++i)
            {
                m_channel_data[i] = new int16_t[needed_capacity];
                std::fill_n(m_channel_data[i], needed_capacity, 0);
            }

            m_channel_ids = new uint16_t[needed_capacity];
            m_capacity = needed_capacity;
            m_size = buffer_size;

            // Allocate timestamps if not already allocated
            if (!m_timestamps)
            {
                m_timestamps = new PROCTIME[buffer_size];
                std::fill_n(m_timestamps, buffer_size, 0);
            }
        } catch (...) {
            // Allocation failed - cleanup partial allocation
            if (m_channel_data)
            {
                for (uint32_t i = 0; i < buffer_size; ++i)
                {
                    if (m_channel_data[i])
                        delete[] m_channel_data[i];
                }
                delete[] m_channel_data;
                m_channel_data = nullptr;
            }
            if (m_channel_ids)
            {
                delete[] m_channel_ids;
                m_channel_ids = nullptr;
            }
            m_capacity = 0;
            return false;
        }
    }

    // Update channel list
    m_num_channels = n_chans;
    memcpy(m_channel_ids, chan_ids, n_chans * sizeof(uint16_t));

    // Reset indices on reallocation (lose any buffered data)
    m_write_index = 0;
    m_write_start_index = 0;
    m_sample_rate = rate;

    return true;
}

bool GroupContinuousData::writeSample(PROCTIME timestamp, const int16_t* data, uint32_t n_chans)
{
    if (!m_channel_data || n_chans != m_num_channels)
        return false;  // Not allocated or channel count mismatch

    // Store timestamp for this sample
    m_timestamps[m_write_index] = timestamp;

    // Store data for all channels in one memcpy
    // Layout is [samples][channels], so channel_data[write_index] is a pointer to
    // an array of channels for this sample, which perfectly matches packet data layout
    memcpy(m_channel_data[m_write_index], data, n_chans * sizeof(int16_t));

    // Advance write index (circular buffer)
    const uint32_t next_write_index = (m_write_index + 1) % m_size;

    // Check for buffer overflow
    bool overflow = false;
    if (next_write_index == m_write_start_index)
    {
        // Buffer is full - overwrite oldest data
        overflow = true;
        m_write_start_index = (m_write_start_index + 1) % m_size;
    }

    m_write_index = next_write_index;
    return overflow;
}

void GroupContinuousData::reset()
{
    if (m_size && m_timestamps)
        std::fill_n(m_timestamps, m_size, static_cast<PROCTIME>(0));

    if (m_channel_data)
    {
        // Iterate over samples (outer dimension)
        for (uint32_t i = 0; i < m_size; ++i)
        {
            if (m_channel_data[i])
                std::fill_n(m_channel_data[i], m_capacity, static_cast<int16_t>(0));
        }
    }

    m_write_index = 0;
    m_write_start_index = 0;
    m_read_end_index = 0;
}

void GroupContinuousData::cleanup()
{
    if (m_timestamps)
    {
        delete[] m_timestamps;
        m_timestamps = nullptr;
    }

    if (m_channel_data)
    {
        // Iterate over samples (outer dimension)
        for (uint32_t i = 0; i < m_size; ++i)
        {
            if (m_channel_data[i])
            {
                delete[] m_channel_data[i];
                m_channel_data[i] = nullptr;
            }
        }
        delete[] m_channel_data;
        m_channel_data = nullptr;
    }

    if (m_channel_ids)
    {
        delete[] m_channel_ids;
        m_channel_ids = nullptr;
    }

    m_num_channels = 0;
    m_capacity = 0;
}