///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   gemini_example.cpp
/// @author CereLink Development Team
/// @date   2025-11-12
///
/// @brief  Example demonstrating cbsdk_v2 with Gemini devices
///
/// This example shows:
/// - Auto-discovery of all connected Gemini devices (NSP, Hub1, Hub2, Hub3)
/// - Simplified device configuration using DeviceType enum
/// - Automatic client address detection
/// - Setting up packet callbacks with timestamp analysis
/// - Monitoring statistics for all connected devices
/// - Proper shared memory cleanup
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cbsdk_v2/sdk_session.h"
#include <iostream>
#include <iomanip>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <cmath>
#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include <unistd.h>  // for getpid()
#include <condition_variable>
#include <cerrno>

// POSIX shared memory cleanup (macOS/Linux)
#ifndef _WIN32
    #include <sys/mman.h>
#endif

// Need full protocol definitions for device verification
extern "C" {
    #include <cbproto/cbproto.h>  // Upstream version with full packet types
}

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running.store(false);
}

// Timestamp analysis structure
struct TimestampStats {
    std::mutex mutex;
    uint64_t last_timestamp = 0;
    uint64_t interval_sum = 0;
    uint64_t interval_sum_sq = 0;
    uint64_t interval_count = 0;

    void addTimestamp(uint64_t ts) {
        std::lock_guard<std::mutex> lock(mutex);
        if (last_timestamp > 0 && ts > last_timestamp) {
            uint64_t interval = ts - last_timestamp;
            interval_sum += interval;
            interval_sum_sq += interval * interval;
            interval_count++;
        }
        last_timestamp = ts;
    }

    std::pair<double, double> getMeanAndStdDev() {
        std::lock_guard<std::mutex> lock(mutex);
        if (interval_count == 0) {
            return {0.0, 0.0};
        }
        double mean = static_cast<double>(interval_sum) / interval_count;
        double variance = (static_cast<double>(interval_sum_sq) / interval_count) - (mean * mean);
        double stddev = std::sqrt(variance);
        return {mean, stddev};
    }
};

void printStats(const cbsdk::SdkSession& session, const std::string& name, TimestampStats* ts_stats = nullptr) {
    auto stats = session.getStats();

    std::cout << "\n=== " << name << " Statistics ===\n";
    std::cout << "Packets received:  " << stats.packets_received_from_device << "\n";
    std::cout << "Packets in shmem:  " << stats.packets_stored_to_shmem << "\n";
    std::cout << "Packets queued:    " << stats.packets_queued_for_callback << "\n";
    std::cout << "Packets delivered: " << stats.packets_delivered_to_callback << "\n";
    std::cout << "Packets dropped:   " << stats.packets_dropped << "\n";
    std::cout << "Queue depth:       " << stats.queue_current_depth
              << " / " << stats.queue_max_depth << " (current/peak)\n";
    std::cout << "Errors (shmem):    " << stats.shmem_store_errors << "\n";
    std::cout << "Errors (receive):  " << stats.receive_errors << "\n";

    // Print timestamp interval statistics if available
    if (ts_stats != nullptr) {
        auto [mean, stddev] = ts_stats->getMeanAndStdDev();
        if (mean > 0) {
            // Expected interval for 30k pkt/s: 1e9/30000 ≈ 33,333 ns
            double expected_interval = 1e9 / 30000.0;
            std::cout << "\nSample group (0x0006) timestamp intervals:\n";
            std::cout << "  Mean:     " << std::fixed << std::setprecision(1) << mean << " ns\n";
            std::cout << "  Std Dev:  " << std::fixed << std::setprecision(1) << stddev << " ns\n";
            std::cout << "  Expected: " << std::fixed << std::setprecision(1) << expected_interval << " ns (for 30k pkt/s)\n";
            std::cout << "  Rate from timestamps: " << std::fixed << std::setprecision(1) << (1e9 / mean) << " pkt/s\n";
        }
    }
}

