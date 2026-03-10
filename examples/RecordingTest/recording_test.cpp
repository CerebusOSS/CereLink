///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   recording_test.cpp
/// @brief  Test Central recording start/stop
///
/// Usage:
///   recording_test [device_type]
///   recording_test HUB1
///
/// Requires Central to be running with a device connected.
///
/// Uses the two-step sequence from the old cbsdk/cbpy:
///   1. openCentralFileDialog()   — sends cbFILECFG_OPT_OPEN
///   2. sleep 250ms               — Central needs time to open the dialog
///   3. startCentralRecording()   — sends recording=1
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk/sdk_session.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

using namespace cbsdk;

DeviceType parseDeviceType(const std::string& s) {
    if (s == "NSP")        return DeviceType::LEGACY_NSP;
    if (s == "GEMINI_NSP") return DeviceType::NSP;
    if (s == "HUB1")       return DeviceType::HUB1;
    if (s == "HUB2")       return DeviceType::HUB2;
    if (s == "HUB3")       return DeviceType::HUB3;
    if (s == "NPLAY")      return DeviceType::NPLAY;
    std::cerr << "Unknown device type: " << s << "\n";
    std::exit(1);
}

static const char* filecfgOptName(uint32_t opt) {
    switch (opt) {
        case cbFILECFG_OPT_NONE:      return "NONE";
        case cbFILECFG_OPT_KEEPALIVE: return "KEEPALIVE";
        case cbFILECFG_OPT_REC:       return "REC";
        case cbFILECFG_OPT_STOP:      return "STOP";
        case cbFILECFG_OPT_NMREC:     return "NMREC";
        case cbFILECFG_OPT_CLOSE:     return "CLOSE";
        case cbFILECFG_OPT_SYNCH:     return "SYNCH";
        case cbFILECFG_OPT_OPEN:      return "OPEN";
        case cbFILECFG_OPT_TIMEOUT:   return "TIMEOUT";
        case cbFILECFG_OPT_PAUSE:     return "PAUSE";
        default:                       return "UNKNOWN";
    }
}

