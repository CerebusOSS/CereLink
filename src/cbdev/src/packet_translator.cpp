//
// Created by Chadwick Boulay on 2025-11-17.
//

#include "cbdev/packet_translator.h"

size_t cbdev::PacketTranslator::translate_DINP_pre400_to_current(const uint8_t* src_payload, cbPKT_DINP* dest) {
    // 3.11 -> 4.0: Eliminated data array and added new fields:
    // uint32_t valueRead;
    // uint32_t bitsChanged;
    // uint32_t eventType;
    // memcpy the 3 quadlets
    memcpy(&dest->valueRead, src_payload, 12);
    dest->cbpkt_header.dlen = 3;
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_DINP_current_to_pre400(const cbPKT_DINP &src, uint8_t* dest_payload) {
    // memcpy the 3 quadlets
    memcpy(dest_payload, &src.valueRead, 12);
    return 3;
}

size_t cbdev::PacketTranslator::translate_NPLAY_pre400_to_current(const uint8_t* src_payload, cbPKT_NPLAY* dest) {
    // ftime, stime, etime, val fields all changed from uint32_t to PROCTIME (uint64_t).
    dest->ftime = static_cast<PROCTIME>(*reinterpret_cast<const uint32_t*>(src_payload)) * 1000000000/30000;
    dest->stime = static_cast<PROCTIME>(*reinterpret_cast<const uint32_t*>(src_payload + 4)) * 1000000000/30000;
    dest->etime = static_cast<PROCTIME>(*reinterpret_cast<const uint32_t*>(src_payload + 8)) * 1000000000/30000;
    dest->val   = static_cast<PROCTIME>(*reinterpret_cast<const uint32_t*>(src_payload + 12)) * 1000000000/30000;
    // memcpy the remaining dlen - 4 quadlets from src to dest.
    memcpy(
        &(dest->mode),
        src_payload + 4 + 4 + 4 + 4,
        dest->cbpkt_header.dlen * 4 - 4 - 4 - 4 - 4
        // dest dlen is the same as src dlen at this point.
    );
    // Add 1 quadlet per timestamp.
    dest->cbpkt_header.dlen += 4;
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_NPLAY_current_to_pre400(const cbPKT_NPLAY &src, uint8_t* dest_payload) {
    // ftime, stime, etime, val fields must be narrowed from PROCTIME (uint64_t) to uint32_t.
    *reinterpret_cast<uint32_t*>(dest_payload)       = static_cast<uint32_t>(src.ftime * 30000 / 1000000000);
    *reinterpret_cast<uint32_t*>(dest_payload + 4)   = static_cast<uint32_t>(src.stime * 30000 / 1000000000);
    *reinterpret_cast<uint32_t*>(dest_payload + 8)   = static_cast<uint32_t>(src.etime * 30000 / 1000000000);
    *reinterpret_cast<uint32_t*>(dest_payload + 12)  = static_cast<uint32_t>(src.val * 30000 / 1000000000);
    // Copy the rest of the payload
    memcpy(
        dest_payload + 4 + 4 + 4 + 4,
        &(src.mode),
        src.cbpkt_header.dlen * 4 - 8 - 8 - 8 - 8
    );
    // dlen decrease: 4 fields shrink from PROCTIME (8 bytes) to uint32_t (4 bytes) = 16 bytes = 4 quadlets
    return src.cbpkt_header.dlen - 4;
}

size_t cbdev::PacketTranslator::translate_COMMENT_pre400_to_current(const uint8_t* src_payload, cbPKT_COMMENT* dest, const uint32_t hdr_timestamp) {
    // cbPKT_COMMENT's 2nd field is a `info` struct with fields:
    //   * In 3.11: `uint8_t type;`, `uint8_t flags`, and `uint8_t reserved[2];`
    //   * In 4.0: `uint8_t charset;` and `uint8_t reserved[3];`
    // --> No change in size.
    // Immediately after `info`, we have:
    //   * In 3.11: `uint32_t data;` -- can be rgba if flags is 0x00, or timeStarted if flags is 0x01
    //   * In 4.0: `PROCTIME timeStarted;`, `uint32_t rgba;`
    // --> Inserted 8 bytes!
    // auto src_info_type = src_payload[0];
    const auto src_info_flags = src_payload[1];
    const auto src_data = *reinterpret_cast<const uint32_t*>(&src_payload[4]);
    dest->info.charset = 0;
    if (src_info_flags) {
        dest->timeStarted = static_cast<PROCTIME>(src_data) * 1000000000/30000;
    } else {
        dest->rgba = src_data;
        dest->timeStarted = static_cast<PROCTIME>(hdr_timestamp) * 1000000000/30000;
    }
    // Finally, the `comment` char array
    memcpy(
        &(dest->comment),
        src_payload + 1 + 1 + 2 + 4,
        dest->cbpkt_header.dlen * 4 - 1 - 1 - 2 - 4
    );
    // The new dlen is just the old + 2 quadlets (8 bytes) for timeStarted.
    dest->cbpkt_header.dlen += 2;
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_COMMENT_current_to_pre400(const cbPKT_COMMENT &src, uint8_t* dest_payload) {
    // dest_payload[0] = ??;  // type -- Is this related to modern charset?
    dest_payload[1] = 0x01;  // flags -- we always set to timeStarted
    // dest.data = timeStarted:
    *reinterpret_cast<uint32_t*>(&dest_payload[4]) = static_cast<uint32_t>(src.timeStarted * 30000 / 1000000000);
    // Copy char array from src.comment to dest.comment.
    memcpy(
        dest_payload + 1 + 1 + 2 + 4,
        &(src.comment),
        src.cbpkt_header.dlen * 4 - 4 - 8 - 4
    );
    // Removal of timeStarted field.
    return src.cbpkt_header.dlen - 2;
}

size_t cbdev::PacketTranslator::translate_SYSPROTOCOLMONITOR_pre410_to_current(const uint8_t* src_payload, cbPKT_SYSPROTOCOLMONITOR* dest) {
    dest->sentpkts = *reinterpret_cast<const uint32_t*>(src_payload);
    // 4.1 added uint32_t counter field at the end of the payload.
    //   If we are coming from protocol 3.11, we can use its header.time field because that was device ticks at 30 kHz.
    //   TODO: This only works for 3.11 -> current. What about 4.0 -> current?
    dest->counter = dest->cbpkt_header.time;
    dest->cbpkt_header.dlen += 1;
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_SYSPROTOCOLMONITOR_current_to_pre410(const cbPKT_SYSPROTOCOLMONITOR &src, uint8_t* dest_payload) {
    *reinterpret_cast<uint32_t*>(dest_payload) = src.sentpkts;
    // Ignore .counter field and drop it from dlen.
    return src.cbpkt_header.dlen - 1;
}

size_t cbdev::PacketTranslator::translate_CHANINFO_pre410_to_current(const uint8_t* src_payload, cbPKT_CHANINFO* dest) {
    // Copy everything up to and including eopchar -- unchanged
    constexpr size_t payload_to_union = offsetof(cbPKT_CHANINFO, eopchar) + sizeof(dest->eopchar) - cbPKT_HEADER_SIZE;
    std::memcpy(&dest->chan, src_payload, payload_to_union);
    size_t src_offset = payload_to_union;
    // Narrow 3.11's uint32_t monsource to 4.1's uint16_t moninst, set uint16_t monchan to 0.
    dest->moninst = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(&src_payload[src_offset]));
    src_offset += 4;
    dest->monchan = 0;  // New field; set to 0.
    // outvalue and trigtype are unchanged
    dest->outvalue = *reinterpret_cast<const int32_t*>(&src_payload[src_offset]);
    src_offset += 4;
    dest->trigtype = src_payload[src_offset];
    src_offset += 1;
    // New fields:
    dest->reserved[0] = 0;
    dest->reserved[1] = 0;
    dest->triginst = 0;
    // memcpy rest. Copy all remaining bytes from source payload.
    std::memcpy(&dest->trigchan,
                 &src_payload[src_offset],
                 dest->cbpkt_header.dlen * 4 - src_offset);
    dest->cbpkt_header.dlen += 1;  // Actually 3/4 of a quadlet, rounded up.
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_CHANINFO_current_to_pre410(const cbPKT_CHANINFO &pkt, uint8_t* dest_payload) {
    // Copy everything up to and including eopchar -- unchanged
    constexpr size_t payload_to_union = offsetof(cbPKT_CHANINFO, eopchar) + sizeof(pkt.eopchar) - cbPKT_HEADER_SIZE;
    memcpy(dest_payload, &pkt.chan, payload_to_union);
    size_t dest_offset = payload_to_union;
    // Expand 4.1's uint16_t moninst to 3.11's uint32_t monsource; ignore uint16_t monchan.
    *reinterpret_cast<uint32_t*>(&dest_payload[dest_offset]) = static_cast<uint32_t>(pkt.moninst);
    dest_offset += 4;
    // outvalue is unchanged
    *reinterpret_cast<int32_t*>(&dest_payload[dest_offset]) = pkt.outvalue;
    dest_offset += 4;
    // trigtype is unchanged
    dest_payload[dest_offset] = pkt.trigtype;
    dest_offset += 1;
    // Skip reserved[0], reserved[1], triginst -- not present in 3.11
    // memcpy rest. Copy all remaining fields from trigchan onwards.
    // Size = remaining destination space = (result_dlen * 4) - dest_offset
    size_t result_dlen = pkt.cbpkt_header.dlen - 1;
    std::memcpy(&dest_payload[dest_offset], &pkt.trigchan,
                result_dlen * 4 - dest_offset);
    return result_dlen;
}

size_t cbdev::PacketTranslator::translate_CHANRESET_pre420_to_current(const uint8_t* src_payload, cbPKT_CHANRESET* dest) {
    // In 4.2, cbPKT_CHANRESET renamed uint8_t monsource to uint8_t moninst and inserted uint8_t monchan.
    // First, we copy everything from outvalue (after monchan) onward.
    // Second, we copy everything up to and including monsource.
    // Note: We go backwards because our src and dest might be the same memory depending on who called this function.
    constexpr size_t payload_to_moninst = offsetof(cbPKT_CHANRESET, moninst) + sizeof(dest->moninst) - cbPKT_HEADER_SIZE;
    constexpr size_t outvalue_to_end = sizeof(cbPKT_CHANRESET) - offsetof(cbPKT_CHANRESET, outvalue);
    std::memcpy(&dest->outvalue, &src_payload[payload_to_moninst], outvalue_to_end);
    std::memcpy(&dest->chan, src_payload, payload_to_moninst);
    // Payload from 29 bytes to 30 bytes, or 7.25 to 7.5 quadlets. dlen will stay truncated at 7.
    return dest->cbpkt_header.dlen;
}

size_t cbdev::PacketTranslator::translate_CHANRESET_current_to_pre420(const cbPKT_CHANRESET &pkt, uint8_t* dest_payload) {
    // In 4.2, cbPKT_CHANRESET renamed uint8_t monsource to uint8_t moninst and inserted uint8_t monchan.
    // First, we copy everything from outvalue (after monchan) onward.
    // Second, we copy everything up to and including monsource.
    // Note: We go backwards because our src and dest might be the same memory depending on who called this function.
    constexpr size_t payload_to_moninst = offsetof(cbPKT_CHANRESET, moninst) + sizeof(pkt.moninst) - cbPKT_HEADER_SIZE;
    constexpr size_t outvalue_to_end = sizeof(cbPKT_CHANRESET) - offsetof(cbPKT_CHANRESET, outvalue);
    std::memcpy(&dest_payload[payload_to_moninst], &pkt.outvalue, outvalue_to_end);
    std::memcpy(dest_payload, &pkt.chan, payload_to_moninst);
    return pkt.cbpkt_header.dlen;
}
