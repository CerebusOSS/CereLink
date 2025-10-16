#include "ContinuousData.h"
#include <algorithm>
#include <cstring>

GroupContinuousData::GroupContinuousData() :
    m_size(0), m_sample_rate(0), m_write_index(0), m_write_start_index(0),
    m_read_end_index(0), m_num_channels(0),
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
    if (m_num_channels != n_chans || m_size != buffer_size)
    {
        // Channel count or buffer size changed - need to reallocate
        // Clean up old allocation
        if (m_channel_data)
        {
            delete[] m_channel_data;
            m_channel_data = nullptr;
        }
        if (m_channel_ids)
        {
            delete[] m_channel_ids;
            m_channel_ids = nullptr;
        }
        if (m_timestamps)
        {
            delete[] m_timestamps;
            m_timestamps = nullptr;
        }

        // Allocate new arrays with exact size needed
        try {
            // Single contiguous allocation: [buffer_size * n_chans]
            m_channel_data = new int16_t[buffer_size * n_chans];
            std::fill_n(m_channel_data, buffer_size * n_chans, static_cast<int16_t>(0));

            m_channel_ids = new uint16_t[n_chans];

            m_timestamps = new PROCTIME[buffer_size];
            std::fill_n(m_timestamps, buffer_size, static_cast<PROCTIME>(0));

            m_num_channels = n_chans;
            m_size = buffer_size;
        } catch (...) {
            // Allocation failed - cleanup partial allocation
            if (m_channel_data)
            {
                delete[] m_channel_data;
                m_channel_data = nullptr;
            }
            if (m_channel_ids)
            {
                delete[] m_channel_ids;
                m_channel_ids = nullptr;
            }
            if (m_timestamps)
            {
                delete[] m_timestamps;
                m_timestamps = nullptr;
            }
            m_num_channels = 0;
            m_size = 0;
            return false;
        }

        // Reset indices on reallocation (lose any buffered data)
        m_write_index = 0;
        m_write_start_index = 0;
    }

    // Update channel list and sample rate
    memcpy(m_channel_ids, chan_ids, n_chans * sizeof(uint16_t));
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
    // Contiguous layout: data for sample at write_index starts at [write_index * num_channels]
    memcpy(&m_channel_data[m_write_index * m_num_channels], data, n_chans * sizeof(int16_t));

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

    if (m_channel_data && m_size && m_num_channels)
    {
        // Fill entire contiguous block
        std::fill_n(m_channel_data, m_size * m_num_channels, static_cast<int16_t>(0));
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
        delete[] m_channel_data;
        m_channel_data = nullptr;
    }

    if (m_channel_ids)
    {
        delete[] m_channel_ids;
        m_channel_ids = nullptr;
    }

    m_num_channels = 0;
    m_size = 0;
}