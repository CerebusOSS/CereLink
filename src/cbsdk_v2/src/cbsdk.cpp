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

#include "cbsdk_v2/cbsdk.h"
#include "cbsdk_v2/sdk_session.h"
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

    cbsdk_error_callback_fn error_callback;
    void* error_callback_user_data;

    cbsdk_session_impl()
        : packet_callback(nullptr)
        , packet_callback_user_data(nullptr)
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
        case CBSDK_DEVICE_LEGACY_NSP:
            cpp_config.device_type = cbsdk::DeviceType::LEGACY_NSP;
            break;
        case CBSDK_DEVICE_GEMINI_NSP:
            cpp_config.device_type = cbsdk::DeviceType::GEMINI_NSP;
            break;
        case CBSDK_DEVICE_GEMINI_HUB1:
            cpp_config.device_type = cbsdk::DeviceType::GEMINI_HUB1;
            break;
        case CBSDK_DEVICE_GEMINI_HUB2:
            cpp_config.device_type = cbsdk::DeviceType::GEMINI_HUB2;
            break;
        case CBSDK_DEVICE_GEMINI_HUB3:
            cpp_config.device_type = cbsdk::DeviceType::GEMINI_HUB3;
            break;
        case CBSDK_DEVICE_NPLAY:
            cpp_config.device_type = cbsdk::DeviceType::NPLAY;
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
    c_stats->shmem_store_errors = cpp_stats.shmem_store_errors;
    c_stats->receive_errors = cpp_stats.receive_errors;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// C API Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {

cbsdk_config_t cbsdk_config_default(void) {
    cbsdk_config_t config;
    config.device_type = CBSDK_DEVICE_LEGACY_NSP;
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
                return CBSDK_RESULT_ALREADY_RUNNING;
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
        // Store C callback
        session->packet_callback = callback;
        session->packet_callback_user_data = user_data;

        if (callback) {
            // Wrap C callback in C++ lambda
            session->cpp_session->setPacketCallback(
                [session](const cbPKT_GENERIC* pkts, size_t count) {
                    if (session->packet_callback) {
                        session->packet_callback(pkts, count, session->packet_callback_user_data);
                    }
                }
            );
        } else {
            // Clear callback
            session->cpp_session->setPacketCallback(nullptr);
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

} // extern "C"
