///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   check_protocol_version.cpp
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Simple example demonstrating protocol version detection
///
/// This example shows how to use the cbdev protocol detector to determine which protocol
/// version a device is using. It demonstrates:
/// - Using the protocol detector with custom addresses/ports
/// - Interpreting the detection result
/// - Handling detection errors
///
/// Usage:
///   check_protocol_version [device_addr] [device_port] [client_addr] [client_port] [timeout_ms]
///
/// Arguments (all optional):
///   device_addr  - Device IP address (default: 192.168.137.128)
///   device_port  - Device UDP port (default: 51001)
///   client_addr  - Client IP address for binding (default: 0.0.0.0)
///   client_port  - Client UDP port for binding (default: 51002)
///   timeout_ms   - Timeout in milliseconds (default: 500)
///
/// Examples:
///   check_protocol_version                           # Use defaults for NSP
///   check_protocol_version 192.168.137.128 51001     # Custom device, default client
///   check_protocol_version 127.0.0.1 51001 127.0.0.1 51002 1000  # Full custom with 1s timeout
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbdev/protocol_detector.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

using namespace cbdev;

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name
              << " [device_addr] [device_port] [client_addr] [client_port] [timeout_ms]\n\n";
    std::cout << "Arguments (all optional):\n";
    std::cout << "  device_addr  - Device IP address (default: 192.168.137.128)\n";
    std::cout << "  send_port  - Device reads config packets on this port (default: 51001)\n";
    std::cout << "  client_addr  - Client IP address for binding (default: 0.0.0.0)\n";
    std::cout << "  recv_port  - Client UDP port for binding (default: 51002)\n";
    std::cout << "  timeout_ms   - Timeout in milliseconds (default: 500)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog_name << "\n";
    std::cout << "  " << prog_name << " 192.168.137.128 51001\n";
    std::cout << "  " << prog_name << " 127.0.0.1 51001 127.0.0.1 51002 1000\n";
}

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  CereLink Protocol Version Detector\n";
    std::cout << "================================================\n\n";

    // Parse command line arguments with defaults
    const char* device_addr = "192.168.137.128";
    uint16_t send_port = 51001;
    const char* client_addr = "0.0.0.0";
    uint16_t recv_port = 51002;
    uint32_t timeout_ms = 500;

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        device_addr = argv[1];
    }
    if (argc > 2) {
        send_port = static_cast<uint16_t>(std::atoi(argv[2]));
    }
    if (argc > 3) {
        client_addr = argv[3];
    }
    if (argc > 4) {
        recv_port = static_cast<uint16_t>(std::atoi(argv[4]));
    }
    if (argc > 5) {
        timeout_ms = static_cast<uint32_t>(std::atoi(argv[5]));
    }

    // Display configuration
    std::cout << "Configuration:\n";
    std::cout << "  Device Address:  " << device_addr << "\n";
    std::cout << "  Device Port:     " << send_port << "\n";
    std::cout << "  Client Address:  " << client_addr << "\n";
    std::cout << "  Client Port:     " << recv_port << "\n";
    std::cout << "  Timeout:         " << timeout_ms << " ms\n\n";

    // Detect protocol version
    std::cout << "Detecting protocol version...\n";
    std::cout << "  (Sending dual-format probe packets and analyzing response)\n\n";

    auto result = detectProtocol(device_addr, send_port,
                                 client_addr, recv_port,
                                 timeout_ms);

    // Handle result
    if (result.isError()) {
        std::cerr << "ERROR: Protocol detection failed\n";
        std::cerr << "  Reason: " << result.error() << "\n\n";
        std::cerr << "Possible causes:\n";
        std::cerr << "  - Device is not responding or is offline\n";
        std::cerr << "  - Incorrect device address or port\n";
        std::cerr << "  - Network configuration issue\n";
        std::cerr << "  - Client address/port already in use\n";
        std::cerr << "  - Timeout too short for device response\n";
        return 1;
    }

    // Display detected version
    ProtocolVersion version = result.value();
    std::cout << "Protocol Detection Result:\n";
    std::cout << "==========================\n";
    std::cout << "  Detected Version: " << protocolVersionToString(version) << "\n\n";

    // Provide additional information based on version
    switch (version) {
        case ProtocolVersion::PROTOCOL_311:
            std::cout << "Details:\n";
            std::cout << "  - This device uses the legacy protocol 3.11\n";
            std::cout << "  - Uses 32-bit timestamps and 8-bit packet types\n";
            std::cout << "  - Requires special handling for compatibility\n";
            break;

        case ProtocolVersion::PROTOCOL_400:
            std::cout << "Details:\n";
            std::cout << "  - This device uses the legacy protocol 4.0\n";
            std::cout << "  - Uses 64-bit timestamps and 8-bit packet types\n";
            std::cout << "  - Requires special handling for compatibility\n";
            break;

        case ProtocolVersion::PROTOCOL_CURRENT:
            std::cout << "Details:\n";
            std::cout << "  - This device uses the current protocol (4.1/4.2)\n";
            std::cout << "  - Uses 64-bit timestamps and 16-bit packet types\n";
            std::cout << "  - Recommended for all new development\n";
            break;

        case ProtocolVersion::UNKNOWN:
            std::cout << "Details:\n";
            std::cout << "  - Device responded but protocol version could not be determined\n";
            std::cout << "  - This may indicate an unsupported protocol version\n";
            break;
    }

    std::cout << "\nDetection complete!\n";
    return 0;
}
