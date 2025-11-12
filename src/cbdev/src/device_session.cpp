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
#include "cbshmem/upstream_protocol.h"  // For cbPKT_GENERIC
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

    struct ifaddrs *ifaddr, *ifa;
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
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)ifa->ifa_addr;
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

        case DeviceType::GEMINI:
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
    SOCKADDR_IN recv_addr;  // Address we bind to (client side)
    SOCKADDR_IN send_addr;  // Address we send to (device side)

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

    if (bind(session.m_impl->socket, (SOCKADDR*)&session.m_impl->recv_addr,
            sizeof(session.m_impl->recv_addr)) != 0) {
        closeSocket(session.m_impl->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<DeviceSession>::error("Failed to bind socket to " +
            config.client_address + ":" + std::to_string(config.recv_port));
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

        unsigned int if_index = GetInterfaceIndexForIP(config.client_address.c_str());
        if (if_index > 0) {
            // Best effort - don't fail if this doesn't work
            setsockopt(session.m_impl->socket, IPPROTO_IP, IP_BOUND_IF,
                      &if_index, sizeof(if_index));
        }
    }
#endif

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

    int bytes_sent = sendto(m_impl->socket,
                           (const char*)&pkt,
                           sizeof(cbPKT_GENERIC),
                           0,
                           (SOCKADDR*)&m_impl->send_addr,
                           sizeof(m_impl->send_addr));

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

Result<void> DeviceSession::sendPackets(const cbPKT_GENERIC* pkts, size_t count) {
    if (!pkts || count == 0) {
        return Result<void>::error("Invalid packet array");
    }

    // Send each packet individually
    for (size_t i = 0; i < count; ++i) {
        auto result = sendPacket(pkts[i]);
        if (result.isError()) {
            return result;
        }
    }

    return Result<void>::ok();
}

Result<bool> DeviceSession::pollPacket(cbPKT_GENERIC& pkt, int timeout_ms) {
    if (!isOpen()) {
        return Result<bool>::error("Session is not open");
    }

    // Use select() for timeout support
    if (timeout_ms > 0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_impl->socket, &readfds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(m_impl->socket + 1, &readfds, nullptr, nullptr, &tv);
        if (ret == 0) {
            // Timeout - no data available
            return Result<bool>::ok(false);
        } else if (ret < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            return Result<bool>::error("Select failed with error: " + std::to_string(err));
#else
            return Result<bool>::error("Select failed with error: " + std::string(strerror(errno)));
#endif
        }
    }

    // Receive packet
    int bytes_recv = recv(m_impl->socket, (char*)&pkt, sizeof(cbPKT_GENERIC), 0);

    if (bytes_recv == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return Result<bool>::ok(false);  // No data available (non-blocking)
        }
        std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
        m_impl->stats.recv_errors++;
        return Result<bool>::error("Receive failed with error: " + std::to_string(err));
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result<bool>::ok(false);  // No data available (non-blocking)
        }
        std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
        m_impl->stats.recv_errors++;
        return Result<bool>::error("Receive failed with error: " + std::string(strerror(errno)));
#endif
    }

    if (bytes_recv == 0) {
        // Socket closed
        return Result<bool>::ok(false);
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_impl->stats_mutex);
        m_impl->stats.packets_received++;
        m_impl->stats.bytes_received += bytes_recv;
    }

    return Result<bool>::ok(true);
}

///--------------------------------------------------------------------------------------------
/// Callback-Based Receive
///--------------------------------------------------------------------------------------------

void DeviceSession::setPacketCallback(PacketCallback callback) {
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
                cbPKT_HEADER* hdr = reinterpret_cast<cbPKT_HEADER*>(udp_buffer.get() + offset);

                // Calculate actual packet size on wire: header + (dlen * 4 bytes)
                size_t wire_pkt_size = HEADER_SIZE + (hdr->dlen * 4);

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

void DeviceSession::stopReceiveThread() {
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

void DeviceSession::setTransmitCallback(TransmitCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->transmit_mutex);
    m_impl->transmit_callback = std::move(callback);
}

Result<void> DeviceSession::startSendThread() {
    if (!isOpen()) {
        return Result<void>::error("Session is not open");
    }

    if (m_impl->send_thread_running.load()) {
        return Result<void>::error("Send thread is already running");
    }

    m_impl->send_thread_running.store(true);
    m_impl->send_thread = std::make_unique<std::thread>([this]() {
        cbPKT_GENERIC pkt;

        while (m_impl->send_thread_running.load()) {
            bool has_packets = false;

            // Try to dequeue and send all available packets
            {
                std::lock_guard<std::mutex> lock(m_impl->transmit_mutex);
                if (m_impl->transmit_callback) {
                    while (m_impl->transmit_callback(pkt)) {
                        has_packets = true;

                        // Send the packet
                        int bytes_sent = sendto(m_impl->socket,
                                              (const char*)&pkt,
                                              sizeof(cbPKT_GENERIC),
                                              0,
                                              (SOCKADDR*)&m_impl->send_addr,
                                              sizeof(m_impl->send_addr));

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

void DeviceSession::stopSendThread() {
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
