///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Device transport layer implementation
///
/// Provides platform-specific UDP socket communication with Cerebus devices.
/// Includes macOS multi-interface routing fix (IP_BOUND_IF).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "cbdev/device_session.h"
#include <cbproto/cbproto.h>  // For cbPKT_GENERIC
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <iomanip>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    typedef int socklen_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #ifdef __APPLE__
        #include <net/if.h>
        #include <ifaddrs.h>
    #endif
    typedef int SOCKET;
    typedef struct sockaddr SOCKADDR;
    typedef struct sockaddr_in SOCKADDR_IN;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

namespace {
    // Platform-specific socket close function
    inline void closeSocket(SOCKET sock) {
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
    }
}

namespace cbdev {

///////////////////////////////////////////////////////////////////////////////////////////////////
// Platform-Specific Helpers
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __APPLE__
/// Helper function to find the interface index for a given IP address (macOS only)
/// This is used with IP_BOUND_IF to ensure packets are routed via the correct interface
static unsigned int GetInterfaceIndexForIP(const char* ip_address) {
    if (ip_address == nullptr || std::strlen(ip_address) == 0)
        return 0;

    struct ifaddrs *ifaddr;
    unsigned int if_index = 0;

    if (getifaddrs(&ifaddr) == -1)
        return 0;

    // Convert the IP address to compare
    struct in_addr target_addr;
    if (inet_pton(AF_INET, ip_address, &target_addr) != 1) {
        freeifaddrs(ifaddr);
        return 0;
    }

    // Walk through linked list to find matching interface
    for (const struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto *addr_in = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
            if (addr_in->sin_addr.s_addr == target_addr.s_addr) {
                if_index = if_nametoindex(ifa->ifa_name);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return if_index;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceConfig Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

DeviceConfig DeviceConfig::forDevice(DeviceType type) {
    DeviceConfig config;
    config.type = type;

    switch (type) {
        case DeviceType::NSP:
            // Legacy NSP: different ports for send/recv
            config.device_address = DeviceDefaults::NSP_ADDRESS;
            config.client_address = "";  // Auto-detect
            config.recv_port = DeviceDefaults::LEGACY_NSP_RECV_PORT;
            config.send_port = DeviceDefaults::LEGACY_NSP_SEND_PORT;
            break;

        case DeviceType::GEMINI_NSP:
            // Gemini NSP: same port for both send & recv
            config.device_address = DeviceDefaults::GEMINI_NSP_ADDRESS;
            config.client_address = "";  // Auto-detect
            config.recv_port = DeviceDefaults::GEMINI_NSP_PORT;
            config.send_port = DeviceDefaults::GEMINI_NSP_PORT;
            break;

        case DeviceType::HUB1:
            // Gemini Hub1: same port for both send & recv
            config.device_address = DeviceDefaults::GEMINI_HUB1_ADDRESS;
            config.client_address = "";  // Auto-detect
            config.recv_port = DeviceDefaults::GEMINI_HUB1_PORT;
            config.send_port = DeviceDefaults::GEMINI_HUB1_PORT;
            break;

        case DeviceType::HUB2:
            // Gemini Hub2: same port for both send & recv
            config.device_address = DeviceDefaults::GEMINI_HUB2_ADDRESS;
            config.client_address = "";  // Auto-detect
            config.recv_port = DeviceDefaults::GEMINI_HUB2_PORT;
            config.send_port = DeviceDefaults::GEMINI_HUB2_PORT;
            break;

        case DeviceType::HUB3:
            // Gemini Hub3: same port for both send & recv
            config.device_address = DeviceDefaults::GEMINI_HUB3_ADDRESS;
            config.client_address = "";  // Auto-detect
            config.recv_port = DeviceDefaults::GEMINI_HUB3_PORT;
            config.send_port = DeviceDefaults::GEMINI_HUB3_PORT;
            break;

        case DeviceType::NPLAY:
            // nPlayServer: loopback for both device and client
            config.device_address = DeviceDefaults::NPLAY_ADDRESS;
            config.client_address = "127.0.0.1";  // Always use loopback for NPLAY
            config.recv_port = DeviceDefaults::NPLAY_PORT;
            config.send_port = DeviceDefaults::NPLAY_PORT;
            break;

        case DeviceType::CUSTOM:
            // User must set addresses manually
            break;
    }

    return config;
}

DeviceConfig DeviceConfig::custom(const std::string& device_addr,
                                  const std::string& client_addr,
                                  uint16_t recv_port,
                                  uint16_t send_port) {
    DeviceConfig config;
    config.type = DeviceType::CUSTOM;
    config.device_address = device_addr;
    config.client_address = client_addr;
    config.recv_port = recv_port;
    config.send_port = send_port;
    return config;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceSession::Impl - Platform-Specific Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

struct DeviceSession::Impl {
    DeviceConfig config;
    SOCKET socket = INVALID_SOCKET_VALUE;
    SOCKADDR_IN recv_addr{};  // Address we bind to (client side)
    SOCKADDR_IN send_addr{};  // Address we send to (device side)

    // Receive thread
    std::unique_ptr<std::thread> recv_thread;
    std::atomic<bool> recv_thread_running{false};
    PacketCallback packet_callback;
    std::mutex callback_mutex;

    // Send thread
    std::unique_ptr<std::thread> send_thread;
    std::atomic<bool> send_thread_running{false};
    std::atomic<bool> send_thread_waiting{false};
    TransmitCallback transmit_callback;
    std::mutex transmit_mutex;
    std::condition_variable transmit_cv;

    // Statistics
    DeviceStats stats;
    std::mutex stats_mutex;

    // Device handshake state (for performStartupHandshake() and connect() methods)
    std::atomic<uint32_t> device_runlevel{0};      // Current runlevel from SYSREP
    std::atomic<bool> received_sysrep{false};       // Have we received any SYSREP?
    std::mutex handshake_mutex;
    std::condition_variable handshake_cv;

    // Configuration buffer (internal or external)
    cbConfigBuffer* m_cfg_ptr = nullptr;              // Points to internal or external buffer
    std::unique_ptr<cbConfigBuffer> m_cfg_owned;      // Internal storage (standalone mode)
    std::mutex cfg_mutex;                             // Thread-safe access to config buffer

    ~Impl() {
        // Ensure threads are stopped before destroying
        if (recv_thread_running.load()) {
            recv_thread_running.store(false);
            if (recv_thread && recv_thread->joinable()) {
                recv_thread->join();
            }
        }

        if (send_thread_running.load()) {
            send_thread_running.store(false);
            transmit_cv.notify_one();
            if (send_thread && send_thread->joinable()) {
                send_thread->join();
            }
        }

        if (socket != INVALID_SOCKET_VALUE) {
            closeSocket(socket);
        }

#ifdef _WIN32
        WSACleanup();
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceSession::parseConfigPacket(const cbPKT_GENERIC& pkt) {
    // Early exit if no config buffer is set
    if (!m_impl->m_cfg_ptr) {
        return;
    }

    // Lock config buffer for thread-safe access
    std::lock_guard<std::mutex> lock(m_impl->cfg_mutex);

    // Helper lambda to check if packet needs config buffer storage (and thus instrument ID validation)
    // Based on Central's InstNetwork.cpp logic - only packets actually stored to config buffer need valid instrument IDs
    auto isConfigPacket = [](const cbPKT_HEADER& header) -> bool {
        // Config packets are sent on the configuration channel
        if (header.chid != cbPKTCHAN_CONFIGURATION) {
            return false;
        }

        uint16_t type = header.type;

        // Channel config packets (0x40-0x4F range)
        if ((type & 0xF0) == cbPKTTYPE_CHANREP) return true;

        // System config packets (0x10-0x1F range)
        if ((type & 0xF0) == cbPKTTYPE_SYSREP) return true;

        // Other specific config packet types that Central stores
        switch (type) {
            case cbPKTTYPE_GROUPREP:
            case cbPKTTYPE_FILTREP:
            case cbPKTTYPE_PROCREP:
            case cbPKTTYPE_BANKREP:
            case cbPKTTYPE_ADAPTFILTREP:
            case cbPKTTYPE_REFELECFILTREP:
            case cbPKTTYPE_SS_MODELREP:
            case cbPKTTYPE_SS_STATUSREP:
            case cbPKTTYPE_SS_DETECTREP:
            case cbPKTTYPE_SS_ARTIF_REJECTREP:
            case cbPKTTYPE_SS_NOISE_BOUNDARYREP:
            case cbPKTTYPE_SS_STATISTICSREP:
            case cbPKTTYPE_FS_BASISREP:
            case cbPKTTYPE_LNCREP:
            case cbPKTTYPE_REPFILECFG:
            case cbPKTTYPE_REPNTRODEINFO:
            case cbPKTTYPE_NMREP:
            case cbPKTTYPE_WAVEFORMREP:
            case cbPKTTYPE_NPLAYREP:
                return true;
            default:
                return false;
        }
    };

    // Only validate instrument ID for config packets that need to be stored
    if (isConfigPacket(pkt.cbpkt_header)) {
        const uint16_t pkt_type = pkt.cbpkt_header.type;
        // Extract instrument ID from packet header
        const cbproto::InstrumentId id = cbproto::InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);

        if (!id.isValid()) {
            // Invalid instrument ID for config packet - skip storing to config buffer
            return;
        }

        // Use packet.instrument as index (mode-independent!)
        const uint8_t idx = id.toIndex();

        if ((pkt_type & 0xF0) == cbPKTTYPE_CHANREP) {
            // Channel info packets (0x40-0x4F range)
            const auto* chan_pkt = reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
            // Channel index is 1-based in packet, but chaninfo array is 0-based
            if (chan_pkt->chan > 0 && chan_pkt->chan <= cbCONFIG_MAXCHANS) {
                std::memcpy(&m_impl->m_cfg_ptr->chaninfo[chan_pkt->chan - 1], &pkt, sizeof(cbPKT_CHANINFO));
            }
        }
        else if ((pkt_type & 0xF0) == cbPKTTYPE_SYSREP) {
            // System info packets (0x10-0x1F range) - all store to same sysinfo
            std::memcpy(&m_impl->m_cfg_ptr->sysinfo, &pkt, sizeof(cbPKT_SYSINFO));
        }
        else if (pkt_type == cbPKTTYPE_GROUPREP) {
            // Store sample group info (group index is 1-based in packet)
            const auto* group_pkt = reinterpret_cast<const cbPKT_GROUPINFO*>(&pkt);
            if (group_pkt->group > 0 && group_pkt->group <= cbCONFIG_MAXGROUPS) {
                std::memcpy(&m_impl->m_cfg_ptr->groupinfo[idx][group_pkt->group - 1], &pkt, sizeof(cbPKT_GROUPINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_FILTREP) {
            // Store filter info (filter index is 1-based in packet)
            const auto* filt_pkt = reinterpret_cast<const cbPKT_FILTINFO*>(&pkt);
            if (filt_pkt->filt > 0 && filt_pkt->filt <= cbCONFIG_MAXFILTS) {
                std::memcpy(&m_impl->m_cfg_ptr->filtinfo[idx][filt_pkt->filt - 1], &pkt, sizeof(cbPKT_FILTINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_PROCREP) {
            // Store processor info
            std::memcpy(&m_impl->m_cfg_ptr->procinfo[idx], &pkt, sizeof(cbPKT_PROCINFO));

            // Mark instrument as active when we receive its PROCINFO
            m_impl->m_cfg_ptr->instrument_status[idx] = static_cast<uint32_t>(InstrumentStatus::ACTIVE);
        }
        else if (pkt_type == cbPKTTYPE_BANKREP) {
            // Store bank info (bank index is 1-based in packet)
            const auto* bank_pkt = reinterpret_cast<const cbPKT_BANKINFO*>(&pkt);
            if (bank_pkt->bank > 0 && bank_pkt->bank <= cbCONFIG_MAXBANKS) {
                std::memcpy(&m_impl->m_cfg_ptr->bankinfo[idx][bank_pkt->bank - 1], &pkt, sizeof(cbPKT_BANKINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_ADAPTFILTREP) {
            // Store adaptive filter info (per-instrument)
            m_impl->m_cfg_ptr->adaptinfo[idx] = *reinterpret_cast<const cbPKT_ADAPTFILTINFO*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_REFELECFILTREP) {
            // Store reference electrode filter info (per-instrument)
            m_impl->m_cfg_ptr->refelecinfo[idx] = *reinterpret_cast<const cbPKT_REFELECFILTINFO*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_STATUSREP) {
            // Store spike sorting status (system-wide, in isSortingOptions)
            m_impl->m_cfg_ptr->isSortingOptions.pktStatus = *reinterpret_cast<const cbPKT_SS_STATUS*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_DETECTREP) {
            // Store spike detection parameters (system-wide)
            m_impl->m_cfg_ptr->isSortingOptions.pktDetect = *reinterpret_cast<const cbPKT_SS_DETECT*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_ARTIF_REJECTREP) {
            // Store artifact rejection parameters (system-wide)
            m_impl->m_cfg_ptr->isSortingOptions.pktArtifReject = *reinterpret_cast<const cbPKT_SS_ARTIF_REJECT*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_NOISE_BOUNDARYREP) {
            // Store noise boundary (per-channel, 1-based in packet)
            const auto* noise_pkt = reinterpret_cast<const cbPKT_SS_NOISE_BOUNDARY*>(&pkt);
            if (noise_pkt->chan > 0 && noise_pkt->chan <= cbCONFIG_MAXCHANS) {
                m_impl->m_cfg_ptr->isSortingOptions.pktNoiseBoundary[noise_pkt->chan - 1] = *noise_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_SS_STATISTICSREP) {
            // Store spike sorting statistics (system-wide)
            m_impl->m_cfg_ptr->isSortingOptions.pktStatistics = *reinterpret_cast<const cbPKT_SS_STATISTICS*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_MODELREP) {
            // Store spike sorting model (per-channel, per-unit)
            // Note: Central calls UpdateSortModel() which validates and constrains unit numbers
            // For now, store directly with validation
            const auto* model_pkt = reinterpret_cast<const cbPKT_SS_MODELSET*>(&pkt);
            uint32_t nChan = model_pkt->chan;
            uint32_t nUnit = model_pkt->unit_number;

            // Validate channel and unit numbers (0-based in packet)
            if (nChan < cbCONFIG_MAXCHANS && nUnit < (cbMAXUNITS + 2)) {
                m_impl->m_cfg_ptr->isSortingOptions.asSortModel[nChan][nUnit] = *model_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_FS_BASISREP) {
            // Store feature space basis (per-channel)
            // Note: Central calls UpdateBasisModel() for additional processing
            // For now, store directly with validation
            const auto* basis_pkt = reinterpret_cast<const cbPKT_FS_BASIS*>(&pkt);

            // Validate channel number (1-based in packet)
            if (const uint32_t nChan = basis_pkt->chan; nChan > 0 && nChan <= cbCONFIG_MAXCHANS) {
                m_impl->m_cfg_ptr->isSortingOptions.asBasis[nChan - 1] = *basis_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_LNCREP) {
            // Store line noise cancellation info (per-instrument)
            std::memcpy(&m_impl->m_cfg_ptr->isLnc[idx], &pkt, sizeof(cbPKT_LNC));
        }
        else if (pkt_type == cbPKTTYPE_REPFILECFG) {
            // Store file configuration info (only for specific options)
            const auto* file_pkt = reinterpret_cast<const cbPKT_FILECFG*>(&pkt);
            if (file_pkt->options == cbFILECFG_OPT_REC ||
                file_pkt->options == cbFILECFG_OPT_STOP ||
                file_pkt->options == cbFILECFG_OPT_TIMEOUT) {
                m_impl->m_cfg_ptr->fileinfo = *file_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_REPNTRODEINFO) {
            // Store n-trode information (1-based in packet)
            const auto* ntrode_pkt = reinterpret_cast<const cbPKT_NTRODEINFO*>(&pkt);
            if (ntrode_pkt->ntrode > 0 && ntrode_pkt->ntrode <= cbMAXNTRODES) {
                m_impl->m_cfg_ptr->isNTrodeInfo[ntrode_pkt->ntrode - 1] = *ntrode_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_WAVEFORMREP) {
            // Store analog output waveform configuration
            // Based on Central's logic (InstNetwork.cpp:415)
            const auto* wave_pkt = reinterpret_cast<const cbPKT_AOUT_WAVEFORM*>(&pkt);

            // Validate channel number (0-based) and trigger number (0-based)
            if (wave_pkt->chan < AOUT_NUM_GAIN_CHANS && wave_pkt->trigNum < cbMAX_AOUT_TRIGGER) {
                m_impl->m_cfg_ptr->isWaveform[wave_pkt->chan][wave_pkt->trigNum] = *wave_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_NPLAYREP) {
            // Store nPlay information
            m_impl->m_cfg_ptr->isNPlay = *reinterpret_cast<const cbPKT_NPLAY*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_NMREP) {
            // Store NeuroMotive (video/tracking) information
            // Based on Central's logic (InstNetwork.cpp:367-397)
            const auto* nm_pkt = reinterpret_cast<const cbPKT_NM*>(&pkt);

            if (nm_pkt->mode == cbNM_MODE_SETVIDEOSOURCE) {
                // Video source configuration (1-based index in flags field)
                if (nm_pkt->flags > 0 && nm_pkt->flags <= cbMAXVIDEOSOURCE) {
                    std::memcpy(m_impl->m_cfg_ptr->isVideoSource[nm_pkt->flags - 1].name,
                                nm_pkt->name, cbLEN_STR_LABEL);
                    m_impl->m_cfg_ptr->isVideoSource[nm_pkt->flags - 1].fps =
                        static_cast<float>(nm_pkt->value) / 1000.0f;
                }
            }
            else if (nm_pkt->mode == cbNM_MODE_SETTRACKABLE) {
                // Trackable object configuration (1-based index in flags field)
                if (nm_pkt->flags > 0 && nm_pkt->flags <= cbMAXTRACKOBJ) {
                    std::memcpy(m_impl->m_cfg_ptr->isTrackObj[nm_pkt->flags - 1].name,
                                nm_pkt->name, cbLEN_STR_LABEL);
                    m_impl->m_cfg_ptr->isTrackObj[nm_pkt->flags - 1].type =
                        static_cast<uint16_t>(nm_pkt->value & 0xff);
                    m_impl->m_cfg_ptr->isTrackObj[nm_pkt->flags - 1].pointCount =
                        static_cast<uint16_t>((nm_pkt->value >> 16) & 0xff);
                }
            }
            // Note: cbNM_MODE_SETRPOS does not exist in upstream cbproto.h
            // If reset functionality is needed, it should be implemented using a different mode
            /*
            else if (nm_pkt->mode == cbNM_MODE_SETRPOS) {
                // Clear all trackable objects
                std::memset(m_impl->m_cfg_ptr->isTrackObj, 0, sizeof(m_impl->m_cfg_ptr->isTrackObj));
                std::memset(m_impl->m_cfg_ptr->isVideoSource, 0, sizeof(m_impl->m_cfg_ptr->isVideoSource));
            }
            */
        }

        // All recognized config packet types now have storage
    }
}

DeviceSession::DeviceSession()
    : m_impl(std::make_unique<Impl>()) {
}

DeviceSession::DeviceSession(DeviceSession&&) noexcept = default;
DeviceSession& DeviceSession::operator=(DeviceSession&&) noexcept = default;

DeviceSession::~DeviceSession() {
    close();
}

Result<DeviceSession> DeviceSession::create(const DeviceConfig& config) {
    DeviceSession session;
    session.m_impl->config = config;

    // Auto-detect client address if not specified
    if (session.m_impl->config.client_address.empty()) {
        if (session.m_impl->config.type == DeviceType::NPLAY) {
            // NPLAY: Always use loopback
            session.m_impl->config.client_address = "127.0.0.1";
        } else {
            // Other devices: Use platform-specific detection
            session.m_impl->config.client_address = detectLocalIP();
        }
    }

#ifdef _WIN32
    // Initialize Winsock 2.2
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return Result<DeviceSession>::error("Failed to initialize Winsock");
    }
#endif

    // Create UDP socket
#ifdef _WIN32
    session.m_impl->socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
#else
    session.m_impl->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif

    if (session.m_impl->socket == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<DeviceSession>::error("Failed to create socket");
    }

    // Set socket options
    int opt_one = 1;

    if (config.broadcast) {
        if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_BROADCAST,
                      (char*)&opt_one, sizeof(opt_one)) != 0) {
            closeSocket(session.m_impl->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return Result<DeviceSession>::error("Failed to set SO_BROADCAST");
        }
    }

    // Set SO_REUSEADDR to allow multiple binds to same port
    if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_REUSEADDR,
                  (char*)&opt_one, sizeof(opt_one)) != 0) {
        closeSocket(session.m_impl->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<DeviceSession>::error("Failed to set SO_REUSEADDR");
    }

    // Set receive buffer size
    if (config.recv_buffer_size > 0) {
        int buffer_size = config.recv_buffer_size;
        if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVBUF,
                      (char*)&buffer_size, sizeof(buffer_size)) != 0) {
            closeSocket(session.m_impl->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return Result<DeviceSession>::error("Failed to set receive buffer size");
        }

        // Verify buffer size was set
        socklen_t opt_len = sizeof(int);
        if (getsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVBUF,
                      (char*)&buffer_size, &opt_len) != 0) {
            closeSocket(session.m_impl->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return Result<DeviceSession>::error("Failed to verify receive buffer size");
        }

#ifdef __linux__
        // Linux returns double the requested size
        buffer_size /= 2;
#endif

        if (buffer_size < config.recv_buffer_size) {
            closeSocket(session.m_impl->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return Result<DeviceSession>::error(
                "Receive buffer size too small (got " + std::to_string(buffer_size) +
                ", requested " + std::to_string(config.recv_buffer_size) + ")");
        }
    }

    // Set receive timeout (1 second) so recv() doesn't block forever
    // This allows the receive thread to check the running flag periodically
#ifdef _WIN32
    DWORD timeout_ms = 1000;
    if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVTIMEO,
                  (char*)&timeout_ms, sizeof(timeout_ms)) != 0) {
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVTIMEO,
                  (char*)&tv, sizeof(tv)) != 0) {
#endif
        closeSocket(session.m_impl->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<DeviceSession>::error("Failed to set receive timeout");
    }

    // Set non-blocking mode
    if (config.non_blocking) {
#ifdef _WIN32
        u_long arg_val = 1;
        if (ioctlsocket(session.m_impl->socket, FIONBIO, &arg_val) == SOCKET_ERROR_VALUE) {
#else
        if (fcntl(session.m_impl->socket, F_SETFL, O_NONBLOCK) != 0) {
#endif
            closeSocket(session.m_impl->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return Result<DeviceSession>::error("Failed to set non-blocking mode");
        }
    }

    // Bind to client address and receive port
    std::memset(&session.m_impl->recv_addr, 0, sizeof(session.m_impl->recv_addr));
    session.m_impl->recv_addr.sin_family = AF_INET;
    session.m_impl->recv_addr.sin_port = htons(config.recv_port);

    if (config.client_address == "0.0.0.0" || config.client_address.empty()) {
        session.m_impl->recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        session.m_impl->recv_addr.sin_addr.s_addr = inet_addr(config.client_address.c_str());
    }

#ifdef __APPLE__
    session.m_impl->recv_addr.sin_len = sizeof(session.m_impl->recv_addr);
#endif

    if (bind(session.m_impl->socket, reinterpret_cast<SOCKADDR *>(&session.m_impl->recv_addr),
            sizeof(session.m_impl->recv_addr)) != 0) {
        const int bind_error = errno;  // Capture errno immediately
        closeSocket(session.m_impl->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<DeviceSession>::error("Failed to bind socket to " +
            config.client_address + ":" + std::to_string(config.recv_port) +
            " (errno: " + std::to_string(bind_error) + " - " + std::strerror(bind_error) + ")");
    }

    // Set up send address (device side)
    std::memset(&session.m_impl->send_addr, 0, sizeof(session.m_impl->send_addr));
    session.m_impl->send_addr.sin_family = AF_INET;
    session.m_impl->send_addr.sin_port = htons(config.send_port);
    session.m_impl->send_addr.sin_addr.s_addr = inet_addr(config.device_address.c_str());

#ifdef __APPLE__
    session.m_impl->send_addr.sin_len = sizeof(session.m_impl->send_addr);

    // macOS multi-interface routing fix: Bind socket to specific interface if using
    // a specific IP address (not INADDR_ANY)
    if (!config.client_address.empty() &&
        config.client_address != "0.0.0.0" &&
        session.m_impl->recv_addr.sin_addr.s_addr != htonl(INADDR_ANY)) {

        const unsigned int if_index = GetInterfaceIndexForIP(config.client_address.c_str());
        if (if_index > 0) {
            // Best effort - don't fail if this doesn't work
            setsockopt(session.m_impl->socket, IPPROTO_IP, IP_BOUND_IF,
                      &if_index, sizeof(if_index));
        }
    }
#endif

    // Initialize internal config buffer (standalone mode)
    // If using cbsdk, this will be replaced with external buffer via setConfigBuffer()
    session.setConfigBuffer(nullptr);

    return Result<DeviceSession>::ok(std::move(session));
}

void DeviceSession::close() {
    if (!m_impl) return;

    // Stop both threads
    stopReceiveThread();
    stopSendThread();

    // Close socket
    if (m_impl->socket != INVALID_SOCKET_VALUE) {
        closeSocket(m_impl->socket);
        m_impl->socket = INVALID_SOCKET_VALUE;
    }
}

bool DeviceSession::isOpen() const {
    return m_impl && m_impl->socket != INVALID_SOCKET_VALUE;
}

///--------------------------------------------------------------------------------------------
/// Packet Operations
///--------------------------------------------------------------------------------------------

Result<void> DeviceSession::sendPacket(const cbPKT_GENERIC& pkt) {
    if (!isOpen()) {
        return Result<void>::error("Session is not open");
    }

    // Calculate actual packet size from header
    // dlen is in quadlets (4-byte units), so packet size = header + (dlen * 4)
    const size_t packet_size = cbPKT_HEADER_SIZE + (pkt.cbpkt_header.dlen * 4);

    const int bytes_sent = sendto(
        m_impl->socket,
        (const char*)&pkt,
        packet_size,
        0,
        reinterpret_cast<SOCKADDR *>(&m_impl->send_addr),
        sizeof(m_impl->send_addr)
    );

    if (bytes_sent == SOCKET_ERROR_VALUE) {
        std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
        m_impl->stats.send_errors++;
#ifdef _WIN32
        int err = WSAGetLastError();
        return Result<void>::error("Send failed with error: " + std::to_string(err));
#else
        return Result<void>::error("Send failed with error: " + std::string(strerror(errno)));
#endif
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
        m_impl->stats.packets_sent++;
        m_impl->stats.bytes_sent += bytes_sent;
    }

    return Result<void>::ok();
}

Result<void> DeviceSession::sendPackets(const cbPKT_GENERIC* pkts, const size_t count) {
    if (!pkts || count == 0) {
        return Result<void>::error("Invalid packet array");
    }

    // Send each packet individually
    for (size_t i = 0; i < count; ++i) {
        if (auto result = sendPacket(pkts[i]); result.isError()) {
            return result;
        }
    }

    return Result<void>::ok();
}

///--------------------------------------------------------------------------------------------
/// Callback-Based Receive
///--------------------------------------------------------------------------------------------

void DeviceSession::setPacketCallback(PacketCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
    m_impl->packet_callback = std::move(callback);
}

Result<void> DeviceSession::startReceiveThread() {
    if (!isOpen()) {
        return Result<void>::error("Session is not open");
    }

    if (m_impl->recv_thread_running.load()) {
        return Result<void>::error("Receive thread is already running");
    }

    m_impl->recv_thread_running.store(true);
    m_impl->recv_thread = std::make_unique<std::thread>([this]() {
        // UDP datagrams can contain MULTIPLE aggregated packets (up to 58080 bytes)
        // Receive into large buffer and parse out all cbPKT_GENERIC packets
        // Allocate on heap to avoid stack overflow
        constexpr size_t UDP_BUFFER_SIZE = 58080;  // cbCER_UDP_SIZE_MAX
        auto udp_buffer = std::make_unique<uint8_t[]>(UDP_BUFFER_SIZE);

        constexpr size_t MAX_BATCH = 512;
        auto packets = std::make_unique<cbPKT_GENERIC[]>(MAX_BATCH);

        while (m_impl->recv_thread_running.load()) {
            // Receive UDP datagram (may contain multiple packets)
            int bytes_recv = recv(m_impl->socket, (char*)udp_buffer.get(), UDP_BUFFER_SIZE, 0);

            if (bytes_recv == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    // No data available - yield and continue
                    std::this_thread::yield();
                    continue;
                }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available - yield and continue
                    std::this_thread::yield();
                    continue;
                }
#endif
                // Real error - log and continue
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.recv_errors++;
                continue;
            }

            if (bytes_recv == 0) {
                // Socket closed
                continue;
            }

            // Parse all packets from the UDP datagram
            // Wire format: variable-sized packets with size = sizeof(cbPKT_HEADER) + (dlen * 4)
            // We expand each to cbPKT_GENERIC (1024 bytes zero-padded) for callbacks
            size_t count = 0;
            size_t offset = 0;
            constexpr size_t HEADER_SIZE = sizeof(cbPKT_HEADER);
            constexpr size_t MAX_PKT_SIZE = sizeof(cbPKT_GENERIC);  // 1024 bytes

            while (offset + HEADER_SIZE <= static_cast<size_t>(bytes_recv) && count < MAX_BATCH) {
                // Peek at header to get packet size
                const auto* hdr = reinterpret_cast<cbPKT_HEADER*>(udp_buffer.get() + offset);

                // Calculate actual packet size on wire: header + (dlen * 4 bytes)
                const size_t wire_pkt_size = HEADER_SIZE + (hdr->dlen * 4);

                // Validate packet size
                constexpr size_t MAX_DLEN = (MAX_PKT_SIZE - HEADER_SIZE) / 4;
                if (hdr->dlen > MAX_DLEN || wire_pkt_size > MAX_PKT_SIZE) {
                    // Invalid packet - stop parsing this datagram
                    break;
                }

                // Check if full packet is available in datagram
                if (offset + wire_pkt_size > static_cast<size_t>(bytes_recv)) {
                    // Truncated datagram - stop parsing
                    break;
                }

                // Zero out the cbPKT_GENERIC buffer
                std::memset(&packets[count], 0, MAX_PKT_SIZE);

                // Copy wire packet (variable size) into cbPKT_GENERIC (zero-padded)
                std::memcpy(&packets[count], udp_buffer.get() + offset, wire_pkt_size);
                count++;
                offset += wire_pkt_size;
            }

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
                m_impl->stats.packets_received += count;
                m_impl->stats.bytes_received += bytes_recv;
            }

            // Monitor for SYSREP packets (for handshake) BEFORE delivering to user callback
            // SYSREP packets have type 0x10-0x1F (cbPKTTYPE_SYSREP base is 0x10)
            for (size_t i = 0; i < count; ++i) {
                const auto& pkt = packets[i];
                if ((pkt.cbpkt_header.type & 0xF0) == cbPKTTYPE_SYSREP) {
                    const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(&pkt);
                    m_impl->device_runlevel.store(sysinfo->runlevel, std::memory_order_release);
                    m_impl->received_sysrep.store(true, std::memory_order_release);
                    m_impl->handshake_cv.notify_all();
                }

                // Parse config packets and update config buffer
                parseConfigPacket(pkt);
            }

            // Deliver packets if we received any
            if (count > 0) {
                std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
                if (m_impl->packet_callback) {
                    m_impl->packet_callback(packets.get(), count);
                }
            }
        }
    });

    return Result<void>::ok();
}

void DeviceSession::stopReceiveThread() const {
    if (!m_impl->recv_thread_running.load()) {
        return;
    }

    m_impl->recv_thread_running.store(false);

    if (m_impl->recv_thread && m_impl->recv_thread->joinable()) {
        m_impl->recv_thread->join();
    }

    m_impl->recv_thread.reset();
}

bool DeviceSession::isReceiveThreadRunning() const {
    return m_impl && m_impl->recv_thread_running.load();
}

///--------------------------------------------------------------------------------------------
/// Send Thread (for transmit queue)
///--------------------------------------------------------------------------------------------

void DeviceSession::setTransmitCallback(TransmitCallback callback) const {
    std::lock_guard<std::mutex> lock(m_impl->transmit_mutex);
    m_impl->transmit_callback = std::move(callback);
}

Result<void> DeviceSession::startSendThread() const {
    if (!isOpen()) {
        return Result<void>::error("Session is not open");
    }

    if (m_impl->send_thread_running.load()) {
        return Result<void>::error("Send thread is already running");
    }

    m_impl->send_thread_running.store(true);
    m_impl->send_thread = std::make_unique<std::thread>([this]() {
        while (m_impl->send_thread_running.load()) {
            bool has_packets = false;

            // Try to dequeue and send all available packets
            {
                std::lock_guard<std::mutex> lock(m_impl->transmit_mutex);
                if (m_impl->transmit_callback) {
                    while (true) {
                        cbPKT_GENERIC pkt = {};

                        if (!m_impl->transmit_callback(pkt)) {
                            break;  // No more packets
                        }
                        has_packets = true;

                        // Calculate actual packet size from header
                        // Packet size in bytes = sizeof(header) + (dlen * 4)
                        // With 64-bit PROCTIME, header is 16 bytes (4 dwords)
                        const size_t packet_size = sizeof(cbPKT_HEADER) + (pkt.cbpkt_header.dlen * 4);

                        // Send the packet (only actual size, not full cbPKT_GENERIC)
                        const int bytes_sent = sendto(
                            m_impl->socket,
                            (const char*)&pkt,
                            packet_size,
                            0,
                            reinterpret_cast<SOCKADDR *>(&m_impl->send_addr),
                            sizeof(m_impl->send_addr)
                        );

                        if (bytes_sent == SOCKET_ERROR_VALUE) {
                            std::lock_guard<std::mutex> stats_lock(m_impl->stats_mutex);
                            m_impl->stats.send_errors++;
                        } else {
                            std::lock_guard<std::mutex> stats_lock(m_impl->stats_mutex);
                            m_impl->stats.packets_sent++;
                            m_impl->stats.bytes_sent += bytes_sent;
                        }
                    }
                }
            }

            if (has_packets) {
                // Had packets - mark not waiting and check again quickly
                m_impl->send_thread_waiting.store(false, std::memory_order_relaxed);
                std::this_thread::yield();
            } else {
                // No packets - wait for notification or timeout
                m_impl->send_thread_waiting.store(true, std::memory_order_release);

                std::unique_lock<std::mutex> lock(m_impl->transmit_mutex);
                m_impl->transmit_cv.wait_for(lock, std::chrono::milliseconds(10),
                    [this] { return !m_impl->send_thread_running.load(); });
            }
        }
    });

    return Result<void>::ok();
}

void DeviceSession::stopSendThread() const {
    if (!m_impl->send_thread_running.load()) {
        return;
    }

    m_impl->send_thread_running.store(false);
    m_impl->transmit_cv.notify_one();

    if (m_impl->send_thread && m_impl->send_thread->joinable()) {
        m_impl->send_thread->join();
    }

    m_impl->send_thread.reset();
}

bool DeviceSession::isSendThreadRunning() const {
    return m_impl && m_impl->send_thread_running.load();
}

///--------------------------------------------------------------------------------------------
/// Statistics & Monitoring
///--------------------------------------------------------------------------------------------

DeviceStats DeviceSession::getStats() const {
    std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
    return m_impl->stats;
}

void DeviceSession::resetStats() {
    std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
    m_impl->stats.reset();
}

const DeviceConfig& DeviceSession::getConfig() const {
    return m_impl->config;
}

void DeviceSession::setAutorun(const bool autorun) {
    m_impl->config.autorun = autorun;
}

///--------------------------------------------------------------------------------------------
/// Device Startup & Handshake
///--------------------------------------------------------------------------------------------

Result<void> DeviceSession::setSystemRunLevel(const uint32_t runlevel, const uint32_t resetque, const uint32_t runflags) {
    // Create runlevel command packet
    cbPKT_SYSINFO sysinfo = {};

    // Fill header
    sysinfo.cbpkt_header.time = 1;
    sysinfo.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    sysinfo.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;  // Use macro (accounts for 64-bit PROCTIME)
    sysinfo.cbpkt_header.instrument = 0;

    // Fill payload
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = runflags;

    // Cast to generic packet and send
    cbPKT_GENERIC pkt;
    std::memcpy(&pkt, &sysinfo, sizeof(sysinfo));

    return sendPacket(pkt);
}

Result<void> DeviceSession::requestConfiguration() {
    // Create REQCONFIGALL packet
    cbPKT_GENERIC pkt = {};

    // Fill header
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_REQCONFIGALL;
    pkt.cbpkt_header.dlen = 0;  // No payload
    pkt.cbpkt_header.instrument = 0;

    return sendPacket(pkt);
}

Result<void> DeviceSession::performStartupHandshake(const uint32_t timeout_ms) {
    // Complete device startup sequence to transition device from any state to RUNNING
    //
    // Sequence:
    // 1. Quick device presence check (100ms timeout) - fail fast if device not on network
    // 2. Send cbRUNLEVEL_RUNNING - check if device is already running
    // 3. If not running, send cbRUNLEVEL_HARDRESET - wait for STANDBY
    // 4. Send REQCONFIGALL - wait for config flood ending with SYSREP
    // 5. Send cbRUNLEVEL_RESET - wait for device to transition to RUNNING

    // Reset handshake state
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    m_impl->device_runlevel.store(0, std::memory_order_relaxed);

    // Quick presence check - use shorter timeout to fail fast for non-existent devices
    const uint32_t presence_check_timeout = std::min(100u, timeout_ms);

    // Helper lambda to wait for SYSREP with optional expected runlevel
    // If expected_runlevel is provided, waits for that specific runlevel
    // If not provided (0), waits for any SYSREP
    auto waitForSysrep = [this](const uint32_t timeout_ms_, uint32_t expected_runlevel = 0) -> bool {
        std::unique_lock<std::mutex> lock(m_impl->handshake_mutex);
        return m_impl->handshake_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms_),
            [this, expected_runlevel] {
                if (!(m_impl->received_sysrep.load(std::memory_order_acquire))) {
                    return false;  // Haven't received SYSREP yet
                }
                if (expected_runlevel == 0) {
                    return true;  // Any SYSREP is acceptable
                }
                // Check if we got the expected runlevel
                const uint32_t current = m_impl->device_runlevel.load(std::memory_order_acquire);
                return current == expected_runlevel;
            });
    };

    // Step 1: Quick presence check - send cbRUNLEVEL_RUNNING with short timeout
    Result<void> result = setSystemRunLevel(cbRUNLEVEL_RUNNING);
    if (result.isError()) {
        return Result<void>::error("Failed to send RUNNING command: " + result.error());
    }

    // Wait for SYSREP response with short timeout - fail fast if device not reachable
    if (!waitForSysrep(presence_check_timeout)) {
        // No response - device not on network
        return Result<void>::error("Device not reachable (no response to initial probe - check network connection and IP address)");
    }

    // Step 2: Got response - check if device is already running
    if (m_impl->device_runlevel.load(std::memory_order_acquire) == cbRUNLEVEL_RUNNING) {
        // Device is already running - request config and we're done
        goto request_config;
    }

    // Step 3: Device responded but not running - send HARDRESET
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    result = setSystemRunLevel(cbRUNLEVEL_HARDRESET);
    if (result.isError()) {
        return Result<void>::error("Failed to send HARDRESET command: " + result.error());
    }

    // Wait for device to respond with STANDBY (device responds with HARDRESET, then STANDBY)
    if (!waitForSysrep(timeout_ms, cbRUNLEVEL_STANDBY)) {
        return Result<void>::error("Device not responding to HARDRESET (no STANDBY runlevel received)");
    }

request_config:
    // Step 4: Request all configuration (always performed)
    m_impl->received_sysrep.store(false, std::memory_order_relaxed);
    result = requestConfiguration();
    if (result.isError()) {
        return Result<void>::error("Failed to send REQCONFIGALL: " + result.error());
    }

    // Wait for final SYSREP packet from config flood
    // The device sends many config packets and finishes with a SYSREP containing current runlevel
    if (!waitForSysrep(timeout_ms)) {
        return Result<void>::error("Device not responding to REQCONFIGALL (no final SYSREP received)");
    }

    // Step 5: Get current runlevel and transition to RUNNING if needed
    uint32_t current_runlevel = m_impl->device_runlevel.load(std::memory_order_acquire);

    if (current_runlevel != cbRUNLEVEL_RUNNING) {
        // Send RESET to complete handshake
        // Device is in STANDBY (30) after REQCONFIGALL - send RESET which transitions to RUNNING (50)
        // The device responds first with RESET, then on next iteration with RUNNING
        m_impl->received_sysrep.store(false, std::memory_order_relaxed);
        result = setSystemRunLevel(cbRUNLEVEL_RESET);
        if (result.isError()) {
            return Result<void>::error("Failed to send RESET command: " + result.error());
        }

        // Wait for device to transition to RUNNING runlevel
        if (!waitForSysrep(timeout_ms, cbRUNLEVEL_RUNNING)) {
            return Result<void>::error("Device not responding to RESET command (no RUNNING runlevel received)");
        }
    }

    // Success - device is now in RUNNING state
    return Result<void>::ok();
}

Result<void> DeviceSession::connect(const uint32_t timeout_ms) {
    // Step 1: Start receive thread
    auto result = startReceiveThread();
    if (result.isError()) {
        return result;
    }

    // Step 2: Perform startup handshake or request configuration based on autorun flag
    if (m_impl->config.autorun) {
        // Fully start device to RUNNING state
        result = performStartupHandshake(timeout_ms);
        if (result.isError()) {
            return Result<void>::error("Handshake failed: " + result.error());
        }
    } else {
        // Just request configuration without changing runlevel
        result = requestConfiguration();
        if (result.isError()) {
            return Result<void>::error("Failed to request configuration: " + result.error());
        }
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Buffer Management
///////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceSession::setConfigBuffer(cbConfigBuffer* external_buffer) {
    std::lock_guard<std::mutex> lock(m_impl->cfg_mutex);

    if (external_buffer) {
        // Use external buffer (shared memory mode)
        m_impl->m_cfg_ptr = external_buffer;
        m_impl->m_cfg_owned.reset();  // Release internal buffer if any
    } else {
        // Create internal buffer (standalone mode)
        if (!m_impl->m_cfg_owned) {
            m_impl->m_cfg_owned = std::make_unique<cbConfigBuffer>();
            std::memset(m_impl->m_cfg_owned.get(), 0, sizeof(cbConfigBuffer));
        }
        m_impl->m_cfg_ptr = m_impl->m_cfg_owned.get();
    }
}

cbConfigBuffer* DeviceSession::getConfigBuffer() {
    std::lock_guard<std::mutex> lock(m_impl->cfg_mutex);
    return m_impl->m_cfg_ptr;
}

const cbConfigBuffer* DeviceSession::getConfigBuffer() const {
    std::lock_guard<std::mutex> lock(m_impl->cfg_mutex);
    return m_impl->m_cfg_ptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

std::string detectLocalIP() {
#ifdef __APPLE__
    // On macOS with multiple interfaces, use 0.0.0.0 (bind to all interfaces)
    // macOS routing will handle interface selection via IP_BOUND_IF
    return "0.0.0.0";
#else
    // On Windows/Linux: try to find local adapter on 192.168.137.x subnet
    // This is the typical subnet for Cerebus devices

#ifdef _WIN32
    // Windows: Use GetAdaptersAddresses to find 192.168.137.x adapter
    // For now, use 0.0.0.0 as safe default (binds to all interfaces)
    // TODO: Implement Windows adapter detection
    return "0.0.0.0";
#else
    // Linux: Use getifaddrs to find 192.168.137.x adapter
    struct ifaddrs *ifaddr, *ifa;
    std::string result = "0.0.0.0";  // Default fallback

    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);

            // Check if this is a 192.168.137.x address
            if (std::strncmp(ip_str, "192.168.137.", 12) == 0) {
                result = ip_str;
                break;  // Found it!
            }
        }
        freeifaddrs(ifaddr);
    }

    return result;
#endif
#endif
}

} // namespace cbdev
