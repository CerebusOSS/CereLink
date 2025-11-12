///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_types.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Central-compatible shared memory structure definitions
///
/// This file defines the shared memory structures using Central's constants (cbMAXPROCS=4,
/// cbNUM_FE_CHANS=768) to ensure compatibility with Central when it creates shared memory.
///
/// CRITICAL: These structures MUST match Central's cbhwlib.h exactly!
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHMEM_CENTRAL_TYPES_H
#define CBSHMEM_CENTRAL_TYPES_H

// Include InstrumentId from protocol module
#include <cbproto/instrument_id.h>

// TODO: Phase 3 - Extract all packet types to cbproto
// For now, we need the packet structure definitions from upstream.
// We use a wrapper header to avoid conflicts with cbproto's minimal cbproto.h
#include <cbshmem/upstream_protocol.h>

#include <cstdint>

// Ensure tight packing for shared memory structures
#pragma pack(push, 1)

namespace cbshmem {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Central Constants
/// @{

// These MUST match Central's constants
constexpr uint32_t CENTRAL_cbMAXPROCS = 4;          ///< Central supports up to 4 NSPs
constexpr uint32_t CENTRAL_cbNUM_FE_CHANS = 768;    ///< Central supports 768 FE channels
constexpr uint32_t CENTRAL_cbMAXGROUPS = 8;         ///< Sample rate groups
constexpr uint32_t CENTRAL_cbMAXFILTS = 32;         ///< Digital filters

// Channel counts
constexpr uint32_t CENTRAL_cbNUM_ANAIN_CHANS = 16 * CENTRAL_cbMAXPROCS;
constexpr uint32_t CENTRAL_cbNUM_ANALOG_CHANS = CENTRAL_cbNUM_FE_CHANS + CENTRAL_cbNUM_ANAIN_CHANS;
constexpr uint32_t CENTRAL_cbNUM_ANAOUT_CHANS = 4 * CENTRAL_cbMAXPROCS;
constexpr uint32_t CENTRAL_cbNUM_AUDOUT_CHANS = 2 * CENTRAL_cbMAXPROCS;
constexpr uint32_t CENTRAL_cbNUM_ANALOGOUT_CHANS = CENTRAL_cbNUM_ANAOUT_CHANS + CENTRAL_cbNUM_AUDOUT_CHANS;
constexpr uint32_t CENTRAL_cbNUM_DIGIN_CHANS = 1 * CENTRAL_cbMAXPROCS;
constexpr uint32_t CENTRAL_cbNUM_SERIAL_CHANS = 1 * CENTRAL_cbMAXPROCS;
constexpr uint32_t CENTRAL_cbNUM_DIGOUT_CHANS = 4 * CENTRAL_cbMAXPROCS;

// Total channels
constexpr uint32_t CENTRAL_cbMAXCHANS = (CENTRAL_cbNUM_ANALOG_CHANS + CENTRAL_cbNUM_ANALOGOUT_CHANS +
                                          CENTRAL_cbNUM_DIGIN_CHANS + CENTRAL_cbNUM_SERIAL_CHANS +
                                          CENTRAL_cbNUM_DIGOUT_CHANS);

// Bank definitions
constexpr uint32_t CENTRAL_cbCHAN_PER_BANK = 32;
constexpr uint32_t CENTRAL_cbNUM_FE_BANKS = CENTRAL_cbNUM_FE_CHANS / CENTRAL_cbCHAN_PER_BANK;
constexpr uint32_t CENTRAL_cbNUM_ANAIN_BANKS = 1;
constexpr uint32_t CENTRAL_cbNUM_ANAOUT_BANKS = 1;
constexpr uint32_t CENTRAL_cbNUM_AUDOUT_BANKS = 1;
constexpr uint32_t CENTRAL_cbNUM_DIGIN_BANKS = 1;
constexpr uint32_t CENTRAL_cbNUM_SERIAL_BANKS = 1;
constexpr uint32_t CENTRAL_cbNUM_DIGOUT_BANKS = 1;

constexpr uint32_t CENTRAL_cbMAXBANKS = (CENTRAL_cbNUM_FE_BANKS + CENTRAL_cbNUM_ANAIN_BANKS +
                                          CENTRAL_cbNUM_ANAOUT_BANKS + CENTRAL_cbNUM_AUDOUT_BANKS +
                                          CENTRAL_cbNUM_DIGIN_BANKS + CENTRAL_cbNUM_SERIAL_BANKS +
                                          CENTRAL_cbNUM_DIGOUT_BANKS);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Instrument status flags (bit field)
///
/// Used to track which instruments are active in shared memory
///
enum class InstrumentStatus : uint32_t {
    INACTIVE = 0x00000000,  ///< Instrument slot is not in use
    ACTIVE   = 0x00000001,  ///< Instrument is active and has data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Transmit buffer for outgoing packets (simplified for Phase 2)
///
/// Ring buffer for packets waiting to be transmitted to device.
/// Buffer stores raw packet data as uint32_t words (Central's format).
///
/// NOTE: We use a fixed-size buffer for simplicity. Central uses a variable-length buffer
/// allocated at runtime, but for shared memory cross-platform compatibility, fixed size is easier.
///
constexpr uint32_t CENTRAL_cbXMTBUFFLEN = 8192;  ///< Buffer size in uint32_t words (32KB of packet data)

struct CentralTransmitBuffer {
    uint32_t transmitted;                       ///< How many packets have been sent
    uint32_t headindex;                         ///< First empty position (write index)
    uint32_t tailindex;                         ///< One past last emptied position (read index)
    uint32_t last_valid_index;                  ///< Greatest valid starting index
    uint32_t bufferlen;                         ///< Number of indices in buffer (CENTRAL_cbXMTBUFFLEN)
    uint32_t buffer[CENTRAL_cbXMTBUFFLEN];      ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Central-compatible configuration buffer
///
/// CRITICAL: This structure MUST match Central's cbCFGBUFF layout exactly!
/// All arrays are sized using CENTRAL_* constants (cbMAXPROCS=4, etc.)
///
/// Note: We're using a simplified version for Phase 2. Additional fields from upstream
/// will be added as needed in later phases.
///
struct CentralConfigBuffer {
    uint32_t version;           ///< Buffer structure version
    uint32_t sysflags;          ///< System-wide flags

