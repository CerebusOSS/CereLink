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

#include <cbproto/types.h>
#include <cbproto/config.h>
#include <cbshm/shmem_session.h>
#include <cbshm/central_types/v4_2.h>
#include <cbshm/central_types/v4_1.h>
#include <cbshm/central_types/v4_0.h>
#include <cbshm/central_types/v3_11.h>

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
public:
    virtual ~CentralAdapterBase() = default;

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
    virtual Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const = 0;
    virtual Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank) const = 0;
    virtual Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter) const = 0;
    virtual Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const = 0;
    virtual Result<::cbPKT_SYSINFO> getSysInfo() const = 0;
    virtual Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const = 0;
    virtual Result<NativeConfigBuffer> getConfigBuffer(uint8_t instrument_idx) const = 0;
    virtual Result<NativePCStatus> getPcStatus(uint8_t instrument_idx) const = 0;

    /// Config write operations
    virtual Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) = 0;
    virtual Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) = 0;
    virtual Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) = 0;
    virtual Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) = 0;
    virtual Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) = 0;
    virtual Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) = 0;
    virtual Result<void> setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const = 0; // TODO: currently single-instrument only!  needs to be multi-instrument safe
};

namespace central_v4_2 {

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    cbCFGBUFF* cfg;
    cbRECBUFF* rec;
    cbXMTBUFF* xmt;
    cbXMTBUFFLOCAL* xmt_local;
    cbPcStatus* status;
    cbSPKBUFF* spike;

public:
    Adapter(void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr);
    ~Adapter() = default;

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
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer(uint8_t instrument_idx) const override;
    Result<NativePCStatus> getPcStatus(uint8_t instrument_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const override;
};

} // namespace central_v4_2

namespace central_v4_1 {

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    cbCFGBUFF* cfg;
    cbRECBUFF* rec;
    cbXMTBUFF* xmt;
    cbXMTBUFFLOCAL* xmt_local;
    cbPcStatus* status;
    cbSPKBUFF* spike;

public:
    Adapter(void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr);
    ~Adapter() = default;

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
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer(uint8_t instrument_idx) const override;
    Result<NativePCStatus> getPcStatus(uint8_t instrument_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const override;
};

} // namespace central_v4_1

namespace central_v4_0 {

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    cbCFGBUFF* cfg;
    cbRECBUFF* rec;
    cbXMTBUFF* xmt;
    cbXMTBUFFLOCAL* xmt_local;
    cbPcStatus* status;
    cbSPKBUFF* spike;

public:
    Adapter(void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr);
    ~Adapter() = default;

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
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer(uint8_t instrument_idx) const override;
    Result<NativePCStatus> getPcStatus(uint8_t instrument_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const override;
};

} // namespace central_v4_0

namespace central_v3_11 {

///////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Adapter for Central-compatible shared memory access
///
class Adapter : public CentralAdapterBase {
private:
    cbCFGBUFF* cfg;
    cbRECBUFF* rec;
    cbXMTBUFF* xmt;
    cbXMTBUFFLOCAL* xmt_local;
    cbPcStatus* status;
    cbSPKBUFF* spike;

public:
    Adapter(void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr);
    ~Adapter() = default;

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
    Result<::cbPKT_PROCINFO> getProcInfo(uint8_t instrument_idx) const override;
    Result<::cbPKT_BANKINFO> getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const override;
    Result<::cbPKT_FILTINFO> getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const override;
    Result<::cbPKT_CHANINFO> getChanInfo(uint32_t channel_idx) const override;
    Result<::cbPKT_SYSINFO> getSysInfo() const override;
    Result<::cbPKT_GROUPINFO> getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const override;
    Result<NativeConfigBuffer> getConfigBuffer(uint8_t instrument_idx) const override;
    Result<NativePCStatus> getPcStatus(uint8_t instrument_idx) const override;

    /// Config write operations
    Result<void> setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) override;
    Result<void> setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) override;
    Result<void> setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) override;
    Result<void> setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) override;
    Result<void> setSysInfo(const ::cbPKT_SYSINFO& info) override;
    Result<void> setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) override;
    Result<void> setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const override;
};

} // namespace central_v3_11

} // namespace cbshm

#endif // CBSHM_CENTRAL_TYPES_ADAPTERS_H
