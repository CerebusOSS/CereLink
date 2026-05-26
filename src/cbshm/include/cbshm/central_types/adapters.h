///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   adapters.h
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Adapters for Central-compatible shared memory access
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbproto/types.h>
#include <cbproto/config.h>
#include <cbshm/shmem_session.h>
#include <cbshm/central_types/v4_2.h>
#include <cbshm/central_types/v4_1.h>
#include <cbshm/central_types/v4_0.h>
#include <cbshm/central_types/v3_11.h>

#ifndef CBSHM_CENTRAL_TYPES_ADAPTERS_H
#define CBSHM_CENTRAL_TYPES_ADAPTERS_H 

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Base-class adapter for Central-compatible shared memory access
///
class CentralAdapterBase {
public:
    virtual ~CentralAdapterBase() = default;

    /// Config read operations
    virtual Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const = 0;
    virtual Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank) const = 0;
    virtual Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter) const = 0;
    virtual Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel) const = 0;
    virtual Result<::cbPKT_SYSINFO> getSysInfo() const = 0;
    virtual Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const = 0;

    /// Config write operations
    virtual Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) = 0;
    virtual Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) = 0;
    virtual Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) = 0;
    virtual Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) = 0;
    virtual Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) = 0;
    virtual Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) = 0;
};

namespace central_v4_2 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    CentralLegacyCFGBUFF* cfg;

public:
    Adapter(void* cfg_ptr);
    ~Adapter() = default;

    /// Config read operations
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
};

} // namespace central_v4_2

namespace central_v4_1 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    CentralLegacyCFGBUFF* cfg;

public:
    Adapter(void* cfg_ptr);
    ~Adapter() = default;

    /// Config read operations
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
};

} // namespace central_v4_1

namespace central_v4_0 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    CentralLegacyCFGBUFF* cfg;

public:
    Adapter(void* cfg_ptr);
    ~Adapter() = default;

    /// Config read operations
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
};

} // namespace central_v4_0

namespace central_v3_11 {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    CentralLegacyCFGBUFF* cfg;

public:
    Adapter(void* cfg_ptr);
    ~Adapter() = default;

    /// Config read operations
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
};

} // namespace central_v3_11

} // namespace cbshm

#endif // CBSHM_CENTRAL_TYPES_ADAPTERS_H
