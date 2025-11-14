///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   config_buffer.h
/// @author CereLink Development Team
/// @date   2025-11-14
///
/// @brief  Device configuration buffer structure
///
/// This file defines the configuration buffer structure used to store device state.
/// It supports up to 4 instruments (NSPs) to match Central's capabilities.
///
/// This structure is shared between:
/// - cbdev: For standalone device sessions (stores config internally)
/// - cbshmem: For shared memory (multiple clients access same config)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_CONFIG_BUFFER_H
#define CBPROTO_CONFIG_BUFFER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Configuration Buffer Constants
/// @{

/// Maximum number of instruments (NSPs) supported
/// This matches Central's multi-instrument capability (up to 4 NSPs)
#define cbCONFIG_MAXPROCS 4

/// Maximum number of sample rate groups per instrument
#define cbCONFIG_MAXGROUPS 8

/// Maximum number of digital filters per instrument
#define cbCONFIG_MAXFILTS 32

/// Number of front-end channels per instrument (Gemini = 768)
#define cbCONFIG_NUM_FE_CHANS 768

/// Number of analog input channels per instrument
#define cbCONFIG_NUM_ANAIN_CHANS (16 * cbCONFIG_MAXPROCS)

/// Total analog channels
#define cbCONFIG_NUM_ANALOG_CHANS (cbCONFIG_NUM_FE_CHANS + cbCONFIG_NUM_ANAIN_CHANS)

/// Number of analog output channels
#define cbCONFIG_NUM_ANAOUT_CHANS (4 * cbCONFIG_MAXPROCS)

/// Number of audio output channels
#define cbCONFIG_NUM_AUDOUT_CHANS (2 * cbCONFIG_MAXPROCS)

/// Total analog output channels
#define cbCONFIG_NUM_ANALOGOUT_CHANS (cbCONFIG_NUM_ANAOUT_CHANS + cbCONFIG_NUM_AUDOUT_CHANS)

/// Number of digital input channels
#define cbCONFIG_NUM_DIGIN_CHANS (1 * cbCONFIG_MAXPROCS)

/// Number of serial channels
#define cbCONFIG_NUM_SERIAL_CHANS (1 * cbCONFIG_MAXPROCS)

/// Number of digital output channels
#define cbCONFIG_NUM_DIGOUT_CHANS (4 * cbCONFIG_MAXPROCS)

/// Total channels supported
#define cbCONFIG_MAXCHANS (cbCONFIG_NUM_ANALOG_CHANS + cbCONFIG_NUM_ANALOGOUT_CHANS + \
                           cbCONFIG_NUM_DIGIN_CHANS + cbCONFIG_NUM_SERIAL_CHANS + \
                           cbCONFIG_NUM_DIGOUT_CHANS)

/// Channels per bank
#define cbCONFIG_CHAN_PER_BANK 32

/// Number of front-end banks
#define cbCONFIG_NUM_FE_BANKS (cbCONFIG_NUM_FE_CHANS / cbCONFIG_CHAN_PER_BANK)

/// Number of banks per type
#define cbCONFIG_NUM_ANAIN_BANKS 1
#define cbCONFIG_NUM_ANAOUT_BANKS 1
#define cbCONFIG_NUM_AUDOUT_BANKS 1
#define cbCONFIG_NUM_DIGIN_BANKS 1
#define cbCONFIG_NUM_SERIAL_BANKS 1
#define cbCONFIG_NUM_DIGOUT_BANKS 1

/// Total banks per instrument
#define cbCONFIG_MAXBANKS (cbCONFIG_NUM_FE_BANKS + cbCONFIG_NUM_ANAIN_BANKS + \
                           cbCONFIG_NUM_ANAOUT_BANKS + cbCONFIG_NUM_AUDOUT_BANKS + \
                           cbCONFIG_NUM_DIGIN_BANKS + cbCONFIG_NUM_SERIAL_BANKS + \
                           cbCONFIG_NUM_DIGOUT_BANKS)

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Instrument status enumeration
///
/// Used to track which instruments are active in the config buffer
///
typedef enum {
    cbINSTRUMENT_STATUS_INACTIVE = 0x00000000,  ///< Instrument slot is not in use
    cbINSTRUMENT_STATUS_ACTIVE   = 0x00000001,  ///< Instrument is active and has data
} cbInstrumentStatus;

