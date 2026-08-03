///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   clear_config_repro.cpp
/// @brief  Reproduce Orion's "Clear-config sync failed: Response timeout" against a real device.
///
/// Faithfully mirrors Orion CereLink.cpp initialConfig()'s clear-config phase:
///   setSampleGroup(NONE) -> per-channel clear LNC (CHANSETAINP)
///   -> per-channel clear spike (CHANSETSPK) -> sync(), retried in a loop.
///
/// Usage: clear_config_repro [HUB1|HUB2|HUB3|NSP|GEMINI_NSP|NPLAY]   (default HUB1)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbsdk/sdk_session.h>
#include <cbproto/cbproto.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>

using cbsdk::SdkSession;
using cbsdk::SdkConfig;
using cbsdk::ChannelType;

static cbsdk::DeviceType parseType(const std::string& s) {
    if (s == "NSP") return cbsdk::DeviceType::LEGACY_NSP;
    if (s == "GEMINI_NSP") return cbsdk::DeviceType::NSP;
    if (s == "HUB2") return cbsdk::DeviceType::HUB2;
    if (s == "HUB3") return cbsdk::DeviceType::HUB3;
    if (s == "NPLAY") return cbsdk::DeviceType::NPLAY;
    return cbsdk::DeviceType::HUB1;
}

int main(int argc, char* argv[]) {
    const std::string type_str = (argc > 1) ? argv[1] : "HUB1";

    SdkConfig config;
    config.device_type = parseType(type_str);
    config.autorun = true;
    config.non_blocking = false;

    std::cout << "Connecting to " << type_str << " ...\n";
    auto result = SdkSession::create(config);
    if (result.isError()) {
        std::cout << "  create() failed: " << result.error() << "\n";
        return 1;
    }
    auto session = std::move(result.value());
    std::cout << "  connected. protocol code=" << session.getProtocolVersion()
              << "  ident=\"" << session.getProcIdent() << "\"";
    auto sysinfo = session.getSysInfo();
    if (sysinfo.isOk()) {
        std::cout << "  sysfreq=" << sysinfo.value().sysfreq;
    }
    std::cout << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Gather all analog-in channels (mirrors Orion's allChannels).
    std::vector<uint32_t> allChannels;
    for (uint32_t ch = 1; ch <= cbNUM_ANALOG_CHANS; ++ch) {
        if (session.getChanInfo(ch).isOk()) allChannels.push_back(ch);
    }
    std::cout << "  discovered " << allChannels.size() << " analog channels\n";
    if (allChannels.empty()) return 2;

    // Orion clears these bits (see CereLink.cpp clearLineNoiseCancellation / clearSpikeProcessing).
    constexpr uint32_t SPIKE_PROCESSING_OPTION_MASK =
        cbAINPSPK_EXTRACT | cbAINPSPK_REJART | cbAINPSPK_REJCLIP | cbAINPSPK_ALIGNPK |
        cbAINPSPK_REJAMPL | cbAINPSPK_THRLEVEL | cbAINPSPK_THRENERGY | cbAINPSPK_THRAUTO |
        cbAINPSPK_ALLSORT;

    auto applyClearConfig = [&]() -> bool {
        auto grp = session.setSampleGroup(static_cast<uint32_t>(allChannels.size()),
                                          ChannelType::ANALOG_IN,
                                          cbsdk::SampleRate::NONE, false, allChannels.data());
        if (grp.isError()) { std::cout << "    setSampleGroup: " << grp.error() << "\n"; return false; }

        // Optional pacing: env PACE=<n> sleeps 1ms every n setChannelConfig sends.
        const char* pace_env = std::getenv("PACE");
        const int pace = pace_env ? std::atoi(pace_env) : 0;
        int sent = 0;
        auto paced = [&]() {
            if (pace > 0 && (++sent % pace) == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };

        for (uint32_t ch : allChannels) {          // clear LNC
            auto base = session.getChanInfo(ch);
            if (base.isError()) return false;
            auto& ci = base.value();
            ci.chan = ch;
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
            ci.ainpopts &= ~(cbAINP_LNC_MASK | cbAINP_RAWSTREAM);
            ci.lncrate = 0;
            if (auto r = session.setChannelConfig(ci); r.isError()) {
                std::cout << "    clearLNC ch" << ch << ": " << r.error() << "\n"; return false;
            }
            paced();
        }
        for (uint32_t ch : allChannels) {          // clear spike processing
            auto base = session.getChanInfo(ch);
            if (base.isError()) return false;
            auto& ci = base.value();
            ci.chan = ch;
            ci.cbpkt_header.type = cbPKTTYPE_CHANSETSPK;
            ci.spkopts &= ~SPIKE_PROCESSING_OPTION_MASK;
            if (auto r = session.setChannelConfig(ci); r.isError()) {
                std::cout << "    clearSpike ch" << ch << ": " << r.error() << "\n"; return false;
            }
            paced();
        }

        auto s = session.sync(3000);
        if (s.isError()) { std::cout << "    sync: " << s.error() << "\n"; return false; }
        return true;
    };

    // Orion's runConfigLoop retries for ~5 s.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    int attempt = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        ++attempt;
        std::cout << "  clear-config attempt " << attempt << ":\n";
        if (applyClearConfig()) {
            std::cout << "\nRESULT: clear-config + sync SUCCEEDED (attempt " << attempt << ").\n";
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nRESULT: clear-config timed out (reproduces Orion failure).\n";
    return 3;
}
