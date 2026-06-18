///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v4_1.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Adapter for Central-compatible shared memory access
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_ADAPTERS_V4_1_H
#define CBSHM_CENTRAL_ADAPTERS_V4_1_H 

#include <cbshm/central_adapters/base.h>
#include <cbshm/central_types/v4_1.h>

namespace cbshm {

namespace central_v4_1 {

///
/// @brief Adapter that provides information for fetching pointers to Central's shared memory
///
class BootstrapAdapter : public ::cbshm::CentralBootstrapAdapterBase {
public:
    // Buffer sizes
    size_t getConfigBufferSize() const override;
    size_t getReceiveBufferSize() const override;
    size_t getTransmitBufferSize() const override;
    size_t getTransmitBufferLocalSize() const override;
    size_t getStatusBufferSize() const override;
    size_t getSpikeBufferSize() const override;
    size_t getReceiveBufferLen() const override;
};

///
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    uint8_t instrument_idx;
    cbCFGBUFF* cfg;
    cbRECBUFF* rec;
    cbXMTBUFF* xmt;
    cbXMTBUFFLOCAL* xmt_local;
    cbPcStatus* status;
    cbSPKBUFF* spike;

    void fromLegacy(::cbPKT_HEADER& cur, const cbPKT_HEADER& leg) const;
    void fromLegacy(::cbPKT_SYSINFO& cur, const cbPKT_SYSINFO& leg) const;
    void fromLegacy(::cbPKT_PROCINFO& cur, const cbPKT_PROCINFO& leg) const;
    void fromLegacy(::cbPKT_BANKINFO& cur, const cbPKT_BANKINFO& leg) const;
    void fromLegacy(::cbPKT_GROUPINFO& cur, const cbPKT_GROUPINFO& leg) const;
    void fromLegacy(::cbPKT_FILTINFO& cur, const cbPKT_FILTINFO& leg) const;
    void fromLegacy(::cbPKT_ADAPTFILTINFO& cur, const cbPKT_ADAPTFILTINFO& leg) const;
    void fromLegacy(::cbPKT_REFELECFILTINFO& cur, const cbPKT_REFELECFILTINFO& leg) const;
    void fromLegacy(::cbSCALING& cur, const cbSCALING& leg) const;
    void fromLegacy(::cbFILTDESC& cur, const cbFILTDESC& leg) const;
    void fromLegacy(::cbMANUALUNITMAPPING& cur, const cbMANUALUNITMAPPING& leg) const;
    void fromLegacy(::cbHOOP& cur, const cbHOOP& leg) const;
    void fromLegacy(::cbPKT_CHANINFO& cur, const cbPKT_CHANINFO& leg) const;
    void fromLegacy(::cbPKT_FS_BASIS& cur, const cbPKT_FS_BASIS& leg) const;
    void fromLegacy(::cbPKT_SS_MODELSET& cur, const cbPKT_SS_MODELSET& leg) const;
    void fromLegacy(::cbPKT_SS_DETECT& cur, const cbPKT_SS_DETECT& leg) const;
    void fromLegacy(::cbPKT_SS_ARTIF_REJECT& cur, const cbPKT_SS_ARTIF_REJECT& leg) const;
    void fromLegacy(::cbPKT_SS_NOISE_BOUNDARY& cur, const cbPKT_SS_NOISE_BOUNDARY& leg) const;
    void fromLegacy(::cbPKT_SS_STATISTICS& cur, const cbPKT_SS_STATISTICS& leg) const;
    void fromLegacy(::cbAdaptControl& cur, const cbAdaptControl& leg) const;
    void fromLegacy(::cbPKT_SS_STATUS& cur, const cbPKT_SS_STATUS& leg) const;
    void fromLegacy(::cbproto::SpikeSorting& cur, const cbSPIKE_SORTING& leg) const;
    void fromLegacy(::cbPKT_NTRODEINFO& cur, const cbPKT_NTRODEINFO& leg) const;
    void fromLegacy(::cbWaveformData& cur, const cbWaveformData& leg) const;
    void fromLegacy(::cbPKT_AOUT_WAVEFORM& cur, const cbPKT_AOUT_WAVEFORM& leg) const;
    void fromLegacy(::cbPKT_LNC& cur, const cbPKT_LNC& leg) const;
    void fromLegacy(::cbPKT_NPLAY& cur, const cbPKT_NPLAY& leg) const;
    void fromLegacy(::cbVIDEOSOURCE& cur, const cbVIDEOSOURCE& leg) const;
    void fromLegacy(::cbTRACKOBJ& cur, const cbTRACKOBJ& leg) const;
    void fromLegacy(::cbPKT_FILECFG& cur, const cbPKT_FILECFG& leg) const;
    void fromLegacy(NativeConfigBuffer& cur, const cbCFGBUFF& leg) const;
    void fromLegacy(NativeNSPStatus& cur, const NSPStatus& leg) const;
    void fromLegacy(::cbPKT_UNIT_SELECTION& cur, const cbPKT_UNIT_SELECTION& leg) const;
    void fromLegacy(NativePCStatus& cur, const cbPcStatus& leg) const;
    void fromLegacy(NativeReceiveBuffer& cur, const cbRECBUFF& leg) const;
    void fromLegacy(NativeTransmitBuffer& cur, const cbXMTBUFF& leg) const;
    void fromLegacy(NativeTransmitBufferLocal& cur, const cbXMTBUFFLOCAL& leg) const;
    void fromLegacy(::cbPKT_SPK& cur, const cbPKT_SPK& leg) const;
    void fromLegacy(NativeSpikeCache& cur, const cbSPKCACHE& leg) const;
    void fromLegacy(NativeSpikeBuffer& cur, const cbSPKBUFF& leg) const;

