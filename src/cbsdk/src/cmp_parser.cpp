///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.cpp
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cmp_parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace cbsdk {

namespace {

struct RawEntry {
    int32_t col;
    int32_t row;
    int32_t bank_idx;   ///< 1-based (A=1, B=2, …)
    int32_t electrode;  ///< 1-based within bank
    std::string label;
};

} // namespace

cbutil::Result<CmpEntries> parseCmpFile(
    const std::string& filepath,
    uint32_t start_chan,
    uint32_t hs_id) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return cbutil::Result<CmpEntries>::error("Failed to open CMP file: " + filepath);
    }

    std::vector<RawEntry> raw;
    bool found_description = false;
    std::string line;

    while (std::getline(file, line)) {
        // Strip trailing whitespace (including \r on Windows line endings)
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty()) continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;

        // First non-comment line is the description — skip it
        if (!found_description) {
            found_description = true;
            continue;
        }

        // Data line: col row bank electrode [label]
        std::istringstream iss(line);
        int32_t col = 0, row = 0, electrode = 0;
        std::string bank_str;
        if (!(iss >> col >> row >> bank_str >> electrode)) continue;
        if (bank_str.empty() || !std::isalpha(static_cast<unsigned char>(bank_str[0]))) continue;

        int32_t bank_idx = std::toupper(static_cast<unsigned char>(bank_str[0])) - 'A' + 1;

        std::string label;
        iss >> label;  // optional

        raw.push_back({col, row, bank_idx, electrode, std::move(label)});
    }

    if (raw.empty()) {
        return cbutil::Result<CmpEntries>::error("No valid entries found in CMP file: " + filepath);
    }

    // Rows are not guaranteed to be in channel order. Sort so the Nth
    // sorted entry maps to (start_chan + N).
    std::sort(raw.begin(), raw.end(), [](const RawEntry& a, const RawEntry& b) {
        if (a.bank_idx != b.bank_idx) return a.bank_idx < b.bank_idx;
        return a.electrode < b.electrode;
    });

    // hs_id == 0 means "single headstage, no prefix needed".
    std::string prefix = (hs_id == 0)
        ? std::string{}
        : "hs" + std::to_string(hs_id) + "-";

    CmpEntries entries;
    entries.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const auto& r = raw[i];
        uint32_t chan_id = start_chan + static_cast<uint32_t>(i);
        CmpEntry entry;
        entry.position = {r.col, r.row, r.bank_idx, r.electrode};
        entry.label = prefix + r.label;
        entries.emplace(chan_id, std::move(entry));
    }

    return cbutil::Result<CmpEntries>::ok(std::move(entries));
}

} // namespace cbsdk
