///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v3_11.h
/// @author Caden Shmookler
/// @date   2026-05-18
///
/// @brief  Central-compatible shared memory structure definitions
///
/// This file defines the shared memory structures using Central's constants to
/// ensure compatibility with Central when it creates shared memory.
///
/// CRITICAL: These structures MUST match Central's cbhwlib.h exactly!
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TYPES_V3_11_H
#define CBSHM_CENTRAL_TYPES_V3_11_H

#include <cstdint>

// Include InstrumentId from protocol module
#include <cbproto/instrument_id.h>

// Ensure tight packing for shared memory structures
#pragma pack(push, 1)

namespace cbshm {

namespace central_v3_11 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Central Constants
/// @{

// These MUST match Central's constants
constexpr uint32_t cbMAXPROCS = 1;          ///< Central supports up to 1 NSPs
constexpr uint32_t cbNUM_FE_CHANS = 256;    ///< Central supports 256 FE channels
constexpr uint32_t cbMAXGROUPS = 8;         ///< Sample rate groups
constexpr uint32_t cbMAXFILTS = 32;         ///< Digital filters
constexpr uint32_t cbMAXVIDEOSOURCE = 1;
constexpr uint32_t cbMAXTRACKOBJ = 20;
constexpr uint32_t cbMAXHOOPS = 4;
constexpr uint32_t cbMAX_AOUT_TRIGGER = 5;

// Channel counts
constexpr uint32_t cbNUM_ANAIN_CHANS = 16 * cbMAXPROCS;
constexpr uint32_t cbNUM_ANALOG_CHANS = cbNUM_FE_CHANS + cbNUM_ANAIN_CHANS;
constexpr uint32_t cbNUM_ANAOUT_CHANS = 4 * cbMAXPROCS;
constexpr uint32_t cbNUM_AUDOUT_CHANS = 2 * cbMAXPROCS;
constexpr uint32_t cbNUM_ANALOGOUT_CHANS = cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS;
constexpr uint32_t cbNUM_DIGIN_CHANS = 1 * cbMAXPROCS;
constexpr uint32_t cbNUM_SERIAL_CHANS = 1 * cbMAXPROCS;
constexpr uint32_t cbNUM_DIGOUT_CHANS = 4 * cbMAXPROCS;

// Total channels
constexpr uint32_t cbMAXCHANS = (cbNUM_ANALOG_CHANS + cbNUM_ANALOGOUT_CHANS +
                                          cbNUM_DIGIN_CHANS + cbNUM_SERIAL_CHANS +
                                          cbNUM_DIGOUT_CHANS);

// Bank definitions
constexpr uint32_t cbCHAN_PER_BANK = 32;
constexpr uint32_t cbNUM_FE_BANKS = cbNUM_FE_CHANS / cbCHAN_PER_BANK;
constexpr uint32_t cbNUM_ANAIN_BANKS = 1;
constexpr uint32_t cbNUM_ANAOUT_BANKS = 1;
constexpr uint32_t cbNUM_AUDOUT_BANKS = 1;
constexpr uint32_t cbNUM_DIGIN_BANKS = 1;
constexpr uint32_t cbNUM_SERIAL_BANKS = 1;
constexpr uint32_t cbNUM_DIGOUT_BANKS = 1;

constexpr uint32_t cbMAXBANKS = (cbNUM_FE_BANKS + cbNUM_ANAIN_BANKS +
                                          cbNUM_ANAOUT_BANKS + cbNUM_AUDOUT_BANKS +
                                          cbNUM_DIGIN_BANKS + cbNUM_SERIAL_BANKS +
                                          cbNUM_DIGOUT_BANKS);

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Buffer Size Constants (must be defined before structures)
/// @{

/// Max UDP packet size (from Central)
constexpr uint32_t cbCER_UDP_SIZE_MAX = 1452;

/// Transmit buffer sizes (Central-compatible)
constexpr uint32_t cbXMT_GLOBAL_BUFFLEN = ((cbCER_UDP_SIZE_MAX / 4) * 5000 + 2);  ///< Room for 5000 packet-sized slots
constexpr uint32_t cbXMT_LOCAL_BUFFLEN = ((cbCER_UDP_SIZE_MAX / 4) * 2000 + 2);   ///< Room for 2000 packet-sized slots

/// N-Trode count
constexpr uint32_t cbMAXNTRODES = cbNUM_ANALOG_CHANS / 2;  ///< = 136

/// Analog output gain channels (Central's multi-instrument count)
constexpr uint32_t AOUT_NUM_GAIN_CHANS = cbNUM_ANAOUT_CHANS + cbNUM_AUDOUT_CHANS;  ///< = 6

/// Spike cache constants
constexpr uint32_t cbPKT_SPKCACHEPKTCNT = 400;                          ///< Packets per channel cache
constexpr uint32_t cbPKT_SPKCACHELINECNT = cbNUM_ANALOG_CHANS;  ///< One cache per channel

/// Receive buffer size
constexpr uint32_t cbRECBUFFLEN = cbNUM_FE_CHANS * 32768 * 4;

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
/// @brief Central's actual binary layout (CentralLegacyCFGBUFF)
///
/// This struct matches Central's cbCFGBUFF field order EXACTLY (from cbhwlib.h).
/// It is NOT the same as CereLink's cbConfigBuffer (which reorders fields and adds
/// instrument_status). This struct is used in CENTRAL_COMPAT mode to read Central's
/// shared memory as a CLIENT.
///
/// Key differences from CereLink's cbConfigBuffer:
/// - optiontable/colortable: 3rd/4th fields here (after sysflags), last fields in CereLink
/// - instrument_status: absent here (Central has no such concept)
/// - isLnc: after isWaveform here, before chaninfo in CereLink
/// - hwndCentral: omitted (at end, variable size, not needed)
///
struct CentralLegacyCFGBUFF {
    uint32_t          version;
    uint32_t          sysflags;
    cbOPTIONTABLE     optiontable;
    cbCOLORTABLE      colortable;
    cbPKT_SYSINFO     sysinfo;
    cbPKT_PROCINFO    procinfo[cbMAXPROCS];
    cbPKT_BANKINFO    bankinfo[cbMAXPROCS][cbMAXBANKS];
    cbPKT_GROUPINFO   groupinfo[cbMAXPROCS][cbMAXGROUPS];
    cbPKT_FILTINFO    filtinfo[cbMAXPROCS][cbMAXFILTS];
    cbPKT_ADAPTFILTINFO adaptinfo[cbMAXPROCS];
    cbPKT_REFELECFILTINFO refelecinfo[cbMAXPROCS];
    cbPKT_CHANINFO    chaninfo[cbMAXCHANS];
    cbSPIKE_SORTING   isSortingOptions;
    cbPKT_NTRODEINFO  isNTrodeInfo[cbMAXNTRODES];
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_LNC         isLnc[cbMAXPROCS];
    cbPKT_NPLAY       isNPlay;
    cbVIDEOSOURCE     isVideoSource[cbMAXVIDEOSOURCE];
    cbTRACKOBJ        isTrackObj[cbMAXTRACKOBJ];
    cbPKT_FILECFG     fileinfo;
    // hwndCentral omitted (at end, variable size, not needed by CereLink)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Transmit buffer for outgoing packets (Global - sent to device)
///
/// Ring buffer for packets waiting to be transmitted to device via UDP.
/// Buffer stores raw packet data as uint32_t words (Central's format).
///
/// This is stored in a separate shared memory segment (not embedded in config buffer)
/// to match Central's architecture.
///
struct CentralTransmitBuffer {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[cbXMT_GLOBAL_BUFFLEN];          ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Local transmit buffer (IPC-only packets)
///
/// Smaller than Global buffer, used for cbSendLoopbackPacket() - packets meant only for
/// local processes, not sent to device.
///
struct CentralTransmitBufferLocal {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[cbXMT_LOCAL_BUFFLEN];           ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Spike cache buffer
///
/// Caches recent spike packets for each channel to allow quick access without
/// scanning the entire receive buffer.
///
struct CentralSpikeCache {
    uint32_t chid;                                          ///< Channel ID
    uint32_t pktcnt;                                        ///< Number of packets that can be saved
    uint32_t pktsize;                                       ///< Size of individual packet
    uint32_t head;                                          ///< Where to place next packet (circular)
    uint32_t valid;                                         ///< How many packets since last config
    cbPKT_SPK spkpkt[cbPKT_SPKCACHEPKTCNT];        ///< Circular buffer of cached spikes
};

struct CentralSpikeBuffer {
    uint32_t flags;                                         ///< Status flags
    uint32_t chidmax;                                       ///< Maximum channel ID
    uint32_t linesize;                                      ///< Size of each cache line
    uint32_t spkcount;                                      ///< Total spike count
    CentralSpikeCache cache[cbPKT_SPKCACHELINECNT]; ///< Per-channel spike caches
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief PC Status buffer (flattened from cbPcStatus class)
///
/// IMPROVEMENT: Flattened to C struct for ABI stability and cross-compiler compatibility.
/// Central uses a C++ class which is fragile across different build environments.
///
enum class NSPStatus : uint32_t {
    NSP_INIT = 0,
    NSP_NOIPADDR = 1,
    NSP_NOREPLY = 2,
    NSP_FOUND = 3,
    NSP_INVALID = 4
};

struct CentralPCStatus {
    // Public data
    cbPKT_UNIT_SELECTION isSelection[cbMAXPROCS];   ///< Unit selection per instrument

