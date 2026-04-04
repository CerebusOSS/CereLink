"""
cffi C declarations for the cbsdk C API.

These must match the types and function signatures in cbsdk.h exactly.
Complex structs (cbPKT_CHANINFO, cbPKT_SYSINFO, etc.) are NOT defined here;
instead, FFI-friendly accessor functions are used.
"""

CDEF = """
///////////////////////////////////////////////////////////////////////////
// Enums
///////////////////////////////////////////////////////////////////////////

typedef enum {
    CBSDK_RESULT_SUCCESS             =  0,
    CBSDK_RESULT_INVALID_PARAMETER   = -1,
    CBSDK_RESULT_ALREADY_RUNNING     = -2,
    CBSDK_RESULT_NOT_RUNNING         = -3,
    CBSDK_RESULT_SHMEM_ERROR         = -4,
    CBSDK_RESULT_DEVICE_ERROR        = -5,
    CBSDK_RESULT_INTERNAL_ERROR      = -6,
} cbsdk_result_t;

typedef enum {
    CBPROTO_DEVICE_TYPE_LEGACY_NSP = 0,
    CBPROTO_DEVICE_TYPE_NSP        = 1,
    CBPROTO_DEVICE_TYPE_HUB1       = 2,
    CBPROTO_DEVICE_TYPE_HUB2       = 3,
    CBPROTO_DEVICE_TYPE_HUB3       = 4,
    CBPROTO_DEVICE_TYPE_NPLAY      = 5,
    CBPROTO_DEVICE_TYPE_CUSTOM     = 6,
} cbproto_device_type_t;

typedef enum {
    CBPROTO_CHANNEL_TYPE_FRONTEND    = 0,
    CBPROTO_CHANNEL_TYPE_ANALOG_IN   = 1,
    CBPROTO_CHANNEL_TYPE_ANALOG_OUT  = 2,
    CBPROTO_CHANNEL_TYPE_AUDIO       = 3,
    CBPROTO_CHANNEL_TYPE_DIGITAL_IN  = 4,
    CBPROTO_CHANNEL_TYPE_SERIAL      = 5,
    CBPROTO_CHANNEL_TYPE_DIGITAL_OUT = 6,
} cbproto_channel_type_t;

typedef enum {
    CBPROTO_GROUP_RATE_NONE     = 0,
    CBPROTO_GROUP_RATE_500Hz    = 1,
    CBPROTO_GROUP_RATE_1000Hz   = 2,
    CBPROTO_GROUP_RATE_2000Hz   = 3,
    CBPROTO_GROUP_RATE_10000Hz  = 4,
    CBPROTO_GROUP_RATE_30000Hz  = 5,
    CBPROTO_GROUP_RATE_RAW      = 6
} cbproto_group_rate_t;

///////////////////////////////////////////////////////////////////////////
// Packet Structures (protocol-defined, stable layout)
///////////////////////////////////////////////////////////////////////////

// Packet header: 16 bytes
typedef struct {
    uint64_t time;
    uint16_t chid;
    uint16_t type;
    uint16_t dlen;
    uint8_t  instrument;
    uint8_t  reserved;
} cbPKT_HEADER;

// Generic packet: 1024 bytes (header + 1008 byte payload)
typedef struct {
    cbPKT_HEADER cbpkt_header;
    union {
        uint8_t  data_u8[1008];
        uint16_t data_u16[504];
        uint32_t data_u32[252];
    };
} cbPKT_GENERIC;

// Group (continuous data) packet: header + int16 samples
typedef struct {
    cbPKT_HEADER cbpkt_header;
    int16_t data[272];
} cbPKT_GROUP;

///////////////////////////////////////////////////////////////////////////
// Configuration and Statistics Structures
///////////////////////////////////////////////////////////////////////////

typedef struct {
    int device_type;  // cbproto_device_type_t
    size_t callback_queue_depth;
    _Bool enable_realtime_priority;
    _Bool drop_on_overflow;
    int recv_buffer_size;
    _Bool non_blocking;
    const char* custom_device_address;
    const char* custom_client_address;
    uint16_t custom_device_port;
    uint16_t custom_client_port;
} cbsdk_config_t;

typedef struct {
    uint64_t packets_received_from_device;
    uint64_t bytes_received_from_device;
    uint64_t packets_stored_to_shmem;
    uint64_t packets_queued_for_callback;
    uint64_t packets_delivered_to_callback;
    uint64_t packets_dropped;
    uint64_t queue_current_depth;
    uint64_t queue_max_depth;
    uint64_t packets_sent_to_device;
    uint64_t shmem_store_errors;
    uint64_t receive_errors;
    uint64_t send_errors;
} cbsdk_stats_t;

typedef struct {
    int16_t  digmin;
    int16_t  digmax;
    int32_t  anamin;
    int32_t  anamax;
    int32_t  anagain;
    char     anaunit[8];
} cbsdk_channel_scaling_t;

///////////////////////////////////////////////////////////////////////////
// Callback Types
///////////////////////////////////////////////////////////////////////////

typedef uint32_t cbsdk_callback_handle_t;

typedef void (*cbsdk_packet_callback_fn)(const cbPKT_GENERIC* pkts, size_t count, void* user_data);
typedef void (*cbsdk_event_callback_fn)(const cbPKT_GENERIC* pkt, void* user_data);
typedef void (*cbsdk_group_callback_fn)(const cbPKT_GROUP* pkt, void* user_data);
typedef void (*cbsdk_group_batch_callback_fn)(const int16_t* samples, size_t n_samples,
                                               size_t n_channels, const uint64_t* timestamps,
                                               void* user_data);
typedef void (*cbsdk_config_callback_fn)(const cbPKT_GENERIC* pkt, void* user_data);
typedef void (*cbsdk_error_callback_fn)(const char* error_message, void* user_data);

///////////////////////////////////////////////////////////////////////////
// Opaque Handle
///////////////////////////////////////////////////////////////////////////

typedef struct cbsdk_session_impl* cbsdk_session_t;

///////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////

// Config
cbsdk_config_t cbsdk_config_default(void);

// Session lifecycle
cbsdk_result_t cbsdk_session_create(cbsdk_session_t* session, const cbsdk_config_t* config);
void cbsdk_session_destroy(cbsdk_session_t session);
cbsdk_result_t cbsdk_session_start(cbsdk_session_t session);
void cbsdk_session_stop(cbsdk_session_t session);
_Bool cbsdk_session_is_running(cbsdk_session_t session);

// Legacy callbacks
void cbsdk_session_set_packet_callback(cbsdk_session_t session,
    cbsdk_packet_callback_fn callback, void* user_data);
void cbsdk_session_set_error_callback(cbsdk_session_t session,
    cbsdk_error_callback_fn callback, void* user_data);

// Typed callback registration
cbsdk_callback_handle_t cbsdk_session_register_packet_callback(
    cbsdk_session_t session, cbsdk_packet_callback_fn callback, void* user_data);
cbsdk_callback_handle_t cbsdk_session_register_event_callback(
    cbsdk_session_t session, cbproto_channel_type_t channel_type,
    cbsdk_event_callback_fn callback, void* user_data);
cbsdk_callback_handle_t cbsdk_session_register_group_callback(
    cbsdk_session_t session, cbproto_group_rate_t rate,
    cbsdk_group_callback_fn callback, void* user_data);
cbsdk_callback_handle_t cbsdk_session_register_group_batch_callback(
    cbsdk_session_t session, cbproto_group_rate_t rate,
    cbsdk_group_batch_callback_fn callback, void* user_data);
cbsdk_callback_handle_t cbsdk_session_register_config_callback(
    cbsdk_session_t session, uint16_t packet_type,
    cbsdk_config_callback_fn callback, void* user_data);
void cbsdk_session_unregister_callback(cbsdk_session_t session,
    cbsdk_callback_handle_t handle);

// Statistics
void cbsdk_session_get_stats(cbsdk_session_t session, cbsdk_stats_t* stats);
void cbsdk_session_reset_stats(cbsdk_session_t session);

// Configuration access
uint32_t cbsdk_session_get_runlevel(cbsdk_session_t session);
int cbsdk_session_is_standalone(cbsdk_session_t session);
uint32_t cbsdk_session_get_protocol_version(cbsdk_session_t session);
uint32_t cbsdk_session_get_proc_ident(cbsdk_session_t session, char* buf, uint32_t buf_size);
uint32_t cbsdk_session_get_spike_length(cbsdk_session_t session);
uint32_t cbsdk_session_get_spike_pretrigger(cbsdk_session_t session);
cbsdk_result_t cbsdk_session_set_spike_length(cbsdk_session_t session,
    uint32_t spike_length, uint32_t spike_pretrigger);
uint32_t cbsdk_get_max_chans(void);
uint32_t cbsdk_get_num_fe_chans(void);
uint32_t cbsdk_get_num_analog_chans(void);
const char* cbsdk_session_get_channel_label(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_smpgroup(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_chancaps(cbsdk_session_t session, uint32_t chan_id);
const char* cbsdk_session_get_group_label(cbsdk_session_t session, uint32_t group_id);
cbsdk_result_t cbsdk_session_get_group_list(cbsdk_session_t session,
    uint32_t group_id, uint16_t* list, uint32_t* count);

// Channel configuration
cbsdk_result_t cbsdk_session_set_channel_sample_group(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    cbproto_group_rate_t rate, _Bool disable_others);
cbsdk_result_t cbsdk_session_set_ac_input_coupling(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    _Bool enabled);

// Per-channel getters
cbproto_channel_type_t cbsdk_session_get_channel_type(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_smpfilter(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_spkfilter(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_spkopts(cbsdk_session_t session, uint32_t chan_id);
int32_t  cbsdk_session_get_channel_spkthrlevel(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_ainpopts(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_lncrate(cbsdk_session_t session, uint32_t chan_id);
uint32_t cbsdk_session_get_channel_refelecchan(cbsdk_session_t session, uint32_t chan_id);
int16_t  cbsdk_session_get_channel_amplrejpos(cbsdk_session_t session, uint32_t chan_id);
int16_t  cbsdk_session_get_channel_amplrejneg(cbsdk_session_t session, uint32_t chan_id);
cbsdk_result_t cbsdk_session_get_channel_scaling(
    cbsdk_session_t session, uint32_t chan_id, cbsdk_channel_scaling_t* scaling);

// Per-channel setters
cbsdk_result_t cbsdk_session_set_channel_label(cbsdk_session_t session,
    uint32_t chan_id, const char* label);
cbsdk_result_t cbsdk_session_set_channel_smpfilter(cbsdk_session_t session,
    uint32_t chan_id, uint32_t filter_id);
cbsdk_result_t cbsdk_session_set_channel_spkfilter(cbsdk_session_t session,
    uint32_t chan_id, uint32_t filter_id);
cbsdk_result_t cbsdk_session_set_channel_ainpopts(cbsdk_session_t session,
    uint32_t chan_id, uint32_t ainpopts);
cbsdk_result_t cbsdk_session_set_channel_lncrate(cbsdk_session_t session,
    uint32_t chan_id, uint32_t lncrate);
cbsdk_result_t cbsdk_session_set_channel_spkopts(cbsdk_session_t session,
    uint32_t chan_id, uint32_t spkopts);
cbsdk_result_t cbsdk_session_set_channel_spkthrlevel(cbsdk_session_t session,
    uint32_t chan_id, int32_t level);
cbsdk_result_t cbsdk_session_set_channel_autothreshold(cbsdk_session_t session,
    uint32_t chan_id, _Bool enabled);

// Channel info field selector
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

// Generic single-channel field getter
int64_t cbsdk_session_get_channel_field(cbsdk_session_t session,
    uint32_t chan_id, cbsdk_chaninfo_field_t field);

// Bulk channel queries

cbsdk_result_t cbsdk_session_get_matching_channels(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    uint32_t* out_ids, uint32_t* out_count);
cbsdk_result_t cbsdk_session_get_channels_field(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    cbsdk_chaninfo_field_t field, int64_t* out_values, uint32_t* out_count);
cbsdk_result_t cbsdk_session_get_channels_labels(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    char* out_buf, size_t label_stride, uint32_t* out_count);

cbsdk_result_t cbsdk_session_get_channels_positions(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    int32_t* out_positions, uint32_t* out_count);

// Bulk configuration access
uint32_t cbsdk_session_get_sysfreq(cbsdk_session_t session);
uint32_t cbsdk_get_num_filters(void);
const char* cbsdk_session_get_filter_label(cbsdk_session_t session, uint32_t filter_id);
uint32_t cbsdk_session_get_filter_hpfreq(cbsdk_session_t session, uint32_t filter_id);
uint32_t cbsdk_session_get_filter_hporder(cbsdk_session_t session, uint32_t filter_id);
uint32_t cbsdk_session_get_filter_lpfreq(cbsdk_session_t session, uint32_t filter_id);
uint32_t cbsdk_session_get_filter_lporder(cbsdk_session_t session, uint32_t filter_id);

// Instrument time
cbsdk_result_t cbsdk_session_get_time(cbsdk_session_t session, uint64_t* time);

// Patient information
cbsdk_result_t cbsdk_session_set_patient_info(cbsdk_session_t session,
    const char* id, const char* firstname, const char* lastname,
    uint32_t dob_month, uint32_t dob_day, uint32_t dob_year);

// Analog output
cbsdk_result_t cbsdk_session_set_analog_output_monitor(cbsdk_session_t session,
    uint32_t aout_chan_id, uint32_t monitor_chan_id,
    _Bool track_last, _Bool spike_only);

// Commands
cbsdk_result_t cbsdk_session_send_comment(cbsdk_session_t session,
    const char* comment, uint32_t rgba, uint8_t charset);
cbsdk_result_t cbsdk_session_send_packet(cbsdk_session_t session,
    const cbPKT_GENERIC* pkt);
cbsdk_result_t cbsdk_session_set_digital_output(cbsdk_session_t session,
    uint32_t chan_id, uint16_t value);
cbsdk_result_t cbsdk_session_set_runlevel(cbsdk_session_t session,
    uint32_t runlevel);

// CCF configuration files
cbsdk_result_t cbsdk_session_load_channel_map(cbsdk_session_t session, const char* filepath, uint32_t bank_offset);
cbsdk_result_t cbsdk_session_save_ccf(cbsdk_session_t session, const char* filename);
cbsdk_result_t cbsdk_session_load_ccf(cbsdk_session_t session, const char* filename);
cbsdk_result_t cbsdk_session_load_ccf_sync(cbsdk_session_t session, const char* filename, uint32_t timeout_ms);

// Recording control (Central)
cbsdk_result_t cbsdk_session_start_central_recording(cbsdk_session_t session,
    const char* filename, const char* comment);
cbsdk_result_t cbsdk_session_stop_central_recording(cbsdk_session_t session);
cbsdk_result_t cbsdk_session_open_central_file_dialog(cbsdk_session_t session);
cbsdk_result_t cbsdk_session_close_central_file_dialog(cbsdk_session_t session);

// Spike sorting
cbsdk_result_t cbsdk_session_set_channel_spike_sorting(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    uint32_t sort_options);

// Spike extraction (enable/disable cbAINPSPK_EXTRACT via CHANSETSPK)
cbsdk_result_t cbsdk_session_set_spike_extraction(
    cbsdk_session_t session, size_t n_chans, cbproto_channel_type_t chan_type,
    bool enabled);

// Clock synchronization
cbsdk_result_t cbsdk_session_get_clock_offset(cbsdk_session_t session, int64_t* offset_ns);
cbsdk_result_t cbsdk_session_get_clock_uncertainty(cbsdk_session_t session, int64_t* uncertainty_ns);
cbsdk_result_t cbsdk_session_send_clock_probe(cbsdk_session_t session);

// Utility
int64_t cbsdk_get_steady_clock_ns(void);

// Error handling & version
const char* cbsdk_get_error_message(cbsdk_result_t result);
const char* cbsdk_get_version(void);

"""
