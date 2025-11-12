///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   instrument_id.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Type-safe instrument ID with explicit 0-based/1-based conversions
///
/// This type prevents the common indexing bug where instrument IDs are confused between:
/// - 1-based values (cbNSP1 = 1, used in API calls like cbGetProcInfo(cbNSP1, ...))
/// - 0-based indices (packet header instrument field, array indices)
///
/// Ground truth from upstream/cbproto/cbproto.h:
/// - cbNSP1 = 1 (first instrument in 1-based numbering)
/// - cbMAXOPEN = 4 (max number of instruments)
/// - cbMAXPROCS = 1 (processors per instrument)
/// - Packet header instrument field is 0-based (0, 1, 2, 3 for 4 instruments)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_INSTRUMENT_ID_H
#define CBPROTO_INSTRUMENT_ID_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus

namespace cbproto {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief  Type-safe instrument identifier
///
/// Provides explicit conversion methods to prevent 0-based/1-based indexing bugs.
///
/// Usage:
/// @code
///     // From API constant (1-based)
///     InstrumentId id = InstrumentId::fromOneBased(cbNSP1);  // id = 1
///     uint8_t idx = id.toIndex();  // idx = 0 (for array access)
///
///     // From packet header (0-based)
///     InstrumentId id = InstrumentId::fromPacketField(pkt->cbpkt_header.instrument);
///     uint8_t oneBased = id.toOneBased();  // For API calls
///
///     // From array index (0-based)
///     InstrumentId id = InstrumentId::fromIndex(0);
///     uint8_t oneBased = id.toOneBased();  // 1
/// @endcode
///
///////////////////////////////////////////////////////////////////////////////////////////////////
class InstrumentId {
public:
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Construction from different representations
    /// @{

    /// @brief Create from 1-based ID (e.g., cbNSP1 = 1, cbNSP2 = 2)
    /// @param id 1-based instrument ID (1-4 for cbMAXOPEN=4)
    /// @return InstrumentId instance
    static InstrumentId fromOneBased(uint8_t id) {
        return InstrumentId(id);
    }

    /// @brief Create from 0-based array index (0-3 for cbMAXOPEN=4)
    /// @param idx 0-based array index
    /// @return InstrumentId instance
    static InstrumentId fromIndex(uint8_t idx) {
        return InstrumentId(idx + 1);
    }

    /// @brief Create from packet header instrument field (0-based)
    /// @param pkt_inst Value from cbPKT_HEADER.instrument field
    /// @return InstrumentId instance
    static InstrumentId fromPacketField(uint8_t pkt_inst) {
        return InstrumentId(pkt_inst + 1);
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Conversion to different representations
    /// @{

    /// @brief Convert to 1-based ID for API calls
    /// @return 1-based instrument ID (1-4)
    uint8_t toOneBased() const {
        return m_id;
    }

    /// @brief Convert to 0-based array index
    /// @return 0-based index (0-3)
    uint8_t toIndex() const {
        return m_id - 1;
    }

    /// @brief Convert to packet header instrument field value
    /// @return 0-based value for packet header (0-3)
    uint8_t toPacketField() const {
        return m_id - 1;
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Validation
    /// @{

    /// @brief Check if ID is valid (1 <= id <= cbMAXOPEN)
    /// @return true if valid, false otherwise
    bool isValid() const {
        return m_id >= 1 && m_id <= 4;  // cbMAXOPEN = 4
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    /// @name Comparison operators
    /// @{

    bool operator==(const InstrumentId& other) const { return m_id == other.m_id; }
    bool operator!=(const InstrumentId& other) const { return m_id != other.m_id; }
    bool operator<(const InstrumentId& other) const { return m_id < other.m_id; }
    bool operator<=(const InstrumentId& other) const { return m_id <= other.m_id; }
    bool operator>(const InstrumentId& other) const { return m_id > other.m_id; }
    bool operator>=(const InstrumentId& other) const { return m_id >= other.m_id; }

    /// @}

private:
    /// @brief Private constructor - use static factory methods
    /// @param id 1-based instrument ID
    explicit InstrumentId(uint8_t id) : m_id(id) {}

    uint8_t m_id;  ///< Stored as 1-based ID internally
};

} // namespace cbproto

#endif // __cplusplus

#endif // CBPROTO_INSTRUMENT_ID_H
