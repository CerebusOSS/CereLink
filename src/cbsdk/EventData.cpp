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

uint32_t EventData::getAvailableSamples(uint16_t channel) const
{
    if (channel == 0 || channel > cbMAXCHANS)
        return 0;

    const uint32_t ch_idx = channel - 1;

    if (!m_timestamps[ch_idx])
        return 0;  // Not allocated

    const uint32_t write_idx = m_write_index[ch_idx];
    const uint32_t start_idx = m_write_start_index[ch_idx];

    // Calculate available samples accounting for ring buffer
    int32_t num_samples = write_idx - start_idx;
    if (num_samples < 0)
        num_samples += m_size;

    return static_cast<uint32_t>(num_samples);
}

void EventData::countSamplesPerUnit(uint16_t channel,
                                    uint32_t num_samples_per_unit[cbMAXUNITS + 1],
                                    bool is_digital_or_serial) const
{
    // Initialize output
    std::fill_n(num_samples_per_unit, cbMAXUNITS + 1, 0u);

    if (channel == 0 || channel > cbMAXCHANS)
        return;  // Invalid channel

    const uint32_t ch_idx = channel - 1;

    if (!m_timestamps[ch_idx])
        return;  // Not allocated

    const uint32_t available = getAvailableSamples(channel);
    if (available == 0)
        return;

    // Count samples per unit
    uint32_t read_index = m_write_start_index[ch_idx];
    const uint32_t write_idx = m_write_index[ch_idx];

    while (read_index != write_idx)
    {
        uint16_t unit = m_units[ch_idx][read_index];

        // For digital/serial, all events count as unit 0
        if (is_digital_or_serial || unit > cbMAXUNITS)
            unit = 0;

        num_samples_per_unit[unit]++;

        // Advance (ring buffer)
        read_index++;
        if (read_index >= m_size)
            read_index = 0;
    }
}

void EventData::readChannelEvents(uint16_t channel,
                                  const uint32_t max_samples_per_unit[cbMAXUNITS + 1],
                                  PROCTIME* timestamps[cbMAXUNITS + 1],
                                  uint16_t* digital_data,
                                  uint32_t num_samples_per_unit[cbMAXUNITS + 1],
                                  bool is_digital_or_serial,
                                  bool bSeek,
                                  uint32_t& final_read_index)
{
    // Initialize output
    std::fill_n(num_samples_per_unit, cbMAXUNITS + 1, 0u);

    if (channel == 0 || channel > cbMAXCHANS)
        return;  // Invalid channel

    const uint32_t ch_idx = channel - 1;

    if (!m_timestamps[ch_idx])
        return;  // Not allocated

    // Get available samples
    const uint32_t available = getAvailableSamples(channel);
    if (available == 0)
    {
        final_read_index = m_write_start_index[ch_idx];
        return;
    }

    // Calculate total max samples we can read
    uint32_t total_max = 0;
    for (int unit_ix = 0; unit_ix < (cbMAXUNITS + 1); ++unit_ix)
        total_max += max_samples_per_unit[unit_ix];

    const uint32_t num_samples = std::min(available, total_max);

    // Read samples from ring buffer
    uint32_t read_index = m_write_start_index[ch_idx];

    for (uint32_t sample_ix = 0; sample_ix < num_samples; ++sample_ix)
    {
        uint16_t unit = m_units[ch_idx][read_index];

        // For digital or serial data, 'unit' holds data, not unit number
        if (is_digital_or_serial)
        {
            // Copy digital data if requested
            if (digital_data && num_samples_per_unit[0] < max_samples_per_unit[0])
            {
                digital_data[num_samples_per_unit[0]] = unit;
            }
            unit = 0;  // Treat all digital/serial events as unit 0
        }

        // Copy timestamp if unit is valid and there's space
        if (unit <= cbMAXUNITS && num_samples_per_unit[unit] < max_samples_per_unit[unit])
        {
            // Copy timestamp if output pointer is provided
            if (timestamps[unit])
            {
                timestamps[unit][num_samples_per_unit[unit]] = m_timestamps[ch_idx][read_index];
            }
            num_samples_per_unit[unit]++;

            // Advance read index (ring buffer)
            read_index++;
            if (read_index >= m_size)
                read_index = 0;
        }
        else
        {
            // No space for this unit, stop reading
            break;
        }
    }

    final_read_index = read_index;

    // Update read position if seeking
    if (bSeek)
    {
        m_write_start_index[ch_idx] = read_index;
    }
}
