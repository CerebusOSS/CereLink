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

#include "cbdev/device_session_410.h"
#include "cbdev/packet_translator.h"
#include <cstring>

namespace cbdev {

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

Result<int> DeviceSession_410::receivePackets(void* buffer, const size_t buffer_size) {
    // Receive from underlying device (in 4.10 format)
    // Format is similar enough to current that we can receive directly into buffer.
    // CHANRESET adds 1 byte to the payload but that packet is not actually sent by the device so we can ignore.
    auto result = m_device.receivePackets(buffer, buffer_size);
    if (result.isError()) {
        return result;
    }

    const int bytes_received = result.value();
    if (bytes_received == 0) {
        return Result<int>::ok(0);  // No data available
    }

    // Translate 4.10 format to current format
    auto* buff_bytes = static_cast<uint8_t*>(buffer);
    size_t offset = 0;
    while (offset < static_cast<size_t>(bytes_received)) {
        if (offset + HEADER_SIZE_410 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }
        // -- Header -- unchanged
        auto header = *reinterpret_cast<cbPKT_HEADER*>(&buff_bytes[offset]);
        // -- Payload --
        if (offset + HEADER_SIZE_410 + header.dlen * 4 > static_cast<size_t>(bytes_received)) {
            break;  // Incomplete packet
        }
        // For packets that have different payload structures, additional translation may be needed
        const size_t dest_dlen = PacketTranslator::translatePayload_410_to_current(
            &buff_bytes[offset], &buff_bytes[offset]);
        header.dlen = dest_dlen;  // This was likely modified in place, but just in case...

        // Advance offsets
        offset += HEADER_SIZE_410 + header.dlen * 4;
    }

    return Result<int>::ok(static_cast<int>(offset));
}

Result<void> DeviceSession_410::sendPacket(const cbPKT_GENERIC& pkt) {
    // Formats are nearly identical.
    // Nevertheless, the src pkt is const so we make a copy to modify.
    cbPKT_GENERIC new_pkt;
    std::memcpy(&new_pkt, &pkt, sizeof(cbPKT_GENERIC));
    auto* pkt_bytes = reinterpret_cast<uint8_t*>(&new_pkt);
    const size_t dest_dlen = PacketTranslator::translatePayload_current_to_410(pkt, pkt_bytes);
    new_pkt.cbpkt_header.dlen = dest_dlen;
    const size_t packet_size_410 = HEADER_SIZE_410 + new_pkt.cbpkt_header.dlen * 4;

    // Send raw bytes directly via the device's sendRaw method
    return m_device.sendRaw(pkt_bytes, packet_size_410);
}

Result<void> DeviceSession_410::sendPackets(const cbPKT_GENERIC* pkts, const size_t count) {
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

Result<void> DeviceSession_410::sendRaw(const void* buffer, const size_t size) {
    // Pass through to underlying device
    return m_device.sendRaw(buffer, size);
}

bool DeviceSession_410::isConnected() const {
    return m_device.isConnected();
}

const ConnectionParams& DeviceSession_410::getConfig() const {
    return m_device.getConfig();
}

} // namespace cbdev
