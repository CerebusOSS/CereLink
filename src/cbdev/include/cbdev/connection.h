///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   connection.h
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Device connection parameters for Cerebus devices
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_DEVICE_CONFIG_H
#define CBDEV_DEVICE_CONFIG_H

#include <string>
#include <cstdint>
#include <cbproto/types.h>
#include <cbproto/connection.h>

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Connection Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Device type enumeration (C++ wrapper around C enum for type safety)
enum class DeviceType : uint32_t {
    LEGACY_NSP = CBPROTO_DEVICE_TYPE_LEGACY_NSP,  ///< Neural Signal Processor (legacy)
    NSP        = CBPROTO_DEVICE_TYPE_NSP,         ///< Gemini NSP
    HUB1       = CBPROTO_DEVICE_TYPE_HUB1,        ///< Hub 1 (legacy addressing)
    HUB2       = CBPROTO_DEVICE_TYPE_HUB2,        ///< Hub 2 (legacy addressing)
    HUB3       = CBPROTO_DEVICE_TYPE_HUB3,        ///< Hub 3 (legacy addressing)
    NPLAY      = CBPROTO_DEVICE_TYPE_NPLAY,       ///< nPlayServer
    CUSTOM     = CBPROTO_DEVICE_TYPE_CUSTOM       ///< Custom IP/port configuration
};

/// Protocol version enumeration (C++ wrapper around C enum for type safety)
enum class ProtocolVersion : uint32_t {
    UNKNOWN        = CBPROTO_PROTOCOL_UNKNOWN,    ///< Unknown or undetected protocol
    PROTOCOL_311   = CBPROTO_PROTOCOL_311,        ///< Legacy cbproto_311 (32-bit timestamps, deprecated)
    PROTOCOL_400   = CBPROTO_PROTOCOL_400,        ///< Legacy cbproto_400 (64-bit timestamps, deprecated)
    PROTOCOL_410   = CBPROTO_PROTOCOL_410,        ///< Protocol 4.1 (64-bit timestamps, 16-bit packet types)
    PROTOCOL_CURRENT = CBPROTO_PROTOCOL_CURRENT   ///< Current protocol (64-bit timestamps)
};

/// Channel type enumeration (C++ wrapper around C enum for type safety)
enum class ChannelType : uint32_t {
    FRONTEND    = CBPROTO_CHANNEL_TYPE_FRONTEND,    ///< Front-end analog input (isolated)
    ANALOG_IN   = CBPROTO_CHANNEL_TYPE_ANALOG_IN,   ///< Analog input (non-isolated)
    ANALOG_OUT  = CBPROTO_CHANNEL_TYPE_ANALOG_OUT,  ///< Analog output (non-audio)
    AUDIO       = CBPROTO_CHANNEL_TYPE_AUDIO,       ///< Audio output
    DIGITAL_IN  = CBPROTO_CHANNEL_TYPE_DIGITAL_IN,  ///< Digital input
    SERIAL      = CBPROTO_CHANNEL_TYPE_SERIAL,      ///< Serial input
    DIGITAL_OUT = CBPROTO_CHANNEL_TYPE_DIGITAL_OUT  ///< Digital output
};

/// Convert protocol version to string for logging
/// @param version Protocol version
/// @return Human-readable string
const char* protocolVersionToString(ProtocolVersion version);

/// Convert device type to string for logging
/// @param type Device type
/// @return Human-readable string
const char* deviceTypeToString(DeviceType type);

/// Convert channel type to string for logging
/// @param type Channel type
/// @return Human-readable string
const char* channelTypeToString(ChannelType type);

/// Connection parameters for device communication
/// Note: This contains network/socket configuration only.
/// Device operating configuration (sample rates, channels, etc.) is in shared memory.
struct ConnectionParams {
    DeviceType type = DeviceType::LEGACY_NSP;

    // Network addresses
    std::string device_address;     ///< Device IP address (where to send packets)
    std::string client_address;     ///< Client IP address (where to bind receive socket)

    // Ports
    uint16_t recv_port = cbNET_UDP_PORT_CNT;     ///< Port to receive packets on (client side)
    uint16_t send_port = cbNET_UDP_PORT_BCAST;     ///< Port to send packets to (device side)

    // Socket options
    bool broadcast = false;         ///< Enable broadcast mode
    bool non_blocking = false;      ///< Non-blocking socket (false = blocking, better for dedicated receive thread)
    int recv_buffer_size = 6000000; ///< Receive buffer size (6MB default)
    int send_buffer_size = 0;       ///< Send buffer size (0 = OS default, typically 64KB-256KB)

    // Connection options
    bool autorun = true;            ///< Auto-start device on connect (true = performStartupHandshake, false = requestConfiguration only)

    /// Create connection parameters for a known device type
    static ConnectionParams forDevice(DeviceType type);

    /// Create custom connection parameters with explicit addresses
    static ConnectionParams custom(const std::string& device_addr,
                                   const std::string& client_addr = "0.0.0.0",
                                   uint16_t recv_port = cbNET_UDP_PORT_CNT,
                                   uint16_t send_port = cbNET_UDP_PORT_BCAST);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Device Defaults
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Default device addresses and ports (from upstream/cbproto/cbproto.h)
namespace ConnectionDefaults {
    // Device addresses
    constexpr const char* LEGACY_NSP_ADDRESS        = cbNET_UDP_ADDR_CNT;  // Legacy NSP
    constexpr const char* NSP_ADDRESS = cbNET_UDP_ADDR_GEMINI_NSP;  // Gemini NSP
    constexpr const char* HUB1_ADDRESS = cbNET_UDP_ADDR_GEMINI_HUB; // Gemini Hub1
    constexpr const char* HUB2_ADDRESS = cbNET_UDP_ADDR_GEMINI_HUB2; // Gemini Hub2
    constexpr const char* HUB3_ADDRESS = cbNET_UDP_ADDR_GEMINI_HUB3; // Gemini Hub3
    constexpr const char* NPLAY_ADDRESS      = "127.0.0.1";        // nPlayServer (loopback)

    // Client/Host addresses (empty = auto-detect based on device type and platform)
    constexpr const char* DEFAULT_CLIENT_ADDRESS = "";  // Auto-detect (was 192.168.137.199)

    // Ports
    constexpr uint16_t LEGACY_NSP_RECV_PORT   = cbNET_UDP_PORT_CNT;
    constexpr uint16_t LEGACY_NSP_SEND_PORT   = cbNET_UDP_PORT_BCAST;
    constexpr uint16_t NSP_PORT        = cbNET_UDP_PORT_GEMINI_NSP;
    constexpr uint16_t HUB1_PORT       = cbNET_UDP_PORT_GEMINI_HUB;  // cbNET_UDP_PORT_GEMINI_HUB (both send & recv)
    constexpr uint16_t HUB2_PORT       = cbNET_UDP_PORT_GEMINI_HUB2;  // cbNET_UDP_PORT_GEMINI_HUB2 (both send & recv)
    constexpr uint16_t HUB3_PORT       = cbNET_UDP_PORT_GEMINI_HUB3;  // cbNET_UDP_PORT_GEMINI_HUB3 (both send & recv)
    constexpr uint16_t NPLAY_PORT             = 51001;  // nPlayServer port
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Detect local IP address for binding receive socket
/// On macOS with multiple interfaces, returns "0.0.0.0" (bind to all)
/// On other platforms, attempts to find the adapter on the Cerebus subnet
/// @return IP address string, or "0.0.0.0" if detection fails
std::string detectLocalIP();

} // namespace cbdev

#endif // CBDEV_DEVICE_CONFIG_H
