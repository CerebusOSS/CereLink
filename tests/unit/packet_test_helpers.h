///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   packet_test_helpers.h
/// @author CereLink Development Team
/// @date   2025-01-19
///
/// @brief  Helper functions for creating test packets in different protocol versions
///
/// Provides factory functions to generate well-formed packets for testing packet translation
/// between protocol versions 3.11, 4.0, 4.10, and current (4.2+).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CERELINK_PACKET_TEST_HELPERS_H
#define CERELINK_PACKET_TEST_HELPERS_H

#include <cbproto/cbproto.h>
#include <vector>
#include <cstdint>
#include <cstring>

namespace test_helpers {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol Version Constants
/// @{

/// Protocol 3.11: 32-bit timestamps, 8-byte header
/// Layout: time(32b) chid(16b) type(8b) dlen(8b)
constexpr size_t HEADER_SIZE_311 = 8;

/// Protocol 4.0: 64-bit timestamps, 16-byte header, 8-bit type
/// Layout: time(64b) chid(16b) type(8b) dlen(16b) instrument(8b) reserved(16b)
constexpr size_t HEADER_SIZE_400 = 16;

/// Protocol 4.10: Same header as current (only payload differences for some packets)
/// Layout: time(64b) chid(16b) type(16b) dlen(16b) instrument(8b) reserved(8b)
constexpr size_t HEADER_SIZE_410 = 16;

/// Current protocol (4.2+): 64-bit timestamps, 16-byte header, 16-bit type
/// Layout: time(64b) chid(16b) type(16b) dlen(16b) instrument(8b) reserved(8b)
constexpr size_t HEADER_SIZE_CURRENT = 16;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Header Factory Functions
/// @{

/// Create a protocol 3.11 header (8 bytes)
/// @param time 32-bit timestamp in 30kHz ticks
/// @param chid Channel ID
/// @param type Packet type (8-bit)
/// @param dlen Payload length in quadlets (8-bit, max 255)
/// @return 8-byte header in 3.11 format
std::vector<uint8_t> make_311_header(uint32_t time, uint16_t chid, uint8_t type, uint8_t dlen);

/// Create a protocol 4.0 header (16 bytes)
/// @param time 64-bit timestamp in nanoseconds
/// @param chid Channel ID
/// @param type Packet type (8-bit)
/// @param dlen Payload length in quadlets (16-bit)
/// @param instrument Instrument ID (0-3)
/// @return 16-byte header in 4.0 format
std::vector<uint8_t> make_400_header(uint64_t time, uint16_t chid, uint8_t type,
                                     uint16_t dlen, uint8_t instrument);

/// Create a current protocol header (16 bytes)
/// @param time 64-bit timestamp in nanoseconds
/// @param chid Channel ID
/// @param type Packet type (16-bit)
/// @param dlen Payload length in quadlets (16-bit)
/// @param instrument Instrument ID (0-3)
/// @return Filled cbPKT_HEADER structure
cbPKT_HEADER make_current_header(uint64_t time, uint16_t chid, uint16_t type,
                                 uint16_t dlen, uint8_t instrument);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Legacy Protocol (3.11) Packet Factories
/// @{

/// Create a protocol 3.11 SYSPROTOCOLMONITOR packet
/// @param sentpkts Number of packets sent
/// @param time Header timestamp (30kHz ticks)
/// @return Complete packet as byte vector
std::vector<uint8_t> make_311_SYSPROTOCOLMONITOR(uint32_t sentpkts, uint32_t time = 1000);

/// Create a protocol 3.11 NPLAY packet
/// @param ftime File time in 30kHz ticks
/// @param stime Start time in 30kHz ticks
/// @param etime End time in 30kHz ticks
/// @param val Current value in 30kHz ticks
/// @param mode Play mode
/// @return Complete packet as byte vector
std::vector<uint8_t> make_311_NPLAY(uint32_t ftime, uint32_t stime, uint32_t etime,
                                    uint32_t val, uint32_t mode);

/// Create a protocol 3.11 COMMENT packet
/// @param flags Comment flags (0x00=RGBA, 0x01=timeStarted)
/// @param data RGBA color (if flags=0) or timeStarted in ticks (if flags=1)
/// @param comment Comment text (null-terminated)
/// @param time Header timestamp (30kHz ticks)
/// @return Complete packet as byte vector
std::vector<uint8_t> make_311_COMMENT(uint8_t flags, uint32_t data,
                                      const char* comment, uint32_t time = 1000);

/// Create a protocol 3.11 DINP (Digital Input) packet
/// Note: 3.11 DINP has a different structure than 4.0+
/// @return Complete packet as byte vector
std::vector<uint8_t> make_311_DINP(uint32_t time = 1000);

/// Create a protocol 3.11 CHANINFO packet
/// @param chan Channel number
/// @param monsource Monitor source (32-bit in 3.11, becomes moninst in 4.1+)
/// @return Complete packet as byte vector
std::vector<uint8_t> make_311_CHANINFO(uint32_t chan, uint32_t monsource);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol 4.0 Packet Factories
/// @{

/// Create a protocol 4.0 SYSPROTOCOLMONITOR packet
/// Note: 4.0 has same payload structure as 3.11 (no counter field)
std::vector<uint8_t> make_400_SYSPROTOCOLMONITOR(uint32_t sentpkts, uint64_t time = 1000000);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol 4.10 Packet Factories
/// @{
/// Note: 4.10 header is identical to current, only some payload structures differ

/// Create a protocol 4.10 CHANRESET packet (pre-4.2 structure)
/// @param chan Channel number
/// @param monsource Monitor source (single uint8_t, becomes moninst in 4.2+)
/// @return Complete packet as cbPKT_GENERIC
cbPKT_GENERIC make_410_CHANRESET(uint32_t chan, uint8_t monsource);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Current Protocol Packet Factories
/// @{

/// Create a current protocol SYSPROTOCOLMONITOR packet
cbPKT_SYSPROTOCOLMONITOR make_current_SYSPROTOCOLMONITOR(uint32_t sentpkts, uint32_t counter);

/// Create a current protocol NPLAY packet
cbPKT_NPLAY make_current_NPLAY(PROCTIME ftime, PROCTIME stime, PROCTIME etime,
                               PROCTIME val, uint32_t mode);

/// Create a current protocol COMMENT packet
cbPKT_COMMENT make_current_COMMENT(uint8_t charset, PROCTIME timeStarted,
                                   uint32_t rgba, const char* comment);

/// Create a current protocol DINP packet
cbPKT_DINP make_current_DINP(uint32_t valueRead, uint32_t bitsChanged, uint32_t eventType);

/// Create a current protocol CHANINFO packet
cbPKT_CHANINFO make_current_CHANINFO(uint32_t chan, uint16_t moninst, uint16_t monchan);

/// Create a current protocol CHANRESET packet (4.2+ structure)
cbPKT_CHANRESET make_current_CHANRESET(uint32_t chan, uint8_t moninst, uint8_t monchan);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Comparison Helpers
/// @{

/// Compare two packets for equality, ignoring minor precision differences in timestamps
/// @param expected Expected packet
/// @param actual Actual packet
/// @param tolerance Allowable difference in nanosecond timestamps
/// @return true if packets match within tolerance
bool packets_equal(const cbPKT_GENERIC& expected, const cbPKT_GENERIC& actual,
                   uint64_t tolerance = 0);

/// Convert 30kHz ticks to nanoseconds (for 3.11 timestamps)
/// Uses same formula as actual translation code: multiply first, then divide
/// This preserves precision: 30000 * 1000000000 / 30000 = 1000000000 (exact)
/// vs. 30000 * (1000000000 / 30000) = 30000 * 33333 = 999990000 (loses precision)
inline uint64_t ticks_to_ns(uint32_t ticks) {
    return static_cast<uint64_t>(ticks) * 1000000000ULL / 30000ULL;
}

/// Convert nanoseconds to 30kHz ticks (for reverse translation)
/// Uses same formula as actual translation code
inline uint32_t ns_to_ticks(uint64_t ns) {
    return static_cast<uint32_t>(ns * 30000ULL / 1000000000ULL);
}

/// @}

} // namespace test_helpers

#endif // CERELINK_PACKET_TEST_HELPERS_H