// Device information structure
struct DeviceInfo {
    std::string name;
    cbsdk::DeviceType type;
    std::unique_ptr<cbsdk::SdkSession> session;
    std::atomic<uint64_t> packet_count{0};
    TimestampStats timestamps;
    uint64_t last_count = 0;  // For rate calculation
};

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "  Gemini Device Monitor (cbsdk_v2)\n";
    std::cout << "===========================================\n\n";

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Define all possible Gemini devices to try
    std::vector<std::unique_ptr<DeviceInfo>> devices;

    auto nsp = std::make_unique<DeviceInfo>();
    nsp->name = "Gemini NSP";
    nsp->type = cbsdk::DeviceType::GEMINI_NSP;
    devices.push_back(std::move(nsp));

    auto hub1 = std::make_unique<DeviceInfo>();
    hub1->name = "Gemini Hub1";
    hub1->type = cbsdk::DeviceType::GEMINI_HUB1;
    devices.push_back(std::move(hub1));

    auto hub2 = std::make_unique<DeviceInfo>();
    hub2->name = "Gemini Hub2";
    hub2->type = cbsdk::DeviceType::GEMINI_HUB2;
    devices.push_back(std::move(hub2));

    auto hub3 = std::make_unique<DeviceInfo>();
    hub3->name = "Gemini Hub3";
    hub3->type = cbsdk::DeviceType::GEMINI_HUB3;
    devices.push_back(std::move(hub3));

    try {
        // === Clean up any stale shared memory segments ===
#ifndef _WIN32
        // Forcefully unlink all possible shared memory segments to ensure STANDALONE mode
        // POSIX requires shared memory names to start with "/"
        // Use Central-compatible names: "cbCFGbuffer", "XmtGlobal", etc.
        for (const auto& device : devices) {
            std::string cfg_name;
            std::string xmt_name;

            // Map device type to Central-compatible shared memory names
            switch (device->type) {
                case cbsdk::DeviceType::LEGACY_NSP:
                case cbsdk::DeviceType::GEMINI_NSP:
                case cbsdk::DeviceType::NPLAY:
                    // Instance 0 uses base names without suffix
                    cfg_name = "cbCFGbuffer";
                    xmt_name = "XmtGlobal";
                    break;
                case cbsdk::DeviceType::GEMINI_HUB1:
                    cfg_name = "cbCFGbuffer1";
                    xmt_name = "XmtGlobal1";
                    break;
                case cbsdk::DeviceType::GEMINI_HUB2:
                    cfg_name = "cbCFGbuffer2";
                    xmt_name = "XmtGlobal2";
                    break;
                case cbsdk::DeviceType::GEMINI_HUB3:
                    cfg_name = "cbCFGbuffer3";
                    xmt_name = "XmtGlobal3";
                    break;
            }

            std::string posix_cfg_name = "/" + cfg_name;
            std::string posix_xmt_name = "/" + xmt_name;
            shm_unlink(posix_cfg_name.c_str());  // Ignore errors
            shm_unlink(posix_xmt_name.c_str());  // Ignore errors
        }
#endif

        // === Try to connect to all Gemini devices ===
        std::cout << "\nScanning for Gemini devices...\n\n";

        std::vector<DeviceInfo*> connected_devices;

        for (auto& device : devices) {
            std::cout << "Trying " << device->name << "...\n";

            cbsdk::SdkConfig config;
            config.device_type = device->type;

            auto result = cbsdk::SdkSession::create(config);
            if (result.isError()) {
                std::cout << "  Failed to create session: " << result.error() << "\n";
                continue;
            }

            std::cout << "  Session created successfully\n";
            device->session = std::make_unique<cbsdk::SdkSession>(std::move(result.value()));

            // Set up packet callback
            device->session->setPacketCallback([dev = device.get()](const cbPKT_GENERIC* pkts, size_t count) {
                dev->packet_count.fetch_add(count);

                // Track timestamps for interval analysis (only sample group packets - type 0x0006)
                for (size_t i = 0; i < count; ++i) {
                    if (pkts[i].cbpkt_header.type == 0x0006) {
                        dev->timestamps.addTimestamp(pkts[i].cbpkt_header.time);
                    }

                    // Detect SYSREP packets (type 0x10-0x1F)
                    if ((pkts[i].cbpkt_header.type & 0xF0) == 0x10) {
                        // Cast to cbPKT_SYSINFO to read runlevel
                        const cbPKT_SYSINFO* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkts[i]);
                        std::cout << "[" << dev->name << "] SYSREP packet received - runlevel: "
                                  << sysinfo->runlevel << "\n";
                    }
                }

                // Print first few packets for demonstration (including instrument ID)
                static std::map<std::string, uint64_t> printed_counts;
                if (printed_counts[dev->name] < 5) {
                    for (size_t i = 0; i < count && printed_counts[dev->name] < 5; ++i) {
                        std::cout << "[" << dev->name << "] Packet type: 0x"
                                  << std::hex << std::setw(4) << std::setfill('0')
                                  << pkts[i].cbpkt_header.type << std::dec
                                  << ", dlen: " << pkts[i].cbpkt_header.dlen
                                  << ", instrument: " << static_cast<int>(pkts[i].cbpkt_header.instrument) << "\n";
                        printed_counts[dev->name]++;
                    }
                }
            });

            // Set up error callback
            device->session->setErrorCallback([name = device->name](const std::string& error) {
                std::cerr << "[" << name << " ERROR] " << error << "\n";
            });

            // Start the session
            auto start_result = device->session->start();
            if (start_result.isError()) {
                std::cout << "  Failed to start: " << start_result.error() << "\n";
                device->session.reset();
                continue;
            }

            std::cout << "  Session started successfully\n";
            connected_devices.push_back(device.get());
        }

        if (connected_devices.empty()) {
            std::cerr << "\nNo sessions could be created!\n";
            return 1;
        }

        // === Verify which devices are actually receiving data ===
        std::cout << "\nVerifying devices (waiting 1 second for packets)...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::vector<DeviceInfo*> active_devices;
        for (auto* dev : connected_devices) {
            if (dev->packet_count.load() > 0) {
                std::cout << "  ✓ " << dev->name << " - receiving data\n";
                active_devices.push_back(dev);
            } else {
                std::cout << "  ✗ " << dev->name << " - no data (device not present)\n";
                // Stop session for inactive device
                dev->session->stop();
                dev->session.reset();
            }
        }

        if (active_devices.empty()) {
            std::cerr << "\nNo active Gemini devices detected!\n";
            return 1;
        }

        std::cout << "\n=== Found " << active_devices.size() << " active device(s) ===\n";
        for (const auto* dev : active_devices) {
            std::cout << "  - " << dev->name << "\n";
        }

        // Update connected_devices to only include active ones
        connected_devices = active_devices;

        // === Send runlevel command to transition devices to RUNNING state ===
        std::cout << "\nSending cbRUNLEVEL_RUNNING command to active devices...\n";
        for (auto* dev : connected_devices) {
            auto result = dev->session->setSystemRunLevel(cbRUNLEVEL_RUNNING);
            if (result.isOk()) {
                std::cout << "  ✓ " << dev->name << " - runlevel command sent\n";
            } else {
                std::cout << "  ✗ " << dev->name << " - failed: " << result.error() << "\n";
            }
        }
        std::cout << "\nPress Ctrl+C to stop...\n\n";

        // === Main Loop - Print Statistics Every 5 Seconds ===
        auto last_print = std::chrono::steady_clock::now();

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print);

            if (elapsed.count() >= 5000) {
                double elapsed_sec = elapsed.count() / 1000.0;

                // Print packet counts and rates
                std::cout << "\n=== Packet Counts ===\n";
                for (auto* dev : connected_devices) {
                    uint64_t current_count = dev->packet_count.load();
                    double rate = (current_count - dev->last_count) / elapsed_sec;
                    std::cout << dev->name << ": " << current_count << " packets ("
                              << std::fixed << std::setprecision(1) << rate << " pkt/s)\n";
                }

                // Print detailed statistics for each device
                for (auto* dev : connected_devices) {
                    printStats(*dev->session, dev->name, &dev->timestamps);
                }

                std::cout << "\nPress Ctrl+C to stop...\n";

                // Update for next iteration
                last_print = now;
                for (auto* dev : connected_devices) {
                    dev->last_count = dev->packet_count.load();
                }
            }
        }

        // === Clean Shutdown ===
        std::cout << "\nStopping sessions...\n";
        for (auto* dev : connected_devices) {
            dev->session->stop();
        }

        // Final statistics
        std::cout << "\n=== Final Statistics ===\n";
        std::cout << "Total packets received:\n";
        for (auto* dev : connected_devices) {
            std::cout << "  " << dev->name << ": " << dev->packet_count.load() << "\n";
        }

        for (auto* dev : connected_devices) {
            printStats(*dev->session, dev->name + " (Final)", &dev->timestamps);
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nShutdown complete.\n";
    return 0;
}
