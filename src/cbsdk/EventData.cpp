#include "EventData.h"
#include <algorithm>
#include <cstring>

EventData::EventData() :
    m_size(0),
    m_timestamps(nullptr),
    m_channels(nullptr),
    m_units(nullptr),
    m_waveform_data(nullptr),
    m_write_index(0),
    m_write_start_index(0)
{
}

EventData::~EventData()
{
    cleanup();
}

bool EventData::allocate(uint32_t buffer_size)
{
    if (m_size == buffer_size && m_timestamps != nullptr)
    {
        // Already allocated with same size - just reset
        reset();
        return true;
    }

    // Clean up old allocation if size changed
    if (m_size != buffer_size)
    {
        cleanup();
    }

    try {
        // Allocate flat arrays
        m_timestamps = new PROCTIME[buffer_size];
        std::fill_n(m_timestamps, buffer_size, static_cast<PROCTIME>(0));

        m_channels = new uint16_t[buffer_size];
        std::fill_n(m_channels, buffer_size, static_cast<uint16_t>(0));

        m_units = new uint16_t[buffer_size];
        std::fill_n(m_units, buffer_size, static_cast<uint16_t>(0));

        m_size = buffer_size;
        m_write_index = 0;
        m_write_start_index = 0;
        m_waveform_data = nullptr;  // Managed externally

        return true;

    } catch (...) {
        // Allocation failed - cleanup partial allocation
        if (m_timestamps)
        {
            delete[] m_timestamps;
            m_timestamps = nullptr;
        }
        if (m_channels)
        {
            delete[] m_channels;
            m_channels = nullptr;
        }
        if (m_units)
        {
            delete[] m_units;
            m_units = nullptr;
        }
        m_size = 0;
        return false;
    }
}

bool EventData::writeEvent(uint16_t channel, PROCTIME timestamp, uint16_t unit)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return false;  // Invalid channel

    if (!m_timestamps)
        return false;  // Not allocated

    // Store event data
    m_timestamps[m_write_index] = timestamp;
    m_channels[m_write_index] = channel;
    m_units[m_write_index] = unit;

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

void EventData::reset()
{
    if (m_size)
    {
        if (m_timestamps)
            std::fill_n(m_timestamps, m_size, static_cast<PROCTIME>(0));

        if (m_channels)
            std::fill_n(m_channels, m_size, static_cast<uint16_t>(0));

        if (m_units)
            std::fill_n(m_units, m_size, static_cast<uint16_t>(0));

        m_write_index = 0;
        m_write_start_index = 0;
    }

    m_waveform_data = nullptr;  // Managed externally, don't delete
}

void EventData::cleanup()
{
    if (m_timestamps)
    {
        delete[] m_timestamps;
        m_timestamps = nullptr;
    }

    if (m_channels)
    {
        delete[] m_channels;
        m_channels = nullptr;
    }

    if (m_units)
    {
        delete[] m_units;
        m_units = nullptr;
    }

    m_waveform_data = nullptr;  // Managed externally, don't delete
    m_size = 0;
    m_write_index = 0;
    m_write_start_index = 0;
}

uint32_t EventData::getNumEvents() const
{
    if (!m_timestamps)
        return 0;

    // Calculate number of events in ring buffer
    int32_t num_events = m_write_index - m_write_start_index;
    if (num_events < 0)
        num_events += m_size;

    return static_cast<uint32_t>(num_events);
}

void EventData::setWriteStartIndex(uint32_t index)
{
    m_write_start_index = index;
}

void EventData::setWriteIndex(uint32_t index)
{
    m_write_index = index;
}

uint32_t EventData::readEvents(PROCTIME* output_timestamps,
                               uint16_t* output_channels,
                               uint16_t* output_units,
                               uint32_t max_events,
                               bool bSeek)
{
    if (!m_timestamps)
        return 0;  // Not allocated

    const uint32_t available = getNumEvents();
    if (available == 0)
        return 0;

    const uint32_t num_to_read = std::min(available, max_events);

    // Read from ring buffer
    uint32_t read_index = m_write_start_index;
    for (uint32_t i = 0; i < num_to_read; ++i)
    {
        if (output_timestamps)
            output_timestamps[i] = m_timestamps[read_index];
        if (output_channels)
            output_channels[i] = m_channels[read_index];
        if (output_units)
            output_units[i] = m_units[read_index];

        // Advance read index (ring buffer)
        read_index++;
        if (read_index >= m_size)
            read_index = 0;
    }

    // Update read position if seeking
    if (bSeek)
    {
        m_write_start_index = read_index;
    }

    return num_to_read;
}
