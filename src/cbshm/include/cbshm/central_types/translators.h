///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   translators.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Translators for Central-compatible shared memory access
///
/// Downstream functions require that translations always succeed.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TYPES_TRANSLATORS_H
#define CBSHM_CENTRAL_TYPES_TRANSLATORS_H 

#include <cbproto/types.h>
#include <cbproto/config.h>
#include <cbshm/central_types/v4_2.h>
#include <cbshm/central_types/v4_1.h>
#include <cbshm/central_types/v4_0.h>
#include <cbshm/central_types/v3_11.h>
#include <cbshm/native_types.h>

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Additional context for the translation functions
///
/// Not all translators need this context, but it's provided to all functions so the signatures
/// remain the same.
///
/// TODO: Move all translations to within the adapter classes as well as the translation context.
///
struct TranslationContext {
    // cbCFGBUFF, cbPcStatus
    uint8_t instrument_idx;
};

namespace central_v4_2 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg, const TranslationContext& ctx);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg, const TranslationContext& ctx);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg, const TranslationContext& ctx);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg, const TranslationContext& ctx);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg, const TranslationContext& ctx);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg, const TranslationContext& ctx);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg, const TranslationContext& ctx);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg, const TranslationContext& ctx);
::cbSCALING fromLegacy(const cbSCALING& leg, const TranslationContext& ctx);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg, const TranslationContext& ctx);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg, const TranslationContext& ctx);
::cbHOOP fromLegacy(const cbHOOP& leg, const TranslationContext& ctx);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg, const TranslationContext& ctx);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg, const TranslationContext& ctx);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg, const TranslationContext& ctx);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg, const TranslationContext& ctx);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg, const TranslationContext& ctx);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg, const TranslationContext& ctx);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg, const TranslationContext& ctx);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg, const TranslationContext& ctx);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg, const TranslationContext& ctx);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg, const TranslationContext& ctx);
::cbWaveformData fromLegacy(const cbWaveformData& leg, const TranslationContext& ctx);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg, const TranslationContext& ctx);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg, const TranslationContext& ctx);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg, const TranslationContext& ctx);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg, const TranslationContext& ctx);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg, const TranslationContext& ctx);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg, const TranslationContext& ctx);
NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg, const TranslationContext& ctx);
NativeNSPStatus fromLegacy(const NSPStatus& leg, const TranslationContext& ctx);
::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg, const TranslationContext& ctx);
NativePCStatus fromLegacy(const cbPcStatus& leg, const TranslationContext& ctx);
NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg, const TranslationContext& ctx);
NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg, const TranslationContext& ctx);
NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg, const TranslationContext& ctx);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur, const TranslationContext& ctx);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur, const TranslationContext& ctx);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur, const TranslationContext& ctx);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur, const TranslationContext& ctx);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur, const TranslationContext& ctx);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur, const TranslationContext& ctx);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur, const TranslationContext& ctx);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur, const TranslationContext& ctx);
cbSCALING toLegacy(const ::cbSCALING& cur, const TranslationContext& ctx);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur, const TranslationContext& ctx);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur, const TranslationContext& ctx);
cbHOOP toLegacy(const ::cbHOOP& cur, const TranslationContext& ctx);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur, const TranslationContext& ctx);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur, const TranslationContext& ctx);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur, const TranslationContext& ctx);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur, const TranslationContext& ctx);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur, const TranslationContext& ctx);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur, const TranslationContext& ctx);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur, const TranslationContext& ctx);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur, const TranslationContext& ctx);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur, const TranslationContext& ctx);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur, const TranslationContext& ctx);
cbWaveformData toLegacy(const ::cbWaveformData& cur, const TranslationContext& ctx);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur, const TranslationContext& ctx);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur, const TranslationContext& ctx);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur, const TranslationContext& ctx);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur, const TranslationContext& ctx);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur, const TranslationContext& ctx);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur, const TranslationContext& ctx);
// cbCFGBUFF -> NativeConfigBuffer is a lossy translation and cannot be reversed
NSPStatus toLegacy(const NativeNSPStatus& cur, const TranslationContext& ctx);
cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur, const TranslationContext& ctx);
cbPcStatus toLegacy(const NativePCStatus& cur, const TranslationContext& ctx);
cbRECBUFF toLegacy(const NativeReceiveBuffer& cur, const TranslationContext& ctx);
cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur, const TranslationContext& ctx);
cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur, const TranslationContext& ctx);

} // namespace central_v4_2

