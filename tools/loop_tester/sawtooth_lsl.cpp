/**
 * LSL Sawtooth Stream Source
 *
 * Generates a 30 kHz LSL stream with:
 * - Channel 1-2: Sawtooth signal (ramps from -32768 to 32767)
 * - Channels 3-130: Constant values (channel_index * 100)
 *
 * This is used to test long-running cbsdk connections via nPlayServer with LSL support.
 */

#include <lsl_cpp.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <csignal>
#include <atomic>

// Configuration constants
constexpr int NUM_CHANNELS = 128;
constexpr int16_t SAWTOOTH_MIN = -32768;
constexpr int16_t SAWTOOTH_MAX = 32767;
constexpr int32_t UNIQUE_VALUES = SAWTOOTH_MAX - SAWTOOTH_MIN + 1; // 65536
constexpr int SAMPLE_RATE = 30000;  // 30 kHz
constexpr int CHUNK_SIZE = 100;      // Send 100 samples at a time

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

void signal_handler(const int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
}

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        // Parse command line arguments
        std::string stream_name = "SawtoothTest";
        if (argc > 1) {
            stream_name = argv[1];
        }

        std::cout << "Creating LSL stream: " << stream_name << std::endl;
        std::cout << "  Channels: " << NUM_CHANNELS << std::endl;
        std::cout << "  Sample Rate: " << SAMPLE_RATE << " Hz" << std::endl;
        std::cout << "  Chunk Size: " << CHUNK_SIZE << " samples" << std::endl;
        std::cout << std::endl;

        // Create stream info
        lsl::stream_info info(
            stream_name,              // Stream name
            "Simulation",                    // Content type
            NUM_CHANNELS,             // Number of channels
            SAMPLE_RATE,              // Sampling rate
            lsl::cf_int16,           // Channel format (16-bit integers)
            std::string("SawtoothTest_") + std::to_string(time(nullptr))  // Unique source ID
        );

        // Add channel metadata
        lsl::xml_element channels = info.desc().append_child("channels");
        for (int i = 0; i < NUM_CHANNELS; i++) {
            lsl::xml_element ch = channels.append_child("channel");
            ch.append_child_value("label", "Chan" + std::to_string(i + 1));
            ch.append_child_value("type", i < 2 ? "Sawtooth" : "Constant");
            ch.append_child_value("unit", "quarter-microvolts");
        }

        // Create outlet
        lsl::stream_outlet outlet(info, CHUNK_SIZE, 30);  // 30 seconds max buffering

        std::cout << "LSL outlet created. Waiting for connections..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        std::cout << std::endl;

        // Prepare data buffer - LSL expects vector of samples, where each sample is a vector of channel values
        std::vector<std::vector<int16_t>> chunk(CHUNK_SIZE, std::vector<int16_t>(NUM_CHANNELS));

        int64_t sample_counter = 0;
        auto start_time = std::chrono::steady_clock::now();
        auto next_send_time = start_time;
        const auto chunk_duration = std::chrono::microseconds(
            (1000000LL * CHUNK_SIZE) / SAMPLE_RATE
        );

        // Statistics
        uint64_t chunks_sent = 0;
        auto last_stats_time = start_time;

        while (keep_running) {
            // Generate chunk of data
            for (int sample = 0; sample < CHUNK_SIZE; sample++) {
                for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                    int16_t value;
                    if (ch == 0) {
                        // Ramps from -32768 to 32767, 1 integer per sample.
                        value = static_cast<int16_t>(SAWTOOTH_MIN + sample_counter % UNIQUE_VALUES);
                    } else if (ch == 1) {
                        // Ramps in reverse order
                        value = static_cast<int16_t>(SAWTOOTH_MAX - sample_counter % UNIQUE_VALUES);
                    } else {
                        // Channels 3-: Constant value based on channel number
                        value = static_cast<int16_t>((ch + 1) * 100);
                    }
                    chunk[sample][ch] = value;
                }
                sample_counter++;
            }

            // Wait until it's time to send this chunk (maintain timing)
            std::this_thread::sleep_until(next_send_time);

            // Send chunk with automatic timestamp
            outlet.push_chunk(chunk);

            chunks_sent++;
            next_send_time += chunk_duration;

            // Print statistics every 10 seconds
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_stats_time
            ).count();

            if (elapsed >= 10) {
                auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - start_time
                ).count();

                std::cout << "Running for " << total_elapsed << "s"
                          << " | Chunks sent: " << chunks_sent
                          << " | Samples: " << sample_counter
                          << " | Rate: " << (chunks_sent * CHUNK_SIZE / (total_elapsed + 1)) << " Hz"
                          << std::endl;

                last_stats_time = now;
            }
        }

        std::cout << "\nShutting down..." << std::endl;
        std::cout << "Total chunks sent: " << chunks_sent << std::endl;
        std::cout << "Total samples sent: " << sample_counter << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}