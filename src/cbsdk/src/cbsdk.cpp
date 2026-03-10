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
    uint8_t group_id,
    cbsdk_group_callback_fn callback,
    void* user_data) {
    if (!session || !session->cpp_session || !callback) {
        return 0;
    }
    try {
        return session->cpp_session->registerGroupCallback(group_id,
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
    uint32_t group_id,
    bool disable_others) {
    if (!session || !session->cpp_session) {
        return CBSDK_RESULT_INVALID_PARAMETER;
    }
    try {
        auto result = session->cpp_session->setChannelSampleGroup(
            n_chans, to_cpp_channel_type(chan_type), group_id, disable_others);
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

} // extern "C"