namespace central_v4_1 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg, const TranslationContext& ctx);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg, const TranslationContext& ctx);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg, const TranslationContext& ctx);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg, const TranslationContext& ctx);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg, const TranslationContext& ctx);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg, const TranslationContext& ctx);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg, const TranslationContext& ctx);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg, const TranslationContext& ctx);
::cbSCALING fromLegacy(const cbSCALING& leg, const TranslationContext& ctx);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg, const TranslationContext& ctx);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg, const TranslationContext& ctx);
::cbHOOP fromLegacy(const cbHOOP& leg, const TranslationContext& ctx);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg, const TranslationContext& ctx);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg, const TranslationContext& ctx);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg, const TranslationContext& ctx);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg, const TranslationContext& ctx);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg, const TranslationContext& ctx);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg, const TranslationContext& ctx);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg, const TranslationContext& ctx);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg, const TranslationContext& ctx);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg, const TranslationContext& ctx);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg, const TranslationContext& ctx);
::cbWaveformData fromLegacy(const cbWaveformData& leg, const TranslationContext& ctx);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg, const TranslationContext& ctx);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg, const TranslationContext& ctx);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg, const TranslationContext& ctx);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg, const TranslationContext& ctx);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg, const TranslationContext& ctx);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg, const TranslationContext& ctx);
NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg, const TranslationContext& ctx);
NativeNSPStatus fromLegacy(const NSPStatus& leg, const TranslationContext& ctx);
::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg, const TranslationContext& ctx);
NativePCStatus fromLegacy(const cbPcStatus& leg, const TranslationContext& ctx);
NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg, const TranslationContext& ctx);
NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg, const TranslationContext& ctx);
NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg, const TranslationContext& ctx);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur, const TranslationContext& ctx);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur, const TranslationContext& ctx);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur, const TranslationContext& ctx);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur, const TranslationContext& ctx);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur, const TranslationContext& ctx);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur, const TranslationContext& ctx);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur, const TranslationContext& ctx);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur, const TranslationContext& ctx);
cbSCALING toLegacy(const ::cbSCALING& cur, const TranslationContext& ctx);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur, const TranslationContext& ctx);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur, const TranslationContext& ctx);
cbHOOP toLegacy(const ::cbHOOP& cur, const TranslationContext& ctx);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur, const TranslationContext& ctx);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur, const TranslationContext& ctx);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur, const TranslationContext& ctx);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur, const TranslationContext& ctx);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur, const TranslationContext& ctx);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur, const TranslationContext& ctx);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur, const TranslationContext& ctx);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur, const TranslationContext& ctx);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur, const TranslationContext& ctx);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur, const TranslationContext& ctx);
cbWaveformData toLegacy(const ::cbWaveformData& cur, const TranslationContext& ctx);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur, const TranslationContext& ctx);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur, const TranslationContext& ctx);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur, const TranslationContext& ctx);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur, const TranslationContext& ctx);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur, const TranslationContext& ctx);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur, const TranslationContext& ctx);
// cbCFGBUFF -> NativeConfigBuffer is a lossy translation and cannot be reversed
NSPStatus toLegacy(const NativeNSPStatus& cur, const TranslationContext& ctx);
cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur, const TranslationContext& ctx);
cbPcStatus toLegacy(const NativePCStatus& cur, const TranslationContext& ctx);
cbRECBUFF toLegacy(const NativeReceiveBuffer& cur, const TranslationContext& ctx);
cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur, const TranslationContext& ctx);
cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur, const TranslationContext& ctx);

} // namespace central_v4_1

