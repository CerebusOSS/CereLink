///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   connection.h
/// @author CereLink Development Team
/// @date   2025-01-23
///
/// @brief  C-compatible connection enumerations for Cerebus devices
///
/// This header provides C-compatible enumerations for device types, protocol versions, and
/// channel types. These can be used from both C and C++ code and serve as the canonical
/// definitions used throughout the codebase.
///
/// Usage from C:
///   #include <cbproto/connection.h>
///   cbproto_device_type_t device = CBPROTO_DEVICE_TYPE_NSP;
///
/// Usage from C++:
///   #include <cbproto/connection.h>
///   cbproto_device_type_t device = CBPROTO_DEVICE_TYPE_NSP;
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBPROTO_CONNECTION_H
#define CBPROTO_CONNECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Device Type Enumeration
/// @brief Enumeration of supported Cerebus device types
///
/// Each device type maps to specific network addresses and ports. These values are ABI-stable
/// and must not be changed to maintain binary compatibility.
/// @{
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum cbproto_device_type {
    CBPROTO_DEVICE_TYPE_LEGACY_NSP = 0,  ///< Neural Signal Processor (legacy, 192.168.137.128)
    CBPROTO_DEVICE_TYPE_NSP        = 1,  ///< Gemini NSP (192.168.137.128, port 51001)
    CBPROTO_DEVICE_TYPE_HUB1       = 2,  ///< Gemini Hub 1 (192.168.137.200, port 51002)
    CBPROTO_DEVICE_TYPE_HUB2       = 3,  ///< Gemini Hub 2 (192.168.137.201, port 51003)
    CBPROTO_DEVICE_TYPE_HUB3       = 4,  ///< Gemini Hub 3 (192.168.137.202, port 51004)
    CBPROTO_DEVICE_TYPE_NPLAY      = 5,  ///< nPlayServer (127.0.0.1, ports 51001/51002)
    CBPROTO_DEVICE_TYPE_CUSTOM     = 6,  ///< Custom IP/port configuration
    CBPROTO_DEVICE_TYPE_GEMINI_NPLAY = 7 ///< Gemini nPlayServer (127.0.0.1, port 51002 bidirectional)
} cbproto_device_type_t;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Protocol Version Enumeration
/// @brief Enumeration of Cerebus protocol versions
///
/// Different device firmware versions use different protocol formats. These values are
/// ABI-stable and must not be changed to maintain binary compatibility.
/// @{
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum cbproto_protocol_version {
    CBPROTO_PROTOCOL_UNKNOWN     = 0,  ///< Unknown or undetected protocol version
    CBPROTO_PROTOCOL_311         = 1,  ///< Legacy protocol 3.11 (32-bit timestamps, 8-bit packet types)
    CBPROTO_PROTOCOL_400         = 2,  ///< Legacy protocol 4.0 (64-bit timestamps, 8-bit packet types)
    CBPROTO_PROTOCOL_410         = 3,  ///< Protocol 4.1 (64-bit timestamps, 16-bit packet types)
    CBPROTO_PROTOCOL_CURRENT     = 4   ///< Current protocol 4.2+ (64-bit timestamps, 16-bit packet types)
} cbproto_protocol_version_t;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Channel Type Enumeration
/// @brief Enumeration of Cerebus channel types based on capabilities
///
/// Channels are categorized by their capabilities (analog input, digital I/O, etc.).
/// These values are ABI-stable and must not be changed to maintain binary compatibility.
/// @{
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum cbproto_channel_type {
    CBPROTO_CHANNEL_TYPE_FRONTEND    = 0,  ///< Front-end analog input (isolated, cbCHAN_AINP | cbCHAN_ISOLATED)
    CBPROTO_CHANNEL_TYPE_ANALOG_IN   = 1,  ///< Analog input (non-isolated, cbCHAN_AINP only)
    CBPROTO_CHANNEL_TYPE_ANALOG_OUT  = 2,  ///< Analog output (non-audio, cbCHAN_AOUT, not cbAOUT_AUDIO)
    CBPROTO_CHANNEL_TYPE_AUDIO       = 3,  ///< Audio output (cbCHAN_AOUT with cbAOUT_AUDIO)
    CBPROTO_CHANNEL_TYPE_DIGITAL_IN  = 4,  ///< Digital input (cbCHAN_DINP, not serial)
    CBPROTO_CHANNEL_TYPE_SERIAL      = 5,  ///< Serial input (cbCHAN_DINP with cbDINP_SERIALMASK)
    CBPROTO_CHANNEL_TYPE_DIGITAL_OUT = 6   ///< Digital output (cbCHAN_DOUT)
} cbproto_channel_type_t;

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name Device Rate Type Enumeration
/// @brief Enumeration of Cerebus sampling group rates
///
/// @{
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum cbproto_group_rate {
    CBPROTO_GROUP_RATE_NONE     = 0,
    CBPROTO_GROUP_RATE_500Hz    = 1,
    CBPROTO_GROUP_RATE_1000Hz   = 2,
    CBPROTO_GROUP_RATE_2000Hz   = 3,
    CBPROTO_GROUP_RATE_10000Hz  = 4,
    CBPROTO_GROUP_RATE_30000Hz  = 5,
    CBPROTO_GROUP_RATE_RAW      = 6
} cbproto_group_rate_t;

/// @}

#ifdef __cplusplus
}
#endif

#endif // CBPROTO_CONNECTION_H
