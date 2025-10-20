/**
 * spikes_cbsdk.cpp
 *
 * Validates spike event timing from the spikes_lsl.py test pattern.
 *
 * Expected pattern:
 * - Single spikes (9s): 36 spikes rotating through channels 1-4, every 0.25s (7500 samples)
 * - Burst period (1s): 99 spikes on all channels simultaneously, every 0.01s (300 samples)
 *
 * Synchronization strategy:
 * 1. Look for burst period (4 channels spiking within ~1ms window)
 * 2. First non-coincident spike after burst marks start of single-spike pattern
 * 3. Validate single-spike timing matches expected pattern
 */

#include <cerelink/cbsdk.h>
#include <cerelink/cbhwlib.h>
#include <cerelink/cbproto.h>
#include <iostream>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <cinttypes>
#include <thread>
#include <chrono>
#include <memory>

constexpr uint32_t INST = 0;
constexpr uint32_t SAMPLE_RATE = 30000;
constexpr float SAMPLES_TO_MS = 1000.0f / SAMPLE_RATE;

// Expected pattern parameters
constexpr uint32_t SINGLE_SPIKE_INTERVAL = 7500;  // 0.25s in samples
constexpr uint32_t BURST_SPIKE_INTERVAL = 300;    // 0.01s in samples
constexpr uint32_t TOLERANCE_SAMPLES = 100;       // Tolerance for timing (±3.3ms)

// Channels to monitor (1-based)
constexpr uint16_t TEST_CHANNELS[] = {1, 2, 3, 4};
constexpr size_t NUM_TEST_CHANNELS = 4;

struct SpikeEvent {
    PROCTIME timestamp;
    uint16_t channel;
    uint16_t unit;
};

enum class PatternState {
    SEARCHING_FOR_BURST,
    IN_BURST,
    VALIDATING_SINGLE_SPIKES
};

class SpikeValidator {
public:
    SpikeValidator() : state_(PatternState::SEARCHING_FOR_BURST),
                       burst_spike_count_(0),
                       pattern_start_time_(0),
                       next_expected_channel_(0),
                       single_spike_count_(0),
                       timing_errors_(0) {}

    void processEvents(const std::vector<SpikeEvent>& events) {
        for (const auto& event : events) {
            // Only process test channels
            bool is_test_channel = false;
            for (const auto& ch : TEST_CHANNELS) {
                if (event.channel == ch) {
                    is_test_channel = true;
                    break;
                }
            }
            if (!is_test_channel) continue;

            switch (state_) {
                case PatternState::SEARCHING_FOR_BURST:
                    checkForBurstStart(event);
                    break;

                case PatternState::IN_BURST:
                    processBurstEvent(event);
                    break;

                case PatternState::VALIDATING_SINGLE_SPIKES:
                    validateSingleSpike(event);
                    break;
            }
        }
    }