namespace central_v4_0 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg, const TranslationContext& ctx);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg, const TranslationContext& ctx);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg, const TranslationContext& ctx);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg, const TranslationContext& ctx);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg, const TranslationContext& ctx);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg, const TranslationContext& ctx);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg, const TranslationContext& ctx);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg, const TranslationContext& ctx);
::cbSCALING fromLegacy(const cbSCALING& leg, const TranslationContext& ctx);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg, const TranslationContext& ctx);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg, const TranslationContext& ctx);
::cbHOOP fromLegacy(const cbHOOP& leg, const TranslationContext& ctx);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg, const TranslationContext& ctx);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg, const TranslationContext& ctx);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg, const TranslationContext& ctx);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg, const TranslationContext& ctx);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg, const TranslationContext& ctx);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg, const TranslationContext& ctx);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg, const TranslationContext& ctx);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg, const TranslationContext& ctx);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg, const TranslationContext& ctx);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg, const TranslationContext& ctx);
::cbWaveformData fromLegacy(const cbWaveformData& leg, const TranslationContext& ctx);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg, const TranslationContext& ctx);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg, const TranslationContext& ctx);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg, const TranslationContext& ctx);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg, const TranslationContext& ctx);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg, const TranslationContext& ctx);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg, const TranslationContext& ctx);
NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg, const TranslationContext& ctx);
NativeNSPStatus fromLegacy(const NSPStatus& leg, const TranslationContext& ctx);
::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg, const TranslationContext& ctx);
NativePCStatus fromLegacy(const cbPcStatus& leg, const TranslationContext& ctx);
NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg, const TranslationContext& ctx);
NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg, const TranslationContext& ctx);
NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg, const TranslationContext& ctx);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur, const TranslationContext& ctx);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur, const TranslationContext& ctx);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur, const TranslationContext& ctx);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur, const TranslationContext& ctx);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur, const TranslationContext& ctx);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur, const TranslationContext& ctx);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur, const TranslationContext& ctx);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur, const TranslationContext& ctx);
cbSCALING toLegacy(const ::cbSCALING& cur, const TranslationContext& ctx);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur, const TranslationContext& ctx);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur, const TranslationContext& ctx);
cbHOOP toLegacy(const ::cbHOOP& cur, const TranslationContext& ctx);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur, const TranslationContext& ctx);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur, const TranslationContext& ctx);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur, const TranslationContext& ctx);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur, const TranslationContext& ctx);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur, const TranslationContext& ctx);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur, const TranslationContext& ctx);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur, const TranslationContext& ctx);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur, const TranslationContext& ctx);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur, const TranslationContext& ctx);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur, const TranslationContext& ctx);
cbWaveformData toLegacy(const ::cbWaveformData& cur, const TranslationContext& ctx);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur, const TranslationContext& ctx);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur, const TranslationContext& ctx);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur, const TranslationContext& ctx);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur, const TranslationContext& ctx);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur, const TranslationContext& ctx);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur, const TranslationContext& ctx);
// cbCFGBUFF -> NativeConfigBuffer is a lossy translation and cannot be reversed
NSPStatus toLegacy(const NativeNSPStatus& cur, const TranslationContext& ctx);
cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur, const TranslationContext& ctx);
cbPcStatus toLegacy(const NativePCStatus& cur, const TranslationContext& ctx);
cbRECBUFF toLegacy(const NativeReceiveBuffer& cur, const TranslationContext& ctx);
cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur, const TranslationContext& ctx);
cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur, const TranslationContext& ctx);

} // namespace central_v4_0

