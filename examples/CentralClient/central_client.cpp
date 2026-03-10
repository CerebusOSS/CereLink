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

#include <cbshm/shmem_session.h>
#include <cbshm/central_types.h>
#include <cbproto/cbproto.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstring>

using namespace cbshm;

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

static std::string makeName(const char* base, int instance) {
    if (instance == 0) return base;
    return std::string(base) + std::to_string(instance);
}

int main(int argc, char* argv[]) {
    int instance = 0;
    if (argc >= 2) instance = std::atoi(argv[1]);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "==============================================\n";
    std::cout << "  CereLink Central Client Diagnostic\n";
    std::cout << "==============================================\n\n";

    // Print struct sizes for comparison with Central
    std::cout << "=== Struct Size Verification ===\n";
    std::cout << "  sizeof(CentralLegacyCFGBUFF):   " << sizeof(CentralLegacyCFGBUFF) << "\n";
    std::cout << "  sizeof(CentralReceiveBuffer):    " << sizeof(CentralReceiveBuffer) << "\n";
    std::cout << "  sizeof(CentralTransmitBuffer):   " << sizeof(CentralTransmitBuffer) << "\n";
    std::cout << "  sizeof(CentralTransmitBufferLocal): " << sizeof(CentralTransmitBufferLocal) << "\n";
    std::cout << "  sizeof(CentralPCStatus):         " << sizeof(CentralPCStatus) << "\n";
    std::cout << "  sizeof(CentralSpikeBuffer):      " << sizeof(CentralSpikeBuffer) << "\n";
    std::cout << "  sizeof(CentralSpikeCache):       " << sizeof(CentralSpikeCache) << "\n";
    std::cout << "  sizeof(CentralAppWorkspace):     " << sizeof(CentralAppWorkspace) << "\n";
    std::cout << "\n";

    // Print key constants
    std::cout << "=== Key Constants ===\n";
    std::cout << "  CENTRAL_cbMAXPROCS:              " << CENTRAL_cbMAXPROCS << "\n";
    std::cout << "  CENTRAL_cbNUM_FE_CHANS:          " << CENTRAL_cbNUM_FE_CHANS << "\n";
    std::cout << "  CENTRAL_cbMAXCHANS:              " << CENTRAL_cbMAXCHANS << "\n";
    std::cout << "  CENTRAL_cbMAXBANKS:              " << CENTRAL_cbMAXBANKS << "\n";
    std::cout << "  CENTRAL_cbMAXNTRODES:            " << CENTRAL_cbMAXNTRODES << "\n";
    std::cout << "  CENTRAL_AOUT_NUM_GAIN_CHANS:     " << CENTRAL_AOUT_NUM_GAIN_CHANS << "\n";
    std::cout << "  CENTRAL_cbPKT_SPKCACHELINECNT:   " << CENTRAL_cbPKT_SPKCACHELINECNT << "\n";
    std::cout << "  CENTRAL_cbMAXAPPWORKSPACES:      " << CENTRAL_cbMAXAPPWORKSPACES << "\n";
    std::cout << "  sizeof(PROCTIME):                " << sizeof(PROCTIME) << "\n";
    std::cout << "\n";

    // Construct names for this instance
    std::string cfg_name    = makeName("cbCFGbuffer", instance);
    std::string rec_name    = makeName("cbRECbuffer", instance);
    std::string xmt_name    = makeName("XmtGlobal", instance);
    std::string xmtl_name   = makeName("XmtLocal", instance);
    std::string status_name = makeName("cbSTATUSbuffer", instance);
    std::string spk_name    = makeName("cbSPKbuffer", instance);
    std::string signal_name = makeName("cbSIGNALevent", instance);

    std::cout << "=== Attempting Central CLIENT mode (instance " << instance << ") ===\n";
    std::cout << "  Config:  " << cfg_name << "\n";
    std::cout << "  Receive: " << rec_name << "\n";
    std::cout << "  XmtGlob: " << xmt_name << "\n";
    std::cout << "  XmtLoc:  " << xmtl_name << "\n";
    std::cout << "  Status:  " << status_name << "\n";
    std::cout << "  Spike:   " << spk_name << "\n";
    std::cout << "  Signal:  " << signal_name << "\n\n";

    auto result = ShmemSession::create(
        cfg_name, rec_name, xmt_name, xmtl_name,
        status_name, spk_name, signal_name,
        Mode::CLIENT, ShmemLayout::CENTRAL_COMPAT);

    if (result.isError()) {
        std::cerr << "FAILED to attach to Central's shared memory: " << result.error() << "\n";
        std::cerr << "\nIs Central running?\n";
        return 1;
    }

    auto session = std::move(result.value());
    std::cout << "SUCCESS: Attached to Central's shared memory!\n\n";

    // Read config buffer
    auto* cfg = session.getLegacyConfigBuffer();
    if (!cfg) {
        std::cerr << "ERROR: getLegacyConfigBuffer() returned null\n";
        return 1;
    }

    std::cout << "=== Config Buffer Contents ===\n";
    std::cout << "  version:   " << cfg->version << "\n";
    std::cout << "  sysflags:  0x" << std::hex << cfg->sysflags << std::dec << "\n";

    // Read procinfo for each instrument
    std::cout << "\n=== Processor Info ===\n";
    for (uint32_t i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
        auto& proc = cfg->procinfo[i];
        // procinfo version field = (major << 16) | minor
        uint32_t ver = proc.cbpkt_header.type;  // Version is stored in a known field
        std::cout << "  Proc[" << i << "]:"
                  << " time=" << proc.cbpkt_header.time
                  << " chid=" << proc.cbpkt_header.chid
                  << " type=0x" << std::hex << proc.cbpkt_header.type << std::dec
                  << " dlen=" << proc.cbpkt_header.dlen
                  << " inst=" << (int)proc.cbpkt_header.instrument
                  << "\n";
    }

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

    // Read status buffer
    std::cout << "\n=== PC Status ===\n";
    auto num_total = session.getNumTotalChans();
    if (num_total.isOk()) {
        std::cout << "  Total channels:  " << num_total.value() << "\n";
    }
    for (uint32_t i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
        auto nsp = session.getNspStatus(cbproto::InstrumentId::fromIndex(i));
        if (nsp.isOk()) {
            const char* status_str = "?";
            switch (nsp.value()) {
                case NSPStatus::NSP_INIT:     status_str = "INIT"; break;
                case NSPStatus::NSP_NOIPADDR: status_str = "NOIPADDR"; break;
                case NSPStatus::NSP_NOREPLY:  status_str = "NOREPLY"; break;
                case NSPStatus::NSP_FOUND:    status_str = "FOUND"; break;
                case NSPStatus::NSP_INVALID:  status_str = "INVALID"; break;
            }
            std::cout << "  NSP[" << i << "] status:  " << status_str << "\n";
        }
    }
    auto gemini = session.isGeminiSystem();
    if (gemini.isOk()) {
        std::cout << "  Gemini system:   " << (gemini.value() ? "YES" : "NO") << "\n";
    }

    // Read some channel info
    std::cout << "\n=== Sample Channel Info ===\n";
    for (uint32_t ch = 0; ch < 5 && ch < CENTRAL_cbMAXCHANS; ++ch) {
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

    // Set instrument filter for Hub1 (index 0 in GEMSTART=2 mapping)
    session.setInstrumentFilter(0);

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
