///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   packet_test_helpers.cpp
/// @author CereLink Development Team
/// @date   2025-01-19
///
/// @brief  Implementation of packet factory helper functions
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "packet_test_helpers.h"
#include <cstring>
#include <algorithm>

namespace test_helpers {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Header Factories
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> make_311_header(const uint32_t time, const uint16_t chid, const uint8_t type, const uint8_t dlen) {
    std::vector<uint8_t> header(HEADER_SIZE_311);

    // Protocol 3.11 layout: time(32b) chid(16b) type(8b) dlen(8b)
    *reinterpret_cast<uint32_t*>(&header[0]) = time;
    *reinterpret_cast<uint16_t*>(&header[4]) = chid;
    header[6] = type;
    header[7] = dlen;

    return header;
}

std::vector<uint8_t> make_400_header(const uint64_t time, const uint16_t chid, const uint8_t type,
                                     const uint16_t dlen, const uint8_t instrument) {
    std::vector<uint8_t> header(HEADER_SIZE_400);

    // Protocol 4.0 layout: time(64b) chid(16b) type(8b) dlen(16b) instrument(8b) reserved(16b)
    *reinterpret_cast<uint64_t*>(&header[0]) = time;
    *reinterpret_cast<uint16_t*>(&header[8]) = chid;
    header[10] = type;
    *reinterpret_cast<uint16_t*>(&header[11]) = dlen;
    header[13] = instrument;
    *reinterpret_cast<uint16_t*>(&header[14]) = 0;  // reserved

    return header;
}

cbPKT_HEADER make_current_header(const uint64_t time, const uint16_t chid, const uint16_t type,
                                 const uint16_t dlen, const uint8_t instrument) {
    cbPKT_HEADER header = {};
    header.time = time;
    header.chid = chid;
    header.type = type;
    header.dlen = dlen;
    header.instrument = instrument;
    header.reserved = 0;
    return header;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Protocol 3.11 Packet Factories
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> make_311_SYSPROTOCOLMONITOR(const uint32_t sentpkts, const uint32_t time) {
    // 3.11 SYSPROTOCOLMONITOR: header + sentpkts (no counter field)
    constexpr size_t dlen = cbPKTDLEN_SYSPROTOCOLMONITOR - 1;
    auto packet = make_311_header(time, cbPKTCHAN_CONFIGURATION,
                                  cbPKTTYPE_SYSPROTOCOLMONITOR, dlen);

    // Append payload
    const size_t offset = packet.size();
    packet.resize(offset + dlen * 4);
    *reinterpret_cast<uint32_t*>(&packet[offset]) = sentpkts;

    return packet;
}

std::vector<uint8_t> make_311_NPLAY(const uint32_t ftime, const uint32_t stime, const uint32_t etime,
                                    const uint32_t val, const uint32_t mode) {
    // 3.11 NPLAY: header + 4 time fields (uint32_t) + unchanged: mode + flags + speed + fname

    constexpr size_t dlen = cbPKTDLEN_NPLAY - 4;
    auto packet = make_311_header(1000, cbPKTCHAN_CONFIGURATION,
                                  cbPKTTYPE_NPLAYREP, dlen);

    const size_t offset = packet.size();
    packet.resize(offset + dlen * 4);

    *reinterpret_cast<uint32_t*>(&packet[offset + 0]) = ftime;
    *reinterpret_cast<uint32_t*>(&packet[offset + 4]) = stime;
    *reinterpret_cast<uint32_t*>(&packet[offset + 8]) = etime;
    *reinterpret_cast<uint32_t*>(&packet[offset + 12]) = val;
    *reinterpret_cast<uint16_t*>(&packet[offset + 16]) = mode;
    *reinterpret_cast<uint16_t*>(&packet[offset + 18]) = cbNPLAY_FLAG_NONE;
    *reinterpret_cast<float*>(&packet[offset + 20]) = 1.0;  // speed
    // TODO: fname

    return packet;
}

std::vector<uint8_t> make_311_COMMENT(const uint8_t flags, const uint32_t data,
                                      const char* comment, const uint32_t time) {
    // 3.11 COMMENT: header + info(4 bytes) + data(4 bytes) + comment string
    // info: type(1) flags(1) reserved(2)

    const size_t comment_len = comment ? strlen(comment) + 1 : 1;  // Include null terminator
    const size_t total_payload = 8 + comment_len;  // info(4) + data(4) + comment
    // Pad to quadlet boundary
    const size_t padded_payload = ((total_payload + 3) / 4) * 4;
    const auto dlen = static_cast<uint8_t>(padded_payload / 4);

    auto packet = make_311_header(time, cbPKTCHAN_CONFIGURATION,
                                  cbPKTTYPE_COMMENTREP, dlen);

    const size_t offset = packet.size();
    packet.resize(offset + padded_payload, 0);  // Zero-fill padding

    // info structure (4 bytes)
    packet[offset + 0] = 0;  // type
    packet[offset + 1] = flags;
    packet[offset + 2] = 0;  // reserved
    packet[offset + 3] = 0;  // reserved

    // data field (4 bytes)
    *reinterpret_cast<uint32_t*>(&packet[offset + 4]) = data;

    // comment string
    if (comment) {
        std::memcpy(&packet[offset + 8], comment, comment_len);
    }

    return packet;
}


std::vector<uint8_t> make_311_CHANINFO(const uint32_t chan, const uint32_t monsource) {
    // Create a 3.11 CHANINFO packet
    constexpr uint8_t dlen = cbPKTDLEN_CHANINFO - 1;

    auto packet = make_311_header(1000, cbPKTCHAN_CONFIGURATION,
                                  cbPKTTYPE_CHANREP, dlen);

    const size_t offset = packet.size();
    packet.resize(offset + dlen * 4, 0);

    // chan field (first field after header)
    *reinterpret_cast<uint32_t*>(&packet[offset]) = chan;

    // Modify monsource
    constexpr size_t monsource_offset = offsetof(cbPKT_CHANINFO, moninst) - cbPKT_HEADER_SIZE;
    *reinterpret_cast<uint32_t*>(&packet[offset + monsource_offset]) = monsource;

    // Modify trigchan
    constexpr size_t trigchan_offset = offsetof(cbPKT_CHANINFO, trigchan) - cbPKT_HEADER_SIZE - 3;
    *reinterpret_cast<uint16_t*>(&packet[offset + trigchan_offset]) = 42;  // example trigchan

    return packet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Protocol 4.0 Packet Factories
///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> make_400_SYSPROTOCOLMONITOR(const uint32_t sentpkts, const uint64_t time) {
    // 4.0 SYSPROTOCOLMONITOR: same payload as 3.11 (no counter field)
    auto packet = make_400_header(time, cbPKTCHAN_CONFIGURATION,
                                  cbPKTTYPE_SYSPROTOCOLMONITOR, 1, 0);

    const size_t offset = packet.size();
    packet.resize(offset + 4);
    *reinterpret_cast<uint32_t*>(&packet[offset]) = sentpkts;

    return packet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Protocol 4.10 Packet Factories
///////////////////////////////////////////////////////////////////////////////////////////////////

cbPKT_GENERIC make_410_CHANRESET(const uint32_t chan, const uint8_t monsource) {
    // 4.10 CHANRESET has monsource as single uint8_t (before moninst/monchan split in 4.2)
    // Note: 4.10 header is same as current, only payload structure differs

    cbPKT_GENERIC pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_CHANRESET, 7, 0);

    // Create CHANRESET payload with pre-4.2 structure
    // We'll use the current cbPKT_CHANRESET but modify it to match 4.10 layout
    auto* chanreset = reinterpret_cast<cbPKT_CHANRESET*>(&pkt);
    chanreset->chan = chan;
    chanreset->label = 0;
    chanreset->userflags = 0;
    // In 4.10, there's only monsource (uint8_t), not moninst + monchan
    chanreset->moninst = monsource;  // This would be monsource in 4.10
    // monchan doesn't exist in 4.10

    return pkt;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Current Protocol Packet Factories
///////////////////////////////////////////////////////////////////////////////////////////////////

cbPKT_SYSPROTOCOLMONITOR make_current_SYSPROTOCOLMONITOR(const uint32_t sentpkts, const uint32_t counter) {
    cbPKT_SYSPROTOCOLMONITOR pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_SYSPROTOCOLMONITOR,
                                           cbPKTDLEN_SYSPROTOCOLMONITOR, 0);
    pkt.sentpkts = sentpkts;
    pkt.counter = counter;
    return pkt;
}

cbPKT_NPLAY make_current_NPLAY(const PROCTIME ftime, const PROCTIME stime, const PROCTIME etime,
                               const PROCTIME val, const uint32_t mode) {
    cbPKT_NPLAY pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_NPLAYREP,
                                           cbPKTDLEN_NPLAY, 0);
    pkt.ftime = ftime;
    pkt.stime = stime;
    pkt.etime = etime;
    pkt.val = val;
    pkt.mode = mode;
    std::memset(pkt.fname, 0, sizeof(pkt.fname));
    return pkt;
}

cbPKT_COMMENT make_current_COMMENT(const uint8_t charset, const PROCTIME timeStarted,
                                   const uint32_t rgba, const char* comment) {
    cbPKT_COMMENT pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_COMMENTREP,
                                           cbPKTDLEN_COMMENT, 0);
    pkt.info.charset = charset;
    pkt.timeStarted = timeStarted;
    pkt.rgba = rgba;

