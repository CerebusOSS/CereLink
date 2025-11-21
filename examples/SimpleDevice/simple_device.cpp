///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   simple_device.cpp
/// @author CereLink Development Team
/// @date   2025-11-14
///
/// @brief  Simple example demonstrating cbsdk_v2 SDK usage
///
/// This example shows how to use the CereLink SDK. It demonstrates:
/// - Creating an SDK session
/// - Configuring device address and port for different device types
/// - Setting up a packet receive callback
/// - Automatic connection and handshaking
/// - Monitoring statistics
/// - Clean shutdown
///
/// Usage:
///   simple_device [device_type]
///
/// Device types:
///   NSP        - Neural Signal Processor (legacy)
///   GEMINI_NSP - Gemini NSP
///   HUB1       - Hub 1 (legacy addressing)
///   HUB2       - Hub 2 (legacy addressing)
///   HUB3       - Hub 3 (legacy addressing)
///   NPLAY      - nPlayServer
///
/// Examples:
///   simple_device              # Default: NSP
///   simple_device GEMINI_NSP   # Gemini NSP
///   simple_device NPLAY        # nPlay loopback
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk_v2/sdk_session.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

using namespace cbsdk;

// Global flag for clean shutdown on Ctrl+C
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    g_running = false;
}

/// Print packet type name for common types
const char* getPacketTypeName(uint8_t type) {
    switch (type) {
        case cbPKTTYPE_SYSREP:        return "SYSREP";
        case cbPKTTYPE_PROCREP:       return "PROCREP";
        case cbPKTTYPE_BANKREP:       return "BANKREP";
        case cbPKTTYPE_GROUPREP:      return "GROUPREP";
        case cbPKTTYPE_FILTREP:       return "FILTREP";
        case cbPKTTYPE_CHANREP:       return "CHANREP";
        case cbPKTTYPE_CHANREPSPK:    return "CHANREPSPK";
        default:
            // Handle group packets (0x30-0x35)
            if (type >= cbPKTTYPE_GROUPREP && type <= cbPKTTYPE_GROUPREP + 5) {
                return "GROUP[0-5]";
            }
            return "UNKNOWN";
    }
}

/// Print packet information
void printPacket(const cbPKT_GENERIC& pkt) {
    std::cout << "  Type: 0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)pkt.cbpkt_header.type << std::dec << " (" << getPacketTypeName(pkt.cbpkt_header.type) << ")"
              << ", Inst: " << (int)pkt.cbpkt_header.instrument
              << ", Chid: " << pkt.cbpkt_header.chid
              << ", DLen: " << pkt.cbpkt_header.dlen << "\n";
}

/// Map device type string to DeviceType enum
DeviceType parseDeviceType(const std::string& type_str) {
    if (type_str == "NSP")        return DeviceType::LEGACY_NSP;
    if (type_str == "GEMINI_NSP") return DeviceType::GEMINI_NSP;
    if (type_str == "HUB1")       return DeviceType::GEMINI_HUB1;
    if (type_str == "HUB2")       return DeviceType::GEMINI_HUB2;
    if (type_str == "HUB3")       return DeviceType::GEMINI_HUB3;
    if (type_str == "NPLAY")      return DeviceType::NPLAY;

    std::cerr << "ERROR: Unknown device type '" << type_str << "'\n";
    std::cerr << "Valid types: NSP, GEMINI_NSP, HUB1, HUB2, HUB3, NPLAY\n";
    std::exit(1);
}

