///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   translators.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Translators for Central-compatible shared memory access
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbproto/types.h>
#include <cbproto/config.h>
#include <cbshm/central_types/v4_2.h>
#include <cbshm/central_types/v4_1.h>
#include <cbshm/central_types/v4_0.h>
#include <cbshm/central_types/v3_11.h>

#ifndef CBSHM_CENTRAL_TYPES_TRANSLATORS_H
#define CBSHM_CENTRAL_TYPES_TRANSLATORS_H 

namespace cbshm {

namespace central_v4_2 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg);
::cbSCALING fromLegacy(const cbSCALING& leg);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg);
::cbHOOP fromLegacy(const cbHOOP& leg);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg);
::cbWaveformData fromLegacy(const cbWaveformData& leg);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur);
cbSCALING toLegacy(const ::cbSCALING& cur);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur);
cbHOOP toLegacy(const ::cbHOOP& cur);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur);
cbWaveformData toLegacy(const ::cbWaveformData& cur);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur);

} // namespace central_v4_2

namespace central_v4_1 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg);
::cbSCALING fromLegacy(const cbSCALING& leg);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg);
::cbHOOP fromLegacy(const cbHOOP& leg);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg);
::cbWaveformData fromLegacy(const cbWaveformData& leg);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur);
cbSCALING toLegacy(const ::cbSCALING& cur);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur);
cbHOOP toLegacy(const ::cbHOOP& cur);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur);
cbWaveformData toLegacy(const ::cbWaveformData& cur);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur);

} // namespace central_v4_1

namespace central_v4_0 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg);
::cbSCALING fromLegacy(const cbSCALING& leg);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg);
::cbHOOP fromLegacy(const cbHOOP& leg);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg);
::cbWaveformData fromLegacy(const cbWaveformData& leg);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur);
cbSCALING toLegacy(const ::cbSCALING& cur);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur);
cbHOOP toLegacy(const ::cbHOOP& cur);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur);
cbWaveformData toLegacy(const ::cbWaveformData& cur);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur);

} // namespace central_v4_0

namespace central_v3_11 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg);
::cbSCALING fromLegacy(const cbSCALING& leg);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg);
::cbHOOP fromLegacy(const cbHOOP& leg);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg);
::cbWaveformData fromLegacy(const cbWaveformData& leg);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur);
cbSCALING toLegacy(const ::cbSCALING& cur);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur);
cbHOOP toLegacy(const ::cbHOOP& cur);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur);
cbWaveformData toLegacy(const ::cbWaveformData& cur);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur);

} // namespace central_v3_11

} // namespace cbshm

#endif // CBSHM_CENTRAL_TYPES_TRANSLATORS_H