namespace central_v3_11 {

::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg, const TranslationContext& ctx);
::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg, const TranslationContext& ctx);
::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg, const TranslationContext& ctx);
::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg, const TranslationContext& ctx);
::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg, const TranslationContext& ctx);
::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg, const TranslationContext& ctx);
::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg, const TranslationContext& ctx);
::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg, const TranslationContext& ctx);
::cbSCALING fromLegacy(const cbSCALING& leg, const TranslationContext& ctx);
::cbFILTDESC fromLegacy(const cbFILTDESC& leg, const TranslationContext& ctx);
::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg, const TranslationContext& ctx);
::cbHOOP fromLegacy(const cbHOOP& leg, const TranslationContext& ctx);
::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg, const TranslationContext& ctx);
::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg, const TranslationContext& ctx);
::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg, const TranslationContext& ctx);
::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg, const TranslationContext& ctx);
::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg, const TranslationContext& ctx);
::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg, const TranslationContext& ctx);
::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg, const TranslationContext& ctx);
::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg, const TranslationContext& ctx);
::cbproto::SpikeSorting fromLegacy(const cbproto::SpikeSorting& leg, const TranslationContext& ctx);
::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg, const TranslationContext& ctx);
::cbWaveformData fromLegacy(const cbWaveformData& leg, const TranslationContext& ctx);
::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg, const TranslationContext& ctx);
::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg, const TranslationContext& ctx);
::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg, const TranslationContext& ctx);
::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg, const TranslationContext& ctx);
::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg, const TranslationContext& ctx);
::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg, const TranslationContext& ctx);
NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg, const TranslationContext& ctx);
NativeNSPStatus fromLegacy(const NSPStatus& leg, const TranslationContext& ctx);
::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg, const TranslationContext& ctx);
NativePCStatus fromLegacy(const cbPcStatus& leg, const TranslationContext& ctx);
NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg, const TranslationContext& ctx);
NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg, const TranslationContext& ctx);
NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg, const TranslationContext& ctx);

cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur, const TranslationContext& ctx);
cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur, const TranslationContext& ctx);
cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur, const TranslationContext& ctx);
cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur, const TranslationContext& ctx);
cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur, const TranslationContext& ctx);
cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur, const TranslationContext& ctx);
cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur, const TranslationContext& ctx);
cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur, const TranslationContext& ctx);
cbSCALING toLegacy(const ::cbSCALING& cur, const TranslationContext& ctx);
cbFILTDESC toLegacy(const ::cbFILTDESC& cur, const TranslationContext& ctx);
cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur, const TranslationContext& ctx);
cbHOOP toLegacy(const ::cbHOOP& cur, const TranslationContext& ctx);
cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur, const TranslationContext& ctx);
cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur, const TranslationContext& ctx);
cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur, const TranslationContext& ctx);
cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur, const TranslationContext& ctx);
cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur, const TranslationContext& ctx);
cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur, const TranslationContext& ctx);
cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur, const TranslationContext& ctx);
cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur, const TranslationContext& ctx);
cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur, const TranslationContext& ctx);
cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur, const TranslationContext& ctx);
cbWaveformData toLegacy(const ::cbWaveformData& cur, const TranslationContext& ctx);
cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur, const TranslationContext& ctx);
cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur, const TranslationContext& ctx);
cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur, const TranslationContext& ctx);
cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur, const TranslationContext& ctx);
cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur, const TranslationContext& ctx);
cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur, const TranslationContext& ctx);
// cbCFGBUFF -> NativeConfigBuffer is a lossy translation and cannot be reversed
NSPStatus toLegacy(const NativeNSPStatus& cur, const TranslationContext& ctx);
cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur, const TranslationContext& ctx);
cbPcStatus toLegacy(const NativePCStatus& cur, const TranslationContext& ctx);
cbRECBUFF toLegacy(const NativeReceiveBuffer& cur, const TranslationContext& ctx);
cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur, const TranslationContext& ctx);
cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur, const TranslationContext& ctx);

} // namespace central_v3_11

} // namespace cbshm

#endif // CBSHM_CENTRAL_TYPES_TRANSLATORS_H
