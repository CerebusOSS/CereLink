///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   configure_channels.cpp
/// @author CereLink Development Team
/// @date   2025-01-22
///
/// @brief  Example demonstrating channel configuration using cbdev API
///
/// This example shows how to use the cbdev API to configure channels on a Cerebus device.
/// It demonstrates:
/// - Creating a device session with automatic protocol detection
/// - Requesting device configuration synchronously
/// - Querying device configuration (sysinfo, chaninfo)
/// - Setting sampling group synchronously (with device confirmation)
/// - Optionally restoring the original device state
///
/// Usage:
///   configure_channels [device_type] [channel_type] [num_channels] [group_id] [--restore]
///
/// Arguments:
///   device_type    - Device type to connect to (default: NSP)
///                    Valid values: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY
///   channel_type   - Channel type to configure (default: FRONTEND)
///                    Valid values: FRONTEND, ANALOG_IN, ANALOG_OUT, AUDIO,
///                                  DIGITAL_IN, SERIAL, DIGITAL_OUT
///   num_channels   - Number of channels to configure (default: 1, use 0 for all)
///   group_id       - Sampling group ID to set (default: 1, range: 0-6)
///                    Groups 1-4: Set smpgroup and filter
///                    Group 5: Disable groups 1-4 and 6
///                    Group 6: Raw stream
///                    Group 0: Disable all groups
///   --restore      - Restore original configuration after demonstrating change
///
/// Examples:
///   configure_channels                                    # Use defaults
///   configure_channels NSP FRONTEND 10 1                  # Configure 10 FE channels to group 1
///   configure_channels GEMINI_NSP FRONTEND 0 6 --restore  # Configure all FE to raw, then restore
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbdev/connection.h>
#include <cbdev/device_factory.h>
#include <cbdev/device_session.h>
#include <cbproto/cbproto.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace cbdev;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [device_type] [channel_type] [num_channels] [group_id] [--restore]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  device_type    - Device type to connect to (default: NSP)\n";
    std::cout << "                   Valid values: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY\n\n";
    std::cout << "  channel_type   - Channel type to configure (default: FRONTEND)\n";
    std::cout << "                   Valid values: FRONTEND, ANALOG_IN, ANALOG_OUT, AUDIO,\n";
    std::cout << "                                 DIGITAL_IN, SERIAL, DIGITAL_OUT\n\n";
    std::cout << "  num_channels   - Number of channels to configure (default: 1, use 0 for all)\n\n";
    std::cout << "  group_id       - Sampling group ID to set (default: 1, range: 0-6)\n";
    std::cout << "                   Groups 1-4: Set smpgroup and filter\n";
    std::cout << "                   Group 5: Disable groups 1-4 and 6\n";
    std::cout << "                   Group 6: Raw stream\n";
    std::cout << "                   Group 0: Disable all groups\n\n";
    std::cout << "  --restore      - Restore original configuration after demonstrating change\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog_name << "\n";
    std::cout << "  " << prog_name << " NSP FRONTEND 10 1\n";
    std::cout << "  " << prog_name << " GEMINI_NSP FRONTEND 0 6 --restore\n";
}

DeviceType parseDeviceType(const char* str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upper_str == "LEGACY_NSP") return DeviceType::LEGACY_NSP;
    if (upper_str == "NSP") return DeviceType::NSP;
    if (upper_str == "HUB1") return DeviceType::HUB1;
    if (upper_str == "HUB2") return DeviceType::HUB2;
    if (upper_str == "HUB3") return DeviceType::HUB3;
    if (upper_str == "NPLAY") return DeviceType::NPLAY;

    throw std::runtime_error("Invalid device type. Valid values: LEGACY_NSP, NSP, HUB1, HUB2, HUB3, NPLAY");
}

