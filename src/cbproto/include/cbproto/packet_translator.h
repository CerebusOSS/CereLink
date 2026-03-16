//
// Created by Chadwick Boulay on 2025-11-17.
//

#ifndef CBPROTO_PACKET_TRANSLATOR_H
#define CBPROTO_PACKET_TRANSLATOR_H

#include <cbproto/cbproto.h>
#include <cstddef>  // for size_t
#include <cstdint>  // for uint8_t
#include <cstring>  // for std::memcpy

// TODO: Finish the implementation of all the type-specific translation functions.

namespace cbproto {

typedef struct {
    uint32_t time;        ///< Ticks at 30 kHz
    uint16_t chid;        ///< Channel identifier
    uint8_t type;         ///< Packet type
    uint8_t dlen;         ///< Length of data field in 32-bit chunks
} cbPKT_HEADER_311;
constexpr size_t HEADER_SIZE_311 = sizeof(cbPKT_HEADER_311);

typedef struct {
    PROCTIME time;        ///< Ticks at 30 kHz on legacy, or nanoseconds on Gemini
    uint16_t chid;        ///< Channel identifier
    uint8_t type;         ///< Packet type
    uint16_t dlen;        ///< Length of data field in 32-bit chunks
    uint8_t instrument;   ///< Instrument identifier
    uint16_t reserved;    ///< Reserved byte
} cbPKT_HEADER_400;
constexpr size_t HEADER_SIZE_400 = sizeof(cbPKT_HEADER_400);

constexpr size_t HEADER_SIZE_410 = cbPKT_HEADER_SIZE;  // Header unchanged since 4.1


class PacketTranslator {
public:
    // Public methods' args are pointers to the start of the entire packet (header + payload).

    static size_t translatePayload_311_to_current(const uint8_t* src, uint8_t* dest) {
        // Header has already been translated, and we are guaranteed dest has enough space.
        // Copy the payload bytes into the destination packet.

        const auto* src_payload = &src[HEADER_SIZE_311];
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER*>(dest);

        switch (dest_header.type) {
        case cbPKTTYPE_NPLAYREP:
            return translate_NPLAY_pre400_to_current(src_payload, reinterpret_cast<cbPKT_NPLAY *>(dest));
        case cbPKTTYPE_COMMENTREP:
            return translate_COMMENT_pre400_to_current(
                src_payload, reinterpret_cast<cbPKT_COMMENT *>(dest), *reinterpret_cast<const uint32_t*>(src));
        case cbPKTTYPE_SYSPROTOCOLMONITOR:
            // cbPKTTYPE_SYSPROTOCOLMONITOR == 0x01 == cbPKTTYPE_PREVREPLNC (pre-4.2)
            // SYSPROTOCOLMONITOR translation takes priority; PREVREPLNC remap is N/A for 3.11
            return translate_SYSPROTOCOLMONITOR_pre410_to_current(src_payload, reinterpret_cast<cbPKT_SYSPROTOCOLMONITOR*>(dest));
        case cbPKTTYPE_CHANRESETREP:
            return translate_CHANRESET_pre420_to_current(src_payload, reinterpret_cast<cbPKT_CHANRESET*>(dest));
        default:
            // CHANREP family (0x40-0x4F) — bitmask check, can't be a case label
            if ((dest_header.type & 0xF0) == cbPKTTYPE_CHANREP) {
                return translate_CHANINFO_pre410_to_current(src_payload, reinterpret_cast<cbPKT_CHANINFO *>(dest));
            }
            // TODO: cbPKT_DINP — needs chaninfo, unavailable here
            break;
        }

        // No explicit change. Do a memcpy and report the original dlen (already in dest header).
        if (dest_header.dlen > 0) {
            std::memcpy(&dest[cbPKT_HEADER_SIZE],
                       src_payload,
                       dest_header.dlen * 4);
        }
        return dest_header.dlen;
    }

