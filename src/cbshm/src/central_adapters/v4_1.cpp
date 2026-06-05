///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v4_1.cpp
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Central adapter implementation
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbshm/central_types/adapters.h>
#include <cbshm/central_types/translators.h>
#include <cbshm/central_types/v4_1.h>

namespace cbshm {

namespace central_v4_1 {

size_t BootstrapAdapter::getConfigBufferSize() const {
    return sizeof(cbCFGBUFF);
}

size_t BootstrapAdapter::getReceiveBufferSize() const {
    return sizeof(cbRECBUFF);
}

size_t BootstrapAdapter::getTransmitBufferSize() const {
    return sizeof(cbXMTBUFF);
}

size_t BootstrapAdapter::getTransmitBufferLocalSize() const {
    return sizeof(cbXMTBUFFLOCAL);
}

size_t BootstrapAdapter::getStatusBufferSize() const {
    return sizeof(cbPcStatus);
}

size_t BootstrapAdapter::getSpikeBufferSize() const {
    return sizeof(cbSPKBUFF);
}

size_t BootstrapAdapter::getReceiveBufferLen() const {
    return sizeof(cbRECBUFFLEN);
}

Adapter::Adapter(void* cfg_ptr, void* rec_ptr, void* xmt_ptr, void* xmt_local_ptr, void* status_ptr, void* spike_ptr)
    : cfg(static_cast<cbCFGBUFF*>(cfg_ptr))
    , rec(static_cast<cbRECBUFF*>(rec_ptr))
    , xmt(static_cast<cbXMTBUFF*>(xmt_ptr))
    , xmt_local(static_cast<cbXMTBUFFLOCAL*>(xmt_local_ptr))
    , status(static_cast<cbPcStatus*>(status_ptr))
    , spike(static_cast<cbSPKBUFF*>(spike_ptr))
{}

uint32_t& Adapter::getRecReceived() {
    return rec->received;
}

uint64_t Adapter::getRecLasttime() {
    return rec->lasttime;
}

void Adapter::setRecLasttime(uint64_t lasttime) {
    rec->lasttime = lasttime;
}

uint32_t& Adapter::getRecHeadwrapPtr() {
    return rec->headwrap;
}

uint32_t& Adapter::getRecHeadindexPtr() {
    return rec->headindex;
}

uint32_t* Adapter::getRecBufferPtr() {
    return rec->buffer;
}

uint32_t& Adapter::getXmtTransmittedPtr() {
    return xmt->transmitted;
}

uint32_t& Adapter::getXmtHeadindexPtr() {
    return xmt->headindex;
}

uint32_t& Adapter::getXmtTailindexPtr() {
    return xmt->tailindex;
}

uint32_t& Adapter::getXmtLastValidIndexPtr() {
    return xmt->last_valid_index;
}

uint32_t& Adapter::getXmtBufferlenPtr() {
    return xmt->bufferlen;
}

uint32_t* Adapter::getXmtBufferPtr() {
    return xmt->buffer;
}

uint32_t& Adapter::getLocalXmtTransmittedPtr() {
    return xmt->transmitted;
}

uint32_t& Adapter::getLocalXmtHeadindexPtr() {
    return xmt->headindex;
}

uint32_t& Adapter::getLocalXmtTailindexPtr() {
    return xmt->tailindex;
}

uint32_t& Adapter::getLocalXmtLastValidIndexPtr() {
    return xmt->last_valid_index;
}

uint32_t& Adapter::getLocalXmtBufferlenPtr() {
    return xmt->bufferlen;
}

uint32_t* Adapter::getLocalXmtBufferPtr() {
    return xmt->buffer;
}

Result<::cbPKT_PROCINFO> Adapter::getProcInfo(uint8_t instrument_idx) const {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<::cbPKT_PROCINFO>::error("Instrument index out of range");
    }
    return Result<::cbPKT_PROCINFO>::ok(fromLegacy(cfg->procinfo[instrument_idx], {}));
}

Result<::cbPKT_BANKINFO> Adapter::getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const {
    if (instrument_idx >= std::size(cfg->bankinfo)) {
        return Result<::cbPKT_BANKINFO>::error("Instrument index out of range");
    }
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<::cbPKT_BANKINFO>::error("Bank number out of range");
    }
    return Result<::cbPKT_BANKINFO>::ok(fromLegacy(cfg->bankinfo[instrument_idx][bank_idx], {}));
}

