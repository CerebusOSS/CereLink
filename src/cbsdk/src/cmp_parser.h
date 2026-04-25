///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.h
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///
/// CMP files define electrode positions for a single headstage. Format:
///   - Lines starting with // are comments
///   - First non-comment line is a description string
///   - Subsequent lines: col row bank electrode [label]
///     - col: 0-based column (left to right)
///     - row: 0-based row (bottom to top)
///     - bank: letter A-H (32 electrodes per bank within the headstage)
///     - electrode: 1-based electrode within bank (1-32)
///     - label: free-form label (optional)
///
/// Rows in a CMP file are not guaranteed to be in channel order. The parser
/// sorts by (bank_letter, electrode) and assigns each sorted entry a 1-based
/// absolute channel ID starting at @c start_chan. This is how a CMP gets
/// applied to a contiguous range of channels — callers control the starting
/// channel so multiple CMPs (one per headstage) can be loaded on one device.
///
/// Labels are reused across CMP files (e.g. "elec1-1"..."elec1-128" in every
/// file), so the parser prefixes each label with "hs{hs_id}-" to keep them
/// unique on the device. Pass @c hs_id == 0 to skip prefixing entirely
/// (sensible for single-headstage rigs where the original labels are already
/// unique).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_CMP_PARSER_H
#define CBSDK_CMP_PARSER_H

#include <cbutil/result.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace cbsdk {

/// One parsed CMP entry, ready to apply to an absolute channel.
struct CmpEntry {
    std::array<int32_t, 4> position;  ///< {col, row, bank_letter_idx, electrode}
    std::string label;                ///< prefixed label (e.g. "hs1-chan12")
};

/// Map from 1-based absolute channel ID to parsed entry.
using CmpEntries = std::unordered_map<uint32_t, CmpEntry>;

/// Parse a CMP file and assign each row an absolute channel ID.
///
/// Rows are sorted by (bank_letter, electrode) and the Nth sorted row is
/// assigned @c chan_id = @c start_chan + N. Labels get an "hs{hs_id}-"
/// prefix.
///
/// @param filepath   Path to the .cmp file.
/// @param start_chan 1-based channel to assign the first sorted entry.
///                   Typical values: 1 (single headstage), 129 (second
///                   headstage of 128 channels), etc.
/// @param hs_id      Headstage identifier used to prefix labels. The final
///                   label is "hs{hs_id}-{original_label}". Pass 0 (the
///                   default) to leave labels un-prefixed.
/// @return Map of chan_id → CmpEntry on success, or error message.
cbutil::Result<CmpEntries> parseCmpFile(
    const std::string& filepath,
    uint32_t start_chan = 1,
    uint32_t hs_id = 0);

} // namespace cbsdk

#endif // CBSDK_CMP_PARSER_H