    void printStatus() const {
        std::cout << "\n=== Spike Validation Status ===\n";
        switch (state_) {
            case PatternState::SEARCHING_FOR_BURST:
                std::cout << "State: Searching for burst period...\n";
                std::cout << "Recent events in window: " << recent_events_.size() << "\n";
                break;

            case PatternState::IN_BURST:
                std::cout << "State: Processing burst period\n";
                std::cout << "Burst spikes detected: " << burst_spike_count_ << " / 99\n";
                break;

            case PatternState::VALIDATING_SINGLE_SPIKES:
                std::cout << "State: Validating single spike pattern\n";
                std::cout << "Single spikes validated: " << single_spike_count_ << " / 36\n";
                std::cout << "Next expected channel: " << TEST_CHANNELS[next_expected_channel_] << "\n";
                std::cout << "Timing errors: " << timing_errors_ << "\n";

                if (single_spike_count_ >= 36) {
                    std::cout << "\n*** PATTERN VALIDATED SUCCESSFULLY! ***\n";
                    std::cout << "Total timing errors: " << timing_errors_ << " / " << single_spike_count_ << "\n";

                    // Reset to validate next cycle
                    const_cast<SpikeValidator*>(this)->resetForNextCycle();
                }
                break;
        }
        std::cout << "===============================\n";
    }

private:
    void checkForBurstStart(const SpikeEvent& event) {
        // Keep a sliding window of recent events (last 50ms)
        const PROCTIME window_duration = 50 * SAMPLE_RATE / 1000;  // 50ms in samples

        // Add new event
        recent_events_.push_back(event);

        // Remove old events outside window
        while (!recent_events_.empty() &&
               event.timestamp - recent_events_.front().timestamp > window_duration) {
            recent_events_.pop_front();
        }

        // Check if we have simultaneous spikes on all 4 channels
        // (within 1ms = 30 samples)
        if (recent_events_.size() >= NUM_TEST_CHANNELS) {
            if (checkForCoincidentSpikes()) {
                std::cout << "\n*** BURST PERIOD DETECTED! ***\n";
                state_ = PatternState::IN_BURST;
                burst_start_time_ = event.timestamp;
                burst_spike_count_ = 0;
                recent_events_.clear();
            }
        }
    }

    bool checkForCoincidentSpikes() {
        // Look for a time window where all 4 channels spiked
        const PROCTIME coincidence_window = 30;  // 1ms in samples

        for (auto it = recent_events_.begin(); it != recent_events_.end(); ++it) {
            // Count how many different channels spiked within coincidence_window
            std::set<uint16_t> channels_in_window;

            for (auto check_it = it; check_it != recent_events_.end(); ++check_it) {
                if (check_it->timestamp - it->timestamp > coincidence_window) {
                    break;
                }
                channels_in_window.insert(check_it->channel);
            }

            if (channels_in_window.size() >= NUM_TEST_CHANNELS) {
                return true;
            }
        }

        return false;
    }

    void processBurstEvent(const SpikeEvent& event) {
        burst_spike_count_++;

        // Check if we've seen enough burst spikes to be confident
        // After ~50 burst spikes, start watching for single spikes
        if (burst_spike_count_ > 50) {
            // Check if this might be the first single spike
            // (only one channel spikes, not all 4)
            recent_events_.push_back(event);

            // Keep only last 2ms of events
            const PROCTIME window = 60;  // 2ms in samples
            while (!recent_events_.empty() &&
                   event.timestamp - recent_events_.front().timestamp > window) {
                recent_events_.pop_front();
            }

            // If we only see 1 channel in the recent window, burst is over
            std::set<uint16_t> channels_in_window;
            for (const auto& evt : recent_events_) {
                channels_in_window.insert(evt.channel);
            }

            if (channels_in_window.size() == 1) {
                std::cout << "\n*** BURST PERIOD ENDED (detected " << burst_spike_count_ << " spikes) ***\n";
                std::cout << "*** STARTING SINGLE SPIKE VALIDATION ***\n";

                state_ = PatternState::VALIDATING_SINGLE_SPIKES;
                pattern_start_time_ = event.timestamp;
                next_expected_channel_ = 0;  // Expect channel 1 (index 0)
                single_spike_count_ = 0;
                timing_errors_ = 0;

                // Process this event as the first single spike
                validateSingleSpike(event);
            }
        }
    }

