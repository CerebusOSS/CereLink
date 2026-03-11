///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cbsdk.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  C API implementation
///
/// Wraps the C++ SdkSession API with a C interface for language bindings
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include "cbsdk/cbsdk.h"
#include "cbsdk/sdk_session.h"
#include <chrono>
#include <cstring>
#include <memory>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Internal Structures
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Internal session implementation (opaque to C users)
struct cbsdk_session_impl {
    std::unique_ptr<cbsdk::SdkSession> cpp_session;

    // C callback storage
    cbsdk_packet_callback_fn packet_callback;
    void* packet_callback_user_data;
    cbsdk::CallbackHandle packet_callback_handle;

    cbsdk_error_callback_fn error_callback;
    void* error_callback_user_data;

    cbsdk_session_impl()
        : packet_callback(nullptr)
        , packet_callback_user_data(nullptr)
        , packet_callback_handle(0)
        , error_callback(nullptr)
        , error_callback_user_data(nullptr)
    {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Convert C config to C++ config
static cbsdk::SdkConfig to_cpp_config(const cbsdk_config_t* c_config) {
    cbsdk::SdkConfig cpp_config;

    // Map device type
    switch (c_config->device_type) {
        case CBPROTO_DEVICE_TYPE_LEGACY_NSP:
            cpp_config.device_type = cbsdk::DeviceType::LEGACY_NSP;
            break;
        case CBPROTO_DEVICE_TYPE_NSP:
            cpp_config.device_type = cbsdk::DeviceType::NSP;
            break;
        case CBPROTO_DEVICE_TYPE_HUB1:
            cpp_config.device_type = cbsdk::DeviceType::HUB1;
            break;
        case CBPROTO_DEVICE_TYPE_HUB2:
            cpp_config.device_type = cbsdk::DeviceType::HUB2;
            break;
        case CBPROTO_DEVICE_TYPE_HUB3:
            cpp_config.device_type = cbsdk::DeviceType::HUB3;
            break;
        case CBPROTO_DEVICE_TYPE_NPLAY:
            cpp_config.device_type = cbsdk::DeviceType::NPLAY;
            break;
        case CBPROTO_DEVICE_TYPE_CUSTOM:
            // CUSTOM not supported in SDK config - use custom address/port fields instead
            cpp_config.device_type = cbsdk::DeviceType::LEGACY_NSP;
            break;
    }

    cpp_config.callback_queue_depth = c_config->callback_queue_depth;
    cpp_config.enable_realtime_priority = c_config->enable_realtime_priority;
    cpp_config.drop_on_overflow = c_config->drop_on_overflow;

    cpp_config.recv_buffer_size = c_config->recv_buffer_size;
    cpp_config.non_blocking = c_config->non_blocking;

    // Optional custom addresses/ports
    if (c_config->custom_device_address != nullptr) {
        cpp_config.custom_device_address = c_config->custom_device_address;
    }
    if (c_config->custom_client_address != nullptr) {
        cpp_config.custom_client_address = c_config->custom_client_address;
    }
    if (c_config->custom_device_port != 0) {
        cpp_config.custom_device_port = c_config->custom_device_port;
    }
    if (c_config->custom_client_port != 0) {
        cpp_config.custom_client_port = c_config->custom_client_port;
    }

    return cpp_config;
}

/// Convert C++ stats to C stats
static void to_c_stats(const cbsdk::SdkStats& cpp_stats, cbsdk_stats_t* c_stats) {
    c_stats->packets_received_from_device = cpp_stats.packets_received_from_device;
    c_stats->bytes_received_from_device = cpp_stats.bytes_received_from_device;
    c_stats->packets_stored_to_shmem = cpp_stats.packets_stored_to_shmem;
    c_stats->packets_queued_for_callback = cpp_stats.packets_queued_for_callback;
    c_stats->packets_delivered_to_callback = cpp_stats.packets_delivered_to_callback;
    c_stats->packets_dropped = cpp_stats.packets_dropped;
    c_stats->queue_current_depth = cpp_stats.queue_current_depth;
    c_stats->queue_max_depth = cpp_stats.queue_max_depth;
    c_stats->packets_sent_to_device = cpp_stats.packets_sent_to_device;
    c_stats->shmem_store_errors = cpp_stats.shmem_store_errors;
    c_stats->receive_errors = cpp_stats.receive_errors;
    c_stats->send_errors = cpp_stats.send_errors;
}

/// Convert C channel type enum to C++ ChannelType enum
static cbsdk::ChannelType to_cpp_channel_type(cbproto_channel_type_t c_type) {
    switch (c_type) {
        case CBPROTO_CHANNEL_TYPE_FRONTEND:    return cbsdk::ChannelType::FRONTEND;
        case CBPROTO_CHANNEL_TYPE_ANALOG_IN:   return cbsdk::ChannelType::ANALOG_IN;
        case CBPROTO_CHANNEL_TYPE_ANALOG_OUT:  return cbsdk::ChannelType::ANALOG_OUT;
        case CBPROTO_CHANNEL_TYPE_AUDIO:       return cbsdk::ChannelType::AUDIO;
        case CBPROTO_CHANNEL_TYPE_DIGITAL_IN:  return cbsdk::ChannelType::DIGITAL_IN;
        case CBPROTO_CHANNEL_TYPE_SERIAL:      return cbsdk::ChannelType::SERIAL;
        case CBPROTO_CHANNEL_TYPE_DIGITAL_OUT: return cbsdk::ChannelType::DIGITAL_OUT;
        default:                               return cbsdk::ChannelType::ANY;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// C API Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {

cbsdk_config_t cbsdk_config_default(void) {
    cbsdk_config_t config;
    config.device_type = CBPROTO_DEVICE_TYPE_LEGACY_NSP;
    config.callback_queue_depth = 16384;
    config.enable_realtime_priority = false;
    config.drop_on_overflow = true;
    config.recv_buffer_size = 6000000;
    config.non_blocking = true;
    config.custom_device_address = nullptr;
    config.custom_client_address = nullptr;
    config.custom_device_port = 0;
    config.custom_client_port = 0;
    return config;
}

cbsdk_result_t cbsdk_session_create(cbsdk_session_t* session, const cbsdk_config_t* config) {
    if (!session || !config) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }

    try {
        // Create internal implementation
        auto impl = std::make_unique<cbsdk_session_impl>();

        // Convert config and create C++ session
        cbsdk::SdkConfig cpp_config = to_cpp_config(config);
        auto result = cbsdk::SdkSession::create(cpp_config);

        if (result.isError()) {
            // Classify error
            const std::string& error = result.error();
            if (error.find("shared memory") != std::string::npos) {
                return CBSDK_RESULT_SHMEM_ERROR;
            } else if (error.find("device") != std::string::npos) {
                return CBSDK_RESULT_DEVICE_ERROR;
            } else {
                return CBSDK_RESULT_INTERNAL_ERROR;
            }
        }

        impl->cpp_session = std::make_unique<cbsdk::SdkSession>(std::move(result.value()));

        *session = impl.release();
        return CBSDK_RESULT_SUCCESS;

    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

void cbsdk_session_destroy(cbsdk_session_t session) {
    if (session) {
        delete session;
    }
}

cbsdk_result_t cbsdk_session_start(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }

    try {
        auto result = session->cpp_session->start();
        if (result.isError()) {
            if (result.error().find("already running") != std::string::npos) {
                // Session is already started (by create() or previous start() call)
                // Return SUCCESS to make this call idempotent
                return CBSDK_RESULT_SUCCESS;
            }
            return CBSDK_RESULT_INTERNAL_ERROR;
        }
        return CBSDK_RESULT_SUCCESS;

    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

void cbsdk_session_stop(cbsdk_session_t session) {
    if (session && session->cpp_session) {
        try {
            session->cpp_session->stop();
        } catch (...) {
            // Swallow exceptions in stop()
        }
    }
}

bool cbsdk_session_is_running(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return false;
    }

    try {
        return session->cpp_session->isRunning();
    } catch (...) {
        return false;
    }
}

void cbsdk_session_set_packet_callback(cbsdk_session_t session,
                                        cbsdk_packet_callback_fn callback,
                                        void* user_data) {
    if (!session || !session->cpp_session) {
        return;
    }

    try {
        // Unregister previous callback if any
        if (session->packet_callback_handle != 0) {
            session->cpp_session->unregisterCallback(session->packet_callback_handle);
            session->packet_callback_handle = 0;
        }

        session->packet_callback = callback;
        session->packet_callback_user_data = user_data;

        if (callback) {
            // Register per-packet callback, forward to C batch-style callback with count=1
            session->packet_callback_handle = session->cpp_session->registerPacketCallback(
                [session](const cbPKT_GENERIC& pkt) {
                    if (session->packet_callback) {
                        session->packet_callback(&pkt, 1, session->packet_callback_user_data);
                    }
                }
            );
        }

    } catch (...) {
        // Swallow exceptions
    }
}

void cbsdk_session_set_error_callback(cbsdk_session_t session,
                                       cbsdk_error_callback_fn callback,
                                       void* user_data) {
    if (!session || !session->cpp_session) {
        return;
    }

    try {
        // Store C callback
        session->error_callback = callback;
        session->error_callback_user_data = user_data;

        if (callback) {
            // Wrap C callback in C++ lambda
            session->cpp_session->setErrorCallback(
                [session](const std::string& error) {
                    if (session->error_callback) {
                        session->error_callback(error.c_str(), session->error_callback_user_data);
                    }
                }
            );
        } else {
            // Clear callback
            session->cpp_session->setErrorCallback(nullptr);
        }

    } catch (...) {
        // Swallow exceptions
    }
}

void cbsdk_session_get_stats(cbsdk_session_t session, cbsdk_stats_t* stats) {
    if (!session || !session->cpp_session || !stats) {
        return;
    }

    try {
        cbsdk::SdkStats cpp_stats = session->cpp_session->getStats();
        to_c_stats(cpp_stats, stats);
    } catch (...) {
        // Return zeroed stats on error
        std::memset(stats, 0, sizeof(cbsdk_stats_t));
    }
}

void cbsdk_session_reset_stats(cbsdk_session_t session) {
    if (session && session->cpp_session) {
        try {
            session->cpp_session->resetStats();
        } catch (...) {
            // Swallow exceptions
        }
    }
}

const char* cbsdk_get_error_message(cbsdk_result_t result) {
    switch (result) {
        case CBSDK_RESULT_SUCCESS:
            return "Success";
        case CBSDK_RESULT_INVALID_PARAMETER:
            return "Invalid parameter (NULL pointer or invalid value)";
        case CBSDK_RESULT_ALREADY_RUNNING:
            return "Session is already running";
        case CBSDK_RESULT_NOT_RUNNING:
            return "Session is not running";
        case CBSDK_RESULT_SHMEM_ERROR:
            return "Shared memory error";
        case CBSDK_RESULT_DEVICE_ERROR:
            return "Device connection error";
        case CBSDK_RESULT_INTERNAL_ERROR:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

const char* cbsdk_get_version(void) {
    return "2.0.0";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Typed Callback Registration
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_callback_handle_t cbsdk_session_register_packet_callback(
    cbsdk_session_t session,
    cbsdk_packet_callback_fn callback,
    void* user_data) {
    if (!session || !session->cpp_session || !callback) {
        return 0;
    }
    try {
        return session->cpp_session->registerPacketCallback(
            [callback, user_data](const cbPKT_GENERIC& pkt) {
                callback(&pkt, 1, user_data);
            }
        );
    } catch (...) {
        return 0;
    }
}

cbsdk_callback_handle_t cbsdk_session_register_event_callback(
    cbsdk_session_t session,
    cbproto_channel_type_t channel_type,
    cbsdk_event_callback_fn callback,
    void* user_data) {
    if (!session || !session->cpp_session || !callback) {
        return 0;
    }
    try {
        // Use (int)channel_type == -1 as sentinel for ChannelType::ANY
        cbsdk::ChannelType cpp_type = (static_cast<int>(channel_type) == -1)
            ? cbsdk::ChannelType::ANY
            : to_cpp_channel_type(channel_type);
        return session->cpp_session->registerEventCallback(cpp_type,
            [callback, user_data](const cbPKT_GENERIC& pkt) {
                callback(&pkt, user_data);
            }
        );
    } catch (...) {
        return 0;
    }
}

cbsdk_callback_handle_t cbsdk_session_register_group_callback(
    cbsdk_session_t session,
    cbproto_group_rate_t rate,
    cbsdk_group_callback_fn callback,
    void* user_data) {
    if (!session || !session->cpp_session || !callback) {
        return 0;
    }
    try {
        return session->cpp_session->registerGroupCallback(
            static_cast<cbsdk::SampleRate>(rate),
            [callback, user_data](const cbPKT_GROUP& pkt) {
                callback(&pkt, user_data);
            }
        );
    } catch (...) {
        return 0;
    }
}

cbsdk_callback_handle_t cbsdk_session_register_config_callback(
    cbsdk_session_t session,
    uint16_t packet_type,
    cbsdk_config_callback_fn callback,
    void* user_data) {
    if (!session || !session->cpp_session || !callback) {
        return 0;
    }
    try {
        return session->cpp_session->registerConfigCallback(packet_type,
            [callback, user_data](const cbPKT_GENERIC& pkt) {
                callback(&pkt, user_data);
            }
        );
    } catch (...) {
        return 0;
    }
}

void cbsdk_session_unregister_callback(cbsdk_session_t session,
                                        cbsdk_callback_handle_t handle) {
    if (!session || !session->cpp_session || handle == 0) {
        return;
    }
    try {
        session->cpp_session->unregisterCallback(handle);
    } catch (...) {
        // Swallow exceptions
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access
///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t cbsdk_session_get_runlevel(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return 0;
    }
    try {
        return session->cpp_session->getRunLevel();
    } catch (...) {
        return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Information Accessors
///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t cbsdk_get_max_chans(void) {
    return cbMAXCHANS;
}

uint32_t cbsdk_get_num_fe_chans(void) {
    return cbNUM_FE_CHANS;
}

uint32_t cbsdk_get_num_analog_chans(void) {
    return cbNUM_ANALOG_CHANS;
}

const char* cbsdk_session_get_channel_label(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) {
        return nullptr;
    }
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->label : nullptr;
    } catch (...) {
        return nullptr;
    }
}

uint32_t cbsdk_session_get_channel_smpgroup(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) {
        return 0;
    }
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->smpgroup : 0;
    } catch (...) {
        return 0;
    }
}

uint32_t cbsdk_session_get_channel_chancaps(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) {
        return 0;
    }
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->chancaps : 0;
    } catch (...) {
        return 0;
    }
}

cbproto_channel_type_t cbsdk_session_get_channel_type(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) {
        return static_cast<cbproto_channel_type_t>(-1);
    }
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        if (!info) return static_cast<cbproto_channel_type_t>(-1);

        const uint32_t caps = info->chancaps;
        if ((cbCHAN_EXISTS | cbCHAN_CONNECTED) != (caps & (cbCHAN_EXISTS | cbCHAN_CONNECTED)))
            return static_cast<cbproto_channel_type_t>(-1);

        if ((cbCHAN_AINP | cbCHAN_ISOLATED) == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED)))
            return CBPROTO_CHANNEL_TYPE_FRONTEND;
        if (cbCHAN_AINP == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED)))
            return CBPROTO_CHANNEL_TYPE_ANALOG_IN;
        if (cbCHAN_AOUT == (caps & cbCHAN_AOUT)) {
            if (cbAOUT_AUDIO == (info->aoutcaps & cbAOUT_AUDIO))
                return CBPROTO_CHANNEL_TYPE_AUDIO;
            return CBPROTO_CHANNEL_TYPE_ANALOG_OUT;
        }
        if (cbCHAN_DINP == (caps & cbCHAN_DINP)) {
            if (info->dinpcaps & cbDINP_SERIALMASK)
                return CBPROTO_CHANNEL_TYPE_SERIAL;
            return CBPROTO_CHANNEL_TYPE_DIGITAL_IN;
        }
        if (cbCHAN_DOUT == (caps & cbCHAN_DOUT))
            return CBPROTO_CHANNEL_TYPE_DIGITAL_OUT;

        return static_cast<cbproto_channel_type_t>(-1);
    } catch (...) {
        return static_cast<cbproto_channel_type_t>(-1);
    }
}