/// Helper: wait for a REPFILECFG response up to timeout_ms
static bool waitForFileCfgResponse(std::atomic<int>& count, int initial, int timeout_ms) {
    for (int i = 0; i < timeout_ms / 100; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (count.load() > initial)
            return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    std::string device = argc > 1 ? argv[1] : "HUB1";
    DeviceType dt = parseDeviceType(device);

    SdkConfig config;
    config.device_type = dt;
    config.autorun = true;

    std::cerr << "Creating session (" << device << ")...\n";
    auto result = SdkSession::create(config);
    if (result.isError()) {
        std::cerr << "ERROR: " << result.error() << "\n";
        return 1;
    }
    auto session = std::move(result.value());

    // Register callback for REPFILECFG to verify Central's response
    std::atomic<int> filecfg_count{0};
    std::mutex filecfg_mutex;
    std::string last_filecfg_info;

    auto filecfg_handle = session.registerConfigCallback(cbPKTTYPE_REPFILECFG,
        [&](const cbPKT_GENERIC& pkt) {
            cbPKT_FILECFG filecfg;
            std::memcpy(&filecfg, &pkt, std::min(sizeof(filecfg), sizeof(pkt)));
            filecfg_count++;
            std::lock_guard<std::mutex> lock(filecfg_mutex);
            last_filecfg_info = std::string("options=") + filecfgOptName(filecfg.options) +
                " recording=" + std::to_string(filecfg.recording) +
                " filename='" + std::string(filecfg.filename, strnlen(filecfg.filename, sizeof(filecfg.filename))) + "'";
        });

    // Also count total packets to verify we're receiving data
    std::atomic<uint64_t> total_packets{0};
    auto pkt_handle = session.registerPacketCallback([&](const cbPKT_GENERIC&) {
        total_packets++;
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cerr << "Connected. isRunning=" << session.isRunning() << "\n";
    std::cerr << "Packets received in 2s: " << total_packets.load() << "\n";
    std::cerr << "FILECFG responses so far: " << filecfg_count.load() << "\n\n";

    // Step 1: Open file dialog (required by Central before starting recording)
    std::cerr << "Step 1: Opening Central File Storage dialog...\n";
    auto r0 = session.openCentralFileDialog();
    if (r0.isError()) {
        std::cerr << "openCentralFileDialog FAILED: " << r0.error() << "\n";
        session.stop();
        return 1;
    }

    int initial_count = filecfg_count.load();
    bool got_response = waitForFileCfgResponse(filecfg_count, initial_count, 5000);
    if (got_response) {
        std::lock_guard<std::mutex> lock(filecfg_mutex);
        std::cerr << "  -> Central responded: " << last_filecfg_info << "\n";
    } else {
        std::cerr << "  -> No REPFILECFG response after 5s (continuing anyway).\n";
    }

    // Step 2: Wait for dialog to initialize (empirically required, 250ms minimum)
    std::cerr << "  Waiting 500ms for dialog...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Step 3: Start recording (use user's home directory to avoid write permission issues)
    std::string rec_filename = "cerelink_test";
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        rec_filename = std::string(userprofile) + "\\Documents\\cerelink_test";
    }
    std::cerr << "\nStep 2: Starting Central recording (filename='" << rec_filename << "')...\n";
    initial_count = filecfg_count.load();
    auto r1 = session.startCentralRecording(rec_filename, "CereLink C++ test");
    if (r1.isError()) {
        std::cerr << "startCentralRecording FAILED: " << r1.error() << "\n";
        session.stop();
        return 1;
    }

    got_response = waitForFileCfgResponse(filecfg_count, initial_count, 5000);
    if (got_response) {
        std::lock_guard<std::mutex> lock(filecfg_mutex);
        std::cerr << "  -> Central responded: " << last_filecfg_info << "\n\n";
    } else {
        std::cerr << "  -> No REPFILECFG response after 5s.\n";
        std::cerr << "     Total packets received: " << total_packets.load() << "\n\n";
    }

    std::cerr << "Recording for 5 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop recording
    std::cerr << "Step 3: Stopping Central recording...\n";
    initial_count = filecfg_count.load();
    auto r2 = session.stopCentralRecording();
    if (r2.isError()) {
        std::cerr << "stopCentralRecording FAILED: " << r2.error() << "\n";
        session.stop();
        return 1;
    }

    got_response = waitForFileCfgResponse(filecfg_count, initial_count, 5000);
    if (got_response) {
        std::lock_guard<std::mutex> lock(filecfg_mutex);
        std::cerr << "  -> Central responded: " << last_filecfg_info << "\n\n";
    } else {
        std::cerr << "  -> No REPFILECFG response after 5s.\n\n";
    }

    // Step 4: Try to close the File Storage dialog
    std::cerr << "Step 4: Closing Central File Storage dialog...\n";
    initial_count = filecfg_count.load();
    auto r3 = session.closeCentralFileDialog();
    if (r3.isError()) {
        std::cerr << "closeCentralFileDialog FAILED: " << r3.error() << "\n";
    } else {
        got_response = waitForFileCfgResponse(filecfg_count, initial_count, 5000);
        if (got_response) {
            std::lock_guard<std::mutex> lock(filecfg_mutex);
            std::cerr << "  -> Central responded: " << last_filecfg_info << "\n\n";
        } else {
            std::cerr << "  -> No REPFILECFG response after 5s.\n\n";
        }
    }

    std::cerr << "Total packets received: " << total_packets.load() << "\n";
    std::cerr << "Total FILECFG responses: " << filecfg_count.load() << "\n";

    session.unregisterCallback(filecfg_handle);
    session.unregisterCallback(pkt_handle);
    session.stop();
    std::cerr << "Done.\n";
    return 0;
}