    void validateSingleSpike(const SpikeEvent& event) {
        // Check if this is the expected channel
        uint16_t expected_ch = TEST_CHANNELS[next_expected_channel_];

        if (event.channel != expected_ch) {
            std::cout << "WARNING: Expected channel " << expected_ch
                     << " but got " << event.channel << "\n";
            return;
        }

        // Check timing
        PROCTIME expected_time = pattern_start_time_ + (single_spike_count_ * SINGLE_SPIKE_INTERVAL);
        int64_t time_error = static_cast<int64_t>(event.timestamp) - static_cast<int64_t>(expected_time);

        if (std::abs(time_error) > TOLERANCE_SAMPLES) {
            std::cout << "TIMING ERROR: Spike " << single_spike_count_
                     << " on channel " << event.channel
                     << " is " << (time_error * SAMPLES_TO_MS) << " ms off\n";
            timing_errors_++;
        }

        // Move to next expected channel (cycles 0,1,2,3,0,1,2,3...)
        next_expected_channel_ = (next_expected_channel_ + 1) % NUM_TEST_CHANNELS;
        single_spike_count_++;

        if (single_spike_count_ % 9 == 0) {
            std::cout << "Validated " << single_spike_count_ << " / 36 single spikes\n";
        }
    }

    void resetForNextCycle() {
        state_ = PatternState::SEARCHING_FOR_BURST;
        burst_spike_count_ = 0;
        single_spike_count_ = 0;
        timing_errors_ = 0;
        recent_events_.clear();
        std::cout << "\nResetting to validate next 10-second cycle...\n\n";
    }

    PatternState state_;
    std::deque<SpikeEvent> recent_events_;

    // Burst detection
    PROCTIME burst_start_time_;
    uint32_t burst_spike_count_;

    // Single spike validation
    PROCTIME pattern_start_time_;
    uint32_t next_expected_channel_;  // Index into TEST_CHANNELS
    uint32_t single_spike_count_;
    uint32_t timing_errors_;
};

