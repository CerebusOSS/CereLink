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
///   cbsdk_session_t session = NULL;
///   cbsdk_config_t config = cbsdk_config_default();
///   config.device_type = CBPROTO_DEVICE_TYPE_HUB1;
///
///   int result = cbsdk_session_create(&session, &config);
///   if (result != CBSDK_RESULT_SUCCESS) {
///       fprintf(stderr, "Error: %s\n", cbsdk_get_error_message(result));
///       return 1;
///   }
///
///   cbsdk_session_set_packet_callback(session, my_packet_callback, user_data);
///   // ... do work ...
///   cbsdk_session_destroy(session);
/// @endcode
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_V2_CBSDK_H
#define CBSDK_V2_CBSDK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// DLL Export/Import
///////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(CBSDK_SHARED)
  #if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(CBSDK_EXPORTS)
      #define CBSDK_API __declspec(dllexport)
    #else
      #define CBSDK_API __declspec(dllimport)
    #endif
  #elif defined(__GNUC__) && __GNUC__ >= 4
    #define CBSDK_API __attribute__((visibility("default")))
  #else
    #define CBSDK_API
  #endif
#else
  #define CBSDK_API
#endif

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

/// Channel info field selector for bulk extraction
typedef enum {
    CBSDK_CHANINFO_FIELD_SMPGROUP    = 0,
    CBSDK_CHANINFO_FIELD_SMPFILTER   = 1,
    CBSDK_CHANINFO_FIELD_SPKFILTER   = 2,
    CBSDK_CHANINFO_FIELD_AINPOPTS    = 3,
    CBSDK_CHANINFO_FIELD_SPKOPTS     = 4,
    CBSDK_CHANINFO_FIELD_SPKTHRLEVEL = 5,
    CBSDK_CHANINFO_FIELD_LNCRATE     = 6,
    CBSDK_CHANINFO_FIELD_REFELECCHAN = 7,
    CBSDK_CHANINFO_FIELD_AMPLREJPOS  = 8,
    CBSDK_CHANINFO_FIELD_AMPLREJNEG  = 9,
    CBSDK_CHANINFO_FIELD_CHANCAPS    = 10,
    CBSDK_CHANINFO_FIELD_BANK        = 11,
    CBSDK_CHANINFO_FIELD_TERM        = 12,
} cbsdk_chaninfo_field_t;

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

    // Transmit statistics (STANDALONE mode only)
    uint64_t packets_sent_to_device;         ///< Packets sent to device

    // Error counters
    uint64_t shmem_store_errors;             ///< Failed to store to shmem
    uint64_t receive_errors;                 ///< Socket receive errors
    uint64_t send_errors;                    ///< Socket send errors
} cbsdk_stats_t;

/// Channel scaling information (mirrors cbSCALING from cbproto)
typedef struct {
    int16_t  digmin;     ///< Digital value corresponding to anamin
    int16_t  digmax;     ///< Digital value corresponding to anamax
    int32_t  anamin;     ///< Minimum analog value
    int32_t  anamax;     ///< Maximum analog value
    int32_t  anagain;    ///< Gain applied to analog values
    char     anaunit[8]; ///< Unit string (e.g., "uV", "mV", "MPa")
} cbsdk_channel_scaling_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callback Types
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Callback handle for unregistering typed callbacks
typedef uint32_t cbsdk_callback_handle_t;

/// User callback for received packets (all packet types)
/// @param pkts Pointer to array of packets
/// @param count Number of packets in array
/// @param user_data User data pointer passed to registration function
typedef void (*cbsdk_packet_callback_fn)(const cbPKT_GENERIC* pkts, size_t count, void* user_data);

/// Event callback for spike/digital/serial event packets
/// @param pkt Pointer to the event packet
/// @param user_data User data pointer passed to registration function
typedef void (*cbsdk_event_callback_fn)(const cbPKT_GENERIC* pkt, void* user_data);

