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
    static void copyArr(LHS_T(&lhs)[lhs_n], const RHS_T(&rhs)[rhs_n], const A* adapter, void(A::*translation_func)(LHS_T&, const RHS_T&) const) {
        if (lhs_n <= rhs_n) {
            for (size_t i = 0; i < lhs_n; ++i) {
                (adapter->*translation_func)(lhs[i], rhs[i]);
            }
        } else {
            for (size_t i = 0; i < rhs_n; ++i) {
                (adapter->*translation_func)(lhs[i], rhs[i]);
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
    static const void copyArr2D(LHS_T(&lhs)[lhs_ny][lhs_nx], const RHS_T(&rhs)[rhs_ny][rhs_nx], const A* adapter, void(A::*translation_func)(LHS_T&, const RHS_T&) const) {
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
    virtual Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const = 0;
    virtual Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const = 0;
    virtual Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const = 0;
    virtual Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const = 0;
    virtual Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const = 0;
    virtual Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const = 0;
    virtual Result<void> getConfigBuffer(NativeConfigBuffer& buf) const = 0;
    virtual Result<void> getPcStatus(NativePCStatus& buf) const = 0;
    virtual Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const = 0;

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
    Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const override;
    Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const override;
    Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const override;
    Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const override;
    Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const override;
    Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const override;
    Result<void> getConfigBuffer(NativeConfigBuffer& buf) const override;
    Result<void> getPcStatus(NativePCStatus& buf) const override;
    Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const override;

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
    Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const override;
    Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const override;
    Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const override;
    Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const override;
    Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const override;
    Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const override;
    Result<void> getConfigBuffer(NativeConfigBuffer& buf) const override;
    Result<void> getPcStatus(NativePCStatus& buf) const override;
    Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const override;

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
    Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const override;
    Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const override;
    Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const override;
    Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const override;
    Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const override;
    Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const override;
    Result<void> getConfigBuffer(NativeConfigBuffer& buf) const override;
    Result<void> getPcStatus(NativePCStatus& buf) const override;
    Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const override;

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
    Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const override;
    Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const override;
    Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const override;
    Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const override;
    Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const override;
    Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const override;
    Result<void> getConfigBuffer(NativeConfigBuffer& buf) const override;
    Result<void> getPcStatus(NativePCStatus& buf) const override;
    Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const override;

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
