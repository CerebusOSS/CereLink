///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   adapters.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Adapters for Central-compatible shared memory access
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TYPES_ADAPTERS_H
#define CBSHM_CENTRAL_TYPES_ADAPTERS_H 

#include <cstring>
#include <cbproto/types.h>
#include <cbproto/config.h>
#include <cbshm/shmem_session.h>
#include <cbshm/central_types/v4_2.h>
#include <cbshm/central_types/v4_1.h>
#include <cbshm/central_types/v4_0.h>
#include <cbshm/central_types/v3_11.h>
#include <cbshm/native_types.h>

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base-class adapter that provides information used to fetch pointers to Central's shared memory
///
class CentralBootstrapAdapterBase {
public:
    // Buffer sizes
    virtual size_t getConfigBufferSize() const = 0;
    virtual size_t getReceiveBufferSize() const = 0;
    virtual size_t getTransmitBufferSize() const = 0;
    virtual size_t getTransmitBufferLocalSize() const = 0;
    virtual size_t getStatusBufferSize() const = 0;
    virtual size_t getSpikeBufferSize() const = 0;
    virtual size_t getReceiveBufferLen() const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base-class adapter for Central-compatible shared memory access
///
class CentralAdapterBase {
protected:
    template<typename T, size_t lhs_n, size_t rhs_n>
    static void copyArr(T(&lhs)[lhs_n], const T(&rhs)[rhs_n]) {
        if (lhs_n <= rhs_n) {
            std::memcpy(lhs, rhs, lhs_n * sizeof(T));
        } else {
            std::memcpy(lhs, rhs, rhs_n * sizeof(T));
            std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(T));
        }
    }

    template<typename LHS_T, size_t lhs_n, typename RHS_T, size_t rhs_n, class A>
    static void copyArr(LHS_T(&lhs)[lhs_n], const RHS_T(&rhs)[rhs_n], const A* adapter, LHS_T(A::*translation_func)(const RHS_T&) const) {
        if (lhs_n <= rhs_n) {
            for (size_t i = 0; i < lhs_n; ++i) {
                lhs[i] = (adapter->*translation_func)(rhs[i]);
            }
        } else {
            for (size_t i = 0; i < rhs_n; ++i) {
                lhs[i] = (adapter->*translation_func)(rhs[i]);
            }
            std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(LHS_T));
        }
    }

    template<typename T, size_t lhs_ny, size_t lhs_nx, size_t rhs_ny, size_t rhs_nx>
    static void copyArr2D(T(&lhs)[lhs_ny][lhs_nx], const T(&rhs)[rhs_ny][rhs_nx]) {
        if (lhs_ny <= rhs_ny) {
            if (lhs_nx == rhs_nx) {
                std::memcpy(lhs, rhs, (lhs_ny * lhs_nx) * sizeof(T));
            } else {
                for (size_t i = 0; i < lhs_ny; ++i) {
                    copyArr(lhs[i], rhs[i]);
                }
            }
        } else {
            if (lhs_nx == rhs_nx) {
                std::memcpy(lhs, rhs, (rhs_ny * rhs_nx) * sizeof(T));
            } else {
                for (size_t i = 0; i < rhs_ny; ++i) {
                    copyArr(lhs[i], rhs[i]);
                }
            }
            std::memset(lhs + rhs_ny, 0, ((lhs_ny - rhs_ny) * lhs_nx) * sizeof(T));
        }
    }

    template<typename LHS_T, size_t lhs_ny, size_t lhs_nx, typename RHS_T, size_t rhs_ny, size_t rhs_nx, class A>
    static const void copyArr2D(LHS_T(&lhs)[lhs_ny][lhs_nx], const RHS_T(&rhs)[rhs_ny][rhs_nx], const A* adapter, LHS_T(A::*translation_func)(const RHS_T&) const) {
        if (lhs_ny <= rhs_ny) {
            for (size_t i = 0; i < lhs_ny; ++i) {
                copyArr(lhs[i], rhs[i], adapter, translation_func);
            }
        } else {
            for (size_t i = 0; i < rhs_ny; ++i) {
                copyArr(lhs[i], rhs[i], adapter, translation_func);
            }
            std::memset(lhs + rhs_ny, 0, ((lhs_ny - rhs_ny) * lhs_nx) * sizeof(LHS_T));
        }
    }

