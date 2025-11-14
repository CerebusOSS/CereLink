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

// Include packet structure definitions from cbproto
#include <cbproto/cbproto.h>

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
/// @name Buffer Size Constants (must be defined before structures)
/// @{

/// Max UDP packet size (from Central)
constexpr uint32_t CENTRAL_cbCER_UDP_SIZE_MAX = 58080;

/// Transmit buffer sizes (Central-compatible)
constexpr uint32_t CENTRAL_cbXMT_GLOBAL_BUFFLEN = ((CENTRAL_cbCER_UDP_SIZE_MAX / 4) * 5000 + 2);  ///< Room for 5000 packet-sized slots
constexpr uint32_t CENTRAL_cbXMT_LOCAL_BUFFLEN = ((CENTRAL_cbCER_UDP_SIZE_MAX / 4) * 2000 + 2);   ///< Room for 2000 packet-sized slots

/// Spike cache constants
constexpr uint32_t CENTRAL_cbPKT_SPKCACHEPKTCNT = 400;                          ///< Packets per channel cache
constexpr uint32_t CENTRAL_cbPKT_SPKCACHELINECNT = CENTRAL_cbNUM_ANALOG_CHANS;  ///< One cache per analog channel

/// Receive buffer size
constexpr uint32_t CENTRAL_cbRECBUFFLEN = CENTRAL_cbNUM_FE_CHANS * 65536 * 4 - 1;

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
    uint32_t buffer[CENTRAL_cbXMT_GLOBAL_BUFFLEN];          ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Central-compatible configuration buffer
///
/// CRITICAL: This structure MUST match Central's cbCFGBUFF layout exactly!
/// All arrays are sized using CENTRAL_* constants (cbMAXPROCS=4, etc.)
///
/// This structure now includes all major fields from upstream cbCFGBUFF except hwndCentral
/// (which is platform/bitness specific and only used by Central's UI).
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

    // Adaptive and reference electrode filtering (per-instrument)
    cbPKT_ADAPTFILTINFO adaptinfo[CENTRAL_cbMAXPROCS];                  ///< Adaptive filter settings
    cbPKT_REFELECFILTINFO refelecinfo[CENTRAL_cbMAXPROCS];              ///< Reference electrode filter settings

    // Spike sorting configuration
    cbSPIKE_SORTING isSortingOptions;                                   ///< Spike sorting parameters

    // Line noise cancellation (per-instrument)
    cbPKT_LNC isLnc[CENTRAL_cbMAXPROCS];                                ///< Line noise cancellation settings

    // File recording status
    cbPKT_FILECFG fileinfo;                                             ///< File recording configuration

    // N-Trode configuration (stereotrode, tetrode, etc.)
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];                        ///< N-Trode information

    // Analog output waveform configuration
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER]; ///< Waveform parameters

    // nPlay file playback configuration
    cbPKT_NPLAY isNPlay;                                                ///< nPlay information

    // Video tracking (NeuroMotive)
    cbVIDEOSOURCE isVideoSource[cbMAXVIDEOSOURCE];                      ///< Video source configuration
    cbTRACKOBJ isTrackObj[cbMAXTRACKOBJ];                               ///< Trackable objects

    // Central application UI configuration
    cbOPTIONTABLE optiontable;                                          ///< Option table (32 32-bit values)
    cbCOLORTABLE colortable;                                            ///< Color table (96 32-bit values)

    // Note: hwndCentral (HANDLE) is omitted - it's platform/bitness specific and only used by Central
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
    uint32_t buffer[CENTRAL_cbXMT_LOCAL_BUFFLEN];           ///< Ring buffer for packet data
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
    cbPKT_SPK spkpkt[CENTRAL_cbPKT_SPKCACHEPKTCNT];        ///< Circular buffer of cached spikes
};

struct CentralSpikeBuffer {
    uint32_t flags;                                         ///< Status flags
    uint32_t chidmax;                                       ///< Maximum channel ID
    uint32_t linesize;                                      ///< Size of each cache line
    uint32_t spkcount;                                      ///< Total spike count
    CentralSpikeCache cache[CENTRAL_cbPKT_SPKCACHELINECNT]; ///< Per-channel spike caches
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
    cbPKT_UNIT_SELECTION isSelection[CENTRAL_cbMAXPROCS];   ///< Unit selection per instrument

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
    NSPStatus m_nNspStatus[CENTRAL_cbMAXPROCS];             ///< NSP status per instrument
    uint32_t m_nNumNTrodesPerInstrument[CENTRAL_cbMAXPROCS];///< NTrode count per instrument
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
    uint32_t buffer[CENTRAL_cbRECBUFFLEN];      ///< Packet buffer
};

} // namespace cbshmem

#pragma pack(pop)

#endif // CBSHMEM_CENTRAL_TYPES_H
