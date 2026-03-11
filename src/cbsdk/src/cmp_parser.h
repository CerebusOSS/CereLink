///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.h
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///
/// CMP files define electrode positions on NSP arrays. Format:
///   - Lines starting with // are comments
///   - First non-comment line is a description string
///   - Subsequent lines: col row bank electrode [label]
///     - col: 0-based column (left to right)
///     - row: 0-based row (bottom to top)
///     - bank: letter A-H (maps to bank index with offset)
///     - electrode: 1-based electrode within bank (1-32)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_CMP_PARSER_H
#define CBSDK_CMP_PARSER_H

#include <cbutil/result.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <array>

namespace cbsdk {

/// Key for CMP position lookup: (bank, term) packed into uint64_t
/// bank is the absolute bank index (1-based, matching chaninfo.bank)
/// term is the terminal index (0-based, matching chaninfo.term)
inline uint64_t cmpKey(uint32_t bank, uint32_t term) {
    return (static_cast<uint64_t>(bank) << 32) | static_cast<uint64_t>(term);
}

/// Map from (bank, term) key to position[4] = {col, row, bank_idx, electrode}
using CmpPositionMap = std::unordered_map<uint64_t, std::array<int32_t, 4>>;

/// Parse a CMP file and return position data keyed by (absolute_bank, term).
///
/// @param filepath Path to the .cmp file
/// @param bank_offset Offset added to bank indices from the file.
///        CMP bank letter A becomes absolute bank (1 + bank_offset).
///        Use 0 for port 1, 4 for port 2 (if 4 banks per port), etc.
/// @return Map of (bank, term) -> position[4], or error message
cbutil::Result<CmpPositionMap> parseCmpFile(const std::string& filepath, uint32_t bank_offset = 0);

} // namespace cbsdk

#endif // CBSDK_CMP_PARSER_H
