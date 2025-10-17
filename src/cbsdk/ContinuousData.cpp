#include "ContinuousData.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <mutex>

GroupContinuousData::GroupContinuousData() :
    m_size(0), m_write_index(0), m_write_start_index(0),
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

bool GroupContinuousData::allocate(uint32_t buffer_size, uint32_t n_chans, const uint16_t* chan_ids)
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

    // Update channel list
    memcpy(m_channel_ids, chan_ids, n_chans * sizeof(uint16_t));

    return true;
}

bool GroupContinuousData::writeSample(PROCTIME timestamp, const int16_t* data, uint32_t n_chans)
{
    if (!m_channel_data || n_chans != m_num_channels)
        return false;  // Not allocated or channel count mismatch

    // Store timestamp for this sample
    m_timestamps[m_write_index] = timestamp;

    // DEBUG: Log write operations
    static PROCTIME last_written_ts = 0;
    static uint32_t write_log_count = 0;

    if (timestamp != 0) {
        // Log first 200 writes
        if (write_log_count < 200) {
            fprintf(stderr, "[writeSample #%u] write_index=%u, ts=%llu, data[0]=%d, n_chans=%u\n",
                    write_log_count, m_write_index, (unsigned long long)timestamp, data[0], n_chans);
            write_log_count++;
        }

        // Always check for duplicates and jumps
        if (last_written_ts != 0 && timestamp == last_written_ts) {
            fprintf(stderr, "[WRITE] DUPLICATE! write_index=%u, timestamp=%llu, data[0]=%d (prev_ts=%llu)\n",
                    m_write_index, (unsigned long long)timestamp, data[0], (unsigned long long)last_written_ts);
        } else if (last_written_ts != 0 && timestamp != last_written_ts + 1) {
            fprintf(stderr, "[WRITE] JUMP! write_index=%u, timestamp=%llu, delta=%lld, data[0]=%d\n",
                    m_write_index, (unsigned long long)timestamp,
                    (long long)(timestamp - last_written_ts), data[0]);
        }
        last_written_ts = timestamp;
    }

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

// ContinuousData methods implementation

bool ContinuousData::writeSampleThreadSafe(uint32_t group_idx, PROCTIME timestamp,
                                            const int16_t* data, uint32_t n_chans,
                                            const uint16_t* chan_ids)
{
    if (group_idx >= cbMAXGROUPS)
        return false;

    auto& grp = groups[group_idx];
    std::lock_guard<std::mutex> lock(grp.m_mutex);

    // Check if we need to allocate or reallocate
    if (grp.needsReallocation(chan_ids, n_chans))
    {
        // Use existing size if already allocated, otherwise use default
        const uint32_t buffer_size = grp.getSize() ? grp.getSize() : default_size;
        if (!grp.allocate(buffer_size, n_chans, chan_ids))
        {
            return false;  // Allocation failed
        }
    }

    // Write sample to ring buffer (returns true if overflow occurred)
    return grp.writeSample(timestamp, data, n_chans);
}

bool ContinuousData::snapshotForReading(uint32_t group_idx, GroupSnapshot& snapshot)
{
    if (group_idx >= cbMAXGROUPS)
        return false;

    auto& grp = groups[group_idx];
    std::lock_guard<std::mutex> lock(grp.m_mutex);

    snapshot.is_allocated = grp.isAllocated();
    if (!snapshot.is_allocated)
    {
        snapshot.num_samples = 0;
        snapshot.num_channels = 0;
        snapshot.buffer_size = 0;
        snapshot.read_start_index = 0;
        snapshot.read_end_index = 0;
        return true;  // Success, but no data
    }

    // Take snapshot of current state
    snapshot.read_end_index = grp.getWriteIndex();
    snapshot.read_start_index = grp.getWriteStartIndex();
    snapshot.num_channels = grp.getNumChannels();
    snapshot.buffer_size = grp.getSize();

    // Calculate available samples
    auto num_avail = static_cast<int32_t>(snapshot.read_end_index - snapshot.read_start_index);
    if (num_avail < 0)
        num_avail += static_cast<int32_t>(snapshot.buffer_size);  // Wrapped around

    snapshot.num_samples = static_cast<uint32_t>(num_avail);

    // Update the read_end_index in the group (this is the snapshot point)
    grp.setReadEndIndex(snapshot.read_end_index);

    return true;
}

bool ContinuousData::readSamples(uint32_t group_idx, int16_t* output_samples,
                                  PROCTIME* output_timestamps, uint32_t& num_samples,
                                  bool bSeek)
{
    if (group_idx >= cbMAXGROUPS)
        return false;

    if (!output_samples || !output_timestamps)
        return false;

    auto& grp = groups[group_idx];
    std::lock_guard<std::mutex> lock(grp.m_mutex);

    if (!grp.isAllocated())
    {
        num_samples = 0;
        return true;  // Success, but no data
    }

    // Get current read pointers
    const uint32_t read_start_index = grp.getWriteStartIndex();
    const uint32_t read_end_index = grp.getWriteIndex();

    // Calculate available samples
    auto num_avail = static_cast<int32_t>(read_end_index - read_start_index);
    if (num_avail < 0)
        num_avail += static_cast<int32_t>(grp.getSize());  // Wrapped around

    // Don't read more than requested or available
    num_samples = std::min(static_cast<uint32_t>(num_avail), num_samples);

    if (num_samples == 0)
        return true;  // Success, but no data to read

    // Get pointers to internal data
    const int16_t* channel_data = grp.getChannelData();
    const PROCTIME* timestamps = grp.getTimestamps();
    const uint32_t num_channels = grp.getNumChannels();
    const uint32_t buffer_size = grp.getSize();

    // Check if we wrap around the ring buffer
    const bool wraps = (read_start_index + num_samples) > buffer_size;

    if (!wraps)
    {
        // No wraparound - copy everything in bulk
        memcpy(output_timestamps,
               &timestamps[read_start_index],
               num_samples * sizeof(PROCTIME));

        memcpy(output_samples,
               &channel_data[read_start_index * num_channels],
               num_samples * num_channels * sizeof(int16_t));
    }
    else
    {
        // Wraparound case - copy in two chunks
        const uint32_t first_chunk_size = buffer_size - read_start_index;
        const uint32_t second_chunk_size = num_samples - first_chunk_size;

        // First chunk of timestamps
        memcpy(output_timestamps,
               &timestamps[read_start_index],
               first_chunk_size * sizeof(PROCTIME));

        // Second chunk of timestamps
        memcpy(&output_timestamps[first_chunk_size],
               timestamps,
               second_chunk_size * sizeof(PROCTIME));

        // First chunk of sample data
        memcpy(output_samples,
               &channel_data[read_start_index * num_channels],
               first_chunk_size * num_channels * sizeof(int16_t));

        // Second chunk of sample data
        memcpy(&output_samples[first_chunk_size * num_channels],
               channel_data,
               second_chunk_size * num_channels * sizeof(int16_t));
    }

    // DEBUG: Always check for duplicate timestamps in reads
    static uint32_t read_call_count = 0;
    if (num_samples > 0) {
        // Check ALL samples in this read for duplicates
        for (uint32_t i = 1; i < num_samples; i++) {
            if (output_timestamps[i] == output_timestamps[i-1]) {
                fprintf(stderr, "[READ] DUPLICATE in call #%u at index %u/%u: ts=%llu (data[%u]=%d, data[%u]=%d)\n",
                        read_call_count, i, num_samples, (unsigned long long)output_timestamps[i],
                        (i-1)*num_channels, static_cast<int16_t*>(output_samples)[(i-1)*num_channels],
                        i*num_channels, static_cast<int16_t*>(output_samples)[i*num_channels]);
            }
        }
        read_call_count++;
    }

    // Update write_start_index if consuming data (bSeek)
    if (bSeek)
    {
        const uint32_t new_start = (read_start_index + num_samples) % grp.getSize();
        grp.setWriteStartIndex(new_start);
    }

    return true;
}