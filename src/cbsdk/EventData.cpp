#include "EventData.h"
#include <algorithm>
#include <cstring>

EventData::EventData() :
    m_size(0), m_waveform_data(nullptr)
{
    // Initialize all pointers to null
    for (uint32_t i = 0; i < cbMAXCHANS; ++i)
    {
        m_timestamps[i] = nullptr;
        m_units[i] = nullptr;
        m_write_index[i] = 0;
        m_write_start_index[i] = 0;
    }
}

EventData::~EventData()
{
    cleanup();
}

bool EventData::allocate(const uint32_t buffer_size)
{
    if (m_size == buffer_size && m_timestamps[0] != nullptr)
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
        // Allocate per-channel arrays
        // Note: We allocate for all cbMAXCHANS to allow direct indexing by channel number
        for (uint32_t i = 0; i < cbMAXCHANS; ++i)
        {
            m_timestamps[i] = new PROCTIME[buffer_size];
            std::fill_n(m_timestamps[i], buffer_size, static_cast<PROCTIME>(0));

            m_units[i] = new uint16_t[buffer_size];
            std::fill_n(m_units[i], buffer_size, static_cast<uint16_t>(0));

            m_write_index[i] = 0;
            m_write_start_index[i] = 0;
        }

        m_size = buffer_size;
        m_waveform_data = nullptr;  // Managed externally

        return true;

    } catch (...) {
        // Allocation failed - cleanup partial allocation
        for (uint32_t i = 0; i < cbMAXCHANS; ++i)
        {
            if (m_timestamps[i])
            {
                delete[] m_timestamps[i];
                m_timestamps[i] = nullptr;
            }
            if (m_units[i])
            {
                delete[] m_units[i];
                m_units[i] = nullptr;
            }
        }
        m_size = 0;
        return false;
    }
}

bool EventData::writeEvent(const uint16_t channel, const PROCTIME timestamp, const uint16_t unit)
{
    if (channel == 0 || channel > cbMAXCHANS)
        return false;  // Invalid channel

    const uint32_t ch_idx = channel - 1;  // Convert to 0-based index

    if (!m_timestamps[ch_idx])
        return false;  // Not allocated

    // Store timestamp and unit
    m_timestamps[ch_idx][m_write_index[ch_idx]] = timestamp;
    m_units[ch_idx][m_write_index[ch_idx]] = unit;

    // Advance write index (circular buffer)
    const uint32_t next_write_index = (m_write_index[ch_idx] + 1) % m_size;

    // Check for buffer overflow
    bool overflow = false;
    if (next_write_index == m_write_start_index[ch_idx])
    {
        // Buffer is full - overwrite oldest data
        overflow = true;
        m_write_start_index[ch_idx] = (m_write_start_index[ch_idx] + 1) % m_size;
    }

    m_write_index[ch_idx] = next_write_index;
    return overflow;
}

void EventData::reset()
{
    if (m_size)
    {
        for (uint32_t i = 0; i < cbMAXCHANS; ++i)
        {
            if (m_timestamps[i])
                std::fill_n(m_timestamps[i], m_size, static_cast<PROCTIME>(0));

            if (m_units[i])
                std::fill_n(m_units[i], m_size, static_cast<uint16_t>(0));

            m_write_index[i] = 0;
            m_write_start_index[i] = 0;
        }
    }

    m_waveform_data = nullptr;  // Managed externally, don't delete
}

void EventData::cleanup()
{
    for (uint32_t i = 0; i < cbMAXCHANS; ++i)
    {
        if (m_timestamps[i])
        {
            delete[] m_timestamps[i];
            m_timestamps[i] = nullptr;
        }

        if (m_units[i])
        {
            delete[] m_units[i];
            m_units[i] = nullptr;
        }

        m_write_index[i] = 0;
        m_write_start_index[i] = 0;
    }

    m_waveform_data = nullptr;  // Managed externally, don't delete
    m_size = 0;
}

const PROCTIME* EventData::getTimestamps(const uint16_t channel) const
{
    if (channel == 0 || channel > cbMAXCHANS)
        return nullptr;

    return m_timestamps[channel - 1];
}

const uint16_t* EventData::getUnits(const uint16_t channel) const
{
    if (channel == 0 || channel > cbMAXCHANS)
        return nullptr;

    return m_units[channel - 1];
}

uint32_t EventData::getWriteIndex(const uint16_t channel) const
{
    if (channel == 0 || channel > cbMAXCHANS)
        return 0;

    return m_write_index[channel - 1];
}

uint32_t EventData::getWriteStartIndex(const uint16_t channel) const
{
    if (channel == 0 || channel > cbMAXCHANS)
        return 0;

    return m_write_start_index[channel - 1];
}

void EventData::setWriteStartIndex(const uint16_t channel, const uint32_t index)
{
    if (channel > 0 && channel <= cbMAXCHANS)
        m_write_start_index[channel - 1] = index;
}

void EventData::setWriteIndex(const uint16_t channel, const uint32_t index)
{
    if (channel > 0 && channel <= cbMAXCHANS)
        m_write_index[channel - 1] = index;
}
