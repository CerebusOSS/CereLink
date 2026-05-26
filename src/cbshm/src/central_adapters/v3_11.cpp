///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   v3_11.cpp
/// @author Caden Shmookler
/// @date   2026-05-22
///
/// @brief  Central adapter implementation
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbshm/central_types/adapters.h>
#include <cbshm/central_types/translators.h>
#include <cbshm/central_types/v3_11.h>

namespace cbshm {

namespace central_v3_11 {

Adapter::Adapter(void* cfg_ptr) : cfg(static_cast<CentralLegacyCFGBUFF*>(cfg_ptr)) {
}

Result<::cbPKT_PROCINFO> Adapter::getProcInfo(uint8_t instrument_idx) const {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<::cbPKT_PROCINFO>::error("Instrument index out of range");
    }
    return Result<::cbPKT_PROCINFO>::ok(fromLegacy(cfg->procinfo[instrument_idx]));
}

Result<::cbPKT_BANKINFO> Adapter::getBankInfo(uint8_t instrument_idx, uint32_t bank_num) const {
    if (instrument_idx >= std::size(cfg->bankinfo)) {
        return Result<::cbPKT_BANKINFO>::error("Instrument index out of range");
    }
    uint32_t bank_idx = bank_num - 1;
    if (bank_idx >= std::size(cfg->bankinfo[0])) {
        return Result<::cbPKT_BANKINFO>::error("Bank number out of range");
    }
    return Result<::cbPKT_BANKINFO>::ok(fromLegacy(cfg->bankinfo[instrument_idx][bank_idx]));
}

Result<::cbPKT_FILTINFO> Adapter::getFilterInfo(uint8_t instrument_idx, uint32_t filter_num) const {
    if (instrument_idx >= std::size(cfg->filtinfo)) {
        return Result<::cbPKT_FILTINFO>::error("Instrument index out of range");
    }
    uint32_t filter_idx = filter_num - 1;
    if (filter_idx >= std::size(cfg->filtinfo[0])) {
        return Result<::cbPKT_FILTINFO>::error("Filter number out of range");
    }
    return Result<::cbPKT_FILTINFO>::ok(fromLegacy(cfg->filtinfo[instrument_idx][filter_idx]));
}

Result<::cbPKT_CHANINFO> Adapter::getChanInfo(uint32_t channel_idx) const {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<::cbPKT_CHANINFO>::error("Channel number out of range");
    }
    return Result<::cbPKT_CHANINFO>::ok(fromLegacy(cfg->chaninfo[channel_idx]));
}

Result<::cbPKT_SYSINFO> Adapter::getSysInfo() const {
    return Result<::cbPKT_SYSINFO>::ok(fromLegacy(cfg->sysinfo));
}

Result<::cbPKT_GROUPINFO> Adapter::getGroupInfo(uint8_t instrument_idx, uint32_t group_idx) const {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<::cbPKT_GROUPINFO>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<::cbPKT_GROUPINFO>::error("Group index out of range");
    }
    return Result<::cbPKT_GROUPINFO>::ok(fromLegacy(cfg->groupinfo[instrument_idx][group_idx]));
}

Result<void> Adapter::setProcInfo(uint8_t instrument_idx, const ::cbPKT_PROCINFO& info) {
    if (instrument_idx >= std::size(cfg->procinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    cfg->procinfo[instrument_idx] = toLegacy(info);
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
    cfg->bankinfo[instrument_idx][bank_idx] = toLegacy(info);
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
    cfg->filtinfo[instrument_idx][filter_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setChanInfo(uint32_t channel_idx, const ::cbPKT_CHANINFO& info) {
    if (channel_idx >= std::size(cfg->chaninfo)) {
        return Result<void>::error("Channel number out of range");
    }
    cfg->chaninfo[channel_idx] = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setSysInfo(const ::cbPKT_SYSINFO& info) {
    cfg->sysinfo = toLegacy(info);
    return Result<void>::ok();
}

Result<void> Adapter::setGroupInfo(uint8_t instrument_idx, uint32_t group_idx, const ::cbPKT_GROUPINFO& info) {
    if (instrument_idx >= std::size(cfg->groupinfo)) {
        return Result<void>::error("Instrument index out of range");
    }
    if (group_idx >= std::size(cfg->groupinfo[0])) {
        return Result<void>::error("Group index out of range");
    }
    cfg->groupinfo[instrument_idx][group_idx] = toLegacy(info);
    return Result<void>::ok();
}

} // namespace central_v3_11

} // namespace cbshm
