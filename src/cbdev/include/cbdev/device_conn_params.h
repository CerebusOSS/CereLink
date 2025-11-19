///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_conn_params.h
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

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Connection Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Device type enumeration (for connection addressing)
enum class DeviceType {
    NSP,            ///< Neural Signal Processor (legacy)
    GEMINI_NSP,     ///< Gemini NSP
    HUB1,           ///< Hub 1 (legacy addressing)
    HUB2,           ///< Hub 2 (legacy addressing)
    HUB3,           ///< Hub 3 (legacy addressing)
    NPLAY,          ///< nPlayServer
    CUSTOM          ///< Custom IP/port configuration
};

/// Connection parameters for device communication
/// Note: This contains network/socket configuration only.
/// Device operating configuration (sample rates, channels, etc.) is in shared memory.
struct ConnectionParams {
    DeviceType type = DeviceType::NSP;

    // Network addresses
    std::string device_address;     ///< Device IP address (where to send packets)
    std::string client_address;     ///< Client IP address (where to bind receive socket)

    // Ports
    uint16_t recv_port = 51001;     ///< Port to receive packets on (client side)
    uint16_t send_port = 51002;     ///< Port to send packets to (device side)

    // Socket options
    bool broadcast = false;         ///< Enable broadcast mode
    bool non_blocking = false;      ///< Non-blocking socket (false = blocking, better for dedicated receive thread)
    int recv_buffer_size = 6000000; ///< Receive buffer size (6MB default)

    // Connection options
    bool autorun = true;            ///< Auto-start device on connect (true = performStartupHandshake, false = requestConfiguration only)

    /// Create connection parameters for a known device type
    static ConnectionParams forDevice(DeviceType type);

    /// Create custom connection parameters with explicit addresses
    static ConnectionParams custom(const std::string& device_addr,
                                   const std::string& client_addr = "0.0.0.0",
                                   uint16_t recv_port = 51001,
                                   uint16_t send_port = 51002);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Device Defaults
///////////////////////////////////////////////////////////////////////////////////////////////////

/// Default device addresses and ports (from upstream/cbproto/cbproto.h)
namespace ConnectionDefaults {
    // Device addresses
    constexpr const char* NSP_ADDRESS        = "192.168.137.128";  // Legacy NSP (cbNET_UDP_ADDR_CNT)
    constexpr const char* GEMINI_NSP_ADDRESS = "192.168.137.128";  // Gemini NSP (cbNET_UDP_ADDR_GEMINI_NSP)
    constexpr const char* GEMINI_HUB1_ADDRESS = "192.168.137.200"; // Gemini Hub1 (cbNET_UDP_ADDR_GEMINI_HUB)
    constexpr const char* GEMINI_HUB2_ADDRESS = "192.168.137.201"; // Gemini Hub2 (cbNET_UDP_ADDR_GEMINI_HUB2)
    constexpr const char* GEMINI_HUB3_ADDRESS = "192.168.137.202"; // Gemini Hub3 (cbNET_UDP_ADDR_GEMINI_HUB3)
    constexpr const char* NPLAY_ADDRESS      = "127.0.0.1";        // nPlayServer (loopback)

    // Client/Host addresses (empty = auto-detect based on device type and platform)
    constexpr const char* DEFAULT_CLIENT_ADDRESS = "";  // Auto-detect (was 192.168.137.199)

    // Ports
    constexpr uint16_t LEGACY_NSP_RECV_PORT   = 51001;  // cbNET_UDP_PORT_CNT
    constexpr uint16_t LEGACY_NSP_SEND_PORT   = 51002;  // cbNET_UDP_PORT_BCAST
    constexpr uint16_t GEMINI_NSP_PORT        = 51001;  // cbNET_UDP_PORT_GEMINI_NSP (both send & recv)
    constexpr uint16_t GEMINI_HUB1_PORT       = 51002;  // cbNET_UDP_PORT_GEMINI_HUB (both send & recv)
    constexpr uint16_t GEMINI_HUB2_PORT       = 51003;  // cbNET_UDP_PORT_GEMINI_HUB2 (both send & recv)
    constexpr uint16_t GEMINI_HUB3_PORT       = 51004;  // cbNET_UDP_PORT_GEMINI_HUB3 (both send & recv)
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
