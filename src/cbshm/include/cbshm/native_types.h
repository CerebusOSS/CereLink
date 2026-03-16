///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   native_types.h
/// @author CereLink Development Team
/// @date   2026-02-08
///
/// @brief  Native-mode shared memory structure definitions
///
/// This file defines shared memory structures sized for a single instrument (284 channels)
/// rather than Central's 4-instrument layout (848 channels, ~1.2 GB).
///
/// Native mode uses per-device segments named "cbshm_{device}_{segment}" and is
/// right-sized for the common case of one device per client.
///
/// Key differences from Central layout:
/// - Config buffer: single instrument (scalar procinfo, adaptinfo, etc.)
/// - Receive buffer: reuses cbReceiveBuffer from receive_buffer.h (256 FE channels)
/// - Transmit buffers: slots sized for cbPKT_MAX_SIZE (1024 bytes) not cbCER_UDP_SIZE_MAX
/// - Spike cache: 272 analog channels (not 832)
/// - PC Status: single instrument arrays (not [4])
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_NATIVE_TYPES_H
#define CBSHM_NATIVE_TYPES_H

#include <cbproto/cbproto.h>
#include <cbproto/config.h>       // For cbMAXBANKS
#include <cbshm/central_types.h>  // For CentralSpikeCache reuse
#include <cstdint>

#pragma pack(push, 1)

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Native Mode Constants
/// @{

/// Native mode supports a single instrument with cbMAXPROCS=1 channel counts
constexpr uint32_t NATIVE_NUM_FE_CHANS = cbNUM_FE_CHANS;             // 256
constexpr uint32_t NATIVE_NUM_ANALOG_CHANS = cbNUM_ANALOG_CHANS;     // 272
constexpr uint32_t NATIVE_MAXCHANS = cbMAXCHANS;                     // 284
constexpr uint32_t NATIVE_MAXGROUPS = 8;
constexpr uint32_t NATIVE_MAXFILTS = 32;
constexpr uint32_t NATIVE_MAXBANKS = cbMAXBANKS;

/// Receive buffer length (same formula as cbproto, using 256 FE channels)
/// This reuses cbRECBUFFLEN from receive_buffer.h
constexpr uint32_t NATIVE_cbRECBUFFLEN = cbRECBUFFLEN;

/// Transmit buffer sizes - slots sized for cbPKT_MAX_SIZE (1024 bytes = 256 uint32_t words)
/// instead of Central's cbCER_UDP_SIZE_MAX (58080 bytes = 14520 uint32_t words)
constexpr uint32_t NATIVE_XMT_SLOT_WORDS = cbPKT_MAX_SIZE / sizeof(uint32_t);  // 256
constexpr uint32_t NATIVE_cbXMT_GLOBAL_BUFFLEN = (NATIVE_XMT_SLOT_WORDS * 5000 + 2);
constexpr uint32_t NATIVE_cbXMT_LOCAL_BUFFLEN = (NATIVE_XMT_SLOT_WORDS * 2000 + 2);

