///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   central_client.cpp
/// @brief  Diagnostic tool for testing CENTRAL_COMPAT CLIENT mode
///
/// Attaches to Central's shared memory directly (bypassing SDK auto-detection)
/// and prints diagnostic info to verify struct layout compatibility.
///
/// Usage:
///   central_client [instance]
///   central_client           # Default: instance 0
///   central_client 1         # Instance 1 (for multi-instance setups)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cbproto/instrument_id.h"
#include <cbsdk/cbsdk.h>
#include <cbsdk/sdk_session.h>
#include <cbshm/shmem_session.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstring>

using namespace cbshm;

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

int main(int argc, char* argv[]) {
    int instance = 0;
    if (argc >= 2) instance = std::atoi(argv[1]);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "==============================================\n";
    std::cout << "  CereLink Central Client Diagnostic\n";
    std::cout << "==============================================\n\n";

    // For the CENTRAL layout, name_qualifier is the Central instance suffix
    // ("" for the primary instance, "1" for instance 1, etc.).
    std::string instance_suffix = (instance == 0) ? "" : std::to_string(instance);

    std::cout << "=== Attempting Central CLIENT mode (instance " << instance << ") ===\n";
    std::cout << std::endl;

    // Each ShmemSession targets a single instrument for its lifetime, so attach
    // a fresh session per instrument id we want to inspect.
    auto makeSession = [&](cbproto::InstrumentId id) {
        return ShmemSession::create(
            Mode::CLIENT, ShmemLayout::CENTRAL, instance_suffix, id);
    };

    auto result = makeSession(cbproto::InstrumentId::fromOneBased(cbNSP1));

    if (result.isError()) {
        std::cerr << "FAILED to attach to Central's shared memory: " << result.error() << "\n";
        std::cerr << "\nIs Central running?" << std::endl;
        return 1;
    }

    auto session = std::move(result.value());
    std::cout << "SUCCESS: Attached to Central's shared memory!" << std::endl;

    // Detect protocol version
    auto proto = session.getCompatProtocolVersion();
    std::cout << "\n=== Detected Protocol ===\n";
    std::cout << "  Protocol version: ";
    switch (proto) {
        case CBPROTO_PROTOCOL_311:     std::cout << "3.11\n"; break;
        case CBPROTO_PROTOCOL_400:     std::cout << "4.0\n"; break;
        case CBPROTO_PROTOCOL_410:     std::cout << "4.1\n"; break;
        case CBPROTO_PROTOCOL_CURRENT: std::cout << "CURRENT (4.2+)\n"; break;
        default:                       std::cout << "UNKNOWN\n"; break;
    }

    // Read procinfo for each instrument (one session per instrument)
    std::cout << "\n=== Processor Info ===\n";
    uint32_t max_procs = session.getMaxProcs();
    for (uint32_t i = 0; i < max_procs; ++i) {
        auto inst_result = makeSession(cbproto::InstrumentId::fromIndex(i));
        if (inst_result.isError()) {
            std::cerr << "ERROR: failed to attach for instrument " << i
                      << ": " << inst_result.error() << std::endl;
            return 1;
        }
        auto inst_session = std::move(inst_result.value());

        // Read config buffer
        auto cfg = std::make_unique<NativeConfigBuffer>();
        auto cfg_res = inst_session.getLegacyConfigBuffer(*cfg);
        if (cfg_res.isError()) {
            std::cerr << "ERROR: getLegacyConfigBuffer() returned error: " << cfg_res.error() << std::endl;
            return 1;
        }

        auto& proc = cfg->procinfo;
        std::cout << "  Proc: "
                  << " time=" << proc.cbpkt_header.time
                  << " chid=" << proc.cbpkt_header.chid
                  << " type=0x" << std::hex << proc.cbpkt_header.type << std::dec
                  << " dlen=" << proc.cbpkt_header.dlen
                  << " inst=" << (int)proc.cbpkt_header.instrument
                  << "\n";
    }

    // Read status buffer
    std::cout << "\n=== PC Status ===\n";
    auto num_total = session.getNumTotalChans();
    if (num_total.isOk()) {
        std::cout << "  Total channels:  " << num_total.value() << "\n";
    }
    auto nsp = session.getNspStatus();
    if (nsp.isOk()) {
        const char* status_str = "?";
        switch (nsp.value()) {
            case NativeNSPStatus::NSP_INIT:     status_str = "INIT"; break;
            case NativeNSPStatus::NSP_NOIPADDR: status_str = "NOIPADDR"; break;
            case NativeNSPStatus::NSP_NOREPLY:  status_str = "NOREPLY"; break;
            case NativeNSPStatus::NSP_FOUND:    status_str = "FOUND"; break;
            case NativeNSPStatus::NSP_INVALID:  status_str = "INVALID"; break;
        }
        std::cout << "  NSP status:  " << status_str << "\n";
    }
    auto gemini = session.isGeminiSystem();
    if (gemini.isOk()) {
        std::cout << "  Gemini system:   " << (gemini.value() ? "YES" : "NO") << "\n";
    }

    // Read some channel info
    std::cout << "\n=== Sample Channel Info ===\n";
    for (uint32_t ch = 0; ch < 5 && ch < cbMAXCHANS; ++ch) {
        auto ci = session.getChanInfo(ch);
        if (ci.isOk()) {
            auto& chan = ci.value();
            std::cout << "  Chan[" << std::setw(3) << ch << "]: "
                      << " chid=" << chan.cbpkt_header.chid
                      << " type=0x" << std::hex << chan.cbpkt_header.type << std::dec
                      << " dlen=" << chan.cbpkt_header.dlen
                      << " smpgroup=" << chan.smpgroup
                      << " label=\"" << chan.label << "\""
                      << "\n";
        }
    }

    // Now monitor receive buffer for packets
    std::cout << "\n=== Monitoring Receive Buffer ===\n";
    std::cout << "Waiting for packets (Ctrl+C to stop)...\n\n";

    uint64_t total_packets = 0;
    auto start = std::chrono::steady_clock::now();

    while (g_running) {
        auto wait_result = session.waitForData(500);

        cbPKT_GENERIC packets[64];
        size_t packets_read = 0;
        auto read_result = session.readReceiveBuffer(packets, 64, packets_read);

        if (read_result.isOk() && packets_read > 0) {
            total_packets += packets_read;

            // Print first packet details periodically
            if (total_packets <= 10 || total_packets % 10000 == 0) {
                auto& pkt = packets[0];
                std::cout << "[" << total_packets << "] "
                          << "time=" << pkt.cbpkt_header.time
                          << " chid=" << pkt.cbpkt_header.chid
                          << " type=0x" << std::hex << pkt.cbpkt_header.type << std::dec
                          << " dlen=" << pkt.cbpkt_header.dlen
                          << " inst=" << (int)pkt.cbpkt_header.instrument
                          << "\n";
            }
        }

        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if (elapsed > 0 && total_packets > 0) {
            std::cout << "\r  Packets: " << total_packets
                      << " (" << (total_packets / elapsed) << "/sec)"
                      << std::flush;
        }
    }

    std::cout << "\n\nTotal packets read: " << total_packets << "\n";
    std::cout << "Done.\n";
    return 0;
}