#ifdef __cplusplus
/// C++ type-safe version
enum class InstrumentStatus : uint32_t {
    INACTIVE = cbINSTRUMENT_STATUS_INACTIVE,
    ACTIVE   = cbINSTRUMENT_STATUS_ACTIVE,
};
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Spike Sorting Combined Structure (Config Buffer Version)
///
/// This structure aggregates all spike sorting configuration across all channels.
/// This is NOT a packet structure - it's a config storage structure used by Central
/// to maintain the complete spike sorting state.
///
/// IMPORTANT: Uses cbCONFIG_MAXCHANS (multi-device, 848 channels) not cbMAXCHANS (single-device, 256 channels)
///
/// Ground truth from upstream/cbhwlib/cbhwlib.h lines 1012-1025
/// Modified to use config buffer channel counts
///
typedef struct {
    // ***** THESE MUST BE 1ST IN THE STRUCTURE WITH MODELSET LAST OF THESE ***
    // ***** SEE WriteCCFNoPrompt() ***
    cbPKT_FS_BASIS          asBasis[cbCONFIG_MAXCHANS];                     ///< All PCA basis values (config buffer size)
    cbPKT_SS_MODELSET       asSortModel[cbCONFIG_MAXCHANS][cbMAXUNITS + 2]; ///< All spike sorting models (config buffer size)

    //////// Spike sorting options (not channel-specific)
    cbPKT_SS_DETECT         pktDetect;                         ///< Detection parameters
    cbPKT_SS_ARTIF_REJECT   pktArtifReject;                   ///< Artifact rejection
    cbPKT_SS_NOISE_BOUNDARY pktNoiseBoundary[cbCONFIG_MAXCHANS]; ///< Noise boundaries (config buffer size)
    cbPKT_SS_STATISTICS     pktStatistics;                    ///< Statistics information
    cbPKT_SS_STATUS         pktStatus;                        ///< Spike sorting status

} cbSPIKE_SORTING;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Device configuration buffer
///
/// This structure stores the complete configuration state for up to 4 instruments (NSPs).
/// Configuration packets received from the device update this buffer, providing a queryable
/// "database" of the current system configuration.
///
/// Memory layout matches Central's cbCFGBUFF for compatibility.
///
/// Size: ~4MB (large structure, typically heap-allocated or in shared memory)
///
typedef struct {
    uint32_t version;           ///< Buffer structure version
    uint32_t sysflags;          ///< System-wide flags

    // Instrument status (multi-instrument tracking)
    uint32_t instrument_status[cbCONFIG_MAXPROCS];  ///< Active status for each instrument

    // System configuration
    cbPKT_SYSINFO sysinfo;                                      ///< System information

    // Per-instrument configuration
    cbPKT_PROCINFO procinfo[cbCONFIG_MAXPROCS];                 ///< Processor info
    cbPKT_BANKINFO bankinfo[cbCONFIG_MAXPROCS][cbCONFIG_MAXBANKS];   ///< Bank info
    cbPKT_GROUPINFO groupinfo[cbCONFIG_MAXPROCS][cbCONFIG_MAXGROUPS]; ///< Sample group info
    cbPKT_FILTINFO filtinfo[cbCONFIG_MAXPROCS][cbCONFIG_MAXFILTS];    ///< Filter info
    cbPKT_ADAPTFILTINFO adaptinfo[cbCONFIG_MAXPROCS];           ///< Adaptive filter settings
    cbPKT_REFELECFILTINFO refelecinfo[cbCONFIG_MAXPROCS];       ///< Reference electrode filter
    cbPKT_LNC isLnc[cbCONFIG_MAXPROCS];                         ///< Line noise cancellation

    // Channel configuration (shared across all instruments)
    cbPKT_CHANINFO chaninfo[cbCONFIG_MAXCHANS];                 ///< Channel configuration

    // Spike sorting configuration
    cbSPIKE_SORTING isSortingOptions;                           ///< Spike sorting parameters

    // N-Trode configuration (stereotrode, tetrode, etc.)
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

    // Central application UI configuration (option/color tables)
    cbOPTIONTABLE optiontable;                                  ///< Option table (32 values)
    cbCOLORTABLE colortable;                                    ///< Color table (96 values)

    // Note: hwndCentral (HANDLE) is omitted - platform-specific and only used by Central
} cbConfigBuffer;

#ifdef __cplusplus
}
#endif

#endif // CBPROTO_CONFIG_BUFFER_H
