///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   ccf_test.cpp
/// @brief  Test CCF save/load with a real device
///
/// Usage:
///   ccf_test [device_type]
///   ccf_test HUB1
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk/sdk_session.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>

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

int main(int argc, char* argv[]) {
    DeviceType device_type = DeviceType::LEGACY_NSP;
    if (argc >= 2) device_type = parseDeviceType(argv[1]);

    std::cout << "=== CCF Test ===\n\n";

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
    std::cout << "Connected.\n\n";

    // Wait for config to be fully populated
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Inspect device_config
    std::cout << "=== Channel Info (first 5 channels) ===\n";
    for (uint32_t ch = 1; ch <= 5; ++ch) {
        const auto* info = session.getChanInfo(ch);
        if (info) {
            std::cout << "  ch " << ch << ":"
                      << " chan=" << info->chan
                      << " proc=" << info->proc
                      << " bank=" << info->bank
                      << " term=" << info->term
                      << " smpgroup=" << info->smpgroup
                      << " chancaps=0x" << std::hex << info->chancaps << std::dec
                      << " label=" << info->label
                      << "\n";
        } else {
            std::cout << "  ch " << ch << ": null\n";
        }
    }

    // Check group info
    std::cout << "\n=== Group Info ===\n";
    for (uint32_t g = 1; g <= 6; ++g) {
        const auto* ginfo = session.getGroupInfo(g);
        if (ginfo) {
            std::cout << "  Group " << g << ":"
                      << " label=" << ginfo->label
                      << " length=" << ginfo->length
                      << "\n";
        }
    }

    // Manually test extractDeviceConfig
    {
        std::cout << "\n=== Debug extractDeviceConfig ===\n";
        // Note: we can't call extractDeviceConfig directly from here,
        // but we can inspect what saveCCF does by checking the CCF internally.
        // Let's verify the config has data by checking a few fields:
        const auto* ch1 = session.getChanInfo(1);
        const auto* ch2 = session.getChanInfo(2);
        std::cout << "  getChanInfo(1)->chan = " << (ch1 ? ch1->chan : 0)
                  << ", label = " << (ch1 ? ch1->label : "null")
                  << ", dlen = " << (ch1 ? ch1->cbpkt_header.dlen : 0)
                  << ", chid = 0x" << std::hex << (ch1 ? ch1->cbpkt_header.chid : 0) << std::dec
                  << "\n";
        std::cout << "  getChanInfo(2)->chan = " << (ch2 ? ch2->chan : 0) << "\n";
        std::cout << "  sizeof(cbPKT_CHANINFO) = " << sizeof(cbPKT_CHANINFO) << "\n";
    }

    // Test saveCCF
    std::cout << "\n=== Save CCF ===\n";
    auto save_result = session.saveCCF("ccf_test_output.ccf");
    if (save_result.isOk()) {
        std::cout << "Saved to ccf_test_output.ccf\n";
        // Check file size
        std::ifstream f("ccf_test_output.ccf", std::ios::ate);
        if (f.is_open()) {
            std::cout << "  File size: " << f.tellg() << " bytes\n";
        }
    } else {
        std::cerr << "Save failed: " << save_result.error() << "\n";
    }

    // Test loadCCF with one of the user's files
    if (argc >= 3) {
        std::string ccf_path = argv[2];
        std::cout << "\n=== Load CCF: " << ccf_path << " ===\n";
        auto load_result = session.loadCCF(ccf_path);
        if (load_result.isOk()) {
            std::cout << "Loaded successfully\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Re-check channel info
            std::cout << "\n=== Channel Info After Load (first 5) ===\n";
            for (uint32_t ch = 1; ch <= 5; ++ch) {
                const auto* info = session.getChanInfo(ch);
                if (info) {
                    std::cout << "  ch " << ch << ":"
                              << " chan=" << info->chan
                              << " smpgroup=" << info->smpgroup
                              << " chancaps=0x" << std::hex << info->chancaps << std::dec
                              << " label=" << info->label
                              << "\n";
                }
            }
        } else {
            std::cerr << "Load failed: " << load_result.error() << "\n";
        }
    }

    session.stop();
    std::cout << "\nDone.\n";
    return 0;
}