    void toLegacy(cbPKT_HEADER& leg, const ::cbPKT_HEADER& cur) const;
    void toLegacy(cbPKT_SYSINFO& leg, const ::cbPKT_SYSINFO& cur) const;
    void toLegacy(cbPKT_PROCINFO& leg, const ::cbPKT_PROCINFO& cur) const;
    void toLegacy(cbPKT_BANKINFO& leg, const ::cbPKT_BANKINFO& cur) const;
    void toLegacy(cbPKT_GROUPINFO& leg, const ::cbPKT_GROUPINFO& cur) const;
    void toLegacy(cbPKT_FILTINFO& leg, const ::cbPKT_FILTINFO& cur) const;
    void toLegacy(cbPKT_ADAPTFILTINFO& leg, const ::cbPKT_ADAPTFILTINFO& cur) const;
    void toLegacy(cbPKT_REFELECFILTINFO& leg, const ::cbPKT_REFELECFILTINFO& cur) const;
    void toLegacy(cbSCALING& leg, const ::cbSCALING& cur) const;
    void toLegacy(cbFILTDESC& leg, const ::cbFILTDESC& cur) const;
    void toLegacy(cbMANUALUNITMAPPING& leg, const ::cbMANUALUNITMAPPING& cur) const;
    void toLegacy(cbHOOP& leg, const ::cbHOOP& cur) const;
    void toLegacy(cbPKT_CHANINFO& leg, const ::cbPKT_CHANINFO& cur) const;
    void toLegacy(cbPKT_FS_BASIS& leg, const ::cbPKT_FS_BASIS& cur) const;
    void toLegacy(cbPKT_SS_MODELSET& leg, const ::cbPKT_SS_MODELSET& cur) const;
    void toLegacy(cbPKT_SS_DETECT& leg, const ::cbPKT_SS_DETECT& cur) const;
    void toLegacy(cbPKT_SS_ARTIF_REJECT& leg, const ::cbPKT_SS_ARTIF_REJECT& cur) const;
    void toLegacy(cbPKT_SS_NOISE_BOUNDARY& leg, const ::cbPKT_SS_NOISE_BOUNDARY& cur) const;
    void toLegacy(cbPKT_SS_STATISTICS& leg, const ::cbPKT_SS_STATISTICS& cur) const;
    void toLegacy(cbAdaptControl& leg, const ::cbAdaptControl& cur) const;
    void toLegacy(cbPKT_SS_STATUS& leg, const ::cbPKT_SS_STATUS& cur) const;
    void toLegacy(cbSPIKE_SORTING& leg, const ::cbproto::SpikeSorting& cur) const;
    void toLegacy(cbPKT_NTRODEINFO& leg, const ::cbPKT_NTRODEINFO& cur) const;
    void toLegacy(cbWaveformData& leg, const ::cbWaveformData& cur) const;
    void toLegacy(cbPKT_AOUT_WAVEFORM& leg, const ::cbPKT_AOUT_WAVEFORM& cur) const;
    void toLegacy(cbPKT_LNC& leg, const ::cbPKT_LNC& cur) const;
    void toLegacy(cbPKT_NPLAY& leg, const ::cbPKT_NPLAY& cur) const;
    void toLegacy(cbVIDEOSOURCE& leg, const ::cbVIDEOSOURCE& cur) const;
    void toLegacy(cbTRACKOBJ& leg, const ::cbTRACKOBJ& cur) const;
    void toLegacy(cbPKT_FILECFG& leg, const ::cbPKT_FILECFG& cur) const;
    void toLegacy(NSPStatus& leg, const NativeNSPStatus& cur) const;
    void toLegacy(cbPKT_UNIT_SELECTION& leg, const ::cbPKT_UNIT_SELECTION& cur) const;
    void toLegacy(cbRECBUFF& leg, const NativeReceiveBuffer& cur) const;
    void toLegacy(cbXMTBUFF& leg, const NativeTransmitBuffer& cur) const;
    void toLegacy(cbXMTBUFFLOCAL& leg, const NativeTransmitBufferLocal& cur) const;
    void toLegacy(cbPKT_SPK& leg, const ::cbPKT_SPK& cur) const;

public:
    Adapter(
        uint8_t instrument_idx,
        void* cfg_ptr,
        void* rec_ptr,
        void* xmt_ptr,
        void* xmt_local_ptr,
        void* status_ptr,
        void* spike_ptr
    );
    ~Adapter() = default;

