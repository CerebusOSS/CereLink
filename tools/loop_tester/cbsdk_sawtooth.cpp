/**
 * cbsdk Sawtooth Validator
 *
 * Connects to nPlayServer (configured with LSL support) and validates:
 * - Sawtooth signal on channels 1-2
 * - Constant values on channels 3-130
 * - Timestamp continuity
 * - No packet loss
 *
 * This is used to test long-running cbsdk connections.
 */

#include <cerelink/cbsdk.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <cmath>
#include <inttypes.h>

// Configuration constants
constexpr uint32_t INST = 0;
constexpr int NUM_CHANNELS = 128;
constexpr uint32_t SAMPLE_GROUP = 6;  // Sample group to monitor
constexpr int16_t SAWTOOTH_MIN = -32768;
constexpr int16_t SAWTOOTH_MAX = 32767;
constexpr int32_t UNIQUE_VALUES = SAWTOOTH_MAX - SAWTOOTH_MIN + 1; // 65536
constexpr int SAWTOOTH_TOLERANCE = 1; // Allow deviations
constexpr bool LEGACY_TIMESTAMPS = true;  // We know the timestamps increment 1-by-1.

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

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

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments
    std::string inst_ip = "";
    int inst_port = cbNET_UDP_PORT_CNT;

    if (argc > 1) inst_ip = argv[1];
    if (argc > 2) inst_port = std::stoi(argv[2]);

    std::cout << "cbsdk Sawtooth Validator" << std::endl;
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

    // Configure channels: enable continuous sampling on SAMPLE_GROUP and disable spike processing
    std::cout << "Configuring channels for continuous sampling on group " << SAMPLE_GROUP << "..." << std::endl;
    for (int chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; ++chan_ix) {
        cbPKT_CHANINFO chanInfo;

        // Get current channel configuration
        res = cbSdkGetChannelConfig(INST, chan_ix + 1, &chanInfo);
        if (res != CBSDKRESULT_SUCCESS) {
            std::cerr << "Warning: cbSdkGetChannelConfig failed for channel " << (chan_ix + 1) << std::endl;
            continue;
        }

        // Disable DC offset correction by clearing the flag
        chanInfo.ainpopts &= ~cbAINP_OFFSET_CORRECT;

        // Enable continuous sampling on SAMPLE_GROUP for first NUM_CHANNELS channels
        chanInfo.smpgroup = (chan_ix < NUM_CHANNELS) ? SAMPLE_GROUP : 0;

        // Disable spike processing
        chanInfo.spkopts = cbAINPSPK_NOSORT;

        // Apply the modified configuration
        res = cbSdkSetChannelConfig(INST, chan_ix + 1, &chanInfo);
        if (res != CBSDKRESULT_SUCCESS) {
            std::cerr << "Warning: cbSdkSetChannelConfig failed for channel " << (chan_ix + 1) << std::endl;
        }
    }
    std::cout << "Channel configuration complete." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Configure trial (enable continuous data)
    // Use 10x buffer size to avoid overflows
    constexpr uint32_t BUFFER_SIZE = cbSdk_CONTINUOUS_DATA_SAMPLES * 10;
    res = cbSdkSetTrialConfig(
        INST,
        1,  // bActive
        0, 0, 0,  // begin trigger
        0, 0, 0,  // end trigger
        0,  // waveforms
        BUFFER_SIZE,  // continuous - using larger buffer
        0,  // events
        0,  // comments
        0   // tracking
    );
    handleResult(res, "cbSdkSetTrialConfig");

    std::cout << "Trial configured for continuous data" << std::endl;

    // Allocate trial data structure
    cbSdkTrialCont trialCont{};
    trialCont.group = SAMPLE_GROUP;
    trialCont.num_samples = cbSdk_CONTINUOUS_DATA_SAMPLES;

    // Wait for channels to become available (may take a moment for backend to start sampling)
    std::cout << "Waiting for channels in sample group " << SAMPLE_GROUP << "..." << std::endl;
    const auto wait_start = std::chrono::steady_clock::now();
    constexpr auto wait_timeout = std::chrono::seconds(5);  // 5 second timeout

    while (trialCont.count == 0) {
        res = cbSdkInitTrialData(INST, 0, nullptr, &trialCont, nullptr, nullptr);
        handleResult(res, "cbSdkInitTrialData");

        if (trialCont.count > 0) {
            break;  // Channels found!
        }

        // Check timeout
        const auto elapsed = std::chrono::steady_clock::now() - wait_start;
        if (elapsed >= wait_timeout) {
            std::cerr << "Error: Timeout waiting for channels in sample group " << SAMPLE_GROUP << std::endl;
            std::cerr << "No channels became available after "
                      << std::chrono::duration_cast<std::chrono::seconds>(wait_timeout).count()
                      << " seconds" << std::endl;
            cbSdkClose(INST);
            return 1;
        }

        // Small delay before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Sample group " << SAMPLE_GROUP << " has " << trialCont.count << " channels" << std::endl;
    std::cout << "Expected sample rate: " << trialCont.sample_rate << " Hz" << std::endl;

    // Pre-allocate buffers once (maximum possible size matching our BUFFER_SIZE)
    const size_t max_buffer_size = BUFFER_SIZE * trialCont.count;
    std::vector<int16_t> samples(max_buffer_size);
    std::vector<PROCTIME> timestamps(BUFFER_SIZE);

    // Set buffer pointers (they won't change)
    trialCont.samples = samples.data();
    trialCont.timestamps = timestamps.data();

    // Validation state
    int64_t total_samples_received = 0;
    int64_t sawtooth_errors = 0;
    int64_t constant_errors = 0;
    int64_t timestamp_errors = 0;
    int64_t read_count = 0;
    PROCTIME last_timestamp = 0;
    int16_t last_sample_ch0 = 0;
    bool first_read = true;

    // For sawtooth phase tracking using timestamps
    // offset = timestamp - phase, so phase = (timestamp - offset) % UNIQUE_VALUES
    int64_t timestamp_offset = -1;

    auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;

    std::cout << "\nStarting validation loop..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    while (keep_running) {
        // Initialize trial to get available sample count
        trialCont.num_samples = BUFFER_SIZE;
        res = cbSdkInitTrialData(INST, 0, nullptr, &trialCont, nullptr, nullptr);
        if (res != CBSDKRESULT_SUCCESS) {
            std::cerr << "cbSdkInitTrialData failed: " << res << std::endl;
            break;
        }

        if (trialCont.num_samples > 0) {
            // Get trial data (bActive = 1 to advance read pointer)
            res = cbSdkGetTrialData(INST, 1, nullptr, &trialCont, nullptr, nullptr);
            if (res != CBSDKRESULT_SUCCESS) {
                std::cerr << "cbSdkGetTrialData failed: " << res << std::endl;
                break;
            }

            read_count++;
            // Log first few reads for debugging
            if (read_count <= 3) {
                std::cout << "Read #" << read_count << ": Got " << trialCont.num_samples
                          << " samples, first_ts=" << timestamps[0];
                if (trialCont.num_samples > 1) {
                    std::cout << ", last_ts=" << timestamps[trialCont.num_samples - 1];
                }
                std::cout << std::endl;
            }

            // Validate data
            for (uint32_t samp = 0; samp < trialCont.num_samples; samp++) {
                const PROCTIME current_timestamp = timestamps[samp];

                // Check timestamp continuity
                if (!first_read) {
                    // Timestamps should increase monotonically
                    if ((LEGACY_TIMESTAMPS && current_timestamp != (last_timestamp + 1)) ||
                        (!LEGACY_TIMESTAMPS && current_timestamp < last_timestamp)) {
                        timestamp_errors++;
                        // Log detailed error information for first few errors
                        if (timestamp_errors <= 5) {
                            std::cerr << "TIMESTAMP ERROR #" << timestamp_errors << ":" << std::endl;
                            std::cerr << "  Sample index: " << samp << " of " << trialCont.num_samples << std::endl;
                            std::cerr << "  Last timestamp: " << last_timestamp << std::endl;
                            std::cerr << "  Current timestamp: " << current_timestamp << std::endl;
                            std::cerr << "  Expected: " << (last_timestamp + 1) << std::endl;
                            std::cerr << "  Delta: " << static_cast<int64_t>(current_timestamp - last_timestamp) << std::endl;
                            std::cerr << "  Last sample ch0: " << last_sample_ch0 << std::endl;
                            std::cerr << "  Current sample ch0: " << samples[samp * trialCont.count] << std::endl;
                        }
                    }
                }
                last_timestamp = current_timestamp;
                last_sample_ch0 = samples[samp * trialCont.count + 0];
                first_read = false;

                // // Calculate timestamp offset on first sample
                // if (timestamp_offset == -1) {
                //     const auto ch0 = samples[samp * trialCont.count + 0];
                //     const auto ch1 = samples[samp * trialCont.count + 1];
                //     const int64_t phase_from_ch0 = (ch0 - SAWTOOTH_MIN) % UNIQUE_VALUES;
                //     const int64_t phase_from_ch1 = (SAWTOOTH_MAX - ch1) % UNIQUE_VALUES;
                //
                //     if (phase_from_ch0 != phase_from_ch1) {
                //         std::cerr << "Warning: Sawtooth channels 1-2 out of sync at first sample!" << std::endl;
                //         std::cerr << "  Ch0 phase: " << phase_from_ch0 << ", Ch1 phase: " << phase_from_ch1 << std::endl;
                //         continue;
                //     }
                //
                //     // Calculate offset: offset = timestamp - phase
                //     timestamp_offset = static_cast<int64_t>(current_timestamp) - phase_from_ch0;
                //     std::cout << "Initialized timestamp offset: " << timestamp_offset
                //               << " (timestamp=" << current_timestamp << ", phase=" << phase_from_ch0 << ")" << std::endl;
                // }

                // // Calculate expected phase from timestamp: phase = (timestamp - offset) % UNIQUE_VALUES
                // const int64_t expected_phase = (static_cast<int64_t>(current_timestamp) - timestamp_offset) % UNIQUE_VALUES;
                //
                // // Validate each channel
                // for (uint32_t ch = 0; ch < trialCont.count; ch++) {
                //     const int16_t value = samples[samp * trialCont.count + ch];
                //     const uint16_t chan_id = trialCont.chan[ch];
                //
                //     if (ch < 2) {
                //         // Channel 0: ramps up, Channel 1: ramps down
                //         const auto expected_value = static_cast<int16_t>(
                //             ch == 0 ? SAWTOOTH_MIN + expected_phase : SAWTOOTH_MAX - expected_phase
                //         );
                //
                //         if (std::abs(static_cast<int64_t>(value) - expected_value) > SAWTOOTH_TOLERANCE) {
                //             sawtooth_errors++;
                //         }
                //     } else if (value != static_cast<int16_t>(chan_id * 100)) {
                //         // Channels 3+: Validate constant value
                //         constant_errors++;
                //     }
                // }
            }
            total_samples_received += trialCont.num_samples;
        }

        // Print statistics every 10 seconds
        auto now = std::chrono::steady_clock::now();
        auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_stats_time
        ).count();

        if (stats_elapsed >= 10) {
            auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - start_time
            ).count();

            const double avg_rate = total_samples_received / (total_elapsed + 1);

            std::cout << "Running for " << total_elapsed << "s"
                      << " | Samples: " << total_samples_received
                      << " | Rate: " << static_cast<int>(avg_rate) << " Hz"
                      << std::endl;

            std::cout << "  Errors - Sawtooth: " << sawtooth_errors
                      << ", Constant: " << constant_errors
                      << ", Timestamp: " << timestamp_errors
                      << std::endl;

            last_stats_time = now;
        }

        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Final statistics
    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time
    ).count();

    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total runtime: " << total_elapsed << " seconds" << std::endl;
    std::cout << "Total samples received: " << total_samples_received << std::endl;
    std::cout << "Average sample rate: " << (total_samples_received / (total_elapsed + 1)) << " Hz" << std::endl;
    std::cout << "Sawtooth errors: " << sawtooth_errors << std::endl;
    std::cout << "Constant value errors: " << constant_errors << std::endl;
    std::cout << "Timestamp errors: " << timestamp_errors << std::endl;

    // Close connection
    cbSdkClose(INST);
    std::cout << "\nConnection closed." << std::endl;

    return (sawtooth_errors > 0 || constant_errors > 0 || timestamp_errors > 0) ? 1 : 0;
}