    static size_t translatePayload_400_to_current(const uint8_t* src, uint8_t* dest) {
        // Header has already been translated, and we are guaranteed dest has enough space.
        // Copy the payload bytes into the destination packet.

        auto& dest_header = *reinterpret_cast<cbPKT_HEADER*>(dest);
        const auto* src_payload = &src[HEADER_SIZE_400];

        switch (dest_header.type) {
        case cbPKTTYPE_SYSPROTOCOLMONITOR:
            // cbPKTTYPE_SYSPROTOCOLMONITOR == 0x01; PREVREPLNC remap is N/A for 4.0
            return translate_SYSPROTOCOLMONITOR_pre410_to_current(src_payload, reinterpret_cast<cbPKT_SYSPROTOCOLMONITOR*>(dest));
        case cbPKTTYPE_CHANRESETREP:
            return translate_CHANRESET_pre420_to_current(src_payload, reinterpret_cast<cbPKT_CHANRESET*>(dest));
        default:
            if ((dest_header.type & 0xF0) == cbPKTTYPE_CHANREP) {
                return translate_CHANINFO_pre410_to_current(src_payload, reinterpret_cast<cbPKT_CHANINFO *>(dest));
            }
            break;
        }

        // No explicit change. Do a memcpy and report the original dlen (already in dest header).
        if (dest_header.dlen > 0) {
            std::memcpy(&dest[cbPKT_HEADER_SIZE],
                       src_payload,
                       dest_header.dlen * 4);
        }
        return dest_header.dlen;
    }

    static size_t translatePayload_410_to_current(const uint8_t* src, uint8_t* dest) {
        // For 410 to current, we do not use an intermediate buffer; src and dest are the same!
        const auto* src_payload = &src[HEADER_SIZE_410];
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER*>(dest);
        switch (dest_header.type) {
        case cbPKTTYPE_CHANRESETREP:
            return translate_CHANRESET_pre420_to_current(src_payload, reinterpret_cast<cbPKT_CHANRESET*>(dest));
        case 0x01:
            dest_header.type = 0x04;
            return dest_header.dlen;
        default:
            return dest_header.dlen;
        }
    }