uint32_t cbsdk_session_get_channel_smpfilter(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->smpfilter : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_channel_spkfilter(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->spkfilter : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_channel_spkopts(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->spkopts : 0;
    } catch (...) { return 0; }
}

int32_t cbsdk_session_get_channel_spkthrlevel(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->spkthrlevel : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_channel_ainpopts(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->ainpopts : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_channel_lncrate(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->lncrate : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_channel_refelecchan(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->refelecchan : 0;
    } catch (...) { return 0; }
}

int16_t cbsdk_session_get_channel_amplrejpos(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->amplrejpos : 0;
    } catch (...) { return 0; }
}

int16_t cbsdk_session_get_channel_amplrejneg(cbsdk_session_t session, uint32_t chan_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        return info ? info->amplrejneg : 0;
    } catch (...) { return 0; }
}

const char* cbsdk_session_get_group_label(cbsdk_session_t session, uint32_t group_id) {
    if (!session || !session->cpp_session) {
        return nullptr;
    }
    try {
        const cbPKT_GROUPINFO* info = session->cpp_session->getGroupInfo(group_id);
        return info ? info->label : nullptr;
    } catch (...) {
        return nullptr;
    }
}

cbsdk_result_t cbsdk_session_get_group_list(
    cbsdk_session_t session,
    uint32_t group_id,
    uint16_t* list,
    uint32_t* count) {
    if (!session || !session->cpp_session || !list || !count) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        const cbPKT_GROUPINFO* info = session->cpp_session->getGroupInfo(group_id);
        if (!info) {
            *count = 0;
            return CBSDK_RESULT_INVALID_PARAMETER;
        }
        uint32_t n = info->length;
        if (n > *count) n = *count;
        for (uint32_t i = 0; i < n; i++) {
            list[i] = info->list[i];
        }
        *count = n;
        return CBSDK_RESULT_SUCCESS;
    } catch (...) {
        *count = 0;
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_set_channel_sample_group(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    cbproto_group_rate_t rate,
    bool disable_others) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setChannelSampleGroup(
            n_chans, to_cpp_channel_type(chan_type), static_cast<cbsdk::SampleRate>(rate), disable_others);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_set_channel_config(
    cbsdk_session_t session,
    const cbPKT_CHANINFO* chaninfo) {
    if (!session || !session->cpp_session || !chaninfo) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setChannelConfig(*chaninfo);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

/// Helper: read-modify-write a channel config field
/// Gets current chaninfo from shared memory, calls `modify` on a copy, sends the packet.
static cbsdk_result_t modify_and_send_chaninfo(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint16_t pkt_type,
    std::function<void(cbPKT_CHANINFO&)> modify) {
    if (!session || !session->cpp_session) return CBSDK_RESULT_INVALID_PARAMETER;
    try {
        const cbPKT_CHANINFO* info = session->cpp_session->getChanInfo(chan_id);
        if (!info) return CBSDK_RESULT_INVALID_PARAMETER;
        cbPKT_CHANINFO ci = *info;
        ci.chan = chan_id;
        ci.cbpkt_header.type = pkt_type;
        modify(ci);
        auto r = session->cpp_session->sendPacket(
            reinterpret_cast<const cbPKT_GENERIC&>(ci));
        return r.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_set_channel_label(
    cbsdk_session_t session, uint32_t chan_id, const char* label) {
    if (!label) return CBSDK_RESULT_INVALID_PARAMETER;
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETLABEL,
        [label](cbPKT_CHANINFO& ci) {
            std::strncpy(ci.label, label, sizeof(ci.label) - 1);
            ci.label[sizeof(ci.label) - 1] = '\0';
        });
}

cbsdk_result_t cbsdk_session_set_channel_smpfilter(
    cbsdk_session_t session, uint32_t chan_id, uint32_t filter_id) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETSMP,
        [filter_id](cbPKT_CHANINFO& ci) {
            ci.smpfilter = filter_id;
        });
}

cbsdk_result_t cbsdk_session_set_channel_spkfilter(
    cbsdk_session_t session, uint32_t chan_id, uint32_t filter_id) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETSPK,
        [filter_id](cbPKT_CHANINFO& ci) {
            ci.spkfilter = filter_id;
        });
}

