///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   config.h
/// @author CereLink Development Team
/// @date   2025-01-21
///
/// @brief  Device configuration structure
///
/// Streamlined device configuration buffer that holds the packets returned by REQCONFIGALL.
/// This is similar in concept to the upstream cbCFGBUFF but simplified:
/// - Single processor (no cbMAXPROCS arrays)
/// - Uses cbMAXCHANS = 256 (not 768)
/// - Only contains configuration packets (no runtime state)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_CONFIG_H
#define CBPROTO_CONFIG_H

#include <cbproto/cbproto.h>

// Constants not yet in types.h but needed for config structure
// TODO: Move these to types.h when updating from upstream
#ifndef cbMAXBANKS
#define cbMAXBANKS  15  ///< cbNUM_FE_BANKS(8) + ANAIN(1) + ANAOUT(1) + AUDOUT(1) + DIGIN(1) + SERIAL(1) + DIGOUT(1)
#endif

namespace cbproto {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Spike sorting configuration
///
/// Groups all spike-sorting related configuration packets together.
/// This matches the upstream cbSPIKE_SORTING structure.
///
struct SpikeSorting {
    // Spike sorting models and basis functions
    // NOTE: These must be first in the structure (see upstream WriteCCFNoPrompt)
    cbPKT_FS_BASIS          basis[cbMAXCHANS];                      ///< PCA basis values per channel
    cbPKT_SS_MODELSET       models[cbMAXCHANS][cbMAXUNITS + 2];     ///< Sorting models/rules per channel

    // Spike sorting parameters
    cbPKT_SS_DETECT         detect;                                 ///< Detection parameters
    cbPKT_SS_ARTIF_REJECT   artifact_reject;                        ///< Artifact rejection parameters
    cbPKT_SS_NOISE_BOUNDARY noise_boundary[cbMAXCHANS];             ///< Noise boundaries per channel
    cbPKT_SS_STATISTICS     statistics;                             ///< Spike statistics
    cbPKT_SS_STATUS         status;                                 ///< Spike sorting status
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Device configuration buffer
///
/// Contains all configuration packets returned by REQCONFIGALL.
/// This is a streamlined version of the upstream cbCFGBUFF:
/// - Single processor (arrays sized for 1, not cbMAXPROCS)
/// - Uses cbMAXCHANS = 256 (the actual hardware channel count)
/// - Focuses only on configuration packets (no UI state, no Central window handles)
///
struct DeviceConfig {
    // System configuration
    cbPKT_SYSINFO           sysinfo;                                ///< System information and capabilities
    cbPKT_PROCINFO          procinfo;                               ///< Processor information (single proc)

    // Channel configuration
    cbPKT_CHANINFO          chaninfo[cbMAXCHANS];                   ///< Channel configuration (256 channels)
    cbPKT_NTRODEINFO        ntrodeinfo[cbMAXNTRODES];               ///< N-trode configuration

    // Signal processing configuration
    cbPKT_GROUPINFO         groupinfo[cbMAXGROUPS];                 ///< Sample group configuration
    cbPKT_FILTINFO          filtinfo[cbMAXFILTS];                   ///< Digital filter configuration
    cbPKT_BANKINFO          bankinfo[cbMAXBANKS];                   ///< Filter bank configuration
    cbPKT_ADAPTFILTINFO     adaptinfo;                              ///< Adaptive filter settings
    cbPKT_REFELECFILTINFO   refelecinfo;                            ///< Reference electrode filtering

    // Spike sorting configuration
    SpikeSorting            spike_sorting;                          ///< All spike sorting parameters

    // Analog output waveform configuration
    cbPKT_AOUT_WAVEFORM     waveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER]; ///< Waveform triggers per analog output

    // Recording configuration
    cbPKT_LNC               lnc;                                    ///< LNC parameters
    cbPKT_FILECFG           fileinfo;                               ///< File recording configuration
};

} // namespace cbproto

#endif // CBPROTO_CONFIG_H