    if (comment) {
        std::strncpy(reinterpret_cast<char*>(pkt.comment), comment, cbMAX_COMMENT - 1);
        pkt.comment[cbMAX_COMMENT - 1] = '\0';
    } else {
        std::memset(pkt.comment, 0, cbMAX_COMMENT);
    }

    return pkt;
}

cbPKT_CHANINFO make_current_CHANINFO(const uint32_t chan, const uint16_t moninst, const uint16_t monchan) {
    cbPKT_CHANINFO pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_CHANREP,
                                           cbPKTDLEN_CHANINFO, 0);
    pkt.chan = chan;
    pkt.moninst = moninst;
    pkt.monchan = monchan;
    // Fill other fields with defaults
    std::memset(pkt.label, 0, sizeof(pkt.label));
    return pkt;
}

cbPKT_CHANRESET make_current_CHANRESET(const uint32_t chan, const uint8_t moninst, const uint8_t monchan) {
    cbPKT_CHANRESET pkt = {};
    pkt.cbpkt_header = make_current_header(1000, cbPKTCHAN_CONFIGURATION,
                                           cbPKTTYPE_CHANRESET,
                                           cbPKTDLEN_CHANRESET, 0);
    pkt.chan = chan;
    pkt.moninst = moninst;
    pkt.monchan = monchan;
    // Fill other fields with defaults
    pkt.label = 0;
    pkt.userflags = 0;
    return pkt;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Comparison Helpers
///////////////////////////////////////////////////////////////////////////////////////////////////

bool packets_equal(const cbPKT_GENERIC& expected, const cbPKT_GENERIC& actual,
                   uint64_t tolerance) {
    // Compare headers
    if (std::abs(static_cast<int64_t>(expected.cbpkt_header.time) -
                 static_cast<int64_t>(actual.cbpkt_header.time)) > static_cast<int64_t>(tolerance)) {
        return false;
    }

    if (expected.cbpkt_header.chid != actual.cbpkt_header.chid) return false;
    if (expected.cbpkt_header.type != actual.cbpkt_header.type) return false;
    if (expected.cbpkt_header.dlen != actual.cbpkt_header.dlen) return false;
    if (expected.cbpkt_header.instrument != actual.cbpkt_header.instrument) return false;

    // Compare payload (first dlen quadlets after header)
    const auto* expected_bytes = reinterpret_cast<const uint8_t*>(&expected);
    const auto* actual_bytes = reinterpret_cast<const uint8_t*>(&actual);

    const size_t payload_size = expected.cbpkt_header.dlen * 4;
    return std::memcmp(expected_bytes + cbPKT_HEADER_SIZE,
                      actual_bytes + cbPKT_HEADER_SIZE,
                      payload_size) == 0;
}

} // namespace test_helpers
