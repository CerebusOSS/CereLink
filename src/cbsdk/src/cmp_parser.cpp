///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cmp_parser.cpp
/// @brief  Parser for Blackrock .cmp (channel mapping) files
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cmp_parser.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

namespace cbsdk {

namespace {

/// Blackrock Utah arrays have 400 µm inter-electrode spacing. Default index
/// grids (unit-spaced col/row indices, no explicit size) are scaled by this to
/// convert electrode indices to micrometers.
constexpr int32_t kElectrodePitchUm = 400;

/// A column kind in a CMP data row.
enum class Column { Col, Row, Bank, Elec, Size, Label, Unknown };

/// Classify a header token (case-insensitive, punctuation-stripped).
Column classifyColumn(std::string tok) {
    for (auto& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!tok.empty() && !std::isalnum(static_cast<unsigned char>(tok.back()))) tok.pop_back();
    while (!tok.empty() && !std::isalnum(static_cast<unsigned char>(tok.front()))) tok.erase(tok.begin());
    if (tok == "col" || tok == "column" || tok == "c" || tok == "x") return Column::Col;
    if (tok == "row" || tok == "r" || tok == "y") return Column::Row;
    if (tok == "bank" || tok == "b") return Column::Bank;
    if (tok == "elec" || tok == "electrode" || tok == "e" || tok == "term" || tok == "terminal")
        return Column::Elec;
    if (tok == "size" || tok == "s" || tok == "sz") return Column::Size;
    if (tok == "label" || tok == "l" || tok == "name") return Column::Label;
    return Column::Unknown;
}

/// Resolve the column order from a candidate header line. Returns true and
/// fills @c columns / @c has_size if the line names all of col/row/bank/elec.
bool columnsFromHeader(const std::string& header, std::vector<Column>& columns, bool& has_size) {
    std::istringstream hs(header);
    std::vector<Column> cols;
    std::string tok;
    while (hs >> tok) cols.push_back(classifyColumn(tok));
    auto has = [&](Column c) { return std::find(cols.begin(), cols.end(), c) != cols.end(); };
    if (has(Column::Col) && has(Column::Row) && has(Column::Bank) && has(Column::Elec)) {
        has_size = has(Column::Size);  // before the move — has() reads cols
        columns = std::move(cols);
        return true;
    }
    return false;
}

struct RawEntry {
    int32_t col;
    int32_t row;
    int32_t bank_idx;   ///< 1-based (A=1, B=2, …)
    int32_t electrode;  ///< 1-based within bank
    int32_t size;       ///< from a size column, else 0
    std::string label;
};

/// Parse one data row per the resolved column order. Returns false (skip row)
/// if any required column (col/row/bank/elec) is missing or unparseable.
bool parseRow(const std::string& line, const std::vector<Column>& columns, RawEntry& out) {
    std::istringstream iss(line);
    std::vector<std::string> toks;
    std::string t;
    while (iss >> t) toks.push_back(std::move(t));

    out = RawEntry{0, 0, 0, 0, 0, ""};
    bool got_col = false, got_row = false, got_bank = false, got_elec = false;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i >= toks.size()) break;  // optional trailing columns simply absent
        const std::string& tk = toks[i];
        try {
            switch (columns[i]) {
                case Column::Col:  out.col = std::stoi(tk); got_col = true; break;
                case Column::Row:  out.row = std::stoi(tk); got_row = true; break;
                case Column::Elec: out.electrode = std::stoi(tk); got_elec = true; break;
                case Column::Size: out.size = std::stoi(tk); break;
                case Column::Bank:
                    if (tk.empty() || !std::isalpha(static_cast<unsigned char>(tk[0]))) return false;
                    out.bank_idx = std::toupper(static_cast<unsigned char>(tk[0])) - 'A' + 1;
                    got_bank = true;
                    break;
                case Column::Label:   out.label = tk; break;
                case Column::Unknown: break;  // consume and ignore
            }
        } catch (...) {
            return false;  // non-numeric in a numeric column
        }
    }
    return got_col && got_row && got_bank && got_elec;
}

/// True if the smallest non-zero delta among the distinct col and row values is
/// 1 — i.e. a unit-indexed electrode grid, as opposed to values already in
/// physical units. Larger deltas are allowed (e.g. the gap between two arrays
/// in a multi-array map), so a single unit step anywhere is enough.
bool isUnitIndexedGrid(const std::vector<RawEntry>& raw) {
    std::set<int32_t> cols, rows;
    for (const auto& r : raw) { cols.insert(r.col); rows.insert(r.row); }
    std::set<int32_t> deltas;
    for (const auto* s : {&cols, &rows}) {
        int32_t prev = 0;
        bool first = true;
        for (int32_t v : *s) {
            if (!first) deltas.insert(v - prev);
            prev = v;
            first = false;
        }
    }
    return deltas.count(1) > 0;
}

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
    bool description_seen = false;
    std::string header_candidate;  // most recent comment line after the description
    std::vector<Column> columns;
    bool columns_resolved = false;
    bool has_size = false;
    std::string line;

    while (std::getline(file, line)) {
        // Strip trailing whitespace (including \r on Windows line endings)
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') {
            // A comment after the description may be the column header.
            if (description_seen) header_candidate = line.substr(2);
            continue;
        }

        // First non-comment line is the description — skip it.
        if (!description_seen) {
            description_seen = true;
            header_candidate.clear();
            continue;
        }

        // First data row: lock in the column order from the header if it names
        // the required columns, else fall back to the legacy positional order.
        if (!columns_resolved) {
            if (header_candidate.empty() || !columnsFromHeader(header_candidate, columns, has_size)) {
                columns = {Column::Col, Column::Row, Column::Bank, Column::Elec, Column::Label};
                has_size = false;
            }
            columns_resolved = true;
        }

        RawEntry entry;
        if (parseRow(line, columns, entry)) raw.push_back(std::move(entry));
    }

    if (raw.empty()) {
        return cbutil::Result<CmpEntries>::error("No valid entries found in CMP file: " + filepath);
    }

    // When no size column is supplied and the col/row values form a unit-indexed
    // grid (some adjacent electrodes are 1 apart), treat them as electrode
    // indices: size is 1 and all of x/y/size scale by the 400 µm electrode
    // pitch. Otherwise take col/row (and any supplied size) at face value.
    const bool scale = !has_size && isUnitIndexedGrid(raw);
    const int32_t factor = scale ? kElectrodePitchUm : 1;

    // start_chan selects the target banks: shift every CMP bank by this many
    // banks (32 terminals each). start_chan 1 → +0 (banks A…), 129 → +4 (E…).
    const int32_t bank_offset = static_cast<int32_t>(start_chan / 32);

    CmpEntries entries;
    entries.reserve(raw.size());
    for (auto& r : raw) {
        CmpEntry entry;
        entry.x = r.col * factor;
        entry.y = r.row * factor;
        entry.size = scale ? kElectrodePitchUm : (has_size ? r.size : 0);
        entry.headstage = static_cast<int32_t>(hs_id);
        entry.bank = r.bank_idx + bank_offset;
        entry.term = r.electrode;
        entry.label = std::move(r.label);
        entries.emplace(cmpKey(entry.bank, entry.term), std::move(entry));
    }

    return cbutil::Result<CmpEntries>::ok(std::move(entries));
}

} // namespace cbsdk
