///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.h
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///
/// CMP files define electrode positions for a single headstage. Format:
///   - Lines starting with // are comments
///   - First non-comment line is a description string
///   - An optional column-header comment immediately before the data names the
///     columns, e.g. "//col row bank elec label" or "//col row bank elec size
///     label". When present it is the ground truth for column order; column
///     names are matched case-insensitively (col/c, row/r, bank/b, elec/e,
///     size/s, label/l), so the order is not assumed. With no header the
///     legacy positional order "col row bank electrode [label]" is used.
///   - Data rows hold:
///     - col: column index or coordinate (left to right)
///     - row: row index or coordinate (bottom to top)
///     - bank: letter A-H (32 electrodes per bank within the headstage)
///     - electrode: 1-based electrode within bank (1-32)
///     - size: electrode size, same units as col/row (optional)
///     - label: free-form label (optional)
///
/// Units: when no size column is supplied and the col/row values form a
/// unit-spaced index grid (every non-zero delta among the distinct col/row
/// values is exactly 1 — the manufacturer default), they are interpreted as
/// electrode indices: size becomes 1 and x/y/size are scaled by the 400 µm
/// Utah-array electrode pitch. Otherwise (a size column is present, or the
/// spacing is non-uniform) col/row/size are taken at face value.
///
/// Each parsed row carries the electrode geometry plus the device (bank, term)
/// it belongs to. Entries are matched to live channels by (bank, term) — read
/// from the device's own chaninfo — rather than by an ordinal channel ID, so a
/// CMP that is missing channels still lands on the right ones. @c start_chan
/// selects which physical banks the CMP targets: the CMP's bank letters are
/// offset by @c start_chan/32 banks. e.g. a @c start_chan of 129 (= 4 banks)
/// maps CMP bank A onto device bank E, bank C onto bank G, and so on. This is
/// how multiple CMPs (one per headstage) land on disjoint banks of one device.
///
/// @c hs_id identifies the headstage and is stored in each entry's
/// @c headstage field (→ position[3]). Labels are taken verbatim from the CMP
/// file — even though labels are reused across CMP files (e.g. "elec1-1" in
/// every headstage's file), the stored headstage id disambiguates them, so no
/// "hs{hs_id}-" label prefix is applied.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_CMP_PARSER_H
#define CBSDK_CMP_PARSER_H

#include <cbutil/result.h>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace cbsdk {

/// One parsed CMP entry. The geometry fields (@c x, @c y, @c size,
/// @c headstage) populate cbPKT_CHANINFO.position[0..3]; the device
/// (@c bank, @c term) are the keys used to find which channel that is.
struct CmpEntry {
    int32_t x = 0;         ///< electrode x (µm for index grids, else face value) → position[0]
    int32_t y = 0;         ///< electrode y (µm for index grids, else face value) → position[1]
    int32_t size = 0;      ///< electrode size, same units as x/y (0 = unspecified) → position[2]
    int32_t headstage = 0; ///< 1-based headstage id (== hs_id; 0 = none) → position[3]
    int32_t bank = 0;      ///< 1-based device bank (CMP bank + start_chan/32); match key
    int32_t term = 0;      ///< 1-based terminal within bank (CMP electrode); match key
    std::string label;     ///< verbatim label (e.g. "chan12")
};

/// Encode a (bank, term) pair into a single CmpEntries lookup key.
/// @c bank is the 1-based device bank, @c term the 1-based terminal within
/// it. Collision-free for the valid hardware ranges (bank ≤ 8, term ≤ 32).
inline uint32_t cmpKey(const int32_t bank, const int32_t term) {
    return (static_cast<uint32_t>(bank) << 8) | (static_cast<uint32_t>(term) & 0xFFu);
}

/// Map from cmpKey(bank, term) to the parsed entry for that device terminal.
using CmpEntries = std::unordered_map<uint32_t, CmpEntry>;

/// Parse a CMP file into entries keyed by device (bank, term).
///
/// Columns come from the header comment immediately before the data when
/// present (matched by name, any order), else the legacy "col row bank
/// electrode [label]" order. col/row/size are scaled from electrode indices to
/// µm for a unit-spaced index grid with no size column, and taken at face value
/// otherwise (see the file header for details).
///
/// Each row's CMP bank letter is offset by @c start_chan/32 banks to produce
/// the target device bank; the electrode becomes the terminal. Callers join
/// these against live chaninfo by (bank, term).
///
/// @param filepath   Path to the .cmp file.
/// @param start_chan 1-based channel selecting the target banks: the CMP is
///                   shifted by @c start_chan/32 banks. Typical values: 1
///                   (banks A…, single headstage), 129 (banks E…, second
///                   headstage of 128 channels), etc.
/// @param hs_id      Headstage identifier stored in each entry's @c headstage
///                   field (→ position[3]; 0 = none). Does not affect labels.
/// @return Map of cmpKey(bank, term) → CmpEntry on success, or error message.
cbutil::Result<CmpEntries> parseCmpFile(
    const std::string& filepath,
    uint32_t start_chan = 1,
    uint32_t hs_id = 0);

} // namespace cbsdk

#endif // CBSDK_CMP_PARSER_H
