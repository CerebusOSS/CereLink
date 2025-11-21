///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   check_protocol_version.cpp
/// @author CereLink Development Team
/// @date   2025-01-17
///
/// @brief  Simple example demonstrating protocol version detection
///
/// This example shows how to use the cbdev API to determine which protocol version a device
/// is using. It demonstrates:
/// - Creating a device session with automatic protocol detection
/// - Querying the detected protocol version
/// - Interpreting the detection result
/// - Handling detection errors
///
/// Usage:
///   check_protocol_version [device_type]
///
/// Arguments:
///   device_type - Device type to connect to (default: NSP)
///                 Valid values: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY
///
/// Examples:
///   check_protocol_version              # Use defaults for NSP
///   check_protocol_version GEMINI_NSP   # Connect to Gemini NSP
///   check_protocol_version NPLAY        # Connect to nPlayServer
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbdev/connection.h>
#include <cbdev/device_factory.h>
#include <cbdev/device_session.h>
#include <iostream>
#include <cstring>
#include <cctype>
#include <algorithm>

using namespace cbdev;

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [device_type]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  device_type - Device type to connect to (default: NSP)\n";
    std::cout << "                Valid values: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog_name << "\n";
    std::cout << "  " << prog_name << " GEMINI_NSP\n";
    std::cout << "  " << prog_name << " NPLAY\n";
}

DeviceType parseDeviceType(const char* str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upper_str == "NSP") return DeviceType::NSP;
    if (upper_str == "GEMINI_NSP") return DeviceType::GEMINI_NSP;
    if (upper_str == "HUB1") return DeviceType::HUB1;
    if (upper_str == "HUB2") return DeviceType::HUB2;
    if (upper_str == "HUB3") return DeviceType::HUB3;
    if (upper_str == "NPLAY") return DeviceType::NPLAY;

    throw std::runtime_error("Invalid device type. Valid values: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY");
}

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  CereLink Protocol Version Detector\n";
    std::cout << "================================================\n\n";

    // Parse command line arguments
    DeviceType device_type = DeviceType::NSP;  // Default to NSP

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }

        try {
            device_type = parseDeviceType(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << "\n\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Create connection configuration for the specified device type
    const ConnectionParams config = ConnectionParams::forDevice(device_type);

    // Display configuration
    std::cout << "Configuration:\n";
    std::cout << "  Device Type:     ";
    switch (device_type) {
        case DeviceType::NSP:        std::cout << "NSP (Legacy Neural Signal Processor)\n"; break;
        case DeviceType::GEMINI_NSP: std::cout << "GEMINI_NSP (Gemini Neural Signal Processor)\n"; break;
        case DeviceType::HUB1:       std::cout << "HUB1 (Gemini Hub 1)\n"; break;
        case DeviceType::HUB2:       std::cout << "HUB2 (Gemini Hub 2)\n"; break;
        case DeviceType::HUB3:       std::cout << "HUB3 (Gemini Hub 3)\n"; break;
        case DeviceType::NPLAY:      std::cout << "NPLAY (nPlayServer)\n"; break;
        default:                     std::cout << "CUSTOM\n"; break;
    }
    std::cout << "  Device Address:  " << config.device_address << "\n";
    std::cout << "  Send Port:       " << config.send_port << "\n";
    std::cout << "  Client Address:  " << config.client_address << "\n";
    std::cout << "  Recv Port:       " << config.recv_port << "\n\n";

    // Create device session with automatic protocol detection
    std::cout << "Detecting protocol version...\n";
    std::cout << "  (Creating device session with auto-detection)\n\n";

    auto result = createDeviceSession(config, ProtocolVersion::UNKNOWN);

    // Handle result
    if (result.isError()) {
        std::cerr << "ERROR: Device session creation failed\n";
        std::cerr << "  Reason: " << result.error() << "\n\n";
        std::cerr << "Possible causes:\n";
        std::cerr << "  - Device is not responding or is offline\n";
        std::cerr << "  - Incorrect device type or network configuration\n";
        std::cerr << "  - Network connectivity issue\n";
        std::cerr << "  - Port already in use\n";
        return 1;
    }

    // Query the detected protocol version
    const auto device = std::move(result.value());
    const ProtocolVersion version = device->getProtocolVersion();

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

        case ProtocolVersion::PROTOCOL_410:
            std::cout << "Details:\n";
            std::cout << "  - This device uses protocol 4.1\n";
            std::cout << "  - Uses 64-bit timestamps and 16-bit packet types\n";
            std::cout << "  - It differs only slightly from current. Upgrade recommended.\n";
            break;

        case ProtocolVersion::PROTOCOL_CURRENT:
            std::cout << "Details:\n";
            std::cout << "  - This device uses the current protocol (4.2+)\n";
            std::cout << "  - Uses 64-bit timestamps and 16-bit packet types\n";
            std::cout << "  - Recommended for all new development\n";
            break;

        case ProtocolVersion::UNKNOWN:
        default:
            std::cout << "Details:\n";
            std::cout << "  - Device responded but protocol version could not be determined\n";
            std::cout << "  - This may indicate an unsupported protocol version\n";
            break;
    }

    std::cout << "\nDetection complete!\n";
    return 0;
}