/// Get device type name string
const char* getDeviceTypeName(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP:   return "LEGACY_NSP";
        case DeviceType::GEMINI_NSP:   return "GEMINI_NSP";
        case DeviceType::GEMINI_HUB1:  return "GEMINI_HUB1";
        case DeviceType::GEMINI_HUB2:  return "GEMINI_HUB2";
        case DeviceType::GEMINI_HUB3:  return "GEMINI_HUB3";
        case DeviceType::NPLAY:        return "NPLAY";
        default:                       return "UNKNOWN";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  CereLink Simple Device Example (cbdev only)\n";
    std::cout << "================================================\n\n";

    // Parse command line arguments
    DeviceType device_type = DeviceType::LEGACY_NSP;  // Default to NSP

    if (argc >= 2) {
        device_type = parseDeviceType(argv[1]);
    }

    std::cout << "Configuration:\n";
    std::cout << "  Device Type: " << getDeviceTypeName(device_type) << "\n";

    // Register signal handler for clean shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Step 1: Create SDK configuration from device type
    SdkConfig config;
    config.device_type = device_type;
    config.autorun = true;  // Automatically perform startup handshake

    std::cout << "  Autorun:        " << (config.autorun ? "enabled" : "disabled") << "\n\n";

    // Step 2: Create SDK session (automatically starts and performs handshake if autorun=true)
    std::cout << "Creating SDK session...\n";
    auto result = SdkSession::create(config);
    if (result.isError()) {
        std::cerr << "ERROR: Failed to create SDK session: " << result.error() << "\n";
        return 1;
    }
    auto session = std::move(result.value());
    std::cout << "SDK session created successfully!\n\n";

    // Step 3: Set up packet callback
    std::atomic<uint64_t> packet_count{0};
    std::atomic<uint64_t> spike_count{0};
    std::atomic<uint64_t> config_count{0};

    session.setPacketCallback([&](const cbPKT_GENERIC* pkts, size_t count) {
        packet_count += count;

        // Count packet types
        for (size_t i = 0; i < count; ++i) {
            const auto& pkt = pkts[i];

            // Count spikes
            if (pkt.cbpkt_header.type == cbPKTTYPE_CHANREPSPK) {
                spike_count++;
            }

            // Count config packets
            if (pkt.cbpkt_header.chid == cbPKTCHAN_CONFIGURATION) {
                config_count++;

                // Print first few config packets
                if (config_count <= 10) {
                    printPacket(pkt);
                }
            }
        }
    });

    std::cout << "Packet callback registered.\n\n";

    // Step 4: Session is already running (auto-started by SDK)
    std::cout << "SDK session is running and receiving packets...\n\n";

    // Step 5: Run for specified duration, showing statistics
    std::cout << "Receiving packets... (Press Ctrl+C to stop)\n\n";
    std::cout << "Statistics (updated every second):\n";
    std::cout << "-----------------------------------\n";

    auto start_time = std::chrono::steady_clock::now();
    int seconds_elapsed = 0;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get statistics
        auto stats = session.getStats();
        auto now = std::chrono::steady_clock::now();
        seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        // Print statistics
        std::cout << "\r[" << std::setw(3) << seconds_elapsed << "s] "
                  << "Total: " << std::setw(8) << packet_count.load()
                  << " | Config: " << std::setw(6) << config_count.load()
                  << " | Spikes: " << std::setw(8) << spike_count.load()
                  << " | RX: " << std::setw(10) << stats.packets_received_from_device
                  << " pkts"
                  << " | Delivered: " << std::setw(10) << stats.packets_delivered_to_callback
                  << std::flush;
    }

    std::cout << "\n\n";

    // Step 7: Stop SDK session
    std::cout << "Stopping SDK session...\n";
    session.stop();
    std::cout << "SDK session stopped.\n\n";

    // Step 8: Print final statistics
    auto final_stats = session.getStats();
    std::cout << "Final Statistics:\n";
    std::cout << "=================\n";
    std::cout << "  Runtime:                  " << seconds_elapsed << " seconds\n";
    std::cout << "  Packets from Device:      " << final_stats.packets_received_from_device << "\n";
    std::cout << "  Packets to Shmem:         " << final_stats.packets_stored_to_shmem << "\n";
    std::cout << "  Packets Queued:           " << final_stats.packets_queued_for_callback << "\n";
    std::cout << "  Packets Delivered:        " << final_stats.packets_delivered_to_callback << "\n";
    std::cout << "  Packets Dropped:          " << final_stats.packets_dropped << "\n";
    std::cout << "  Config Packets:           " << config_count.load() << "\n";
    std::cout << "  Spike Packets:            " << spike_count.load() << "\n";
    std::cout << "  Total Processed:          " << packet_count.load() << "\n";
    std::cout << "  Packets Sent to Device:   " << final_stats.packets_sent_to_device << "\n";

    if (final_stats.packets_received_from_device > 0 && seconds_elapsed > 0) {
        double packets_per_sec = static_cast<double>(final_stats.packets_received_from_device) / seconds_elapsed;
        std::cout << "  Average Rate:             " << std::fixed << std::setprecision(1)
                  << packets_per_sec << " pkts/sec\n";
    }

    std::cout << "\nShutdown complete!\n";
    return 0;
}