    static size_t translatePayload_current_to_311(const cbPKT_GENERIC& src, uint8_t* dest) {
        // Prepare pointers to specific sections that will be modified
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER_311*>(dest);
        auto* dest_payload = &dest[HEADER_SIZE_311];

        switch (src.cbpkt_header.type) {
        case cbPKTTYPE_NPLAYSET:
            return translate_NPLAY_current_to_pre400(
                *reinterpret_cast<const cbPKT_NPLAY*>(&src), dest_payload);
        case cbPKTTYPE_COMMENTSET:
            return translate_COMMENT_current_to_pre400(
                *reinterpret_cast<const cbPKT_COMMENT*>(&src), dest_payload);
        case cbPKTTYPE_SYSPROTOCOLMONITOR:
            return translate_SYSPROTOCOLMONITOR_current_to_pre410(
                *reinterpret_cast<const cbPKT_SYSPROTOCOLMONITOR*>(&src), dest_payload);
        case cbPKTTYPE_CHANRESET:
            return translate_CHANRESET_current_to_pre420(
                *reinterpret_cast<const cbPKT_CHANRESET*>(&src), dest_payload);
        case cbPKTTYPE_PREVSETLNC:
            dest_header.type = 0x81;
            return dest_header.dlen;
        default:
            if ((src.cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANSET) {
                return translate_CHANINFO_current_to_pre410(
                    *reinterpret_cast<const cbPKT_CHANINFO*>(&src), dest_payload);
            }
            // TODO: cbPKT_DINP — needs chaninfo, unavailable here
            break;
        }

        // memcpy the payload bytes
        const auto* src_bytes = reinterpret_cast<const uint8_t*>(&src);
        if (src.cbpkt_header.dlen > 0) {
            std::memcpy(dest_payload,
                       &src_bytes[cbPKT_HEADER_SIZE],
                       src.cbpkt_header.dlen * 4);
        }
        return src.cbpkt_header.dlen;
    }

    static size_t translatePayload_current_to_400(const cbPKT_GENERIC& src, uint8_t* dest) {
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER_400*>(dest);
        auto* dest_payload = &dest[HEADER_SIZE_400];

        switch (src.cbpkt_header.type) {
        case cbPKTTYPE_SYSPROTOCOLMONITOR:
            return translate_SYSPROTOCOLMONITOR_current_to_pre410(
                *reinterpret_cast<const cbPKT_SYSPROTOCOLMONITOR*>(&src), dest_payload);
        case cbPKTTYPE_CHANRESET:
            return translate_CHANRESET_current_to_pre420(
                *reinterpret_cast<const cbPKT_CHANRESET*>(&src), dest_payload);
        case cbPKTTYPE_PREVSETLNC:
            dest_header.type = 0x81;
            return dest_header.dlen;
        default:
            if ((src.cbpkt_header.type & 0xF0) == cbPKTTYPE_CHANSET) {
                return translate_CHANINFO_current_to_pre410(
                    *reinterpret_cast<const cbPKT_CHANINFO*>(&src), dest_payload);
            }
            break;
        }

        // memcpy the payload bytes and report the original dlen.
        const auto* src_bytes = reinterpret_cast<const uint8_t*>(&src);
        if (src.cbpkt_header.dlen > 0) {
            std::memcpy(dest_payload,
                       &src_bytes[cbPKT_HEADER_SIZE],
                       src.cbpkt_header.dlen * 4);
        }
        return src.cbpkt_header.dlen;
    }

    static size_t translatePayload_current_to_410(const cbPKT_GENERIC& src, uint8_t* dest) {
        // We already copied the entire packet upstream. Here we need to adjust payload only.
        auto& dest_header = *reinterpret_cast<cbPKT_HEADER*>(dest);
        auto* dest_payload = &dest[HEADER_SIZE_410];
        switch (src.cbpkt_header.type) {
        case cbPKTTYPE_CHANRESET:
            return translate_CHANRESET_current_to_pre420(
                *reinterpret_cast<const cbPKT_CHANRESET*>(&src), dest_payload);
        case cbPKTTYPE_PREVSETLNC:
            dest_header.type = 0x81;
            return dest_header.dlen;
        default:
            return src.cbpkt_header.dlen;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Packet-Specific Translation Functions (Public for Unit Testing)
    /// @{
    ///
    /// These methods expect a pointer to the start of the non-current (src/dest) payload
    /// and to the specific current (dest/src) packet. This is because these methods are reused
    /// across non-current protocol versions so we cannot know the header size, whereas the public
    /// translatePayload_* methods are only used for a specific protocol version and know the header size.
    ///
    /// Made public to enable direct unit testing of translation logic.
    /// @}

    static size_t translate_DINP_pre400_to_current(const uint8_t* src_payload, cbPKT_DINP* dest);
    static size_t translate_DINP_current_to_pre400(const cbPKT_DINP &src, uint8_t* dest_payload);

    static size_t translate_NPLAY_pre400_to_current(const uint8_t* src_payload, cbPKT_NPLAY* dest);
    static size_t translate_NPLAY_current_to_pre400(const cbPKT_NPLAY &src, uint8_t* dest_payload);

    static size_t translate_COMMENT_pre400_to_current(const uint8_t* src_payload, cbPKT_COMMENT* dest, uint32_t hdr_timestamp);
    static size_t translate_COMMENT_current_to_pre400(const cbPKT_COMMENT &src, uint8_t* dest_payload);

    static size_t translate_SYSPROTOCOLMONITOR_pre410_to_current(const uint8_t* src_payload, cbPKT_SYSPROTOCOLMONITOR* dest);
    static size_t translate_SYSPROTOCOLMONITOR_current_to_pre410(const cbPKT_SYSPROTOCOLMONITOR &src, uint8_t* dest_payload);

    static size_t translate_CHANINFO_pre410_to_current(const uint8_t* src_payload, cbPKT_CHANINFO* dest);
    static size_t translate_CHANINFO_current_to_pre410(const cbPKT_CHANINFO &pkt, uint8_t* dest_payload);

    static size_t translate_CHANRESET_pre420_to_current(const uint8_t* src_payload, cbPKT_CHANRESET* dest);
    static size_t translate_CHANRESET_current_to_pre420(const cbPKT_CHANRESET &pkt, uint8_t* dest_payload);

private:
    // No private members currently
};

} // namespace cbproto

#endif //CBPROTO_PACKET_TRANSLATOR_H