cbsdk_result_t cbsdk_session_set_channel_ainpopts(
    cbsdk_session_t session, uint32_t chan_id, uint32_t ainpopts) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETAINP,
        [ainpopts](cbPKT_CHANINFO& ci) {
            ci.ainpopts = ainpopts;
        });
}

cbsdk_result_t cbsdk_session_set_channel_lncrate(
    cbsdk_session_t session, uint32_t chan_id, uint32_t lncrate) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETAINP,
        [lncrate](cbPKT_CHANINFO& ci) {
            ci.lncrate = lncrate;
        });
}

cbsdk_result_t cbsdk_session_set_channel_spkopts(
    cbsdk_session_t session, uint32_t chan_id, uint32_t spkopts) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETSPKTHR,
        [spkopts](cbPKT_CHANINFO& ci) {
            ci.spkopts = spkopts;
        });
}

cbsdk_result_t cbsdk_session_set_channel_spkthrlevel(
    cbsdk_session_t session, uint32_t chan_id, int32_t level) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETSPKTHR,
        [level](cbPKT_CHANINFO& ci) {
            ci.spkthrlevel = level;
        });
}

cbsdk_result_t cbsdk_session_set_channel_autothreshold(
    cbsdk_session_t session, uint32_t chan_id, bool enabled) {
    return modify_and_send_chaninfo(session, chan_id, cbPKTTYPE_CHANSETAUTOTHRESHOLD,
        [enabled](cbPKT_CHANINFO& ci) {
            if (enabled)
                ci.spkopts |= cbAINPSPK_THRAUTO;
            else
                ci.spkopts &= ~cbAINPSPK_THRAUTO;
        });
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bulk Configuration Access
///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t cbsdk_session_get_sysfreq(cbsdk_session_t session) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_SYSINFO* info = session->cpp_session->getSysInfo();
        return info ? info->sysfreq : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_get_num_filters(void) {
    return cbMAXFILTS;
}

const char* cbsdk_session_get_filter_label(cbsdk_session_t session, uint32_t filter_id) {
    if (!session || !session->cpp_session) return nullptr;
    try {
        const cbPKT_FILTINFO* info = session->cpp_session->getFilterInfo(filter_id);
        return info ? info->label : nullptr;
    } catch (...) { return nullptr; }
}

uint32_t cbsdk_session_get_filter_hpfreq(cbsdk_session_t session, uint32_t filter_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_FILTINFO* info = session->cpp_session->getFilterInfo(filter_id);
        return info ? info->hpfreq : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_filter_hporder(cbsdk_session_t session, uint32_t filter_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_FILTINFO* info = session->cpp_session->getFilterInfo(filter_id);
        return info ? info->hporder : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_filter_lpfreq(cbsdk_session_t session, uint32_t filter_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_FILTINFO* info = session->cpp_session->getFilterInfo(filter_id);
        return info ? info->lpfreq : 0;
    } catch (...) { return 0; }
}

uint32_t cbsdk_session_get_filter_lporder(cbsdk_session_t session, uint32_t filter_id) {
    if (!session || !session->cpp_session) return 0;
    try {
        const cbPKT_FILTINFO* info = session->cpp_session->getFilterInfo(filter_id);
        return info ? info->lporder : 0;
    } catch (...) { return 0; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Commands
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_send_comment(
    cbsdk_session_t session,
    const char* comment,
    uint32_t rgba,
    uint8_t charset) {
    if (!session || !session->cpp_session || !comment) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->sendComment(comment, rgba, charset);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_send_packet(
    cbsdk_session_t session,
    const cbPKT_GENERIC* pkt) {
    if (!session || !session->cpp_session || !pkt) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->sendPacket(*pkt);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_set_digital_output(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint16_t value) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setDigitalOutput(chan_id, value);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_set_runlevel(
    cbsdk_session_t session,
    uint32_t runlevel) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setSystemRunLevel(runlevel);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Mapping (CMP) Files
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_load_channel_map(cbsdk_session_t session, const char* filepath, uint32_t bank_offset) {
    if (!session || !session->cpp_session || !filepath) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    auto result = session->cpp_session->loadChannelMap(filepath, bank_offset);
    if (result.isOk()) {
        return CBSDK_RESULT_SUCCESS;
    } else {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// CCF Configuration Files
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_save_ccf(cbsdk_session_t session, const char* filename) {
    if (!session || !session->cpp_session || !filename) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->saveCCF(filename);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_load_ccf(cbsdk_session_t session, const char* filename) {
    if (!session || !session->cpp_session || !filename) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->loadCCF(filename);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument Time
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_get_time(cbsdk_session_t session, uint64_t* time) {
    if (!session || !session->cpp_session || !time) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        *time = session->cpp_session->getTime();
        return CBSDK_RESULT_SUCCESS;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Patient Information
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_set_patient_info(
    cbsdk_session_t session,
    const char* id,
    const char* firstname,
    const char* lastname,
    uint32_t dob_month,
    uint32_t dob_day,
    uint32_t dob_year) {
    if (!session || !session->cpp_session || !id) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setPatientInfo(
            id,
            firstname ? firstname : "",
            lastname ? lastname : "",
            dob_month, dob_day, dob_year);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Analog Output
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_set_analog_output_monitor(
    cbsdk_session_t session,
    uint32_t aout_chan_id,
    uint32_t monitor_chan_id,
    bool track_last,
    bool spike_only) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setAnalogOutputMonitor(
            aout_chan_id, monitor_chan_id, track_last, spike_only);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Recording Control
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_start_central_recording(
    cbsdk_session_t session,
    const char* filename,
    const char* comment) {
    if (!session || !session->cpp_session || !filename) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        std::string comment_str = comment ? comment : "";
        auto result = session->cpp_session->startCentralRecording(filename, comment_str);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_stop_central_recording(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->stopCentralRecording();
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_open_central_file_dialog(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->openCentralFileDialog();
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_close_central_file_dialog(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->closeCentralFileDialog();
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Sorting
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_set_channel_spike_sorting(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    uint32_t sort_options) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setChannelSpikeSorting(
            n_chans, to_cpp_channel_type(chan_type), sort_options);
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Synchronization
///////////////////////////////////////////////////////////////////////////////////////////////////

cbsdk_result_t cbsdk_session_get_clock_offset(
    cbsdk_session_t session,
    int64_t* offset_ns) {
    if (!session || !session->cpp_session || !offset_ns) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->getClockOffsetNs();
        if (!result.has_value()) {
            return CBSDK_RESULT_NOT_RUNNING;
        }
        *offset_ns = result.value();
        return CBSDK_RESULT_SUCCESS;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_get_clock_uncertainty(
    cbsdk_session_t session,
    int64_t* uncertainty_ns) {
    if (!session || !session->cpp_session || !uncertainty_ns) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->getClockUncertaintyNs();
        if (!result.has_value()) {
            return CBSDK_RESULT_NOT_RUNNING;
        }
        *uncertainty_ns = result.value();
        return CBSDK_RESULT_SUCCESS;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

cbsdk_result_t cbsdk_session_send_clock_probe(cbsdk_session_t session) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->sendClockProbe();
        return result.isOk() ? CBSDK_RESULT_SUCCESS : CBSDK_RESULT_INTERNAL_ERROR;
    } catch (...) {
        return CBSDK_RESULT_INTERNAL_ERROR;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////////////////////////////////////////

int64_t cbsdk_get_steady_clock_ns(void) {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

} // extern "C"
