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
#include <cbproto/packet_translator.h>
#include <cstring>

namespace cbdev {

using cbproto::PacketTranslator;
using cbproto::cbPKT_HEADER_311;
using cbproto::HEADER_SIZE_311;

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

Result<size_t> DeviceSession_311::translateDatagram(const uint8_t* src, const size_t src_bytes,
                                                    uint8_t* dest, const size_t dest_cap) {
    // Translate 3.11 format to current format.  Headers grow 8 → 16 bytes, so
    // the funnel hands us a separate scratch buffer as `src`.
    size_t dest_offset = 0;
    size_t src_offset = 0;
    while (src_offset < src_bytes) {
        // -- Header --
        // Check if we have enough data for a 3.11 header
        if (src_offset + HEADER_SIZE_311 > src_bytes) {
            break;  // Incomplete packet
        }
        // And enough room for the reformatted header in the dest buffer
        if ((dest_offset + cbPKT_HEADER_SIZE) > dest_cap) {
            return Result<size_t>::error("Output buffer too small for packet header");
        }
        // Copy-convert the header data
        const auto src_header = *reinterpret_cast<const cbPKT_HEADER_311*>(&src[src_offset]);
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER *>(&dest[dest_offset]);
        // Read 3.11 header fields using byte offsets.  The 32-bit time is a
        // raw 30 kHz sample count — widen it unchanged; the receive funnel
        // owns the tick→ns conversion (seeded via kTickHz at construction).
        dest_header.time = static_cast<PROCTIME>(src_header.time);
        dest_header.chid = src_header.chid;
        dest_header.type = static_cast<uint16_t>(src_header.type);
        dest_header.dlen = static_cast<uint16_t>(src_header.dlen);

        // -- Payload --
        if (src_offset + HEADER_SIZE_311 + src_header.dlen * 4 > src_bytes) {
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
        if ((dest_offset + cbPKT_HEADER_SIZE + (dest_header.dlen + pad_quads) * 4) > dest_cap) {
            return Result<size_t>::error("Output buffer too small for translated packets");
        }
        // Translate payload
        const size_t dest_dlen = PacketTranslator::translatePayload_311_to_current(
            &src[src_offset], &dest[dest_offset]);
        dest_header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        src_offset += HEADER_SIZE_311 + src_header.dlen * 4;
        dest_offset += cbPKT_HEADER_SIZE + dest_header.dlen * 4;
    }

    return Result<size_t>::ok(dest_offset);
}

Result<void> DeviceSession_311::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

ProtocolVersion DeviceSession_311::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_311;
}

} // namespace cbdev