    // Max instrument count
    uint32_t getMaxProcs() const override;

    // Receive buffer access
    uint32_t& getRecReceived() override;
    uint64_t getRecLasttime() override;
    void setRecLasttime(uint64_t lasttime) override;
    uint32_t& getRecHeadwrapPtr() override;
    uint32_t& getRecHeadindexPtr() override;
    uint32_t* getRecBufferPtr() override;

    // Transmit buffer access
    uint32_t& getXmtTransmittedPtr() override;
    uint32_t& getXmtHeadindexPtr() override;
    uint32_t& getXmtTailindexPtr() override;
    uint32_t& getXmtLastValidIndexPtr() override;
    uint32_t& getXmtBufferlenPtr() override;
    uint32_t* getXmtBufferPtr() override;

    uint32_t& getLocalXmtTransmittedPtr() override;
    uint32_t& getLocalXmtHeadindexPtr() override;
    uint32_t& getLocalXmtTailindexPtr() override;
    uint32_t& getLocalXmtLastValidIndexPtr() override;
    uint32_t& getLocalXmtBufferlenPtr() override;
    uint32_t* getLocalXmtBufferPtr() override;

    /// Config read operations
    cbutil::Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const override;
    cbutil::Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const override;
    cbutil::Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const override;
    cbutil::Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const override;
    cbutil::Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const override;
    cbutil::Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const override;
    cbutil::Result<void> getConfigBuffer(NativeConfigBuffer& buf) const override;
    cbutil::Result<void> getPcStatus(NativePCStatus& buf) const override;
    cbutil::Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const override;

    /// Config write operations
    cbutil::Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) override;
    cbutil::Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    cbutil::Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    cbutil::Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    cbutil::Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    cbutil::Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    cbutil::Result<void> setNspStatus(const NativeNSPStatus& status) const override;
    cbutil::Result<void> setGeminiSystem(bool is_gemini) const override;
};

} // namespace central_v4_1

} // namespace cbshm

#endif // CBSHM_CENTRAL_ADAPTERS_V4_1_H