public:
    virtual ~CentralAdapterBase() = default;

    // Max instrument count
    virtual uint32_t getMaxProcs() const = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // DANGER !!!
    // These methods are brittle and serve as a workaround for the ShmemSession
    // implementation requiring direct pointer access to the receive and transmit buffers.

    // Receive buffer access
    virtual uint32_t& getRecReceived() = 0;
    virtual uint64_t getRecLasttime() = 0;
    virtual void setRecLasttime(uint64_t lasttime) = 0;
    virtual uint32_t& getRecHeadwrapPtr() = 0;
    virtual uint32_t& getRecHeadindexPtr() = 0;
    virtual uint32_t* getRecBufferPtr() = 0;

    // Transmit buffer access
    virtual uint32_t& getXmtTransmittedPtr() = 0;
    virtual uint32_t& getXmtHeadindexPtr() = 0;
    virtual uint32_t& getXmtTailindexPtr() = 0;
    virtual uint32_t& getXmtLastValidIndexPtr() = 0;
    virtual uint32_t& getXmtBufferlenPtr() = 0;
    virtual uint32_t* getXmtBufferPtr() = 0;

    virtual uint32_t& getLocalXmtTransmittedPtr() = 0;
    virtual uint32_t& getLocalXmtHeadindexPtr() = 0;
    virtual uint32_t& getLocalXmtTailindexPtr() = 0;
    virtual uint32_t& getLocalXmtLastValidIndexPtr() = 0;
    virtual uint32_t& getLocalXmtBufferlenPtr() = 0;
    virtual uint32_t* getLocalXmtBufferPtr() = 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /// Config read operations
    virtual Result<::cbPKT_PROCINFO> getProcInfo() const = 0;
    virtual Result<::cbPKT_BANKINFO> getBankInfo(uint32_t bank) const = 0;
    virtual Result<::cbPKT_FILTINFO> getFilterInfo(uint32_t filter) const = 0;
    virtual Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const = 0;
    virtual Result<::cbPKT_SYSINFO> getSysInfo() const = 0;
    virtual Result<::cbPKT_GROUPINFO> getGroupInfo(uint32_t group_idx) const = 0;
    virtual Result<NativeConfigBuffer> getConfigBuffer() const = 0;
    virtual Result<NativePCStatus> getPcStatus() const = 0;
    virtual Result<NativeSpikeCache> getSpikeCache(uint32_t channel_idx) const = 0;

    /// Config write operations
    virtual Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) = 0;
    virtual Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) = 0;
    virtual Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) = 0;
    virtual Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) = 0;
    virtual Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) = 0;
    virtual Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) = 0;
    virtual Result<void> setNspStatus(const NativeNSPStatus& status) const = 0;
    virtual Result<void> setGeminiSystem(bool is_gemini) const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace central_v4_2 {

///
/// @brief Adapter that provides information for fetching pointers to Central's shared memory
///
class BootstrapAdapter : public CentralBootstrapAdapterBase {
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

    ::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg) const;
    ::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg) const;
    ::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg) const;
    ::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg) const;
    ::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg) const;
    ::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg) const;
    ::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg) const;
    ::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg) const;
    ::cbSCALING fromLegacy(const cbSCALING& leg) const;
    ::cbFILTDESC fromLegacy(const cbFILTDESC& leg) const;
    ::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg) const;
    ::cbHOOP fromLegacy(const cbHOOP& leg) const;
    ::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg) const;
    ::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg) const;
    ::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg) const;
    ::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg) const;
    ::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg) const;
    ::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg) const;
    ::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg) const;
    ::cbAdaptControl fromLegacy(const cbAdaptControl& leg) const;
    ::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg) const;
    ::cbproto::SpikeSorting fromLegacy(const cbSPIKE_SORTING& leg) const;
    ::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg) const;
    ::cbWaveformData fromLegacy(const cbWaveformData& leg) const;
    ::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg) const;
    ::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg) const;
    ::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg) const;
    ::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg) const;
    ::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg) const;
    ::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg) const;
    NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg) const;
    NativeNSPStatus fromLegacy(const NSPStatus& leg) const;
    ::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg) const;
    NativePCStatus fromLegacy(const cbPcStatus& leg) const;
    NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg) const;
    NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg) const;
    NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg) const;
    ::cbPKT_SPK fromLegacy(const cbPKT_SPK& leg) const;
    NativeSpikeCache fromLegacy(const cbSPKCACHE& leg) const;
    NativeSpikeBuffer fromLegacy(const cbSPKBUFF& leg) const;

    cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur) const;
    cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur) const;
    cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur) const;
    cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur) const;
    cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur) const;
    cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur) const;
    cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur) const;
    cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur) const;
    cbSCALING toLegacy(const ::cbSCALING& cur) const;
    cbFILTDESC toLegacy(const ::cbFILTDESC& cur) const;
    cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur) const;
    cbHOOP toLegacy(const ::cbHOOP& cur) const;
    cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur) const;
    cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur) const;
    cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur) const;
    cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur) const;
    cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur) const;
    cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur) const;
    cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur) const;
    cbAdaptControl toLegacy(const ::cbAdaptControl& cur) const;
    cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur) const;
    cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur) const;
    cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur) const;
    cbWaveformData toLegacy(const ::cbWaveformData& cur) const;
    cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur) const;
    cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur) const;
    cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur) const;
    cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur) const;
    cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur) const;
    cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur) const;
    NSPStatus toLegacy(const NativeNSPStatus& cur) const;
    cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur) const;
    cbRECBUFF toLegacy(const NativeReceiveBuffer& cur) const;
    cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur) const;
    cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur) const;
    cbPKT_SPK toLegacy(const ::cbPKT_SPK& cur) const;

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
    Result<::cbPKT_PROCINFO> getProcInfo() const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer() const override;
    Result<NativePCStatus> getPcStatus() const override;
    Result<NativeSpikeCache> getSpikeCache(uint32_t channel_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setNspStatus(const NativeNSPStatus& status) const override;
    Result<void> setGeminiSystem(bool is_gemini) const override;
};

} // namespace central_v4_2

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace central_v4_1 {

///
/// @brief Adapter that provides information for fetching pointers to Central's shared memory
///
class BootstrapAdapter : public CentralBootstrapAdapterBase {
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

    ::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg) const;
    ::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg) const;
    ::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg) const;
    ::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg) const;
    ::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg) const;
    ::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg) const;
    ::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg) const;
    ::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg) const;
    ::cbSCALING fromLegacy(const cbSCALING& leg) const;
    ::cbFILTDESC fromLegacy(const cbFILTDESC& leg) const;
    ::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg) const;
    ::cbHOOP fromLegacy(const cbHOOP& leg) const;
    ::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg) const;
    ::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg) const;
    ::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg) const;
    ::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg) const;
    ::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg) const;
    ::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg) const;
    ::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg) const;
    ::cbAdaptControl fromLegacy(const cbAdaptControl& leg) const;
    ::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg) const;
    ::cbproto::SpikeSorting fromLegacy(const cbSPIKE_SORTING& leg) const;
    ::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg) const;
    ::cbWaveformData fromLegacy(const cbWaveformData& leg) const;
    ::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg) const;
    ::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg) const;
    ::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg) const;
    ::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg) const;
    ::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg) const;
    ::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg) const;
    NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg) const;
    NativeNSPStatus fromLegacy(const NSPStatus& leg) const;
    ::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg) const;
    NativePCStatus fromLegacy(const cbPcStatus& leg) const;
    NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg) const;
    NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg) const;
    NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg) const;
    ::cbPKT_SPK fromLegacy(const cbPKT_SPK& leg) const;
    NativeSpikeCache fromLegacy(const cbSPKCACHE& leg) const;
    NativeSpikeBuffer fromLegacy(const cbSPKBUFF& leg) const;

    cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur) const;
    cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur) const;
    cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur) const;
    cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur) const;
    cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur) const;
    cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur) const;
    cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur) const;
    cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur) const;
    cbSCALING toLegacy(const ::cbSCALING& cur) const;
    cbFILTDESC toLegacy(const ::cbFILTDESC& cur) const;
    cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur) const;
    cbHOOP toLegacy(const ::cbHOOP& cur) const;
    cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur) const;
    cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur) const;
    cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur) const;
    cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur) const;
    cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur) const;
    cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur) const;
    cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur) const;
    cbAdaptControl toLegacy(const ::cbAdaptControl& cur) const;
    cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur) const;
    cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur) const;
    cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur) const;
    cbWaveformData toLegacy(const ::cbWaveformData& cur) const;
    cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur) const;
    cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur) const;
    cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur) const;
    cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur) const;
    cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur) const;
    cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur) const;
    NSPStatus toLegacy(const NativeNSPStatus& cur) const;
    cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur) const;
    cbRECBUFF toLegacy(const NativeReceiveBuffer& cur) const;
    cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur) const;
    cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur) const;
    cbPKT_SPK toLegacy(const ::cbPKT_SPK& cur) const;

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
    Result<::cbPKT_PROCINFO> getProcInfo() const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer() const override;
    Result<NativePCStatus> getPcStatus() const override;
    Result<NativeSpikeCache> getSpikeCache(uint32_t channel_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setNspStatus(const NativeNSPStatus& status) const override;
    Result<void> setGeminiSystem(bool is_gemini) const override;
};

} // namespace central_v4_1

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace central_v4_0 {

///
/// @brief Adapter that provides information for fetching pointers to Central's shared memory
///
class BootstrapAdapter : public CentralBootstrapAdapterBase {
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

    ::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg) const;
    ::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg) const;
    ::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg) const;
    ::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg) const;
    ::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg) const;
    ::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg) const;
    ::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg) const;
    ::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg) const;
    ::cbSCALING fromLegacy(const cbSCALING& leg) const;
    ::cbFILTDESC fromLegacy(const cbFILTDESC& leg) const;
    ::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg) const;
    ::cbHOOP fromLegacy(const cbHOOP& leg) const;
    ::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg) const;
    ::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg) const;
    ::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg) const;
    ::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg) const;
    ::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg) const;
    ::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg) const;
    ::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg) const;
    ::cbAdaptControl fromLegacy(const cbAdaptControl& leg) const;
    ::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg) const;
    ::cbproto::SpikeSorting fromLegacy(const cbSPIKE_SORTING& leg) const;
    ::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg) const;
    ::cbWaveformData fromLegacy(const cbWaveformData& leg) const;
    ::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg) const;
    ::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg) const;
    ::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg) const;
    ::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg) const;
    ::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg) const;
    ::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg) const;
    NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg) const;
    NativeNSPStatus fromLegacy(const NSPStatus& leg) const;
    ::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg) const;
    NativePCStatus fromLegacy(const cbPcStatus& leg) const;
    NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg) const;
    NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg) const;
    NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg) const;
    ::cbPKT_SPK fromLegacy(const cbPKT_SPK& leg) const;
    NativeSpikeCache fromLegacy(const cbSPKCACHE& leg) const;
    NativeSpikeBuffer fromLegacy(const cbSPKBUFF& leg) const;

    cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur) const;
    cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur) const;
    cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur) const;
    cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur) const;
    cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur) const;
    cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur) const;
    cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur) const;
    cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur) const;
    cbSCALING toLegacy(const ::cbSCALING& cur) const;
    cbFILTDESC toLegacy(const ::cbFILTDESC& cur) const;
    cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur) const;
    cbHOOP toLegacy(const ::cbHOOP& cur) const;
    cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur) const;
    cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur) const;
    cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur) const;
    cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur) const;
    cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur) const;
    cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur) const;
    cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur) const;
    cbAdaptControl toLegacy(const ::cbAdaptControl& cur) const;
    cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur) const;
    cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur) const;
    cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur) const;
    cbWaveformData toLegacy(const ::cbWaveformData& cur) const;
    cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur) const;
    cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur) const;
    cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur) const;
    cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur) const;
    cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur) const;
    cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur) const;
    NSPStatus toLegacy(const NativeNSPStatus& cur) const;
    cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur) const;
    cbRECBUFF toLegacy(const NativeReceiveBuffer& cur) const;
    cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur) const;
    cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur) const;
    cbPKT_SPK toLegacy(const ::cbPKT_SPK& cur) const;

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
    Result<::cbPKT_PROCINFO> getProcInfo() const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer() const override;
    Result<NativePCStatus> getPcStatus() const override;
    Result<NativeSpikeCache> getSpikeCache(uint32_t channel_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setNspStatus(const NativeNSPStatus& status) const override;
    Result<void> setGeminiSystem(bool is_gemini) const override;
};

} // namespace central_v4_0

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace central_v3_11 {

///
/// @brief Adapter that provides information for fetching pointers to Central's shared memory
///
class BootstrapAdapter : public CentralBootstrapAdapterBase {
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

    ::cbPKT_HEADER fromLegacy(const cbPKT_HEADER& leg) const;
    ::cbPKT_SYSINFO fromLegacy(const cbPKT_SYSINFO& leg) const;
    ::cbPKT_PROCINFO fromLegacy(const cbPKT_PROCINFO& leg) const;
    ::cbPKT_BANKINFO fromLegacy(const cbPKT_BANKINFO& leg) const;
    ::cbPKT_GROUPINFO fromLegacy(const cbPKT_GROUPINFO& leg) const;
    ::cbPKT_FILTINFO fromLegacy(const cbPKT_FILTINFO& leg) const;
    ::cbPKT_ADAPTFILTINFO fromLegacy(const cbPKT_ADAPTFILTINFO& leg) const;
    ::cbPKT_REFELECFILTINFO fromLegacy(const cbPKT_REFELECFILTINFO& leg) const;
    ::cbSCALING fromLegacy(const cbSCALING& leg) const;
    ::cbFILTDESC fromLegacy(const cbFILTDESC& leg) const;
    ::cbMANUALUNITMAPPING fromLegacy(const cbMANUALUNITMAPPING& leg) const;
    ::cbHOOP fromLegacy(const cbHOOP& leg) const;
    ::cbPKT_CHANINFO fromLegacy(const cbPKT_CHANINFO& leg) const;
    ::cbPKT_FS_BASIS fromLegacy(const cbPKT_FS_BASIS& leg) const;
    ::cbPKT_SS_MODELSET fromLegacy(const cbPKT_SS_MODELSET& leg) const;
    ::cbPKT_SS_DETECT fromLegacy(const cbPKT_SS_DETECT& leg) const;
    ::cbPKT_SS_ARTIF_REJECT fromLegacy(const cbPKT_SS_ARTIF_REJECT& leg) const;
    ::cbPKT_SS_NOISE_BOUNDARY fromLegacy(const cbPKT_SS_NOISE_BOUNDARY& leg) const;
    ::cbPKT_SS_STATISTICS fromLegacy(const cbPKT_SS_STATISTICS& leg) const;
    ::cbAdaptControl fromLegacy(const cbAdaptControl& leg) const;
    ::cbPKT_SS_STATUS fromLegacy(const cbPKT_SS_STATUS& leg) const;
    ::cbproto::SpikeSorting fromLegacy(const cbSPIKE_SORTING& leg) const;
    ::cbPKT_NTRODEINFO fromLegacy(const cbPKT_NTRODEINFO& leg) const;
    ::cbWaveformData fromLegacy(const cbWaveformData& leg) const;
    ::cbPKT_AOUT_WAVEFORM fromLegacy(const cbPKT_AOUT_WAVEFORM& leg) const;
    ::cbPKT_LNC fromLegacy(const cbPKT_LNC& leg) const;
    ::cbPKT_NPLAY fromLegacy(const cbPKT_NPLAY& leg) const;
    ::cbVIDEOSOURCE fromLegacy(const cbVIDEOSOURCE& leg) const;
    ::cbTRACKOBJ fromLegacy(const cbTRACKOBJ& leg) const;
    ::cbPKT_FILECFG fromLegacy(const cbPKT_FILECFG& leg) const;
    NativeConfigBuffer fromLegacy(const cbCFGBUFF& leg) const;
    NativeNSPStatus fromLegacy(const NSPStatus& leg) const;
    ::cbPKT_UNIT_SELECTION fromLegacy(const cbPKT_UNIT_SELECTION& leg) const;
    NativePCStatus fromLegacy(const cbPcStatus& leg) const;
    NativeReceiveBuffer fromLegacy(const cbRECBUFF& leg) const;
    NativeTransmitBuffer fromLegacy(const cbXMTBUFF& leg) const;
    NativeTransmitBufferLocal fromLegacy(const cbXMTBUFFLOCAL& leg) const;
    ::cbPKT_SPK fromLegacy(const cbPKT_SPK& leg) const;
    NativeSpikeCache fromLegacy(const cbSPKCACHE& leg) const;
    NativeSpikeBuffer fromLegacy(const cbSPKBUFF& leg) const;

    cbPKT_HEADER toLegacy(const ::cbPKT_HEADER& cur) const;
    cbPKT_SYSINFO toLegacy(const ::cbPKT_SYSINFO& cur) const;
    cbPKT_PROCINFO toLegacy(const ::cbPKT_PROCINFO& cur) const;
    cbPKT_BANKINFO toLegacy(const ::cbPKT_BANKINFO& cur) const;
    cbPKT_GROUPINFO toLegacy(const ::cbPKT_GROUPINFO& cur) const;
    cbPKT_FILTINFO toLegacy(const ::cbPKT_FILTINFO& cur) const;
    cbPKT_ADAPTFILTINFO toLegacy(const ::cbPKT_ADAPTFILTINFO& cur) const;
    cbPKT_REFELECFILTINFO toLegacy(const ::cbPKT_REFELECFILTINFO& cur) const;
    cbSCALING toLegacy(const ::cbSCALING& cur) const;
    cbFILTDESC toLegacy(const ::cbFILTDESC& cur) const;
    cbMANUALUNITMAPPING toLegacy(const ::cbMANUALUNITMAPPING& cur) const;
    cbHOOP toLegacy(const ::cbHOOP& cur) const;
    cbPKT_CHANINFO toLegacy(const ::cbPKT_CHANINFO& cur) const;
    cbPKT_FS_BASIS toLegacy(const ::cbPKT_FS_BASIS& cur) const;
    cbPKT_SS_MODELSET toLegacy(const ::cbPKT_SS_MODELSET& cur) const;
    cbPKT_SS_DETECT toLegacy(const ::cbPKT_SS_DETECT& cur) const;
    cbPKT_SS_ARTIF_REJECT toLegacy(const ::cbPKT_SS_ARTIF_REJECT& cur) const;
    cbPKT_SS_NOISE_BOUNDARY toLegacy(const ::cbPKT_SS_NOISE_BOUNDARY& cur) const;
    cbPKT_SS_STATISTICS toLegacy(const ::cbPKT_SS_STATISTICS& cur) const;
    cbAdaptControl toLegacy(const ::cbAdaptControl& cur) const;
    cbPKT_SS_STATUS toLegacy(const ::cbPKT_SS_STATUS& cur) const;
    cbSPIKE_SORTING toLegacy(const ::cbproto::SpikeSorting& cur) const;
    cbPKT_NTRODEINFO toLegacy(const ::cbPKT_NTRODEINFO& cur) const;
    cbWaveformData toLegacy(const ::cbWaveformData& cur) const;
    cbPKT_AOUT_WAVEFORM toLegacy(const ::cbPKT_AOUT_WAVEFORM& cur) const;
    cbPKT_LNC toLegacy(const ::cbPKT_LNC& cur) const;
    cbPKT_NPLAY toLegacy(const ::cbPKT_NPLAY& cur) const;
    cbVIDEOSOURCE toLegacy(const ::cbVIDEOSOURCE& cur) const;
    cbTRACKOBJ toLegacy(const ::cbTRACKOBJ& cur) const;
    cbPKT_FILECFG toLegacy(const ::cbPKT_FILECFG& cur) const;
    NSPStatus toLegacy(const NativeNSPStatus& cur) const;
    cbPKT_UNIT_SELECTION toLegacy(const ::cbPKT_UNIT_SELECTION& cur) const;
    cbRECBUFF toLegacy(const NativeReceiveBuffer& cur) const;
    cbXMTBUFF toLegacy(const NativeTransmitBuffer& cur) const;
    cbXMTBUFFLOCAL toLegacy(const NativeTransmitBufferLocal& cur) const;
    cbPKT_SPK toLegacy(const ::cbPKT_SPK& cur) const;

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
    Result<::cbPKT_PROCINFO> getProcInfo() const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer() const override;
    Result<NativePCStatus> getPcStatus() const override;
    Result<NativeSpikeCache> getSpikeCache(uint32_t channel_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setNspStatus(const NativeNSPStatus& status) const override;
    Result<void> setGeminiSystem(bool is_gemini) const override;
};

} // namespace central_v3_11
///////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace cbshm

#endif // CBSHM_CENTRAL_TYPES_ADAPTERS_H
