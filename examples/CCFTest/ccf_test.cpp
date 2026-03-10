///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   ccf_test.cpp
/// @brief  Test CCF save/load with a real device
///
/// Usage:
///   ccf_test <device_type> <ccf_A> <ccf_B>
///   ccf_test HUB1 hub1-raw128-spk128.ccf hub1-raw16.ccf
///
/// The test loads CCF A, saves the device config, loads CCF B, saves again,
/// and compares the two saved files to verify the load actually changed the device.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk/sdk_session.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>

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

void printChannelSummary(SdkSession& session, const char* label, int n = 5) {
    std::cout << "=== " << label << " (channels 1-" << n << ") ===\n";
    for (uint32_t ch = 1; ch <= (uint32_t)n; ++ch) {
        const auto* info = session.getChanInfo(ch);
        if (info) {
            std::cout << "  ch " << std::setw(3) << ch
                      << ": spkopts=0x" << std::hex << std::setw(5) << std::setfill('0') << info->spkopts
                      << " ainpopts=0x" << std::setw(4) << info->ainpopts
                      << std::dec << std::setfill(' ')
                      << " smpgroup=" << info->smpgroup
                      << " label=" << info->label
                      << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: ccf_test <device_type> <ccf_A> <ccf_B>\n"
                  << "  Loads A, saves, loads B, saves, then compares.\n"
                  << "  Example: ccf_test HUB1 hub1-raw128-spk128.ccf hub1-raw16.ccf\n";
        return 1;
    }

    DeviceType device_type = parseDeviceType(argv[1]);
    std::string ccf_a = argv[2];
    std::string ccf_b = argv[3];

    // Create session
    SdkConfig config;
    config.device_type = device_type;
    config.autorun = true;

    std::cout << "Creating session...\n";
    auto result = SdkSession::create(config);
    if (result.isError()) {
        std::cerr << "ERROR: " << result.error() << "\n";
        return 1;
    }
    auto session = std::move(result.value());

    // Wait for config to be fully populated
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Connected.\n\n";

    printChannelSummary(session, "Before any load");

    // Step 1: Load CCF A
    std::cout << "\n--- Loading " << ccf_a << " ---\n";
    auto r1 = session.loadCCF(ccf_a);
    if (r1.isError()) { std::cerr << "Load A failed: " << r1.error() << "\n"; return 1; }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    printChannelSummary(session, ("After loading " + ccf_a).c_str());

    // Save device state after A
    auto s1 = session.saveCCF("ccf_after_A.ccf");
    if (s1.isError()) { std::cerr << "Save after A failed: " << s1.error() << "\n"; return 1; }
    std::cout << "  Saved device state -> ccf_after_A.ccf\n\n";

    // Step 2: Load CCF B
    std::cout << "--- Loading " << ccf_b << " ---\n";
    auto r2 = session.loadCCF(ccf_b);
    if (r2.isError()) { std::cerr << "Load B failed: " << r2.error() << "\n"; return 1; }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    printChannelSummary(session, ("After loading " + ccf_b).c_str());

    // Save device state after B
    auto s2 = session.saveCCF("ccf_after_B.ccf");
    if (s2.isError()) { std::cerr << "Save after B failed: " << s2.error() << "\n"; return 1; }
    std::cout << "  Saved device state -> ccf_after_B.ccf\n\n";

    // Compare: check spkopts for channels 1-5 between A and B states
    std::cout << "=== Comparison ===\n";
    // Re-read saved files to compare
    // (We already know the in-memory state; compare saved CCF sizes as a quick sanity check)
    {
        std::ifstream fa("ccf_after_A.ccf", std::ios::ate);
        std::ifstream fb("ccf_after_B.ccf", std::ios::ate);
        if (fa.is_open() && fb.is_open()) {
            auto sizeA = fa.tellg();
            auto sizeB = fb.tellg();
            std::cout << "  ccf_after_A.ccf: " << sizeA << " bytes\n";
            std::cout << "  ccf_after_B.ccf: " << sizeB << " bytes\n";
            std::cout << "  Files " << (sizeA == sizeB ? "are SAME size (unexpected if configs differ)"
                                                       : "DIFFER in size (good)") << "\n";
        }
    }

    session.stop();
    std::cout << "\nDone.\n";
    return 0;
}
