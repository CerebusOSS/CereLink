
/// @file   base.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Base classes for the Central-compatible shared memory access adapters
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_ADAPTERS_BASE_H
#define CBSHM_CENTRAL_ADAPTERS_BASE_H 

#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cbutil/result.h>
#include <cbshm/native_types.h>
#include <cbproto/types.h>
#include <cbproto/config.h>

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
    virtual cbutil::Result<void> getProcInfo(::cbPKT_PROCINFO& buf) const = 0;
    virtual cbutil::Result<void> getBankInfo(::cbPKT_BANKINFO& buf, uint32_t bank_num) const = 0;
    virtual cbutil::Result<void> getFilterInfo(::cbPKT_FILTINFO& buf, uint32_t filter_num) const = 0;
    virtual cbutil::Result<void> getChanInfo(::cbPKT_CHANINFO& buf, uint32_t channel_idx) const = 0;
    virtual cbutil::Result<void> getSysInfo(::cbPKT_SYSINFO& buf) const = 0;
    virtual cbutil::Result<void> getGroupInfo(::cbPKT_GROUPINFO& buf, uint32_t group_idx) const = 0;
    virtual cbutil::Result<void> getConfigBuffer(NativeConfigBuffer& buf) const = 0;
    virtual cbutil::Result<void> getPcStatus(NativePCStatus& buf) const = 0;
    virtual cbutil::Result<void> getSpikeCache(NativeSpikeCache& buf, uint32_t channel_idx) const = 0;

    /// Config write operations
    virtual cbutil::Result<void> setProcInfo(const ::cbPKT_PROCINFO& info) = 0;
    virtual cbutil::Result<void> setBankInfo(uint32_t bank_num, const ::cbPKT_BANKINFO& info) = 0;
    virtual cbutil::Result<void> setFilterInfo(uint32_t filter_num, const ::cbPKT_FILTINFO& info) = 0;
    virtual cbutil::Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) = 0;
    virtual cbutil::Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) = 0;
    virtual cbutil::Result<void> setGroupInfo(uint32_t group_idx, const ::cbPKT_GROUPINFO& info) = 0;
    virtual cbutil::Result<void> setNspStatus(const NativeNSPStatus& status) const = 0;
    virtual cbutil::Result<void> setGeminiSystem(bool is_gemini) const = 0;
};

} // namespace cbshm

#endif // CBSHM_CENTRAL_ADAPTERS_BASE_H 