/// Group callback for continuous sample data packets (chid == 0)
/// @param pkt Pointer to the group packet
/// @param user_data User data pointer passed to registration function
typedef void (*cbsdk_group_callback_fn)(const cbPKT_GROUP* pkt, void* user_data);

/// Batch group callback — receives multiple contiguous samples from a single queue drain.
/// Called once per batch (~30 samples at 30kHz) instead of once per sample.
/// @param samples Contiguous int16 sample data [n_samples × n_channels], row-major
/// @param n_samples Number of samples (packets) in this batch
/// @param n_channels Number of channels per sample
/// @param timestamps Device timestamp for each sample (array of n_samples)
/// @param user_data User data pointer passed to registration function
typedef void (*cbsdk_group_batch_callback_fn)(const int16_t* samples, size_t n_samples,
                                               size_t n_channels, const uint64_t* timestamps,
                                               void* user_data);

/// Config callback for system/configuration packets (chid & 0x8000)
/// @param pkt Pointer to the config packet
/// @param user_data User data pointer passed to registration function
typedef void (*cbsdk_config_callback_fn)(const cbPKT_GENERIC* pkt, void* user_data);

/// Error callback for queue overflow and other errors
/// @param error_message Description of the error (null-terminated string)
/// @param user_data User data pointer passed to registration function
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
CBSDK_API cbsdk_config_t cbsdk_config_default(void);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Management
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Create a new SDK session
/// @param[out] session Pointer to receive session handle (must not be NULL)
/// @param[in] config Configuration (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_create(cbsdk_session_t* session, const cbsdk_config_t* config);

