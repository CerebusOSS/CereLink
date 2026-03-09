///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   ccf_config.h
/// @brief  Bridge between cbCCF (file data) and DeviceConfig (device state)
///
/// Provides conversion functions between the CCF file format and device configuration.
/// These functions use only cbproto types and do not depend on cbdev.
///
/// Usage:
///   Save: device->getDeviceConfig() -> extractDeviceConfig() -> WriteCCFNoPrompt()
///   Load: ReadCCF() -> buildConfigPackets() -> device->sendPackets()
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CCFUTILS_CCF_CONFIG_H
#define CCFUTILS_CCF_CONFIG_H

#include <CCFUtils.h>
#include <cbproto/config.h>
#include <vector>

namespace ccf {

/// Extract device configuration into CCF data structure.
/// Copies fields from DeviceConfig into cbCCF, following Central's ReadCCFOfNSP() algorithm.
/// After calling this, pass the cbCCF to CCFUtils::WriteCCFNoPrompt() to save.
///
/// @param config  Device configuration obtained from IDeviceSession::getDeviceConfig()
/// @param ccf_data  Output CCF structure (should be zero-initialized before calling)
void extractDeviceConfig(const cbproto::DeviceConfig& config, cbCCF& ccf_data);

/// Build SET packets from CCF data for sending to device.
/// Constructs a vector of configuration packets following Central's SendCCF() algorithm.
/// After calling this, pass the vector to IDeviceSession::sendPackets().
///
/// @param ccf_data  CCF data obtained from CCFUtils::ReadCCF()
/// @return Vector of generic packets ready to send to device
std::vector<cbPKT_GENERIC> buildConfigPackets(const cbCCF& ccf_data);

} // namespace ccf

#endif // CCFUTILS_CCF_CONFIG_H