    // Status fields (was private in cbPcStatus)
    int32_t  m_iBlockRecording;                             ///< Recording block counter
    uint32_t m_nPCStatusFlags;                              ///< PC status flags
    uint32_t m_nNumFEChans;                                 ///< Number of FE channels
    uint32_t m_nNumAnainChans;                              ///< Number of analog input channels
    uint32_t m_nNumAnalogChans;                             ///< Number of analog channels
    uint32_t m_nNumAoutChans;                               ///< Number of analog output channels
    uint32_t m_nNumAudioChans;                              ///< Number of audio channels
    uint32_t m_nNumAnalogoutChans;                          ///< Number of analog output channels
    uint32_t m_nNumDiginChans;                              ///< Number of digital input channels
    uint32_t m_nNumSerialChans;                             ///< Number of serial channels
    uint32_t m_nNumDigoutChans;                             ///< Number of digital output channels
    uint32_t m_nNumTotalChans;                              ///< Total channel count
    NSPStatus m_nNspStatus[cbMAXPROCS];             ///< NSP status per instrument
    uint32_t m_nNumNTrodesPerInstrument[cbMAXPROCS];///< NTrode count per instrument
    uint32_t m_nGeminiSystem;                               ///< Gemini system flag
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Receive buffer for incoming packets (simplified for Phase 2)
///
struct CentralReceiveBuffer {
    uint32_t received;                          ///< Number of packets received
    PROCTIME lasttime;                          ///< Last timestamp
    uint32_t headwrap;                          ///< Head wrap counter
    uint32_t headindex;                         ///< Current head index
    uint32_t buffer[cbRECBUFFLEN];      ///< Packet buffer
};

} // namespace central_v3_11

} // namespace cbshm

#pragma pack(pop)

#endif // CBSHMEM_CENTRAL_TYPES_V3_11_H