/// Spike cache constants (one cache per native analog channel)
constexpr uint32_t NATIVE_cbPKT_SPKCACHEPKTCNT = CENTRAL_cbPKT_SPKCACHEPKTCNT;  // 400
constexpr uint32_t NATIVE_cbPKT_SPKCACHELINECNT = NATIVE_NUM_ANALOG_CHANS;      // 272

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode configuration buffer (single instrument)
///
/// This structure stores the complete configuration state for a single instrument.
/// It uses scalar fields instead of arrays for per-instrument data, and uses
/// cbMAXCHANS (284) instead of cbCONFIG_MAXCHANS (848) for channel arrays.
///
typedef struct {
    uint32_t version;           ///< Buffer structure version
    uint32_t sysflags;          ///< System-wide flags

    // Single instrument status
    uint32_t instrument_status; ///< Active status for this instrument

    // System configuration
    cbPKT_SYSINFO sysinfo;                                      ///< System information

    // Single-instrument configuration (scalar, not arrays)
    cbPKT_PROCINFO procinfo;                                    ///< Processor info
    cbPKT_BANKINFO bankinfo[NATIVE_MAXBANKS];                   ///< Bank info
    cbPKT_GROUPINFO groupinfo[NATIVE_MAXGROUPS];                ///< Sample group info
    cbPKT_FILTINFO filtinfo[NATIVE_MAXFILTS];                   ///< Filter info
    cbPKT_ADAPTFILTINFO adaptinfo;                              ///< Adaptive filter settings
    cbPKT_REFELECFILTINFO refelecinfo;                          ///< Reference electrode filter
    cbPKT_LNC isLnc;                                            ///< Line noise cancellation

    // Channel configuration (single instrument's channels)
    cbPKT_CHANINFO chaninfo[NATIVE_MAXCHANS];                   ///< Channel configuration

    // Spike sorting configuration (uses native channel counts)
    cbPKT_FS_BASIS asBasis[NATIVE_MAXCHANS];                    ///< PCA basis values
    cbPKT_SS_MODELSET asSortModel[NATIVE_MAXCHANS][cbMAXUNITS + 2]; ///< Spike sorting models
    cbPKT_SS_DETECT pktDetect;                                  ///< Detection parameters
    cbPKT_SS_ARTIF_REJECT pktArtifReject;                       ///< Artifact rejection
    cbPKT_SS_NOISE_BOUNDARY pktNoiseBoundary[NATIVE_MAXCHANS];  ///< Noise boundaries
    cbPKT_SS_STATISTICS pktStatistics;                           ///< Statistics information
    cbPKT_SS_STATUS pktStatus;                                  ///< Spike sorting status

    // N-Trode configuration
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];                ///< N-Trode information

    // Analog output waveform configuration
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER]; ///< Waveform params

    // nPlay file playback configuration
    cbPKT_NPLAY isNPlay;                                        ///< nPlay information

    // Video tracking (NeuroMotive)
    cbVIDEOSOURCE isVideoSource[cbMAXVIDEOSOURCE];              ///< Video source configuration
    cbTRACKOBJ isTrackObj[cbMAXTRACKOBJ];                       ///< Trackable objects

    // File recording status
    cbPKT_FILECFG fileinfo;                                     ///< File recording configuration

    // Application UI configuration
    cbOPTIONTABLE optiontable;                                  ///< Option table
    cbCOLORTABLE colortable;                                    ///< Color table

    // Clock synchronization (written by STANDALONE, read by CLIENT)
    int64_t clock_offset_ns;        ///< device_ns - steady_clock_ns (0 if unknown)
    int64_t clock_uncertainty_ns;   ///< Half-RTT uncertainty in nanoseconds (0 if unknown)
    uint32_t clock_sync_valid;      ///< Non-zero if clock_offset_ns is valid
    uint32_t clock_sync_reserved;   ///< Reserved for alignment

    // Ownership tracking (written by STANDALONE at creation, read by CLIENT for liveness check)
    uint32_t owner_pid;             ///< PID of STANDALONE process that created this segment (0 = unknown)
} NativeConfigBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode transmit buffer (slots sized for cbPKT_MAX_SIZE)
///
struct NativeTransmitBuffer {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[NATIVE_cbXMT_GLOBAL_BUFFLEN];           ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode local transmit buffer (IPC-only packets)
///
struct NativeTransmitBufferLocal {
    uint32_t transmitted;                                   ///< How many packets have been sent
    uint32_t headindex;                                     ///< First empty position (write index)
    uint32_t tailindex;                                     ///< One past last emptied position (read index)
    uint32_t last_valid_index;                              ///< Greatest valid starting index
    uint32_t bufferlen;                                     ///< Number of indices in buffer
    uint32_t buffer[NATIVE_cbXMT_LOCAL_BUFFLEN];            ///< Ring buffer for packet data
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode spike cache (272 analog channels)
///
struct NativeSpikeCache {
    uint32_t chid;                                          ///< Channel ID
    uint32_t pktcnt;                                        ///< Number of packets that can be saved
    uint32_t pktsize;                                       ///< Size of individual packet
    uint32_t head;                                          ///< Where to place next packet (circular)
    uint32_t valid;                                         ///< How many packets since last config
    cbPKT_SPK spkpkt[NATIVE_cbPKT_SPKCACHEPKTCNT];         ///< Circular buffer of cached spikes
};

struct NativeSpikeBuffer {
    uint32_t flags;                                         ///< Status flags
    uint32_t chidmax;                                       ///< Maximum channel ID
    uint32_t linesize;                                      ///< Size of each cache line
    uint32_t spkcount;                                      ///< Total spike count
    NativeSpikeCache cache[NATIVE_cbPKT_SPKCACHELINECNT];   ///< Per-channel spike caches
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode PC status (single instrument)
///
struct NativePCStatus {
    cbPKT_UNIT_SELECTION isSelection;                       ///< Unit selection (single instrument)

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
    NSPStatus m_nNspStatus;                                 ///< NSP status (single instrument)
    uint32_t m_nNumNTrodesPerInstrument;                    ///< NTrode count (single instrument)
    uint32_t m_nGeminiSystem;                               ///< Gemini system flag
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Native-mode receive buffer (reuses cbReceiveBuffer from receive_buffer.h)
///
/// The cbReceiveBuffer struct defined in receive_buffer.h already uses cbNUM_FE_CHANS=256,
/// so it's already native-sized. We use it directly for native mode.
///
/// For convenience, create a type alias:
using NativeReceiveBuffer = cbReceiveBuffer;

} // namespace cbshm

#pragma pack(pop)

#endif // CBSHM_NATIVE_TYPES_H