/// Destroy an SDK session and free resources
/// @param session Session handle (can be NULL)
CBSDK_API void cbsdk_session_destroy(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Session Control
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Start receiving packets from device
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_start(cbsdk_session_t session);

/// Stop receiving packets
/// @param session Session handle (must not be NULL)
CBSDK_API void cbsdk_session_stop(cbsdk_session_t session);

/// Check if session is running
/// @param session Session handle (must not be NULL)
/// @return true if running, false otherwise
CBSDK_API bool cbsdk_session_is_running(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set callback for all received packets (legacy convenience -- registers a PACKET callback)
/// @param session Session handle (must not be NULL)
/// @param callback Callback function (can be NULL to clear)
/// @param user_data User data pointer passed to callback
CBSDK_API void cbsdk_session_set_packet_callback(cbsdk_session_t session,
                                                  cbsdk_packet_callback_fn callback,
                                                  void* user_data);

/// Set callback for errors (queue overflow, etc.)
/// @param session Session handle (must not be NULL)
/// @param callback Callback function (can be NULL to clear)
/// @param user_data User data pointer passed to callback
CBSDK_API void cbsdk_session_set_error_callback(cbsdk_session_t session,
                                                 cbsdk_error_callback_fn callback,
                                                 void* user_data);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Typed Callback Registration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Register callback for all packets (catch-all)
/// @param session Session handle (must not be NULL)
/// @param callback Callback function (must not be NULL)
/// @param user_data User data pointer passed to callback
/// @return Handle for unregistration, or 0 on failure
CBSDK_API cbsdk_callback_handle_t cbsdk_session_register_packet_callback(
    cbsdk_session_t session,
    cbsdk_packet_callback_fn callback,
    void* user_data);

/// Register callback for event packets (spikes, digital events, etc.)
/// @param session Session handle (must not be NULL)
/// @param channel_type Channel type filter (use CBPROTO_CHANNEL_TYPE_FRONTEND for spikes, etc.)
///        Pass -1 (cast to cbproto_channel_type_t) for all event channels.
/// @param callback Callback function (must not be NULL)
/// @param user_data User data pointer passed to callback
/// @return Handle for unregistration, or 0 on failure
CBSDK_API cbsdk_callback_handle_t cbsdk_session_register_event_callback(
    cbsdk_session_t session,
    cbproto_channel_type_t channel_type,
    cbsdk_event_callback_fn callback,
    void* user_data);

/// Register callback for continuous sample group packets
/// @param session Session handle (must not be NULL)
/// @param rate Sample rate to match (CBPROTO_GROUP_RATE_500Hz through CBPROTO_GROUP_RATE_RAW)
/// @param callback Callback function (must not be NULL)
/// @param user_data User data pointer passed to callback
/// @return Handle for unregistration, or 0 on failure
CBSDK_API cbsdk_callback_handle_t cbsdk_session_register_group_callback(
    cbsdk_session_t session,
    cbproto_group_rate_t rate,
    cbsdk_group_callback_fn callback,
    void* user_data);

/// Register batch callback for continuous sample group packets.
/// Instead of invoking the callback once per packet, this batches all matching group
/// packets from a single queue drain and invokes the callback once with contiguous data.
/// @param session Session handle (must not be NULL)
/// @param rate Sample rate to match
/// @param callback Callback function (must not be NULL)
/// @param user_data User data pointer passed to callback
/// @return Handle for unregistration, or 0 on failure
CBSDK_API cbsdk_callback_handle_t cbsdk_session_register_group_batch_callback(
    cbsdk_session_t session,
    cbproto_group_rate_t rate,
    cbsdk_group_batch_callback_fn callback,
    void* user_data);

/// Register callback for config/system packets
/// @param session Session handle (must not be NULL)
/// @param packet_type Packet type to match (e.g., cbPKTTYPE_COMMENTREP, cbPKTTYPE_SYSREPRUNLEV)
/// @param callback Callback function (must not be NULL)
/// @param user_data User data pointer passed to callback
/// @return Handle for unregistration, or 0 on failure
CBSDK_API cbsdk_callback_handle_t cbsdk_session_register_config_callback(
    cbsdk_session_t session,
    uint16_t packet_type,
    cbsdk_config_callback_fn callback,
    void* user_data);

/// Unregister a previously registered callback
/// @param session Session handle (must not be NULL)
/// @param handle Handle returned by a register_*_callback function
CBSDK_API void cbsdk_session_unregister_callback(cbsdk_session_t session,
                                                  cbsdk_callback_handle_t handle);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Statistics & Monitoring
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get current statistics
/// @param session Session handle (must not be NULL)
/// @param[out] stats Pointer to receive statistics (must not be NULL)
CBSDK_API void cbsdk_session_get_stats(cbsdk_session_t session, cbsdk_stats_t* stats);

/// Reset statistics counters to zero
/// @param session Session handle (must not be NULL)
CBSDK_API void cbsdk_session_reset_stats(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Access (read from shared memory -- always up-to-date)
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get current device run level
/// @param session Session handle (must not be NULL)
/// @return Current run level (cbRUNLEVEL_*), or 0 if unknown
CBSDK_API uint32_t cbsdk_session_get_runlevel(cbsdk_session_t session);

/// Whether this session owns the device connection (STANDALONE mode)
/// @param session Session handle (must not be NULL)
/// @return 1 if STANDALONE, 0 if CLIENT
CBSDK_API int cbsdk_session_is_standalone(cbsdk_session_t session);

/// Get the protocol version used by this session
/// @param session Session handle (must not be NULL)
/// @return Protocol version (cbproto_protocol_version_t), or 0 (UNKNOWN) if unavailable
CBSDK_API uint32_t cbsdk_session_get_protocol_version(cbsdk_session_t session);

/// Get the processor identification string (e.g. "Gemini Hub 1")
/// @param session Session handle (must not be NULL)
/// @param buf Output buffer for the ident string
/// @param buf_size Size of the output buffer in bytes
/// @return Number of bytes written (excluding null terminator), or 0 if unavailable
CBSDK_API uint32_t cbsdk_session_get_proc_ident(cbsdk_session_t session, char* buf, uint32_t buf_size);

/// Get the global spike event length (samples per spike waveform)
/// @param session Session handle (must not be NULL)
/// @return Spike length in samples, or 0 if unavailable
CBSDK_API uint32_t cbsdk_session_get_spike_length(cbsdk_session_t session);

/// Get the global spike pre-trigger length (samples before threshold crossing)
/// @param session Session handle (must not be NULL)
/// @return Pre-trigger length in samples, or 0 if unavailable
CBSDK_API uint32_t cbsdk_session_get_spike_pretrigger(cbsdk_session_t session);

/// Set the global spike event length and pre-trigger
/// @param session Session handle (must not be NULL)
/// @param spike_length Total spike waveform length in samples
/// @param spike_pretrigger Pre-trigger samples (must be < spike_length)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_spike_length(
    cbsdk_session_t session,
    uint32_t spike_length,
    uint32_t spike_pretrigger);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Information Accessors
//
// These provide FFI-friendly access to channel/group configuration without
// requiring the full struct layouts. Pointers are valid for the session lifetime.
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get the number of channels
/// @return cbMAXCHANS (compile-time constant)
CBSDK_API uint32_t cbsdk_get_max_chans(void);

/// Get the number of front-end channels
/// @return cbNUM_FE_CHANS (compile-time constant)
CBSDK_API uint32_t cbsdk_get_num_fe_chans(void);

/// Get the number of analog channels
/// @return cbNUM_ANALOG_CHANS (compile-time constant)
CBSDK_API uint32_t cbsdk_get_num_analog_chans(void);

/// Get a channel's label
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Pointer to null-terminated label string, or NULL if invalid.
///         Pointer is valid for the lifetime of the session.
CBSDK_API const char* cbsdk_session_get_channel_label(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's sample group assignment
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Sample group (0-6, where 0 = disabled), or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_smpgroup(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's capabilities flags
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Channel capabilities (cbCHAN_* flags), or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_chancaps(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's type classification based on its capabilities
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Channel type, or -1 if invalid
CBSDK_API cbproto_channel_type_t cbsdk_session_get_channel_type(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's continuous-time pathway filter ID
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Filter ID, or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_smpfilter(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's spike pathway filter ID
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Filter ID, or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_spkfilter(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's spike processing options
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Spike options (cbAINPSPK_* flags), or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_spkopts(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's spike threshold level
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Spike threshold level, or 0 on error
CBSDK_API int32_t cbsdk_session_get_channel_spkthrlevel(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's analog input options (LNC, reference electrode, etc.)
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Analog input options (cbAINP_* flags), or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_ainpopts(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's line noise cancellation adaptation rate
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return LNC rate, or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_lncrate(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's software reference electrode channel
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Reference electrode channel ID, or 0 on error
CBSDK_API uint32_t cbsdk_session_get_channel_refelecchan(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's positive amplitude rejection threshold
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Positive amplitude rejection value, or 0 on error
CBSDK_API int16_t cbsdk_session_get_channel_amplrejpos(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's negative amplitude rejection threshold
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @return Negative amplitude rejection value, or 0 on error
CBSDK_API int16_t cbsdk_session_get_channel_amplrejneg(cbsdk_session_t session, uint32_t chan_id);

/// Get a channel's input scaling information (user-defined scalin)
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param[out] scaling Pointer to struct to fill with scaling data
/// @return CBSDK_RESULT_SUCCESS on success, error code otherwise
CBSDK_API cbsdk_result_t cbsdk_session_get_channel_scaling(
    cbsdk_session_t session, uint32_t chan_id, cbsdk_channel_scaling_t* scaling);

/// Get any numeric field from a single channel by field selector
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param field Field to extract
/// @return Field value widened to int64_t, or 0 on error
CBSDK_API int64_t cbsdk_session_get_channel_field(
    cbsdk_session_t session,
    uint32_t chan_id,
    cbsdk_chaninfo_field_t field);

/// Get a sample group's label
/// @param session Session handle (must not be NULL)
/// @param group_id Group ID (1-6)
/// @return Pointer to null-terminated label string, or NULL if invalid
CBSDK_API const char* cbsdk_session_get_group_label(cbsdk_session_t session, uint32_t group_id);

/// Get the list of channels in a sample group
/// @param session Session handle (must not be NULL)
/// @param group_id Group ID (1-6)
/// @param[out] list Pointer to receive channel list (caller-allocated, max cbNUM_ANALOG_CHANS entries)
/// @param[in,out] count On input: size of list array. On output: number of channels written.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_group_list(
    cbsdk_session_t session,
    uint32_t group_id,
    uint16_t* list,
    uint32_t* count);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set sampling rate for channels of a specific type
/// @param session Session handle (must not be NULL)
/// @param n_chans Number of channels to configure (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param rate Sample rate (CBPROTO_GROUP_RATE_NONE to disable, _500Hz through _RAW)
/// @param disable_others If true, disable sampling on unselected channels of this type
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_sample_group(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    cbproto_group_rate_t rate,
    bool disable_others);

/// Set AC input coupling (offset correction) for channels of a specific type
/// @param session Session handle (must not be NULL)
/// @param n_chans Number of channels to configure (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param enabled true = AC coupling, false = DC coupling
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_ac_input_coupling(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    bool enabled);

/// Set full channel configuration by sending a CHANINFO packet
/// @param session Session handle (must not be NULL)
/// @param chaninfo Complete channel info packet to send
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_config(
    cbsdk_session_t session,
    const cbPKT_CHANINFO* chaninfo);

/// Set a channel's label
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param label New label string (max 15 chars, null-terminated)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_label(
    cbsdk_session_t session,
    uint32_t chan_id,
    const char* label);

/// Set a single channel's sample group (fire-and-forget).
/// Handles group-specific logic: RAWSTREAM flag for group 6, filter mapping
/// for groups 1-5, clearing conflicting flags.
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param rate Sample group (0=NONE, 1-5=filtered, 6=RAW)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_smpgroup(
    cbsdk_session_t session,
    uint32_t chan_id,
    cbproto_group_rate_t rate);

/// Set a channel's continuous-time pathway filter
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_smpfilter(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t filter_id);

/// Set a channel's spike pathway filter
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_spkfilter(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t filter_id);

/// Set a channel's analog input options (LNC mode, reference electrode, etc.)
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param ainpopts Analog input option flags (cbAINP_* flags)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_ainpopts(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t ainpopts);

/// Set a channel's line noise cancellation adaptation rate
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param lncrate LNC rate
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_lncrate(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t lncrate);

/// Set a channel's spike processing options
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param spkopts Spike option flags (cbAINPSPK_* flags)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_spkopts(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t spkopts);

/// Set a channel's spike threshold level
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param level Threshold level
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_spkthrlevel(
    cbsdk_session_t session,
    uint32_t chan_id,
    int32_t level);

/// Enable or disable auto-thresholding for a channel
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param enabled true to enable, false to disable
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_autothreshold(
    cbsdk_session_t session,
    uint32_t chan_id,
    bool enabled);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bulk Channel Queries
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get the 1-based IDs of channels matching a type
/// @param session Session handle (must not be NULL)
/// @param n_chans Max channels to return (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param[out] out_ids Caller-allocated array to receive channel IDs
/// @param[in,out] out_count On input: size of out_ids array. On output: channels written.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_matching_channels(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    uint32_t* out_ids,
    uint32_t* out_count);

/// Get a numeric field from all channels matching a type
/// @param session Session handle (must not be NULL)
/// @param n_chans Max channels to query (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param field Which field to extract
/// @param[out] out_values Caller-allocated array to receive field values
/// @param[in,out] out_count On input: size of out_values array. On output: values written.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_channels_field(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    cbsdk_chaninfo_field_t field,
    int64_t* out_values,
    uint32_t* out_count);

/// Get labels from all channels matching a type
/// @param session Session handle (must not be NULL)
/// @param n_chans Max channels to query (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param[out] out_labels Caller-allocated array of char pointers (size out_count)
/// @param[out] out_buf Caller-allocated buffer for label strings (each up to 16 bytes)
/// @param out_buf_size Size of out_buf in bytes
/// @param[in,out] out_count On input: size of out_labels array. On output: labels written.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_channels_labels(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    char* out_buf,
    size_t label_stride,
    uint32_t* out_count);

/// Get positions from all channels matching a type
/// @param session Session handle (must not be NULL)
/// @param n_chans Max channels to query (use cbMAXCHANS for all)
/// @param chan_type Channel type filter
/// @param[out] out_positions Caller-allocated int32_t array (4 values per channel: x,y,z,w)
/// @param[in,out] out_count On input: max channels (out_positions must hold 4 * out_count int32_ts).
///                          On output: number of channels written.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_channels_positions(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    int32_t* out_positions,
    uint32_t* out_count);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bulk Configuration Access
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get the system sampling frequency
/// @param session Session handle (must not be NULL)
/// @return System frequency in Hz, or 0 if unavailable
CBSDK_API uint32_t cbsdk_session_get_sysfreq(cbsdk_session_t session);

/// Get the number of available filters
/// @return cbMAXFILTS (compile-time constant)
CBSDK_API uint32_t cbsdk_get_num_filters(void);

/// Get a filter's label
/// @param session Session handle (must not be NULL)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return Pointer to label string, or NULL if invalid
CBSDK_API const char* cbsdk_session_get_filter_label(cbsdk_session_t session, uint32_t filter_id);

/// Get a filter's high-pass corner frequency
/// @param session Session handle (must not be NULL)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return High-pass frequency in milliHertz, or 0 if invalid
CBSDK_API uint32_t cbsdk_session_get_filter_hpfreq(cbsdk_session_t session, uint32_t filter_id);

/// Get a filter's high-pass order
/// @param session Session handle (must not be NULL)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return High-pass filter order, or 0 if invalid
CBSDK_API uint32_t cbsdk_session_get_filter_hporder(cbsdk_session_t session, uint32_t filter_id);

/// Get a filter's low-pass corner frequency
/// @param session Session handle (must not be NULL)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return Low-pass frequency in milliHertz, or 0 if invalid
CBSDK_API uint32_t cbsdk_session_get_filter_lpfreq(cbsdk_session_t session, uint32_t filter_id);

/// Get a filter's low-pass order
/// @param session Session handle (must not be NULL)
/// @param filter_id Filter ID (0 to cbMAXFILTS-1)
/// @return Low-pass filter order, or 0 if invalid
CBSDK_API uint32_t cbsdk_session_get_filter_lporder(cbsdk_session_t session, uint32_t filter_id);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Commands
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Send a comment string to the device (appears in recorded data)
/// @param session Session handle (must not be NULL)
/// @param comment Comment text (max 127 chars, null-terminated)
/// @param rgba Color as RGBA uint32_t (0 = white)
/// @param charset Character set (0 = ANSI)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_send_comment(
    cbsdk_session_t session,
    const char* comment,
    uint32_t rgba,
    uint8_t charset);

/// Send a raw packet to the device (STANDALONE mode only)
/// @param session Session handle (must not be NULL)
/// @param pkt Packet to send (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_send_packet(
    cbsdk_session_t session,
    const cbPKT_GENERIC* pkt);

/// Set digital output value
/// @param session Session handle (must not be NULL)
/// @param chan_id Channel ID (1-based) of a digital output channel
/// @param value Digital output value (bitmask)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_digital_output(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint16_t value);

/// Set system run level
/// @param session Session handle (must not be NULL)
/// @param runlevel Desired run level (cbRUNLEVEL_*)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_runlevel(
    cbsdk_session_t session,
    uint32_t runlevel);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Mapping (CMP) Files
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Load a channel mapping file (.cmp) and apply electrode positions
///
/// CMP files define physical electrode positions on arrays. Because the device does not
/// persist the position field in chaninfo, positions are stored locally and overlaid
/// onto channel info whenever config data arrives from the device.
///
/// Can be called multiple times for different ports on a Hub device.
///
/// @param session Session handle (must not be NULL)
/// @param filepath Path to the .cmp file (must not be NULL)
/// @param bank_offset Offset added to CMP bank indices. A=1+offset, B=2+offset, etc.
///        Use 0 for port 1, 4 for port 2, 8 for port 3, etc.
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_load_channel_map(
    cbsdk_session_t session,
    const char* filepath,
    uint32_t bank_offset);

///////////////////////////////////////////////////////////////////////////////////////////////////
// CCF Configuration Files
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Save the current device configuration to a CCF (XML) file
/// @param session Session handle (must not be NULL)
/// @param filename Path to the CCF file to write (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_save_ccf(cbsdk_session_t session, const char* filename);

/// Load a CCF file and apply its configuration to the device
/// @param session Session handle (must not be NULL)
/// @param filename Path to the CCF file to read (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_load_ccf(cbsdk_session_t session, const char* filename);

/// Load a CCF file and wait for the device to acknowledge the configuration.
///
/// After sending all configuration packets from the CCF file, a runlevel query
/// is used as a synchronization point.  The function returns once the device
/// responds (confirming it has processed every preceding packet) or after the
/// timeout expires.
///
/// @param session    Session handle (must not be NULL)
/// @param filename   Path to the CCF file to read (must not be NULL)
/// @param timeout_ms Maximum time in milliseconds to wait for acknowledgment
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure or timeout
CBSDK_API cbsdk_result_t cbsdk_session_load_ccf_sync(
    cbsdk_session_t session, const char* filename, uint32_t timeout_ms);

/// Wait for the device to finish processing all previously sent config packets.
/// Sends a no-op runlevel command and waits for the SYSREP response.
/// @param session Session handle (must not be NULL)
/// @param timeout_ms Maximum time in milliseconds to wait for acknowledgment
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure or timeout
CBSDK_API cbsdk_result_t cbsdk_session_sync(cbsdk_session_t session, uint32_t timeout_ms);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument Time
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get most recent device timestamp from shared memory (always nanoseconds).
/// Non-Gemini clock ticks are converted using sysfreq automatically.
/// @param session Session handle (must not be NULL)
/// @param[out] time Pointer to receive device timestamp in nanoseconds (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_get_time(cbsdk_session_t session, uint64_t* time);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Patient Information
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set patient information (embedded in recorded files)
/// Must be called before starting recording.
/// @param session Session handle (must not be NULL)
/// @param id Patient identification string (must not be NULL)
/// @param firstname Patient first name (can be NULL)
/// @param lastname Patient last name (can be NULL)
/// @param dob_month Birth month (1-12, 0 = unset)
/// @param dob_day Birth day (1-31, 0 = unset)
/// @param dob_year Birth year (e.g. 1990, 0 = unset)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_patient_info(
    cbsdk_session_t session,
    const char* id,
    const char* firstname,
    const char* lastname,
    uint32_t dob_month,
    uint32_t dob_day,
    uint32_t dob_year);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Analog Output
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set analog output monitoring (route a channel's audio to an analog/audio output)
/// @param session Session handle (must not be NULL)
/// @param aout_chan_id 1-based channel ID of the analog/audio output channel
/// @param monitor_chan_id 1-based channel ID of the channel to monitor
/// @param track_last If true, track last channel clicked in Central
/// @param spike_only If true, monitor spike signal; if false, monitor continuous signal
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_analog_output_monitor(
    cbsdk_session_t session,
    uint32_t aout_chan_id,
    uint32_t monitor_chan_id,
    bool track_last,
    bool spike_only);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Recording Control
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Start Central file recording on the device
/// @param session Session handle (must not be NULL)
/// @param filename Base filename without extension (must not be NULL)
/// @param comment Recording comment (can be NULL or empty)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_start_central_recording(
    cbsdk_session_t session,
    const char* filename,
    const char* comment);

/// Stop Central file recording on the device
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_stop_central_recording(cbsdk_session_t session);

/// Open Central's File Storage dialog
/// Must be called before start_central_recording (wait ~250ms after for dialog to initialize)
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_open_central_file_dialog(cbsdk_session_t session);

/// Close Central's File Storage dialog
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_close_central_file_dialog(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Sorting
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Set spike sorting options for channels of a specific type
/// @param session Session handle (must not be NULL)
/// @param n_chans Number of channels to configure
/// @param chan_type Channel type filter
/// @param sort_options Spike sorting option flags (cbAINPSPK_*)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_spike_sorting(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    uint32_t sort_options);

/// Set spike sorting options for a single channel (fire-and-forget).
/// Clears cbAINPSPK_ALLSORT bits then sets sort_options.
/// @param session Session handle (must not be NULL)
/// @param chan_id 1-based channel ID (1 to cbMAXCHANS)
/// @param sort_options Spike sorting option flags (cbAINPSPK_*)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_channel_spike_sorting(
    cbsdk_session_t session,
    uint32_t chan_id,
    uint32_t sort_options);

/// Enable or disable spike extraction for channels of a specific type.
/// Controls the cbAINPSPK_EXTRACT bit via cbPKTTYPE_CHANSETSPK.
/// When enabled, the device emits spike event packets for matching channels.
/// @param session Session handle (must not be NULL)
/// @param n_chans Number of channels to configure
/// @param chan_type Channel type filter
/// @param enabled true = enable spike extraction, false = disable
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_set_spike_extraction(
    cbsdk_session_t session,
    size_t n_chans,
    cbproto_channel_type_t chan_type,
    bool enabled);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Synchronization
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get the current clock offset: device_ns - steady_clock_ns
/// @param session Session handle (must not be NULL)
/// @param[out] offset_ns Pointer to receive offset in nanoseconds
/// @return CBSDK_RESULT_SUCCESS if offset is available, CBSDK_RESULT_NOT_RUNNING if no sync data
CBSDK_API cbsdk_result_t cbsdk_session_get_clock_offset(
    cbsdk_session_t session,
    int64_t* offset_ns);

/// Get the clock uncertainty (half-RTT from best probe)
/// @param session Session handle (must not be NULL)
/// @param[out] uncertainty_ns Pointer to receive uncertainty in nanoseconds
/// @return CBSDK_RESULT_SUCCESS if available, CBSDK_RESULT_NOT_RUNNING if no sync data
CBSDK_API cbsdk_result_t cbsdk_session_get_clock_uncertainty(
    cbsdk_session_t session,
    int64_t* uncertainty_ns);

/// Send a clock synchronization probe to the device
/// @param session Session handle (must not be NULL)
/// @return CBSDK_RESULT_SUCCESS on success, error code on failure
CBSDK_API cbsdk_result_t cbsdk_session_send_clock_probe(cbsdk_session_t session);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get the current value of std::chrono::steady_clock in nanoseconds since epoch.
/// Useful for correlating C++ steady_clock with other time sources (e.g., Python time.monotonic).
/// @return Nanoseconds since steady_clock epoch
CBSDK_API int64_t cbsdk_get_steady_clock_ns(void);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Handling
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get human-readable error message for result code
/// @param result Result code
/// @return Error message string (never NULL, always valid)
CBSDK_API const char* cbsdk_get_error_message(cbsdk_result_t result);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Version Information
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Get SDK version string
/// @return Version string (e.g., "2.0.0")
CBSDK_API const char* cbsdk_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // CBSDK_V2_CBSDK_H
