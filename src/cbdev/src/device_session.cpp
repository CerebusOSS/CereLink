///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_session.cpp
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Minimal UDP socket wrapper for device communication
///
/// Provides only socket operations - no threads, no callbacks, no statistics, no parsing.
/// All orchestration logic has been moved to SdkSession (cbsdk_v2).
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "device_session_impl.h"
#include <cbproto/cbproto.h>
#include <cstring>

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
// ConnectionParams Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

ConnectionParams ConnectionParams::forDevice(DeviceType type) {
    ConnectionParams conn_params;
    conn_params.type = type;

    switch (type) {
        case DeviceType::LEGACY_NSP:
            // Legacy NSP: different ports for send/recv
            conn_params.device_address = ConnectionDefaults::LEGACY_NSP_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::LEGACY_NSP_RECV_PORT;
            conn_params.send_port = ConnectionDefaults::LEGACY_NSP_SEND_PORT;
            break;

        case DeviceType::NSP:
            // Gemini NSP: same port for both send & recv
            conn_params.device_address = ConnectionDefaults::NSP_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::NSP_PORT;
            conn_params.send_port = ConnectionDefaults::NSP_PORT;
            break;

        case DeviceType::HUB1:
            // Gemini Hub1: same port for both send & recv
            conn_params.device_address = ConnectionDefaults::HUB1_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::HUB1_PORT;
            conn_params.send_port = ConnectionDefaults::HUB1_PORT;
            break;

        case DeviceType::HUB2:
            // Gemini Hub2: same port for both send & recv
            conn_params.device_address = ConnectionDefaults::HUB2_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::HUB2_PORT;
            conn_params.send_port = ConnectionDefaults::HUB2_PORT;
            break;

        case DeviceType::HUB3:
            // Gemini Hub3: same port for both send & recv
            conn_params.device_address = ConnectionDefaults::HUB3_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::HUB3_PORT;
            conn_params.send_port = ConnectionDefaults::HUB3_PORT;
            break;

        case DeviceType::NPLAY:
            // nPlayServer: loopback for both device and client
            conn_params.device_address = ConnectionDefaults::NPLAY_ADDRESS;
            conn_params.client_address = "127.0.0.1";  // Always use loopback for NPLAY
            conn_params.recv_port = ConnectionDefaults::NPLAY_PORT;
            conn_params.send_port = ConnectionDefaults::NPLAY_PORT;
            break;

        case DeviceType::CUSTOM:
            // User must set addresses manually
            break;
    }

    return conn_params;
}

ConnectionParams ConnectionParams::custom(const std::string& device_addr,
                                  const std::string& client_addr,
                                  const uint16_t recv_port,
                                  const uint16_t send_port) {
    ConnectionParams config;
    config.type = DeviceType::CUSTOM;
    config.device_address = device_addr;
    config.client_address = client_addr;
    config.recv_port = recv_port;
    config.send_port = send_port;
    return config;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceSession::Impl - Minimal Socket-Only Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

struct DeviceSession::Impl {
    ConnectionParams config;
    SOCKET socket = INVALID_SOCKET_VALUE;
    SOCKADDR_IN recv_addr{};  // Address we bind to (client side)
    SOCKADDR_IN send_addr{};  // Address we send to (device side)
    bool connected = false;

    // Platform-specific state
    #ifdef _WIN32
    bool wsa_initialized = false;
    #endif

    ~Impl() {
        if (socket != INVALID_SOCKET_VALUE) {
            closeSocket(socket);
        }

#ifdef _WIN32
        if (wsa_initialized) {
            WSACleanup();
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

DeviceSession::DeviceSession(DeviceSession&&) noexcept = default;
DeviceSession& DeviceSession::operator=(DeviceSession&&) noexcept = default;

DeviceSession::~DeviceSession() {
    close();
}

Result<DeviceSession> DeviceSession::create(const ConnectionParams& config) {
    DeviceSession session;
    session.m_impl = std::make_unique<Impl>();
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
    session.m_impl->wsa_initialized = true;
#endif

    // Create UDP socket
#ifdef _WIN32
    session.m_impl->socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
#else
    session.m_impl->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif

    if (session.m_impl->socket == INVALID_SOCKET_VALUE) {
        return Result<DeviceSession>::error("Failed to create socket");
    }

    // Set socket options
    int opt_one = 1;

    if (config.broadcast) {
        if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_BROADCAST,
                      (char*)&opt_one, sizeof(opt_one)) != 0) {
            return Result<DeviceSession>::error("Failed to set SO_BROADCAST");
        }
    }

    // Set SO_REUSEADDR to allow multiple binds to same port
    if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_REUSEADDR,
                  (char*)&opt_one, sizeof(opt_one)) != 0) {
        return Result<DeviceSession>::error("Failed to set SO_REUSEADDR");
    }

    // Set receive buffer size
    if (config.recv_buffer_size > 0) {
        int buffer_size = config.recv_buffer_size;
        if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVBUF,
                      (char*)&buffer_size, sizeof(buffer_size)) != 0) {
            return Result<DeviceSession>::error("Failed to set receive buffer size");
        }

        // Verify buffer size was set
        socklen_t opt_len = sizeof(int);
        if (getsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVBUF,
                      (char*)&buffer_size, &opt_len) != 0) {
            return Result<DeviceSession>::error("Failed to verify receive buffer size");
        }

