///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   cbsdk.h
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  C API for CereLink SDK v2
///
/// This is the main C API for cbsdk. It provides a clean
/// C interface that wraps the C++ SdkSession implementation.
/// If you are using C++, consider using sdk_session.h instead.
/// This API is designed for easy FFI usage from other languages.
///
/// Key Design Principles:
/// - Opaque handle pattern for session management
/// - Integer error codes for easy FFI
/// - C-style callbacks with user_data pointers
/// - Clear ownership semantics
///
/// Example Usage:
/// @code{.c}
///   cbsdk_session_t* session = NULL;
///   cbsdk_config_t config = cbsdk_config_default();
///   config.device_address = "192.168.137.128";
///
///   int result = cbsdk_session_create(&session, &config);
///   if (result != CBSDK_RESULT_SUCCESS) {
///       fprintf(stderr, "Error: %s\n", cbsdk_get_error_message(result));
///       return 1;
///   }
///
///   cbsdk_session_set_packet_callback(session, my_packet_callback, user_data);
///   cbsdk_session_start(session);
///
///   // ... do work ...
///
///   cbsdk_session_stop(session);
///   cbsdk_session_destroy(session);
/// @endcode
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_V2_CBSDK_H
#define CBSDK_V2_CBSDK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Protocol types (need for callbacks)
#ifdef __cplusplus
extern "C" {
#endif

#include <cbproto/cbproto.h>
#include <cbproto/connection.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Result Codes
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Result codes returned by cbsdk functions
typedef enum {
    CBSDK_RESULT_SUCCESS             =  0,   ///< Operation succeeded
    CBSDK_RESULT_INVALID_PARAMETER   = -1,   ///< Invalid parameter (NULL pointer, etc.)
    CBSDK_RESULT_ALREADY_RUNNING     = -2,   ///< Session is already running
    CBSDK_RESULT_NOT_RUNNING         = -3,   ///< Session is not running
    CBSDK_RESULT_SHMEM_ERROR         = -4,   ///< Shared memory error
    CBSDK_RESULT_DEVICE_ERROR        = -5,   ///< Device connection error
    CBSDK_RESULT_INTERNAL_ERROR      = -6,   ///< Internal error
} cbsdk_result_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Structures
///////////////////////////////////////////////////////////////////////////////////////////////////

/// SDK configuration (C version of SdkConfig)
typedef struct {
    // Device type (automatically maps to correct address/port and shared memory name)
    // Used only when creating new shared memory (STANDALONE mode)
    cbproto_device_type_t device_type;  ///< Device type to connect to

    // Callback thread configuration
    size_t callback_queue_depth;      ///< Packets to buffer (default: 16384)
    bool enable_realtime_priority;    ///< Elevated thread priority
    bool drop_on_overflow;            ///< Drop oldest on overflow (vs newest)

    // Advanced options
    int recv_buffer_size;             ///< UDP receive buffer size (default: 6MB)
    bool non_blocking;                ///< Non-blocking sockets

    // Optional custom device configuration (overrides device_type mapping)
    // Use NULL/0 for automatic detection based on device_type
    const char* custom_device_address; ///< Override device IP (NULL = auto)
    const char* custom_client_address; ///< Override client IP (NULL = auto)
    uint16_t custom_device_port;       ///< Override device port (0 = auto)
    uint16_t custom_client_port;       ///< Override client port (0 = auto)
} cbsdk_config_t;

/// SDK statistics (C version of SdkStats)
typedef struct {
    // Device statistics
    uint64_t packets_received_from_device;   ///< Packets from UDP socket
    uint64_t bytes_received_from_device;     ///< Bytes from UDP socket

    // Shared memory statistics
    uint64_t packets_stored_to_shmem;        ///< Packets written to shmem

    // Callback queue statistics
    uint64_t packets_queued_for_callback;    ///< Packets added to queue
    uint64_t packets_delivered_to_callback;  ///< Packets delivered to user
    uint64_t packets_dropped;                ///< Dropped due to queue overflow
    uint64_t queue_current_depth;            ///< Current queue usage
    uint64_t queue_max_depth;                ///< Peak queue usage

    // Error counters
    uint64_t shmem_store_errors;             ///< Failed to store to shmem
    uint64_t receive_errors;                 ///< Socket receive errors
} cbsdk_stats_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Types
///////////////////////////////////////////////////////////////////////////////////////////////////

/// User callback for received packets
/// @param pkts Pointer to array of packets
/// @param count Number of packets in array
/// @param user_data User data pointer passed to cbsdk_session_set_packet_callback()
typedef void (*cbsdk_packet_callback_fn)(const cbPKT_GENERIC* pkts, size_t count, void* user_data);

/// Error callback for queue overflow and other errors
/// @param error_message Description of the error (null-terminated string)
/// @param user_data User data pointer passed to cbsdk_session_set_error_callback()
typedef void (*cbsdk_error_callback_fn)(const char* error_message, void* user_data);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Opaque Handle
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Opaque session handle (do not access fields directly)
typedef struct cbsdk_session_impl* cbsdk_session_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get default SDK configuration
/// @return Default configuration structure
cbsdk_config_t cbsdk_config_default(void);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Management
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Create a new SDK session
/// @param[out] session Pointer to receive session handle (must not be NULL)
/// @param[in] config Configuration (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
cbsdk_result_t cbsdk_session_create(cbsdk_session_t* session, const cbsdk_config_t* config);

/// Destroy an SDK session and free resources
/// @param session Session handle (can be NULL)
void cbsdk_session_destroy(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Control
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Start receiving packets from device
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
cbsdk_result_t cbsdk_session_start(cbsdk_session_t session);

/// Stop receiving packets
/// @param session Session handle (must not be NULL)
void cbsdk_session_stop(cbsdk_session_t session);

/// Check if session is running
/// @param session Session handle (must not be NULL)
/// @return true if running, false otherwise
bool cbsdk_session_is_running(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set callback for received packets
/// @param session Session handle (must not be NULL)
/// @param callback Callback function (can be NULL to clear)
/// @param user_data User data pointer passed to callback
void cbsdk_session_set_packet_callback(cbsdk_session_t session,
                                        cbsdk_packet_callback_fn callback,
                                        void* user_data);

/// Set callback for errors (queue overflow, etc.)
/// @param session Session handle (must not be NULL)
/// @param callback Callback function (can be NULL to clear)
/// @param user_data User data pointer passed to callback
void cbsdk_session_set_error_callback(cbsdk_session_t session,
                                       cbsdk_error_callback_fn callback,
                                       void* user_data);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics & Monitoring
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get current statistics
/// @param session Session handle (must not be NULL)
/// @param[out] stats Pointer to receive statistics (must not be NULL)
void cbsdk_session_get_stats(cbsdk_session_t session, cbsdk_stats_t* stats);

/// Reset statistics counters to zero
/// @param session Session handle (must not be NULL)
void cbsdk_session_reset_stats(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Handling
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get human-readable error message for result code
/// @param result Result code
/// @return Error message string (never NULL, always valid)
const char* cbsdk_get_error_message(cbsdk_result_t result);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Version Information
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get SDK version string
/// @return Version string (e.g., "2.0.0")
const char* cbsdk_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // CBSDK_V2_CBSDK_H