Result<::cbPKT_FILTINFO> Adapter::getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const {
    if (instrument_idx >= std::size(cfg->filtinfo)) {
        return Result<::cbPKT_FILTINFO>::error("Instrument index out of range");
    }
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<::cbPKT_FILTINFO>::error("Filter number out of range");
    }
    return Result<::cbPKT_FILTINFO>::ok(fromLegacy(cfg->filtinfo[instrument_idx][filter_idx], {}));
}

Result<::cbPKT_CHANINFO> Adapter::getChanInfo(uint32_t channel_idx) const {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<::cbPKT_CHANINFO>::error("Channel number out of range");
    }
    return Result<::cbPKT_CHANINFO>::ok(fromLegacy(cfg->chaninfo[channel_idx], {}));
}

Result<::cbPKT_SYSINFO> Adapter::getSysInfo() const {
    return Result<::cbPKT_SYSINFO>::ok(fromLegacy(cfg->sysinfo, {}));
}

Result<::cbPKT_GROUPINFO> Adapter::getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<::cbPKT_GROUPINFO>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<::cbPKT_GROUPINFO>::error("Group index out of range");
    }
    return Result<::cbPKT_GROUPINFO>::ok(fromLegacy(cfg->groupinfo[instrument_idx][group_idx], {}));
}

Result<NativeConfigBuffer> Adapter::getConfigBuffer(uint8_t instrument_idx) const {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<NativeConfigBuffer>::error("Instrument index out of range");
    }
    return Result<NativeConfigBuffer>::ok(fromLegacy(*cfg, { .instrument_idx =  instrument_idx }));
}

Result<NativePCStatus> Adapter::getPcStatus(uint8_t instrument_idx) const {
    if (instrument_idx >= std::size(status->isSelection)) {
        return Result<NativePCStatus>::error("Instrument index out of range");
    }
    return Result<NativePCStatus>::ok(fromLegacy(*status, { .instrument_idx = instrument_idx }));
}

Result<void> Adapter::setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    cfg->procinfo[instrument_idx] = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setBankInfo(uint8_t instrument_idx, uint32_t bank_num, const ::cbPKT_BANKINFO& info) {
    if (instrument_idx >= std::size(cfg->bankinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<void>::error("Bank number out of range");
    }
    cfg->bankinfo[instrument_idx][bank_idx] = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setFilterInfo(uint8_t instrument_idx, uint32_t filter_num, const ::cbPKT_FILTINFO& info) {
    if (instrument_idx >= std::size(cfg->filtinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<void>::error("Filter number out of range");
    }
    cfg->filtinfo[instrument_idx][filter_idx] = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<void>::error("Channel number out of range");
    }
    cfg->chaninfo[channel_idx] = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setSysInfo(const ::cbPKT_SYSINFO& info) {
    cfg->sysinfo = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<void>::error("Group index out of range");
    }
    cfg->groupinfo[instrument_idx][group_idx] = toLegacy(info, {});
    return Result<void>::ok();
}

Result<void> Adapter::setPcStatus(uint8_t instrument_idx, const NativePCStatus& status) const {
    if (instrument_idx >= std::size(this->status->isSelection)) {
        return Result<void>::error("Instrument index out of range");
    }
    *(this->status) = toLegacy(status, { .instrument_idx = instrument_idx });
    return Result<void>::ok();
}

} // namespace central_v4_1

} // namespace cbshm
