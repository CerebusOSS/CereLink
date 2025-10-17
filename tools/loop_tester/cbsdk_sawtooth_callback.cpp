/**
 * cbsdk Sawtooth Validator - Callback Version
 *
 * Connects to nPlayServer and validates timestamps using callbacks
 * (bypassing the trial buffer system).
 *
 * This tests whether duplicate timestamps appear in the callback path
 * or only in the trial buffer path.
 */

#include <cerelink/cbsdk.h>
#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <inttypes.h>

// Configuration constants
constexpr uint32_t INST = 0;
constexpr int NUM_CHANNELS = 128;
constexpr uint32_t SAMPLE_GROUP = 6;  // Sample group to monitor
constexpr int TEST_DURATION_SEC = 20;

// Global state for callback
std::atomic<bool> keep_running{true};
std::atomic<uint64_t> packet_count{0};
std::atomic<uint64_t> duplicate_count{0};
std::atomic<uint64_t> jump_count{0};
std::atomic<PROCTIME> last_timestamp{0};
std::atomic<int16_t> last_data{0};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
}

void handleResult(cbSdkResult res, const char* operation) {
    if (res != CBSDKRESULT_SUCCESS) {
        std::cerr << operation << " failed with code " << res << std::endl;
        exit(1);
    }
}

// Callback function for continuous packets
void groupCallback(uint32_t nInstance, const cbSdkPktType type, const void* pEventData, void* pCallbackData)
{
    // Cast event data to group packet
    const cbPKT_GROUP* pPkt = static_cast<const cbPKT_GROUP*>(pEventData);

    packet_count++;

    PROCTIME current_ts = pPkt->cbpkt_header.time;
    PROCTIME prev_ts = last_timestamp.load();

    // Only check after first packet
    if (prev_ts != 0) {
        int64_t delta = static_cast<int64_t>(current_ts) - static_cast<int64_t>(prev_ts);

        // Check for duplicates (first 50000 packets to avoid spam)
        if (packet_count <= 50000) {
            if (delta == 0) {
                duplicate_count++;
                fprintf(stderr, "[CALLBACK] DUPLICATE #%llu! ts=%llu, data[0]=%d (prev_ts=%llu, prev_data=%d)\n",
                        (unsigned long long)duplicate_count.load(),
                        (unsigned long long)current_ts,
                        pPkt->data[0],
                        (unsigned long long)prev_ts,
                        last_data.load());
            } else if (delta != 1) {
                jump_count++;
                fprintf(stderr, "[CALLBACK] JUMP #%llu! ts=%llu, delta=%lld, data[0]=%d\n",
                        (unsigned long long)jump_count.load(),
                        (unsigned long long)current_ts,
                        delta,
                        pPkt->data[0]);
            }
        }
    }

    last_timestamp.store(current_ts);
    last_data.store(pPkt->data[0]);
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments
    std::string inst_ip = "";
    int inst_port = cbNET_UDP_PORT_CNT;

    if (argc > 1) inst_ip = argv[1];
    if (argc > 2) inst_port = std::stoi(argv[2]);

    std::cout << "cbsdk Sawtooth Validator (Callback Version)" << std::endl;
    std::cout << "Connecting to: " << (inst_ip.empty() ? "default" : inst_ip) << ":" << inst_port << std::endl;
    std::cout << std::endl;

    // Open connection
    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    cbSdkConnection con{};
    con.szOutIP = inst_ip.empty() ? nullptr : inst_ip.c_str();
    con.nOutPort = inst_port;
    con.szInIP = "";
    con.nInPort = inst_port;

    cbSdkResult res = cbSdkOpen(INST, conType, con);
    handleResult(res, "cbSdkOpen");

    std::cout << "Connection established!" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // // Configure channels: enable continuous sampling on SAMPLE_GROUP
    // std::cout << "Configuring channels for continuous sampling on group " << SAMPLE_GROUP << "..." << std::endl;
    // for (int chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; ++chan_ix) {
    //     cbPKT_CHANINFO chanInfo;
    //
    //     // Get current channel configuration
    //     res = cbSdkGetChannelConfig(INST, chan_ix + 1, &chanInfo);
    //     if (res != CBSDKRESULT_SUCCESS) {
    //         std::cerr << "Warning: cbSdkGetChannelConfig failed for channel " << (chan_ix + 1) << std::endl;
    //         continue;
    //     }
    //
    //     // Disable DC offset correction
    //     chanInfo.ainpopts &= ~cbAINP_OFFSET_CORRECT;
    //
    //     // Enable continuous sampling on SAMPLE_GROUP for first NUM_CHANNELS channels
    //     chanInfo.smpgroup = (chan_ix < NUM_CHANNELS) ? SAMPLE_GROUP : 0;
    //
    //     // Disable spike processing
    //     chanInfo.spkopts = cbAINPSPK_NOSORT;
    //
    //     // Apply the modified configuration
    //     res = cbSdkSetChannelConfig(INST, chan_ix + 1, &chanInfo);
    //     if (res != CBSDKRESULT_SUCCESS) {
    //         std::cerr << "Warning: cbSdkSetChannelConfig failed for channel " << (chan_ix + 1) << std::endl;
    //     }
    // }
    // std::cout << "Channel configuration complete." << std::endl;
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    // Register callback for continuous packets
    std::cout << "Registering callback for continuous packets..." << std::endl;
    res = cbSdkRegisterCallback(INST, CBSDKCALLBACK_CONTINUOUS, groupCallback, nullptr);
    handleResult(res, "cbSdkRegisterCallback");

    std::cout << "\nCallback registered. Monitoring for " << TEST_DURATION_SEC << " seconds..." << std::endl;
    std::cout << "Press Ctrl+C to stop early." << std::endl;
    std::cout << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;
    constexpr auto max_runtime = std::chrono::seconds(TEST_DURATION_SEC);

    while (keep_running) {
        // Check if we've exceeded the runtime limit
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= max_runtime) {
            std::cout << "\nReached " << TEST_DURATION_SEC << " second timeout, exiting..." << std::endl;
            break;
        }

        // Print statistics every 5 seconds
        auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_stats_time
        ).count();

        if (stats_elapsed >= 5) {
            auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();

            const uint64_t pkts = packet_count.load();
            const double avg_rate = pkts / (total_elapsed + 1);

            std::cout << "Running for " << total_elapsed << "s"
                      << " | Packets: " << pkts
                      << " | Rate: " << static_cast<int>(avg_rate) << " Hz"
                      << std::endl;

            std::cout << "  Duplicates: " << duplicate_count.load()
                      << ", Jumps: " << jump_count.load()
                      << std::endl;

            last_stats_time = std::chrono::steady_clock::now();
        }

        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Unregister callback
    cbSdkUnRegisterCallback(INST, CBSDKCALLBACK_CONTINUOUS);

    // Final statistics
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time
    ).count();

    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total runtime: " << total_elapsed << " seconds" << std::endl;
    std::cout << "Total packets received: " << packet_count.load() << std::endl;
    std::cout << "Average packet rate: " << (packet_count.load() / (total_elapsed + 1)) << " Hz" << std::endl;
    std::cout << "Duplicate timestamps: " << duplicate_count.load() << std::endl;
    std::cout << "Timestamp jumps: " << jump_count.load() << std::endl;

    // Close connection
    cbSdkClose(INST);
    std::cout << "\nConnection closed." << std::endl;

    return (duplicate_count.load() > 0 || jump_count.load() > 0) ? 1 : 0;
}
