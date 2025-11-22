///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_400.cpp
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 4.0 wrapper implementation
///
/// Implements packet translation between protocol 4.0 and current (4.1+) formats.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "device_session_400.h"
#include "packet_translator.h"
#include <cstring>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Protocol 4.0 Packet Header Layout (16 bytes total)
///
/// Byte offset layout (from protocol_detector.cpp):
///   0-7:  time (64-bit)
///   8-9:  chid (16-bit)
///   10:   type (8-bit)   <-- Changed to 16-bit in 4.1+
///   11-12: dlen (16-bit)  <-- Different byte offset in 4.1+
///   13:   instrument (8-bit)  <-- Different byte offset in 4.1+
///   14-15: reserved (16-bit)  <-- Changed to 8-bit in 4.1+
///   Total size is the same.
///

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// Factory
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<DeviceSession_400> DeviceSession_400::create(const ConnectionParams& config) {
    // Create underlying device session for actual socket I/O
    auto result = DeviceSession::create(config);
    if (result.isError()) {
        return Result<DeviceSession_400>::error(result.error());
    }

    // Construct wrapper with the device session
    DeviceSession_400 session(std::move(result.value()));
    return Result<DeviceSession_400>::ok(std::move(session));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// IDeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<int> DeviceSession_400::receivePackets(void* buffer, const size_t buffer_size) {
    // Even though the header size is the same, protocol 4.0 had the old CHANINFO structure,
    // which was 3 bytes smaller than the new structure. Given that we typically receive many
    // CHANINFO packets in a row, we cannot reasonably write into `buffer` then 'readjust' the
    // buffer contents, as that would require a lot of moving memory around. Thus, we write into
    // a temporary buffer and then translate into the hopefully-large-enough `buffer`.
    uint8_t src_buffer[cbCER_UDP_SIZE_MAX];

    auto result = m_device.receivePacketsRaw(src_buffer, sizeof(src_buffer));
    if (result.isError()) {
        return result;
    }

    const int bytes_received = result.value();
    if (bytes_received == 0) {
        return Result<int>::ok(0);  // No data available
    }

    // Translate 4.0 format to current format
    auto* dest_buffer = static_cast<uint8_t*>(buffer);
    size_t dest_offset = 0;
    size_t src_offset = 0;
    while (src_offset < static_cast<size_t>(bytes_received)) {
        if (src_offset + HEADER_SIZE_400 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }

        // -- Header --
        auto src_header = *reinterpret_cast<const cbPKT_HEADER_400*>(&src_buffer[src_offset]);
        auto dest_header = *reinterpret_cast<cbPKT_HEADER*>(&dest_buffer[dest_offset]);
        //   When going from 4.0 to current, we fix the header as follows:
        //   1. Read reserved from bytes 15-16, truncate to 8-bit, write to byte 16.
        //   2. Read instrument from byte 14, write to byte 15.
        //   3. Read dlen from bytes 12-13, write to bytes 13-14.
        //   4. Read 8-bit `type` from byte 11, write 16-bit to bytes 11-12.
        //   First 10 bytes are unchanged.
        dest_header.reserved = static_cast<uint8_t>(src_header.reserved);
        dest_header.instrument = src_header.instrument;
        dest_header.dlen = src_header.dlen;
        dest_header.type = static_cast<uint16_t>(src_header.type);
        dest_header.chid = src_header.chid;
        dest_header.time = src_header.time;

        // -- Payload --
        if (src_offset + HEADER_SIZE_400 + src_header.dlen * 4 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }
        // Verify destination buffer has space. Need enough extra room for max difference in payload size.
        size_t pad_quads = 0;
        if (
            (dest_header.type == cbPKTTYPE_SYSPROTOCOLMONITOR)
            || ((dest_header.type & 0xF0) == cbPKTTYPE_CHANREP)
            || (dest_header.type == cbPKTTYPE_CHANRESETREP)){
            pad_quads = 1;
        }
        if ((dest_offset + cbPKT_HEADER_SIZE + (dest_header.dlen + pad_quads) * 4) > buffer_size) {
            return Result<int>::error("Output buffer too small for translated packets");
        }
        // Translate payload
        const size_t dest_dlen = PacketTranslator::translatePayload_400_to_current(
            &src_buffer[src_offset], &dest_buffer[dest_offset]);
        dest_header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        src_offset += HEADER_SIZE_400 + src_header.dlen * 4;
        dest_offset += cbPKT_HEADER_SIZE + dest_header.dlen * 4;
    }

    // Update configuration from translated packets
    if (dest_offset > 0) {
        m_device.updateConfigFromBuffer(dest_buffer, dest_offset);
    }

    return Result<int>::ok(static_cast<int>(dest_offset));
}

Result<void> DeviceSession_400::sendPacket(const cbPKT_GENERIC& pkt) {
    // Translate current format to 4.0 format
    uint8_t temp_buffer[cbPKT_MAX_SIZE];

    // -- Header --
    // Read current format header fields
    ///   When going from current to 4.0, we fix the header as follows:
    ///   1. Read 16-bit type from bytes 11-12, shift right 8 bits to get 8-bit type, set on byte 11.
    ///   2. Read dlen from bytes 13-14, write to bytes 12-13.
    ///   3. Read instrument from byte 15, write to byte 14.
    ///   4. Read reserved from byte 16, write to bytes 15-16 as 16-bit.
    auto dest_header = *reinterpret_cast<cbPKT_HEADER_400*>(temp_buffer);
    dest_header.time = pkt.cbpkt_header.time;  // TODO: What if we are using time ticks, not nanoseconds?
    dest_header.chid = pkt.cbpkt_header.chid;
    dest_header.type = static_cast<uint8_t>(pkt.cbpkt_header.type);
    dest_header.dlen = pkt.cbpkt_header.dlen;
    dest_header.instrument = pkt.cbpkt_header.instrument;
    dest_header.reserved = static_cast<uint16_t>(pkt.cbpkt_header.reserved);

    // -- Payload --
    auto* dest_payload = &temp_buffer[HEADER_SIZE_400];
    const size_t dest_dlen = PacketTranslator::translatePayload_current_to_400(pkt, dest_payload);
    dest_header.dlen = dest_dlen;
    const size_t packet_size_400 = HEADER_SIZE_400 + dest_header.dlen * 4;

    // Send raw bytes directly via the device's sendRaw method
    return m_device.sendRaw(temp_buffer, packet_size_400);
}

Result<void> DeviceSession_400::sendPackets(const cbPKT_GENERIC* pkts, const size_t count) {
    if (!pkts || count == 0) {
        return Result<void>::error("Invalid packet array");
    }

    // Send each packet individually
    for (size_t i = 0; i < count; ++i) {
        if (auto result = sendPacket(pkts[i]); result.isError()) {
            return result;
        }
    }

    return Result<void>::ok();
}

Result<void> DeviceSession_400::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

bool DeviceSession_400::isConnected() const {
    return m_device.isConnected();
}

const ConnectionParams& DeviceSession_400::getConnectionParams() const {
    return m_device.getConnectionParams();
}

Result<void> DeviceSession_400::setSystemRunLevel(const uint32_t runlevel, const uint32_t resetque, const uint32_t runflags) {
    // Delegate to wrapped device
    return m_device.setSystemRunLevel(runlevel, resetque, runflags);
}

Result<void> DeviceSession_400::requestConfiguration() {
    // Delegate to wrapped device
    return m_device.requestConfiguration();
}

ProtocolVersion DeviceSession_400::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_400;
}

} // namespace cbdev
