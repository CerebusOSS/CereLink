///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session_410.cpp
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Protocol 4.10 wrapper implementation
///
/// Implements packet translation between protocol 4.10 and current (4.2+) formats.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include "device_session_410.h"
#include <cbproto/packet_translator.h>
#include <cstring>

namespace cbdev {

using cbproto::PacketTranslator;
using cbproto::HEADER_SIZE_410;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Protocol 4.10 Packet Header Layout (16 bytes total)
/// Identical to Protocol 4.2
///
/// Byte offset layout:
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

Result<DeviceSession_410> DeviceSession_410::create(const ConnectionParams& config) {
    // Create underlying device session for actual socket I/O
    auto result = DeviceSession::create(config);
    if (result.isError()) {
        return Result<DeviceSession_410>::error(result.error());
    }

    // Construct wrapper with the device session
    DeviceSession_410 session(std::move(result.value()));
    return Result<DeviceSession_410>::ok(std::move(session));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// IDeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<size_t> DeviceSession_410::translateDatagram(const uint8_t* src, const size_t src_bytes,
                                                    uint8_t* dest, const size_t dest_cap) {
    // 4.10 is similar enough to current that translation happens in place
    // (src == dest; the funnel received directly into the output buffer).
    // CHANRESET adds 1 byte to the payload but that packet is not actually
    // sent by the device so we can ignore.
    (void)src;
    (void)dest_cap;

    size_t offset = 0;
    while (offset < src_bytes) {
        if (offset + HEADER_SIZE_410 > src_bytes) {
            break;  // Incomplete packet
        }
        // -- Header -- unchanged
        auto header = *reinterpret_cast<cbPKT_HEADER*>(&dest[offset]);
        // -- Payload --
        if (offset + HEADER_SIZE_410 + header.dlen * 4 > src_bytes) {
            break;  // Incomplete packet
        }
        // For packets that have different payload structures, additional translation may be needed
        const size_t dest_dlen = PacketTranslator::translatePayload_410_to_current(
            &dest[offset], &dest[offset]);
        header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        offset += HEADER_SIZE_410 + header.dlen * 4;
    }

    return Result<size_t>::ok(offset);
}

Result<void> DeviceSession_410::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

ProtocolVersion DeviceSession_410::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_410;
}

} // namespace cbdev
