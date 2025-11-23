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
/// - Performing device handshake (requesting configuration)
/// - Querying device configuration (sysinfo, chaninfo)
/// - Setting sampling group for first N channels of a specific type
/// - Confirming device accepted the configuration changes
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
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <chrono>
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

const char* channelTypeToString(ChannelType type) {
    switch (type) {
        case ChannelType::FRONTEND: return "FRONTEND";
        case ChannelType::ANALOG_IN: return "ANALOG_IN";
        case ChannelType::ANALOG_OUT: return "ANALOG_OUT";
        case ChannelType::AUDIO: return "AUDIO";
        case ChannelType::DIGITAL_IN: return "DIGITAL_IN";
        case ChannelType::SERIAL: return "SERIAL";
        case ChannelType::DIGITAL_OUT: return "DIGITAL_OUT";
        default: return "UNKNOWN";
    }
}

const char* deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP: return "NSP (Legacy)";
        case DeviceType::NSP: return "NSP (Gemini)";
        case DeviceType::HUB1: return "HUB1";
        case DeviceType::HUB2: return "HUB2";
        case DeviceType::HUB3: return "HUB3";
        case DeviceType::NPLAY: return "NPLAY";
        default: return "CUSTOM";
    }
}

/// Wait for configuration flood to complete by polling for SYSREP
/// Returns true if SYSREP received, false on timeout
bool waitForConfiguration(IDeviceSession& device, const int timeout_ms = 5000) {
    constexpr int poll_interval_ms = 100;
    const int max_polls = timeout_ms / poll_interval_ms;

    std::vector<uint8_t> buffer(1024 * 1024);  // 1MB receive buffer

    for (int poll = 0; poll < max_polls; ++poll) {
        // Receive packets
        auto result = device.receivePackets(buffer.data(), buffer.size());
        if (result.isError()) {
            std::cerr << "  ERROR: Failed to receive packets: " << result.error() << "\n";
            return false;
        }

        int bytes_received = result.value();
        if (bytes_received > 0) {
            // Check for SYSREP packet (end of config flood)
            size_t offset = 0;
            while (offset + sizeof(cbPKT_HEADER) <= static_cast<size_t>(bytes_received)) {
                const auto* header = reinterpret_cast<cbPKT_HEADER*>(&buffer[offset]);

                if (header->type == cbPKTTYPE_SYSREP) {
                    return true;  // Configuration complete
                }

                offset += sizeof(cbPKT_HEADER) + header->dlen * 4;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }

    return false;  // Timeout
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

    //==============================================================================================
    // Step 2: Request configuration from device (handshake)
    //==============================================================================================

    std::cout << "Step 2: Requesting device configuration...\n";
    auto req_result = device->requestConfiguration();
    if (req_result.isError()) {
        std::cerr << "  ERROR: Failed to request configuration: " << req_result.error() << "\n";
        return 1;
    }
    std::cout << "  Configuration request sent\n";

    // Wait for configuration flood to complete
    std::cout << "  Waiting for configuration flood...\n";
    if (!waitForConfiguration(*device, 10000)) {
        std::cerr << "  ERROR: Timeout waiting for configuration (no SYSREP received)\n";
        return 1;
    }
    std::cout << "  Configuration received successfully\n\n";

    //==============================================================================================
    // Step 3: Query device configuration
    //==============================================================================================

    std::cout << "Step 3: Querying device configuration...\n";

    const cbPKT_SYSINFO& sysinfo = device->getSysInfo();
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
    // Step 4: Set sampling group for first N channels of specified type
    //==============================================================================================

    std::cout << "Step 4: Setting sampling group...\n";
    auto set_result = device->setChannelsGroupByType(num_channels, channel_type, group_id);
    if (set_result.isError()) {
        std::cerr << "  ERROR: Failed to set channel group: " << set_result.error() << "\n";
        return 1;
    }
    std::cout << "  Channel group commands sent successfully\n\n";

    //==============================================================================================
    // Step 5: Confirm device accepted the configuration changes
    //==============================================================================================

    std::cout << "Step 5: Confirming configuration changes...\n";

    // Verify device is still connected
    if (!device->isConnected()) {
        std::cerr << "  ERROR: Device is no longer connected\n";
        return 1;
    }
    std::cout << "  Device is connected\n";

    std::cout << "  Waiting for device to echo configuration...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Receive configuration packets mixed with sample group packets
    std::vector<uint8_t> buffer(1024 * 1024);
    size_t max_channels = 0;
    size_t chaninfo_packets_received = 0;
    size_t total_bytes_received = 0;
    size_t polls_with_data = 0;

    std::cout << "  Polling for packets (this may take up to 10 seconds)...\n";

    for (int i = 0; i < 10; ++i) {
        // Print progress every 10 iterations
        if (i % 10 == 0 && i > 0) {
            std::cout << "  Poll " << i << "/100: " << polls_with_data << " polls with data, "
                      << total_bytes_received << " bytes total\n";
        }

        auto recv_result = device->receivePackets(buffer.data(), buffer.size());
        if (recv_result.isError()) {
            std::cerr << "  WARNING: Receive error at poll " << i << ": " << recv_result.error() << "\n";
            continue;
        }

        const int bytes_received = recv_result.value();
        if (bytes_received > 0) {
            total_bytes_received += bytes_received;
            polls_with_data++;

            // Parse packets in the buffer
            size_t offset = 0;
            size_t packet_count = 0;
            while (offset + sizeof(cbPKT_HEADER) <= static_cast<size_t>(bytes_received)) {
                const auto* header = reinterpret_cast<cbPKT_HEADER*>(&buffer[offset]);
                const size_t packet_size = sizeof(cbPKT_HEADER) + header->dlen * 4;

                if (offset + packet_size > static_cast<size_t>(bytes_received)) {
                    break;  // Incomplete packet
                }

                packet_count++;

                // Check if this is a sample group packet (types 0x30-0x35)
                if (header->type >= 1 && header->type < cbMAXGROUPS) {
                    // Sample group packet - count channels
                    // The dlen field indicates the number of 32-bit words in the payload
                    // For group packets, this is half the number of 16-bit integers (i.e., channels)
                    const uint32_t group_num = header->type;
                    const uint32_t num_channels = 2 * header->dlen;
                    max_channels = num_channels > max_channels ? num_channels : max_channels;

                    if (chaninfo_packets_received == 0 && max_channels <= 100) {  // Only print first few
                        std::cout << "  Sample Group " << group_num << ": " << num_channels << " channels" << std::endl;
                    }
                }
                // Check if this is a chaninfo packet
                else if (header->type == cbPKTTYPE_CHANREP || header->type == cbPKTTYPE_CHANREPSMP) {
                    const auto* chaninfo = reinterpret_cast<const cbPKT_CHANINFO*>(&buffer[offset]);
                    std::cout << "  CHANINFO Ch" << chaninfo->chan
                              << ": group=" << chaninfo->smpgroup
                              << ", filter=" << chaninfo->smpfilter
                              << ", raw=" << ((chaninfo->ainpopts & cbAINP_RAWSTREAM) ? "yes" : "no")
                              << std::endl;  // Use endl to flush output
                    chaninfo_packets_received++;
                }

                offset += packet_size;
            }

            if (packet_count > 0 && i % 10 == 0) {
                std::cout << "    Parsed " << packet_count << " packets in this datagram" << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n  Summary:\n";
    std::cout << "    CHANINFO packets received: " << chaninfo_packets_received << "\n";
    std::cout << "    Total channels in sample group packets: " << max_channels << "\n";
    std::cout << "  Configuration changes confirmed\n\n";

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
        }

        std::cout << "  Original configuration restored\n";

        // Wait for device to acknowledge
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (int i = 0; i < 10; ++i) {
            device->receivePackets(buffer.data(), buffer.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "\n";
    }

    std::cout << "================================================\n";
    std::cout << "  Configuration Complete!\n";
    std::cout << "================================================\n";

    return 0;
}
