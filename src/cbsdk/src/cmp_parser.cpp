///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.cpp
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cmp_parser.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace cbsdk {

cbutil::Result<CmpPositionMap> parseCmpFile(const std::string& filepath, uint32_t bank_offset) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return cbutil::Result<CmpPositionMap>::error("Failed to open CMP file: " + filepath);
    }

    CmpPositionMap positions;
    bool found_description = false;
    std::string line;

    while (std::getline(file, line)) {
        // Strip trailing whitespace/tabs
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) continue;

        // Skip comment lines
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;

        // First non-comment line is the description — skip it
        if (!found_description) {
            found_description = true;
            continue;
        }

        // Parse data line: col row bank electrode [label]
        std::istringstream iss(line);
        int32_t col, row;
        std::string bank_str;
        int32_t electrode;

        if (!(iss >> col >> row >> bank_str >> electrode)) {
            continue;  // Skip malformed lines
        }

        // Bank letter to 1-based index: A=1, B=2, ..., H=8
        if (bank_str.empty() || !std::isalpha(static_cast<unsigned char>(bank_str[0]))) {
            continue;  // Skip lines with invalid bank
        }
        uint32_t bank_from_letter = static_cast<uint32_t>(
            std::toupper(static_cast<unsigned char>(bank_str[0])) - 'A' + 1);

        // Apply bank offset for multi-port Hub configurations
        uint32_t absolute_bank = bank_from_letter + bank_offset;

        // Electrode is 1-based in CMP, term is 0-based in chaninfo
        uint32_t term = static_cast<uint32_t>(electrode - 1);

        // Store position: {col, row, bank_from_letter, electrode}
        // We store the original CMP values (not offset bank) in position[2]
        // so the position data reflects the physical array geometry
        uint64_t key = cmpKey(absolute_bank, term);
        positions[key] = {col, row, static_cast<int32_t>(bank_from_letter), electrode};
    }

    if (positions.empty()) {
        return cbutil::Result<CmpPositionMap>::error("No valid entries found in CMP file: " + filepath);
    }

    return cbutil::Result<CmpPositionMap>::ok(std::move(positions));
}

} // namespace cbsdk