    // Instrument status (not in upstream, but needed for multi-client tracking)
    uint32_t instrument_status[CENTRAL_cbMAXPROCS];  ///< Active status for each instrument

    // Configuration packets
    cbPKT_SYSINFO  sysinfo;                                             ///< System information
    cbPKT_PROCINFO procinfo[CENTRAL_cbMAXPROCS];                        ///< Processor info (indexed by instrument!)
    cbPKT_BANKINFO bankinfo[CENTRAL_cbMAXPROCS][CENTRAL_cbMAXBANKS];    ///< Bank info
    cbPKT_GROUPINFO groupinfo[CENTRAL_cbMAXPROCS][CENTRAL_cbMAXGROUPS]; ///< Sample group info
    cbPKT_FILTINFO filtinfo[CENTRAL_cbMAXPROCS][CENTRAL_cbMAXFILTS];    ///< Filter info

    // Channel configuration (shared across all instruments)
    cbPKT_CHANINFO chaninfo[CENTRAL_cbMAXCHANS];                        ///< Channel configuration

    // Transmit buffer (for sending packets to device)
    // NOTE: Embedded here for Phase 2 simplicity. Will move to separate shared memory
    // segment in Phase 3 for full Central compatibility.
    CentralTransmitBuffer xmt_buffer;                                   ///< Transmit queue

    // TODO: Add remaining fields from upstream cbCFGBUFF as needed:
    // - cbOPTIONTABLE optiontable
    // - cbCOLORTABLE colortable
    // - cbPKT_ADAPTFILTINFO adaptinfo
    // - cbPKT_REFELECFILTINFO refelecinfo
    // - cbSPIKE_SORTING isSortingOptions
    // - cbPKT_NTRODEINFO isNTrodeInfo
    // - etc.
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Receive buffer for incoming packets (simplified for Phase 2)
///
constexpr uint32_t CENTRAL_cbRECBUFFLEN = CENTRAL_cbNUM_FE_CHANS * 65536 * 4 - 1;

struct CentralReceiveBuffer {
    uint32_t received;                          ///< Number of packets received
    PROCTIME lasttime;                          ///< Last timestamp
    uint32_t headwrap;                          ///< Head wrap counter
    uint32_t headindex;                         ///< Current head index
    uint32_t buffer[CENTRAL_cbRECBUFFLEN];      ///< Packet buffer
};

} // namespace cbshmem

#pragma pack(pop)

#endif // CBSHMEM_CENTRAL_TYPES_H
