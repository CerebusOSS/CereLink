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

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include "device_session_400.h"
#include <cbproto/packet_translator.h>
#include <cstring>

namespace cbdev {

using cbproto::PacketTranslator;
using cbproto::cbPKT_HEADER_400;
using cbproto::HEADER_SIZE_400;

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

Result<size_t> DeviceSession_400::translateDatagram(const uint8_t* src, const size_t src_bytes,
                                                    uint8_t* dest, const size_t dest_cap) {
    // Even though the header size is the same, protocol 4.0 had the old CHANINFO structure,
    // which was 3 bytes smaller than the new structure. Given that we typically receive many
    // CHANINFO packets in a row, we cannot reasonably translate in place, as that would
    // require a lot of moving memory around. The funnel therefore hands us a separate
    // scratch buffer as `src`.
    size_t dest_offset = 0;
    size_t src_offset = 0;
    while (src_offset < src_bytes) {
        if (src_offset + HEADER_SIZE_400 > src_bytes) {
            break;  // Incomplete packet
        }

        // -- Header --
        auto src_header = *reinterpret_cast<const cbPKT_HEADER_400*>(&src[src_offset]);
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER*>(&dest[dest_offset]);
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
        if (src_offset + HEADER_SIZE_400 + src_header.dlen * 4 > src_bytes) {
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
        if ((dest_offset + cbPKT_HEADER_SIZE + (dest_header.dlen + pad_quads) * 4) > dest_cap) {
            return Result<size_t>::error("Output buffer too small for translated packets");
        }
        // Translate payload
        const size_t dest_dlen = PacketTranslator::translatePayload_400_to_current(
            &src[src_offset], &dest[dest_offset]);
        dest_header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        src_offset += HEADER_SIZE_400 + src_header.dlen * 4;
        dest_offset += cbPKT_HEADER_SIZE + dest_header.dlen * 4;
    }

    return Result<size_t>::ok(dest_offset);
}

Result<void> DeviceSession_400::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

ProtocolVersion DeviceSession_400::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_400;
}

} // namespace cbdev