ChannelType parseChannelType(const char* str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upper_str == "FRONTEND") return ChannelType::FRONTEND;
    if (upper_str == "ANALOG_IN" || upper_str == "ANAIN") return ChannelType::ANALOG_IN;
    if (upper_str == "ANALOG_OUT" || upper_str == "AOUT") return ChannelType::ANALOG_OUT;
    if (upper_str == "AUDIO") return ChannelType::AUDIO;
    if (upper_str == "DIGITAL_IN" || upper_str == "DIGIN") return ChannelType::DIGITAL_IN;
    if (upper_str == "SERIAL") return ChannelType::SERIAL;
    if (upper_str == "DIGITAL_OUT" || upper_str == "DIGOUT") return ChannelType::DIGITAL_OUT;

    throw std::runtime_error("Invalid channel type. Valid values: FRONTEND, ANALOG_IN, ANALOG_OUT, AUDIO, DIGITAL_IN, SERIAL, DIGITAL_OUT");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  CereLink Channel Configuration Example\n";
    std::cout << "================================================\n\n";

    // Parse command line arguments
    auto device_type = DeviceType::NSP;
    auto channel_type = ChannelType::ANALOG_IN;
    size_t num_channels = 1;
    uint32_t group_id = 1;
    bool restore = false;

    try {
        if (argc > 1) {
            if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
                printUsage(argv[0]);
                return 0;
            }
            device_type = parseDeviceType(argv[1]);
        }
        if (argc > 2) {
            channel_type = parseChannelType(argv[2]);
        }
        if (argc > 3) {
            int nc = std::atoi(argv[3]);
            if (nc < 0 || nc > cbMAXCHANS) {
                std::cerr << "ERROR: num_channels must be between 0 and " << cbMAXCHANS << "\n";
                return 1;
            }
            num_channels = (nc == 0) ? cbMAXCHANS : nc;
        }
        if (argc > 4) {
            int gid = std::atoi(argv[4]);
            if (gid < 0 || gid > 6) {
                std::cerr << "ERROR: group_id must be between 0 and 6\n";
                return 1;
            }
            group_id = gid;
        }
        if (argc > 5 && (strcmp(argv[5], "--restore") == 0 || strcmp(argv[5], "-r") == 0)) {
            restore = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n\n";
        printUsage(argv[0]);
        return 1;
    }

    // Display configuration
    ConnectionParams config = ConnectionParams::forDevice(device_type);
    config.non_blocking = true;  // Enable non-blocking mode to prevent recv() from hanging
    config.send_buffer_size = 1024 * 1024;

    std::cout << "Configuration:\n";
    std::cout << "  Device Type:     " << deviceTypeToString(device_type) << "\n";
    std::cout << "  Device Address:  " << config.device_address << ":" << config.send_port << "\n";
    std::cout << "  Client Address:  " << config.client_address << ":" << config.recv_port << "\n";
    std::cout << "  Channel Type:    " << channelTypeToString(channel_type) << "\n";
    std::cout << "  Num Channels:    " << num_channels << (num_channels == cbMAXCHANS ? " (all)" : "") << "\n";
    std::cout << "  Group ID:        " << group_id << "\n";
    std::cout << "  Restore State:   " << (restore ? "yes" : "no") << "\n\n";

    //==============================================================================================
    // Step 1: Create device session with automatic protocol detection
    //==============================================================================================

    std::cout << "Step 1: Creating device session...\n";
    auto result = createDeviceSession(config, ProtocolVersion::UNKNOWN);

    if (result.isError()) {
        std::cerr << "  ERROR: Device session creation failed: " << result.error() << "\n\n";
        std::cerr << "Possible causes:\n";
        std::cerr << "  - Device is not responding or is offline\n";
        std::cerr << "  - Incorrect device type or network configuration\n";
        std::cerr << "  - Network connectivity issue\n";
        std::cerr << "  - Port already in use\n";
        return 1;
    }

    auto device = std::move(result.value());
    std::cout << "  Device session created successfully\n";
    std::cout << "  Protocol Version: " << protocolVersionToString(device->getProtocolVersion()) << "\n\n";

    // Start background receive thread (required for sync methods to work)
    std::atomic<bool> running{true};
    std::thread receive_thread([&device, &running]() {
        std::vector<uint8_t> buffer(1024 * 1024);  // 1MB receive buffer
        while (running) {
            device->receivePackets(buffer.data(), buffer.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    //=========================================
    // Step 2: Start thread to receive packets
    //=========================================

    // Helper to stop receive thread on exit
    auto stop_receive_thread = [&]() {
        running = false;
        receive_thread.join();
    };

    //============================================================================
    // Step 3: Get device into running state and request configuration (handshake)
    //============================================================================

    std::cout << "Step 3: Handshaking with device..\n";
    auto req_result = device->performHandshakeSync(std::chrono::milliseconds(2000));
    if (req_result.isError()) {
        std::cerr << "  ERROR: Failed to receive configuration: " << req_result.error() << "\n";
        stop_receive_thread();
        return 1;
    }
    std::cout << "  Configuration received successfully\n\n";

    //==============================================================================================
    // Step 4: Query device configuration
    //==============================================================================================

    std::cout << "Step 4: Querying device configuration...\n";

    const auto sysinfo = device->getSysInfo();
    std::cout << "  System Info:\n";
    std::cout << "    Run Level: " << sysinfo.runlevel << "\n";
    std::cout << "    Run Flags: 0x" << std::hex << sysinfo.runflags << std::dec << "\n";

    // Count channels of the requested type and save original states
    size_t channels_found = 0;
    std::vector<cbPKT_CHANINFO> original_configs;

    for (uint32_t chan = 1; chan <= cbMAXCHANS; ++chan) {
        const cbPKT_CHANINFO* chaninfo = device->getChanInfo(chan);
        if (chaninfo == nullptr) continue;

        // Simple channel type check - we'll rely on the device to filter correctly
        // Just count all existing channels for display purposes
        if (chaninfo->chancaps & cbCHAN_EXISTS) {
            channels_found++;
            if (channels_found <= num_channels) {
                original_configs.push_back(*chaninfo);
            }
        }
    }

    std::cout << "  Total Channels: " << channels_found << "\n";
    std::cout << "  Channels to Configure: " << std::min(num_channels, channels_found) << "\n\n";

    //==============================================================================================
    // Step 5: Set sampling group for first N channels of specified type
    //==============================================================================================

    std::cout << "Step 5: Setting sampling group (waiting for device confirmation)...\n";
    auto set_result = device->setChannelsGroupSync(num_channels, channel_type, group_id,
                                                    std::chrono::milliseconds(3000));
    if (set_result.isError()) {
        std::cerr << "  ERROR: Failed to set channel group: " << set_result.error() << "\n";
        stop_receive_thread();
        return 1;
    }
    std::cout << "  Channel group configuration confirmed by device\n\n";

    //==============================================================================================
    // Step 6: Optionally restore original state
    //==============================================================================================

    if (restore && !original_configs.empty()) {
        std::cout << "Step 6: Restoring original configuration...\n";

        for (const auto& original : original_configs) {
            cbPKT_CHANINFO pkt = original;
            pkt.cbpkt_header.type = cbPKTTYPE_CHANSETSMP;

            auto send_result = device->sendPacket(*reinterpret_cast<cbPKT_GENERIC*>(&pkt));
            if (send_result.isError()) {
                std::cerr << "  WARNING: Failed to restore channel " << original.chan << ": " << send_result.error() << "\n";
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "  Original configuration sent\n\n";
    }

    stop_receive_thread();

    std::cout << "================================================\n";
    std::cout << "  Configuration Complete!\n";
    std::cout << "================================================\n";

    return 0;
}