int main(int argc, char* argv[])
{
    cbSdkResult res;

    // Open connection
    std::cout << "Opening cbsdk connection...\n";
    res = cbSdkOpen(INST, CBSDKCONNECTION_DEFAULT, {});
    if (res != CBSDKRESULT_SUCCESS) {
        std::cerr << "Failed to open cbsdk: " << res << "\n";
        return 1;
    }
    std::cout << "Connected!\n";

    // Check connection type
    cbSdkConnectionType conType;
    cbSdkInstrumentType instType;
    res = cbSdkGetType(INST, &conType, &instType);
    if (res == CBSDKRESULT_SUCCESS) {
        std::cout << "Connection type: " << (conType == CBSDKCONNECTION_UDP ? "UDP" : "Central") << "\n";
    }

    // Configure all channels: enable spike detection ONLY on channels 1-4, disable everything else
    std::cout << "\nConfiguring channels...\n";
    for (uint16_t ch = 1; ch <= 128; ch++) {
        cbPKT_CHANINFO chaninfo;

        // Get current channel configuration
        res = cbSdkGetChannelConfig(INST, ch, &chaninfo);
        if (res != CBSDKRESULT_SUCCESS) {
            continue;  // Channel doesn't exist
        }

        // Check if this is a test channel (1-4)
        bool is_test_channel = false;
        for (const auto& test_ch : TEST_CHANNELS) {
            if (ch == test_ch) {
                is_test_channel = true;
                break;
            }
        }

        if (is_test_channel) {
            // Test channels: disable continuous, enable spike detection
            chaninfo.ainpopts &= ~cbAINP_RAWSTREAM;
            chaninfo.ainpopts |= cbAINP_SPKSTREAM;
            chaninfo.ainpopts |= cbAINP_SPKPROC;

            res = cbSdkSetChannelConfig(INST, ch, &chaninfo);
            if (res == CBSDKRESULT_SUCCESS) {
                std::cout << "  Channel " << ch << ": Spike detection enabled\n";
            }
        } else {
            // All other channels: disable both continuous and spike detection
            chaninfo.ainpopts &= ~cbAINP_RAWSTREAM;
            chaninfo.ainpopts &= ~cbAINP_SPKSTREAM;
            chaninfo.ainpopts &= ~cbAINP_SPKPROC;

            res = cbSdkSetChannelConfig(INST, ch, &chaninfo);
            // Only print if we actually changed something
            // (no output to avoid cluttering for 124 channels)
        }
    }

    std::cout << "\nChannel configuration complete!\n";

    // Configure trial collection (enable event collection)
    std::cout << "Configuring trial collection...\n";
    res = cbSdkSetTrialConfig(INST,
                              1,        // bActive = true
                              0, 0, 0,  // begchan, begmask, begval (start immediately)
                              0, 0, 0,  // endchan, endmask, endval (no auto-end)
                              0,        // uWaveforms = 0 (not collecting waveforms)
                              0,        // uConts = 0 (not collecting continuous)
                              cbSdk_EVENT_DATA_SAMPLES,  // uEvents (default buffer size)
                              0,        // uComments = 0
                              0);       // uTrackings = 0
    if (res != CBSDKRESULT_SUCCESS) {
        std::cerr << "Failed to configure trial: " << res << "\n";
        cbSdkClose(INST);
        return 1;
    }
    std::cout << "Trial configured for event collection\n";

    // Allocate trial event structure and buffers
    // Pre-allocate to cbSdk_EVENT_DATA_SAMPLES capacity so cbSdkGetTrialData can fill as many events as available
    auto trialEvent = std::make_unique<cbSdkTrialEvent>();
    std::vector<PROCTIME> event_ts(cbSdk_EVENT_DATA_SAMPLES);
    std::vector<uint16_t> event_channels(cbSdk_EVENT_DATA_SAMPLES);
    std::vector<uint16_t> event_units(cbSdk_EVENT_DATA_SAMPLES);

    // Set up trial event structure with our pre-allocated buffers
    trialEvent->num_events = cbSdk_EVENT_DATA_SAMPLES;  // Buffer capacity
    trialEvent->timestamps = event_ts.data();
    trialEvent->channels = event_channels.data();
    trialEvent->units = event_units.data();
    trialEvent->waveforms = nullptr;

    std::cout << "Allocated event buffers (capacity: " << cbSdk_EVENT_DATA_SAMPLES << " events)\n";

    std::cout << "\nListening for spike events on channels 1-4...\n";
    std::cout << "Searching for burst period to synchronize...\n";
    std::cout << "Will run for 30 seconds...\n";

    SpikeValidator validator;
    uint32_t read_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    const auto max_duration = std::chrono::seconds(30);

    // Main loop - run for 30 seconds
    uint32_t total_events_received = 0;
    while (std::chrono::steady_clock::now() - start_time < max_duration) {
        // Reset num_events to buffer capacity before each call
        uint32_t buffer_capacity = cbSdk_EVENT_DATA_SAMPLES;
        trialEvent->num_events = buffer_capacity;

        // Get trial events (with seek to advance buffer)
        res = cbSdkGetTrialData(INST, 1, trialEvent.get(), nullptr, nullptr, nullptr);

        if (res == CBSDKRESULT_SUCCESS) {
            if (trialEvent->num_events > 0) {
                read_count++;
                total_events_received += trialEvent->num_events;

                // Debug output for first few reads
                if (read_count <= 5) {
                    std::cout << "Read #" << read_count << ": Received " << trialEvent->num_events << " events\n";
                }

                // Convert to vector of SpikeEvent for easier processing
                std::vector<SpikeEvent> events;
                for (uint32_t i = 0; i < trialEvent->num_events; i++) {
                    events.push_back({
                        event_ts[i],
                        event_channels[i],
                        event_units[i]
                    });
                }

                // Process events
                validator.processEvents(events);

                // Print status every 100 reads (~3 seconds at 30Hz)
                if (read_count % 100 == 0) {
                    std::cout << "Total events received so far: " << total_events_received << "\n";
                    validator.printStatus();
                }
            }
        } else {
            std::cerr << "cbSdkGetTrialData failed with error: " << res << "\n";
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 Hz polling
    }

    std::cout << "\n30 seconds elapsed. Final status:\n";
    validator.printStatus();

    // Cleanup
    cbSdkClose(INST);
    return 0;
}