#ifdef __linux__
        // Linux returns double the requested size
        buffer_size /= 2;
#endif

        if (buffer_size < config.recv_buffer_size) {
            return Result<DeviceSession>::error(
                "Receive buffer size too small (got " + std::to_string(buffer_size) +
                ", requested " + std::to_string(config.recv_buffer_size) + ")");
        }
    }

    // Set non-blocking mode
    if (config.non_blocking) {
#ifdef _WIN32
        u_long arg_val = 1;
        if (ioctlsocket(session.m_impl->socket, FIONBIO, &arg_val) == SOCKET_ERROR_VALUE) {
#else
        if (fcntl(session.m_impl->socket, F_SETFL, O_NONBLOCK) != 0) {
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

    session.m_impl->connected = true;
    return Result<DeviceSession>::ok(std::move(session));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// IDeviceSession Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<int> DeviceSession::receivePackets(void* buffer, const size_t buffer_size) {
    if (!m_impl || !m_impl->connected) {
        return Result<int>::error("Device not connected");
    }

    // Receive UDP datagram into provided buffer (non-blocking)
    int bytes_recv = recv(m_impl->socket, (char*)buffer, buffer_size, 0);

    if (bytes_recv == SOCKET_ERROR_VALUE) {
        #ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return Result<int>::ok(0);  // No data available
        }
        return Result<int>::error("recv failed: " + std::to_string(err));
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result<int>::ok(0);  // No data available
        }
        return Result<int>::error("recv failed: " + std::string(strerror(errno)));
        #endif
    }

    return Result<int>::ok(bytes_recv);
}

Result<void> DeviceSession::sendPacket(const cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Calculate actual packet size from header
    // dlen is in quadlets (4-byte units), so packet size = header + (dlen * 4)
    const size_t packet_size = cbPKT_HEADER_SIZE + (pkt.cbpkt_header.dlen * 4);
    return sendRaw(&pkt, packet_size);
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

Result<void> DeviceSession::sendRaw(const void* buffer, const size_t size) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    if (!buffer || size == 0) {
        return Result<void>::error("Invalid buffer or size");
    }

    const int bytes_sent = sendto(
        m_impl->socket,
        (const char*)buffer,
        size,
        0,
        reinterpret_cast<SOCKADDR *>(&m_impl->send_addr),
        sizeof(m_impl->send_addr)
    );

    if (bytes_sent == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        int err = WSAGetLastError();
        return Result<void>::error("Send failed with error: " + std::to_string(err));
#else
        return Result<void>::error("Send failed with error: " + std::string(strerror(errno)));
#endif
    }

    return Result<void>::ok();
}

bool DeviceSession::isConnected() const {
    return m_impl && m_impl->connected;
}

const ConnectionParams& DeviceSession::getConnectionParams() const {
    return m_impl->config;
}

ProtocolVersion DeviceSession::getProtocolVersion() const {
    return ProtocolVersion::PROTOCOL_CURRENT;
}

void DeviceSession::close() {
    if (!m_impl) return;

    if (m_impl->socket != INVALID_SOCKET_VALUE) {
        closeSocket(m_impl->socket);
        m_impl->socket = INVALID_SOCKET_VALUE;
    }

    m_impl->connected = false;
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

///--------------------------------------------------------------------------------------------
/// Protocol Commands
///--------------------------------------------------------------------------------------------

Result<void> DeviceSession::setSystemRunLevel(const uint32_t runlevel, const uint32_t resetque, const uint32_t runflags) {
    // Create runlevel command packet
    cbPKT_SYSINFO sysinfo = {};

    // Fill header
    sysinfo.cbpkt_header.time = 1;
    sysinfo.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    sysinfo.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;
    sysinfo.cbpkt_header.instrument = 0;

    // Fill payload
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = runflags;

    // Send the packet (caller handles waiting for response)
    // Safe to reinterpret_cast since cbPKT_GENERIC is designed to hold any packet type
    return sendPacket(*reinterpret_cast<cbPKT_GENERIC*>(&sysinfo));
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

    // Send the packet (caller handles waiting for config flood and final SYSREP)
    return sendPacket(pkt);
}

} // namespace cbdev
