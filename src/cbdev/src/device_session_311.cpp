///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_311.cpp
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 3.11 wrapper implementation
///
/// Implements packet translation between protocol 3.11 and current (4.1+) formats.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include "device_session_311.h"
#include "packet_translator.h"
#include <cstring>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Protocol 3.11 Packet Header Layout (8 bytes total)
///
/// Byte offset layout (from protocol_detector.cpp):
///   0-3:  time (32-bit)
///   4-5:  chid (16-bit)
///   6:    type (8-bit)
///   7:    dlen (8-bit)
///
/// Current Protocol (4.1+) Header Layout (16 bytes total):
///   0-7:  time (64-bit)
///   8-9:  chid (16-bit)
///   10-11: type (16-bit)
///   12-13: dlen (16-bit)
///   14:   instrument (8-bit)
///   15:   reserved (8-bit)
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
// Factory
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<DeviceSession_311> DeviceSession_311::create(const ConnectionParams& config) {
    // Create underlying device session for actual socket I/O
    auto result = DeviceSession::create(config);
    if (result.isError()) {
        return Result<DeviceSession_311>::error(result.error());
    }

    // Construct wrapper with the device session
    DeviceSession_311 session(std::move(result.value()));
    return Result<DeviceSession_311>::ok(std::move(session));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// IDeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<int> DeviceSession_311::receivePackets(void* buffer, const size_t buffer_size) {
    // Temporary buffer for receiving 3.11 formatted packets
    uint8_t src_buffer[cbCER_UDP_SIZE_MAX];

    // Receive from underlying device (in 3.11 format)
    auto result = m_device.receivePacketsRaw(src_buffer, sizeof(src_buffer));
    if (result.isError()) {
        return result;
    }

    const int bytes_received = result.value();
    if (bytes_received == 0) {
        return Result<int>::ok(0);  // No data available
    }

    // Translate 3.11 format to current format
    auto* dest_buffer = static_cast<uint8_t*>(buffer);
    size_t dest_offset = 0;
    size_t src_offset = 0;
    while (src_offset < static_cast<size_t>(bytes_received)) {
        // -- Header --
        // Check if we have enough data for a 3.11 header
        if (src_offset + HEADER_SIZE_311 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }
        // And enough room for the reformatted header in the dest buffer
        if ((dest_offset + cbPKT_HEADER_SIZE) > buffer_size) {
            return Result<int>::error("Output buffer too small for packet header");
        }
        // Copy-convert the header data
        const auto src_header = *reinterpret_cast<const cbPKT_HEADER_311*>(&src_buffer[src_offset]);
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER *>(&dest_buffer[dest_offset]);
        // Read 3.11 header fields using byte offsets
        dest_header.time = static_cast<PROCTIME>(src_header.time) * 1000000000/30000;
        dest_header.chid = src_header.chid;
        dest_header.type = static_cast<uint16_t>(src_header.type);
        dest_header.dlen = static_cast<uint16_t>(src_header.dlen);

        // -- Payload --
        if (src_offset + HEADER_SIZE_311 + src_header.dlen * 4 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }
        // Verify destination buffer has space.
        // quadlet diff -- NPLAY: +4, COMMENT: +2, SYSPROTOCOLMONITOR: +1, CHANINFO: +0.75, CHANRESET: +0.25
        size_t pad_quads = 0;
        if (dest_header.type == cbPKTTYPE_NPLAYREP) {
            pad_quads = 4;
        } else if (dest_header.type == cbPKTTYPE_COMMENTREP) {
            pad_quads = 2;
        } else if (
            (dest_header.type == cbPKTTYPE_SYSPROTOCOLMONITOR)
            || ((dest_header.type & 0xF0) == cbPKTTYPE_CHANREP)
            || (dest_header.type == cbPKTTYPE_CHANRESETREP)){
            pad_quads = 1;
        }
        if ((dest_offset + cbPKT_HEADER_SIZE + (dest_header.dlen + pad_quads) * 4) > buffer_size) {
            return Result<int>::error("Output buffer too small for translated packets");
        }
        // Translate payload
        const size_t dest_dlen = PacketTranslator::translatePayload_311_to_current(
            &src_buffer[src_offset], &dest_buffer[dest_offset]);
        dest_header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        src_offset += HEADER_SIZE_311 + src_header.dlen * 4;
        dest_offset += cbPKT_HEADER_SIZE + dest_header.dlen * 4;
    }

    // Update configuration from translated packets
    if (dest_offset > 0) {
        m_device.updateConfigFromBuffer(dest_buffer, dest_offset);
    }

    return Result<int>::ok(static_cast<int>(dest_offset));
}

Result<void> DeviceSession_311::sendPacket(const cbPKT_GENERIC& pkt) {
    // Translate current format to 3.11 format
    uint8_t dest[cbPKT_MAX_SIZE];

    if (pkt.cbpkt_header.type > 0xFF) {
        return Result<void>::error("Packet type too large for protocol 3.11 (max 255)");
    }
    if (pkt.cbpkt_header.dlen > 0xFF) {
        return Result<void>::error("Packet dlen too large for protocol 3.11 (max 255)");
    }

    // -- Header --
    auto& dest_header = *reinterpret_cast<cbPKT_HEADER_311*>(&dest[0]);
    dest_header.time = static_cast<uint32_t>(pkt.cbpkt_header.time * 30000 / 1000000000);
    dest_header.chid = pkt.cbpkt_header.chid;
    dest_header.type = static_cast<uint8_t>(pkt.cbpkt_header.type);  // Narrowing!
    dest_header.dlen = static_cast<uint8_t>(pkt.cbpkt_header.dlen);  // Narrowing!

    // -- Payload --
    const size_t dest_dlen = PacketTranslator::translatePayload_current_to_311(pkt, dest);
    dest_header.dlen = dest_dlen;

    // Send raw bytes directly via the device's sendRaw method
    return m_device.sendRaw(dest, HEADER_SIZE_311 + dest_header.dlen * 4);
}

Result<void> DeviceSession_311::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

ProtocolVersion DeviceSession_311::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_311;
}

} // namespace cbdev
