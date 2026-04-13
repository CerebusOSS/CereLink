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

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#ifdef _WIN32
    #include <mmsystem.h>
    #pragma comment(lib, "iphlpapi.lib")
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
    #include <ifaddrs.h>
    #ifdef __APPLE__
        #include <net/if.h>
    #endif
    typedef int SOCKET;
    typedef struct sockaddr SOCKADDR;
    typedef struct sockaddr_in SOCKADDR_IN;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

#include "device_session_impl.h"
#include "cbdev/clock_sync.h"
#include <cbproto/cbproto.h>
#include <cbproto/config.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>  // for std::remove
#include <cctype>     // for std::tolower
#include <functional> // for std::function
#include <numeric>    // for std::gcd
#include <thread>
#include <atomic>
#include <vector>

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
            // nPlayServer: loopback, different ports for send/recv
            conn_params.device_address = ConnectionDefaults::NPLAY_ADDRESS;
            conn_params.client_address = "";  // Auto-detect
            conn_params.recv_port = ConnectionDefaults::LEGACY_NSP_RECV_PORT;
            conn_params.send_port = ConnectionDefaults::LEGACY_NSP_SEND_PORT;
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

    // Device configuration (from REQCONFIGALL)
    cbproto::DeviceConfig device_config{};

    // Timestamp conversion for non-Gemini devices
    // Gemini devices, our primary use case, send timestamps in nanoseconds; default to true.
    // Non-Gemini (i.e. NPlay and Legacy NSP) send sample counts.
    // Determined from PROCREP ident field: if ident contains "gemini", timestamps are nanoseconds.
    // PROCREP may be received several seconds after device start.
    bool timestamps_are_nanoseconds = true;
    uint64_t ts_convert_num = 1;  // Numerator of reduced (1e9/sysfreq) fraction
    uint64_t ts_convert_den = 1;  // Denominator of reduced (1e9/sysfreq) fraction

    // Clock synchronization
    ClockSync clock_sync;
    std::chrono::steady_clock::time_point last_recv_timestamp{};
    struct PendingClockProbe {
        std::chrono::steady_clock::time_point t1_local;
        bool active = false;
    };
    PendingClockProbe pending_clock_probe;
    std::mutex clock_probe_mutex;

    // General response waiting mechanism
    struct PendingResponse {
        std::function<bool(const cbPKT_HEADER*)> matcher;
        std::condition_variable cv;
        std::mutex mutex;
        size_t expected_count = 1;  // How many packets we expect
        size_t received_count = 0;  // How many we've received so far
    };
    std::vector<std::shared_ptr<PendingResponse>> pending_responses;
    std::mutex pending_mutex;

    // Callback registration
    struct CallbackRegistration {
        CallbackHandle handle;
        ReceiveCallback callback;
    };
    struct DatagramCallbackRegistration {
        CallbackHandle handle;
        DatagramCompleteCallback callback;
    };
    std::vector<CallbackRegistration> receive_callbacks;
    std::vector<DatagramCallbackRegistration> datagram_callbacks;
    std::mutex callback_mutex;
    std::atomic<bool> has_callbacks{false};  // Fast-path skip when no callbacks registered
    CallbackHandle next_callback_handle = 1;  // 0 is reserved for "invalid"

    // Protocol monitor — dropped packet detection
    uint32_t pkts_since_monitor = 0;       // Packets counted since last SYSPROTOCOLMONITOR
    bool first_monitor_seen = false;        // Skip comparison until first baseline is established
    uint32_t dropped_accum = 0;             // Drops accumulated since last log
    uint32_t sent_accum = 0;                // Sent accumulated since last log
    std::chrono::steady_clock::time_point last_drop_log_time{};

    // Receive thread state
    std::thread receive_thread;
    std::atomic<bool> receive_thread_running{false};
    std::atomic<bool> receive_thread_stop_requested{false};

    // Platform-specific state
    #ifdef _WIN32
    bool wsa_initialized = false;
    #endif

    void stopReceiveThreadInternal() {
        if (receive_thread_running.load()) {
            receive_thread_stop_requested.store(true);
            if (receive_thread.joinable()) {
                receive_thread.join();
            }
            receive_thread_running.store(false);
            receive_thread_stop_requested.store(false);
        }
    }

    ~Impl() {
        // Stop receive thread before closing socket
        stopReceiveThreadInternal();

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
/// ResponseWaiter::Impl - PImpl pattern to hide internal details
///////////////////////////////////////////////////////////////////////////////////////////////////
struct DeviceSession::ResponseWaiter::Impl {
    std::shared_ptr<DeviceSession::Impl::PendingResponse> response;
    DeviceSession* session;

    Impl(std::shared_ptr<DeviceSession::Impl::PendingResponse> resp, DeviceSession* sess)
        : response(std::move(resp)), session(sess) {}
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

    // LEGACY_NSP is known to use sample-count timestamps (never Gemini).
    // For other types, we default to true and let PROCREP confirm.
    if (config.type == DeviceType::LEGACY_NSP) {
        session.m_impl->timestamps_are_nanoseconds = false;
    }

    // Auto-detect client address if not specified
    if (session.m_impl->config.client_address.empty()) {
        session.m_impl->config.client_address = detectClientAddress(session.m_impl->config.type);
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

    // Set send buffer size (if specified)
    if (config.send_buffer_size > 0) {
        int buffer_size = config.send_buffer_size;
        if (setsockopt(session.m_impl->socket, SOL_SOCKET, SO_SNDBUF,
                      (char*)&buffer_size, sizeof(buffer_size)) != 0) {
            return Result<DeviceSession>::error("Failed to set send buffer size");
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
    } else {
        // For blocking sockets, set a receive timeout so the receive thread
        // can periodically check its stop flag and shut down cleanly.
#ifdef _WIN32
        DWORD timeout_ms = 250;
        setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 250000;  // 250ms
        setsockopt(session.m_impl->socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&tv, sizeof(tv));
#endif
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

Result<int> DeviceSession::receivePacketsRaw(void* buffer, const size_t buffer_size) {
    if (!m_impl || !m_impl->connected) {
        return Result<int>::error("Device not connected");
    }

    // Receive UDP datagram into provided buffer (non-blocking).
    // Use recvfrom to capture the sender address so we can discard datagrams
    // from other devices sharing the same port (e.g. NPLAY and HUB1 both use 51002).
    SOCKADDR_IN sender_addr{};
    socklen_t addr_len = sizeof(sender_addr);
    const int bytes_recv = recvfrom(m_impl->socket, (char*)buffer, buffer_size, 0,
                              reinterpret_cast<SOCKADDR*>(&sender_addr), &addr_len);

    if (bytes_recv == SOCKET_ERROR_VALUE) {
        #ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) {
            return Result<int>::ok(0);  // No data available (non-blocking or timeout)
        }
        return Result<int>::error("recv failed: " + std::to_string(err));
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result<int>::ok(0);  // No data available (non-blocking or timeout)
        }
        return Result<int>::error("recv failed: " + std::string(strerror(errno)));
        #endif
    }

    // Discard datagrams from unexpected sources.
    if (bytes_recv > 0 &&
        sender_addr.sin_addr.s_addr != m_impl->send_addr.sin_addr.s_addr) {
        return Result<int>::ok(0);
    }

    // Capture host timestamp as early as possible after accepting a datagram.
    if (bytes_recv > 0) {
        m_impl->last_recv_timestamp = std::chrono::steady_clock::now();
    }

    return Result<int>::ok(bytes_recv);
}

Result<int> DeviceSession::receivePackets(void* buffer, const size_t buffer_size) {
    // Receive packets from device
    auto result = receivePacketsRaw(buffer, buffer_size);

    if (result.isOk() && result.value() > 0) {
        // TODO: Update statistics

        // Update configuration from received packets (if any)
        updateConfigFromBuffer(buffer, result.value());

        // Convert timestamps from sample counts to nanoseconds for non-Gemini devices.
        // The flag is set when PROCREP is processed in updateConfigFromBuffer above.
        if (!m_impl->timestamps_are_nanoseconds && m_impl->ts_convert_den > 1) {
            auto* bytes_ptr = static_cast<uint8_t*>(buffer);
            const size_t total_bytes = result.value();
            size_t offset = 0;
            while (offset + cbPKT_HEADER_SIZE <= total_bytes) {
                auto* header = reinterpret_cast<cbPKT_HEADER*>(bytes_ptr + offset);
                const size_t packet_size = cbPKT_HEADER_SIZE + (header->dlen * 4);
                if (offset + packet_size > total_bytes) break;

                header->time = header->time * m_impl->ts_convert_num / m_impl->ts_convert_den;

                offset += packet_size;
            }
        }
    }

    return result;
}

Result<void> DeviceSession::sendPacket(const cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Non-Gemini: convert nanosecond timestamp back to device clock ticks.
    // This is the inverse of the ticks→ns conversion in receivePackets().
    if (!m_impl->timestamps_are_nanoseconds && m_impl->ts_convert_den > 1 &&
        pkt.cbpkt_header.time != 0) {
        cbPKT_GENERIC converted = pkt;
        converted.cbpkt_header.time =
            pkt.cbpkt_header.time * m_impl->ts_convert_den / m_impl->ts_convert_num;
        const size_t packet_size = cbPKT_HEADER_SIZE + (converted.cbpkt_header.dlen * 4);
        return sendRaw(&converted, packet_size);
    }

    // Calculate actual packet size from header
    // dlen is in quadlets (4-byte units), so packet size = header + (dlen * 4)
    const size_t packet_size = cbPKT_HEADER_SIZE + (pkt.cbpkt_header.dlen * 4);
    return sendRaw(&pkt, packet_size);
}

Result<void> DeviceSession::sendPackets(const std::vector<cbPKT_GENERIC>& pkts) {
    if (pkts.empty()) {
        return Result<void>::error("Empty packet vector");
    }

    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Send each packet as its own datagram (no coalescing).  Two concerns:
    //
    // 1. Pacing: the receiver's kernel UDP buffer can be as small as 8 KB
    //    (~8 CHANINFO packets).  We must not send faster than the receiver
    //    can drain.
    //
    // 2. CPU fairness: on shared VMs (CI runners), a pure busy-wait
    //    between sends starves nPlayServer of CPU, preventing it from
    //    draining its buffer even when packets arrive slowly.
    //
    // Strategy: send a small batch, then yield the CPU so the receiver
    // process can run.  The batch size (8) matches the worst-case kernel
    // buffer capacity, so even if the yield takes a while the buffer
    // won't overflow from the preceding burst.
    // On Windows, temporarily raise the timer resolution so Sleep(1)
    // actually sleeps ~1 ms instead of ~15 ms.  The RAII guard restores
    // the resolution when sendPackets returns.
#ifdef _WIN32
    timeBeginPeriod(1);
    struct TimerGuard { ~TimerGuard() { timeEndPeriod(1); } } timerGuard;
#endif

    constexpr size_t BATCH = 8;
    for (size_t i = 0; i < pkts.size(); ++i) {
        if (auto result = sendPacket(pkts[i]); result.isError()) {
            return result;
        }
        if ((i % BATCH) == (BATCH - 1)) {
            // Pace sends so the receiver can drain its kernel UDP buffer.
            // Sleep(1) yields for one scheduler quantum (~1 ms with the
            // timer resolution raised above).  On other platforms,
            // sleep_for is already accurate at sub-ms granularity.
#ifdef _WIN32
            Sleep(1);
#else
            std::this_thread::sleep_for(std::chrono::microseconds(50));
#endif
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

const cbproto::DeviceConfig& DeviceSession::getDeviceConfig() const {
    return m_impl->device_config;
}

const cbPKT_SYSINFO& DeviceSession::getSysInfo() const {
    return m_impl->device_config.sysinfo;
}

const cbPKT_CHANINFO* DeviceSession::getChanInfo(const uint32_t chan_id) const {
    if (chan_id < 1 || chan_id > cbMAXCHANS) {
        return nullptr;
    }
    return &m_impl->device_config.chaninfo[chan_id - 1];
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

std::string detectClientAddress(DeviceType type) {
    // nPlayServer sends unicast to 127.0.0.1.  Bind to loopback so the socket
    // only receives loopback traffic, avoiding cross-talk with Gemini devices
    // that share the same port (51002) on the network interface.
    if (type == DeviceType::NPLAY) {
        return "127.0.0.1";
    }

#ifdef __APPLE__
    // On macOS with multiple interfaces, use 0.0.0.0 (bind to all interfaces)
    // macOS routing will handle interface selection via IP_BOUND_IF
    return "0.0.0.0";
#else
#ifdef _WIN32
    // Find a Windows adapter with IPv4 in 192\.168\.137\.* (Cerebus default ICS subnet).
    // If found, return its unicast IPv4 address; otherwise return 0\.0\.0\.0.
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    const ULONG family = AF_UNSPEC;

    ULONG bufLen = 16 * 1024;
    std::vector<uint8_t> buffer(bufLen);
    auto addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    DWORD ret = GetAdaptersAddresses(family, flags, nullptr, addrs, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufLen);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(family, flags, nullptr, addrs, &bufLen);
    }
    if (ret != NO_ERROR) {
        return "0.0.0.0";
    }

    for (IP_ADAPTER_ADDRESSES* aa = addrs; aa != nullptr; aa = aa->Next) {
        // Skip adapters that are down or loopback
        if (aa->OperStatus != IfOperStatusUp) continue;
        if (aa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        for (IP_ADAPTER_UNICAST_ADDRESS* ua = aa->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if (!ua->Address.lpSockaddr) continue;
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ip[INET_ADDRSTRLEN] = {};
            if (!inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip))) continue;

            // Match 192\.168\.137\.* prefix
            if (std::strncmp(ip, "192.168.137.", 12) == 0) {
                return std::string(ip);
            }
        }
    }

    // Fallback: bind to all interfaces
    return "0.0.0.0";
#else
    // Linux: To receive UDP broadcast packets, we must bind to the broadcast address
    // (not the interface's unicast IP). Linux does not deliver broadcast packets to
    // sockets bound to a specific unicast address.
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
                // Found the Cerebus interface - use the broadcast address to receive broadcasts
                result = cbNET_UDP_ADDR_BCAST;  // "192.168.137.255"
                break;
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

Result<void> DeviceSession::setSystemRunLevelSync(uint32_t runlevel, uint32_t resetque,
                                                   uint32_t runflags,
                                                   std::chrono::milliseconds timeout) {
    // Determine expected runlevel in response:
    // - HARDRESET returns STANDBY (after 2 packets)
    // - RESET returns RUNNING (after 2 packets)
    // - Others return the same runlevel
    uint32_t expected_runlevel;
    switch (runlevel) {
        case cbRUNLEVEL_HARDRESET:
            expected_runlevel = cbRUNLEVEL_STANDBY;
            break;
        case cbRUNLEVEL_RESET:
            expected_runlevel = cbRUNLEVEL_RUNNING;
            break;
        default:
            expected_runlevel = runlevel;
            break;
    }

    return sendAndWait(
        [this, runlevel, resetque, runflags]() {
            return setSystemRunLevel(runlevel, resetque, runflags);
        },
        [expected_runlevel](const cbPKT_HEADER* hdr) {
            if ((hdr->chid & cbPKTCHAN_CONFIGURATION) != cbPKTCHAN_CONFIGURATION ||
                hdr->type != cbPKTTYPE_SYSREPRUNLEV) {
                return false;
            }
            // Cast to full packet to check runlevel field
            auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(hdr);
            return sysinfo->runlevel == expected_runlevel;
        },
        timeout
    );
}

Result<void> DeviceSession::requestConfiguration() {
    if (!m_impl) {
        return Result<void>::error("Device not initialized");
    }

    // Create REQCONFIGALL packet
    cbPKT_GENERIC pkt = {};
    pkt.cbpkt_header.time = 1;
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_REQCONFIGALL;
    pkt.cbpkt_header.dlen = 0;  // No payload
    pkt.cbpkt_header.instrument = 0;

    // Send the packet
    return sendPacket(pkt);
}

Result<void> DeviceSession::requestConfigurationSync(std::chrono::milliseconds timeout) {
    return sendAndWait(
        [this]() { return requestConfiguration(); },
        [](const cbPKT_HEADER* hdr) {
            return (hdr->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION &&
                   hdr->type == cbPKTTYPE_SYSREP;
        },
        timeout
    );
}

Result<void> DeviceSession::performHandshakeSync(std::chrono::milliseconds timeout) {
    if (!m_impl) {
        return Result<void>::error("Device not initialized");
    }

    // Step 1: Try to set runlevel to RUNNING
    auto result = setSystemRunLevelSync(cbRUNLEVEL_RUNNING, 0, 0, std::chrono::milliseconds(10));
    if (result.isError()) {
        // If this fails, it's not critical - device may already be in a different state
        // Continue with the handshake process
    }

    // Step 2: Check current runlevel
    uint32_t current_runlevel = m_impl->device_config.sysinfo.runlevel;

    // Step 3: If not RUNNING, do HARDRESET
    if (current_runlevel != cbRUNLEVEL_RUNNING) {
        result = setSystemRunLevelSync(cbRUNLEVEL_HARDRESET, 0, 0, timeout);
        if (result.isError()) {
            return result;  // HARDRESET failed
        }
        // After HARDRESET, we should be in STANDBY
    }

    // Step 4: Request configuration.
    result = requestConfigurationSync(timeout);
    if (result.isError()) {
        return result;
    }

    // Step 5: Do (soft) RESET if not RUNNING
    current_runlevel = m_impl->device_config.sysinfo.runlevel;
    if (current_runlevel != cbRUNLEVEL_RUNNING) {
        result = setSystemRunLevelSync(cbRUNLEVEL_RESET, 0, 0, timeout);
        if (result.isError()) {
            return result;
        }
        // After RESET, we should be in RUNNING
    }

    // Verify we're in RUNNING state
    current_runlevel = m_impl->device_config.sysinfo.runlevel;
    if (current_runlevel != cbRUNLEVEL_RUNNING) {
        return Result<void>::error("Failed to reach RUNNING state after handshake");
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Synchronization
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> DeviceSession::sendClockProbe() {
    if (!m_impl || !m_impl->connected)
        return Result<void>::error("Device not connected");

    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_impl->clock_probe_mutex);
        m_impl->pending_clock_probe.t1_local = now;
        m_impl->pending_clock_probe.active = true;
    }

    // Send nPlay packet as clock probe. The host send time goes in .stime;
    // firmware writes a fresh clock_gettime(ptp_clkid) into .etime before echoing back.
    // Use an undefined mode (0xFFFF) so the device/nPlay server won't act on it
    // but will still echo the packet back for our ping-pong clock loop.
    cbPKT_NPLAY pkt{};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_NPLAYSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_NPLAY;
    pkt.mode = 0xFFFF;
    pkt.stime = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count());

    return sendPacket(*reinterpret_cast<const cbPKT_GENERIC*>(&pkt));
}

std::optional<std::chrono::steady_clock::time_point>
DeviceSession::toLocalTime(uint64_t device_time_ns) const {
    if (!m_impl) return std::nullopt;
    return m_impl->clock_sync.toLocalTime(device_time_ns);
}

std::optional<uint64_t>
DeviceSession::toDeviceTime(std::chrono::steady_clock::time_point local_time) const {
    if (!m_impl) return std::nullopt;
    return m_impl->clock_sync.toDeviceTime(local_time);
}

std::optional<int64_t> DeviceSession::getOffsetNs() const {
    if (!m_impl) return std::nullopt;
    return m_impl->clock_sync.getOffsetNs();
}

std::optional<int64_t> DeviceSession::getUncertaintyNs() const {
    if (!m_impl) return std::nullopt;
    return m_impl->clock_sync.getUncertaintyNs();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Channel Configuration
///////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceSession::channelMatchesType(const cbPKT_CHANINFO& chaninfo, const ChannelType chanType) {
    const uint32_t caps = chaninfo.chancaps;
    const uint32_t ainpcaps = chaninfo.ainpcaps;
    const uint32_t aoutcaps = chaninfo.aoutcaps;
    const uint32_t dinpcaps = chaninfo.dinpcaps;

    // Channel must exist and be connected
    if ((cbCHAN_EXISTS | cbCHAN_CONNECTED) != (caps & (cbCHAN_EXISTS | cbCHAN_CONNECTED))) {
        return false;
    }

    // Check type-specific capabilities
    switch (chanType) {
        case ChannelType::FRONTEND:
            // Front-end: analog input + isolated
            return (cbCHAN_AINP | cbCHAN_ISOLATED) == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED));

        case ChannelType::ANALOG_IN:
            // Analog input but not isolated
            return (cbCHAN_AINP) == (caps & (cbCHAN_AINP | cbCHAN_ISOLATED));

        case ChannelType::ANALOG_OUT:
            // Analog output but not audio
            return (cbCHAN_AOUT == (caps & cbCHAN_AOUT)) &&
                   (cbAOUT_AUDIO != (aoutcaps & cbAOUT_AUDIO));

        case ChannelType::AUDIO:
            // Analog output + audio flag
            return (cbCHAN_AOUT == (caps & cbCHAN_AOUT)) &&
                   (cbAOUT_AUDIO == (aoutcaps & cbAOUT_AUDIO));

        case ChannelType::DIGITAL_IN:
            // Digital input with mask (but not serial)
            return (cbCHAN_DINP == (caps & cbCHAN_DINP)) &&
                   (dinpcaps & cbDINP_MASK) &&
                   !(dinpcaps & cbDINP_SERIALMASK);

        case ChannelType::SERIAL:
            // Digital input with serial mask
            return (cbCHAN_DINP == (caps & cbCHAN_DINP)) &&
                   (dinpcaps & cbDINP_SERIALMASK);

        case ChannelType::DIGITAL_OUT:
            // Digital output
            return cbCHAN_DOUT == (caps & cbCHAN_DOUT);

        default:
            return false;
    }
}

size_t DeviceSession::countChannelsOfType(const ChannelType chanType, const size_t maxCount) const {
    if (!m_impl) return 0;

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS && count < maxCount; ++chan) {
        const auto& chaninfo = m_impl->device_config.chaninfo[chan - 1];
        if (channelMatchesType(chaninfo, chanType)) {
            count++;
        }
    }
    return count;
}

Result<void> DeviceSession::setChannelsGroupByType(const size_t nChans, const ChannelType chanType, const DeviceRate group_id, const bool disableOthers) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Build vector of packets for all matching channels
    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(nChans);

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS; ++chan) {
        if (!disableOthers && count >= nChans) {
            break;  // Only configure beyond nChans if disabling others
        }

        auto& chaninfo = m_impl->device_config.chaninfo[chan - 1];

        // Check if this channel matches the requested type
        if (!channelMatchesType(chaninfo, chanType)) {
            continue;
        }

        // Create channel config packet
        cbPKT_CHANINFO pkt = chaninfo;  // Copy current config
        pkt.chan = chan;

        const auto grp = count < nChans ? group_id : DeviceRate::NONE;  // Set to group_id or disable (0)

        // Apply group-specific logic
        if (grp != DeviceRate::NONE && grp != DeviceRate::SR_RAW) {
            pkt.cbpkt_header.type = cbPKTTYPE_CHANSETSMP; // only need SMP for this.
            pkt.smpgroup = static_cast<uint32_t>(grp);

            // Set filter based on group mapping: {1: 5, 2: 6, 3: 7, 4: 10}
            constexpr uint32_t filter_map[] = {0, 5, 6, 7, 10, 0, 0};
            pkt.smpfilter = filter_map[static_cast<uint32_t>(grp)];

            if (grp == DeviceRate::SR_30000) {
                // Further disable raw stream (group 6) when enabling group 5; requires cbPKTTYPE_CHANSET
                pkt.cbpkt_header.type = cbPKTTYPE_CHANSET;
                pkt.ainpopts &= ~cbAINP_RAWSTREAM;  // Clear group 6 flag
            }
        }
        else if (grp == DeviceRate::SR_RAW) {
            // Group 6: Raw
            pkt.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;
            pkt.ainpopts |= cbAINP_RAWSTREAM;  // Set group 6 flag
        }
        else if (grp == DeviceRate::NONE) {
            // Group 0: disable all groups including raw (group 6)
            pkt.cbpkt_header.type = cbPKTTYPE_CHANSET;
            pkt.smpgroup = 0;
            pkt.ainpopts &= ~cbAINP_RAWSTREAM;  // Clear group 6 flag
        }

        // Add packet to vector
        packets.push_back(*reinterpret_cast<cbPKT_GENERIC*>(&pkt));
        count++;
    }

    if (packets.empty()) {
        return Result<void>::error("No channels found matching type");
    }

    // Send all packets coalesced into minimal datagrams
    return sendPackets(packets);
}

Result<void> DeviceSession::setChannelsGroupSync(const size_t nChans, const ChannelType chanType, const DeviceRate group_id, const std::chrono::milliseconds timeout) {
    // Count matching channels, capped by requested count
    const size_t total_matching = std::min(nChans, countChannelsOfType(chanType, cbMAXCHANS));
    if (total_matching == 0) {
        return Result<void>::error("No channels found matching type");
    }

    return sendAndWait(
        [this, nChans, chanType, group_id]() {
            return setChannelsGroupByType(nChans, chanType, group_id, true);
        },
        [](const cbPKT_HEADER* hdr) {
            return (hdr->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION &&
                   (hdr->type == cbPKTTYPE_CHANREPAINP || hdr->type == cbPKTTYPE_CHANREPSMP || hdr->type == cbPKTTYPE_CHANREP);
        },
        timeout,
        total_matching
    );
}

Result<void> DeviceSession::setChannelsACInputCouplingByType(const size_t nChans, const ChannelType chanType, const bool enabled) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Build vector of packets for all matching channels
    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(nChans);

    size_t count = 0;
    for (uint32_t chan = 1; chan <= cbMAXCHANS && count < nChans; ++chan) {
        const auto& chaninfo = m_impl->device_config.chaninfo[chan - 1];

        // Check if this channel matches the requested type
        if (!channelMatchesType(chaninfo, chanType)) {
            continue;
        }

        // Create channel config packet
        cbPKT_CHANINFO pkt = chaninfo;  // Start with current config
        pkt.cbpkt_header.type = cbPKTTYPE_CHANSETAINP;  // Use analog input set command
        pkt.chan = chan;

        // Set or clear offset correction flag
        if (enabled) {
            pkt.ainpopts |= cbAINP_OFFSET_CORRECT;
        } else {
            pkt.ainpopts &= ~cbAINP_OFFSET_CORRECT;
        }

        // Add packet to vector
        packets.push_back(*reinterpret_cast<cbPKT_GENERIC*>(&pkt));
        count++;
    }

    if (packets.empty()) {
        return Result<void>::error("No channels found matching type");
    }

    // Send all packets coalesced into minimal datagrams
    return sendPackets(packets);
}

Result<void> DeviceSession::setChannelsACInputCouplingSync(const size_t nChans, const ChannelType chanType, const bool enabled, const std::chrono::milliseconds timeout) {
    const size_t total_matching = std::min(nChans, countChannelsOfType(chanType, cbMAXCHANS));
    return sendAndWait(
        [this, nChans, chanType, enabled]() {
            return setChannelsACInputCouplingByType(nChans, chanType, enabled);
        },
        [](const cbPKT_HEADER* hdr) {
            return (hdr->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION &&
                   hdr->type == cbPKTTYPE_CHANREPAINP;
        },
        timeout,
        total_matching
    );
}

Result<void> DeviceSession::setChannelsSpikeSortingByType(const size_t nChans, const ChannelType chanType, const uint32_t sortOptions) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    // Build vector of packets for all matching channels
    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(nChans);

    // Configure first nChans channels of type chanType
    for (size_t i = 0; i < nChans && i < cbMAXCHANS; ++i) {
        const uint32_t chan = i + 1;
        const auto& chaninfo = m_impl->device_config.chaninfo[i];

        // Check if this channel matches the requested type
        if (!channelMatchesType(chaninfo, chanType)) {
            continue;
        }

        // Create channel config packet
        cbPKT_CHANINFO pkt = chaninfo;  // Start with current config
        pkt.cbpkt_header.type = cbPKTTYPE_CHANSETSPKTHR;  // Use spike threshold set command
        pkt.chan = chan;

        // Clear all spike sorting flags and set new ones
        pkt.spkopts &= ~cbAINPSPK_ALLSORT;
        pkt.spkopts |= sortOptions;

        // Add packet to vector
        packets.push_back(*reinterpret_cast<cbPKT_GENERIC*>(&pkt));
    }

    if (packets.empty()) {
        return Result<void>::ok();  // No matching channels, but not an error
    }

    // Send all packets coalesced into minimal datagrams
    return sendPackets(packets);
}

Result<void> DeviceSession::setChannelsSpikeSortingSync(const size_t nChans, const ChannelType chanType, const uint32_t sortOptions, const std::chrono::milliseconds timeout) {
    const size_t total_matching = std::min(nChans, countChannelsOfType(chanType, cbMAXCHANS));

    return sendAndWait(
        [this, nChans, chanType, sortOptions]() {
            return setChannelsSpikeSortingByType(nChans, chanType, sortOptions);
        },
        [](const cbPKT_HEADER* hdr) {
            return (hdr->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION &&
                   hdr->type == cbPKTTYPE_CHANREPSPKTHR;
        },
        timeout,
        total_matching
    );
}

Result<void> DeviceSession::setSpikeExtraction(const size_t nChans, const ChannelType chanType, const bool enabled) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }

    std::vector<cbPKT_GENERIC> packets;
    packets.reserve(nChans);

    for (size_t i = 0; i < nChans && i < cbMAXCHANS; ++i) {
        const uint32_t chan = i + 1;
        const auto& chaninfo = m_impl->device_config.chaninfo[i];

        if (!channelMatchesType(chaninfo, chanType))
            continue;

        cbPKT_CHANINFO pkt = chaninfo;
        pkt.cbpkt_header.type = cbPKTTYPE_CHANSETSPK;  // Spike set (not threshold)
        pkt.chan = chan;

        // Toggle the EXTRACT bit
        pkt.spkopts &= ~cbAINPSPK_EXTRACT;
        if (enabled)
            pkt.spkopts |= cbAINPSPK_EXTRACT;

        packets.push_back(*reinterpret_cast<cbPKT_GENERIC*>(&pkt));
    }

    if (packets.empty())
        return Result<void>::ok();

    return sendPackets(packets);
}

Result<void> DeviceSession::setChannelConfig(const cbPKT_CHANINFO& chaninfo) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }
    const auto& pkt = reinterpret_cast<const cbPKT_GENERIC&>(chaninfo);
    return sendPacket(pkt);
}

Result<void> DeviceSession::setDigitalOutput(const uint32_t chan_id, const uint16_t value) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }
    cbPKT_SET_DOUT pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_SET_DOUTSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_SET_DOUT;
    pkt.chan = static_cast<uint16_t>(chan_id);
    pkt.value = value;
    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

Result<void> DeviceSession::sendComment(const std::string& comment, const uint32_t rgba, const uint8_t charset) {
    if (!m_impl || !m_impl->connected) {
        return Result<void>::error("Device not connected");
    }
    cbPKT_COMMENT pkt = {};
    pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pkt.cbpkt_header.type = cbPKTTYPE_COMMENTSET;
    pkt.cbpkt_header.dlen = cbPKTDLEN_COMMENT;
    pkt.info.charset = charset;
    pkt.timeStarted = 0;
    pkt.rgba = rgba;

    const size_t len = std::min(comment.size(), static_cast<size_t>(cbMAX_COMMENT - 1));
    std::memcpy(pkt.comment, comment.c_str(), len);
    pkt.comment[len] = '\0';

    return sendPacket(reinterpret_cast<const cbPKT_GENERIC&>(pkt));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Management
///////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceSession::updateConfigFromBuffer(const void* buffer, const size_t bytes) {
    if (!buffer || bytes == 0 || !m_impl) {
        return;  // Nothing to process
    }

    // If probe-based clock sync is unreliable (e.g., NSP header->time
    // running at the wrong rate), fall back to data-packet timestamps.
    // Track the LAST data packet's timestamp in the datagram — it is
    // closest to the actual send time (earlier packets may be ~1 ms
    // stale from the start of the device's transmit batch).
    const bool need_fallback_clock_sample = !m_impl->clock_sync.probesAreReliable();
    uint64_t last_data_time = 0;

    // Parse packets in buffer and update device_config for configuration packets
    const auto* buff_bytes = static_cast<const uint8_t*>(buffer);
    size_t offset = 0;

    while (offset + cbPKT_HEADER_SIZE <= bytes) {
        // Read packet header
        const auto* header = reinterpret_cast<const cbPKT_HEADER*>(buff_bytes + offset);
        const size_t packet_size = cbPKT_HEADER_SIZE + (header->dlen * 4);

        // Verify complete packet
        if (offset + packet_size > bytes) {
            break;  // Incomplete packet
        }

        // Count every packet for protocol monitor drop detection
        m_impl->pkts_since_monitor++;

        // Track the last data packet timestamp for fallback clock sync.
        // Data timestamps come from the ADC/PTP clock and are reliable
        // even when probe header->time is broken.
        if (need_fallback_clock_sample &&
            !(header->chid & cbPKTCHAN_CONFIGURATION) &&
            header->time != 0) {
            last_data_time = header->time;
        }

        if ((header->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION) {
            // Configuration packet - process based on type
            if (header->type == cbPKTTYPE_SYSHEARTBEAT) {
                // TODO: Handle heartbeat if needed
                // Central uses this to prevent the system from going idle.
            }
            else if (header->type == cbPKTTYPE_LOGREP) {
                const auto* logrep = reinterpret_cast<const cbPKT_LOG*>(buff_bytes + offset);
                // TODO: if (logrep->mode == cbLOG_MODE_CRITICAL) {}
                // NODO: cbsdk uses this to process comments OnPktLog
            }
            else if ((header->type & 0xF0) == cbPKTTYPE_CHANREP) {
                // NODO: Optionally rename the channel label if this device has a prefix
                //  NSP{instrument}-{label} or Hub{instrument)-{label}
                // Note: Even though CHANSET* packets sent to the device only have some fields that are valid,
                //  and the device indeed only reads those fields, the CHANREP packets sent back by the device
                //  contain the full channel info structure with all fields valid.
                const auto* chaninfo = reinterpret_cast<const cbPKT_CHANINFO*>(buff_bytes + offset);
                if (const uint32_t chan = chaninfo->chan; chan > 0 && chan <= cbMAXCHANS) {
                    // The device always returns the complete channel info, even if only a subset changed.
                    m_impl->device_config.chaninfo[chan-1] = *chaninfo;
                    // Note: If this is exactly type == cbPKTTYPE_CHANREP, then we could invalidate cached spikes.
                    // spk_buffer->cache[chan-1].valid = 0;
                }
            }
            else if (header->type == cbPKTTYPE_REPCONFIGALL) {
                // Config flood starting - no action needed
            }
            else if ((header->type & 0xF0) == cbPKTTYPE_SYSREP) {
                const auto* sysinfo = reinterpret_cast<const cbPKT_SYSINFO*>(buff_bytes + offset);
                m_impl->device_config.sysinfo = *sysinfo;

                // If SYSREP arrives after PROCREP established non-Gemini identity,
                // update conversion factors now that sysfreq is available.
                if (!m_impl->timestamps_are_nanoseconds && sysinfo->sysfreq > 0) {
                    uint64_t g = std::gcd(uint64_t(1000000000), uint64_t(sysinfo->sysfreq));
                    m_impl->ts_convert_num = 1000000000 / g;
                    m_impl->ts_convert_den = sysinfo->sysfreq / g;
                }

                // Note: Clock sync probes now use nPlay packets (NPLAYREP), not SYSREPRUNLEV.
                // SYSREPRUNLEV is still processed here for config tracking (sysinfo update above).
            }
            else if (header->type == cbPKTTYPE_GROUPREP) {
                auto const *groupinfo = reinterpret_cast<const cbPKT_GROUPINFO*>(buff_bytes + offset);
                m_impl->device_config.groupinfo[groupinfo->group-1] = *groupinfo;
            }
            // else if (header->type == cbPKTTYPE_COMMENTREP) {
            //     // cbsdk uses this to update comment shared memory: OnPktComment
            // }
            else if (header->type == cbPKTTYPE_FILTREP) {
                auto const *filtinfo = reinterpret_cast<const cbPKT_FILTINFO*>(buff_bytes + offset);
                m_impl->device_config.filtinfo[filtinfo->filt-1] = *filtinfo;
            }
            else if (header->type == cbPKTTYPE_PROCREP) {
                m_impl->device_config.procinfo = *reinterpret_cast<const cbPKT_PROCINFO*>(buff_bytes + offset);

                // Determine timestamp units from processor identity.
                // Gemini devices report ident containing "gemini" and send nanosecond timestamps.
                // Non-Gemini devices (e.g. NPlay: "256-Channel player...") send sample counts.
                const auto& ident = m_impl->device_config.procinfo.ident;
                size_t ident_len = strnlen(ident, sizeof(m_impl->device_config.procinfo.ident));
                static constexpr char needle[] = "gemini";
                bool is_gemini = std::search(
                    ident, ident + ident_len,
                    std::begin(needle), std::end(needle) - 1,  // exclude null terminator
                    [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }
                ) != ident + ident_len;

                m_impl->timestamps_are_nanoseconds = is_gemini;

                if (!is_gemini) {
                    m_impl->clock_sync.reset();
                    uint32_t sysfreq = m_impl->device_config.sysinfo.sysfreq;
                    if (sysfreq > 0) {
                        uint64_t g = std::gcd(uint64_t(1000000000), uint64_t(sysfreq));
                        m_impl->ts_convert_num = 1000000000 / g;
                        m_impl->ts_convert_den = sysfreq / g;
                    }
                }
            }
            else if (header->type == cbPKTTYPE_BANKREP) {
                auto const *bankinfo = reinterpret_cast<const cbPKT_BANKINFO*>(buff_bytes + offset);
                if (bankinfo->bank < cbMAXBANKS) {
                    m_impl->device_config.bankinfo[bankinfo->bank] = *bankinfo;
                }
            }
            else if (header->type == cbPKTTYPE_SYSPROTOCOLMONITOR) {
                // Not used for clock sync — sent from firmware's 3rd thread after 2 queues,
                // giving it an undefined timestamp delay.
                const auto* mon = reinterpret_cast<const cbPKT_SYSPROTOCOLMONITOR*>(buff_bytes + offset);
                if (m_impl->first_monitor_seen && mon->sentpkts > 0) {
                    const uint32_t received = m_impl->pkts_since_monitor;
                    if (received < mon->sentpkts) {
                        m_impl->dropped_accum += mon->sentpkts - received;
                        m_impl->sent_accum += mon->sentpkts;
                        auto now = std::chrono::steady_clock::now();
                        if (now - m_impl->last_drop_log_time >= std::chrono::seconds(1)) {
                            fprintf(stderr, "[cbdev] dropped %u of %u packets\n",
                                    m_impl->dropped_accum, m_impl->sent_accum);
                            m_impl->dropped_accum = 0;
                            m_impl->sent_accum = 0;
                            m_impl->last_drop_log_time = now;
                        }
                    }
                }
                m_impl->first_monitor_seen = true;
                m_impl->pkts_since_monitor = 0;
            }
            else if (header->type == cbPKTTYPE_ADAPTFILTREP) {
                m_impl->device_config.adaptinfo = *reinterpret_cast<const cbPKT_ADAPTFILTINFO*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_REFELECFILTREP) {
                m_impl->device_config.refelecinfo = *reinterpret_cast<const cbPKT_REFELECFILTINFO*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_SS_MODELREP) {
                auto const *ssmodelrep = reinterpret_cast<const cbPKT_SS_MODELSET*>(buff_bytes + offset);
                uint32_t unit = ssmodelrep->unit_number;
                if (unit == 255) {
                    // Noise. Put it into the last slot.
                    unit = cbMAXUNITS + 1;  // +1 because we init the array with +2.
                }
                // Note: ssmodelrep->chan is 0-based, unlike most other channel fields.
                // Note: ssmodelrep->unit_number is 0-based because unit==0 means unsorted
                m_impl->device_config.spike_sorting.models[ssmodelrep->chan][unit] = *ssmodelrep;
            }
            else if (header->type == cbPKTTYPE_SS_STATUSREP) {
                m_impl->device_config.spike_sorting.status = *reinterpret_cast<const cbPKT_SS_STATUS*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_SS_DETECTREP) {
                m_impl->device_config.spike_sorting.detect = *reinterpret_cast<const cbPKT_SS_DETECT*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_SS_ARTIF_REJECTREP) {
                m_impl->device_config.spike_sorting.artifact_reject = *reinterpret_cast<const cbPKT_SS_ARTIF_REJECT*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_SS_NOISE_BOUNDARYREP) {
                auto const* noise_boundary = reinterpret_cast<const cbPKT_SS_NOISE_BOUNDARY*>(buff_bytes + offset);
                m_impl->device_config.spike_sorting.noise_boundary[noise_boundary->chan-1] = *noise_boundary;
            }
            else if (header->type == cbPKTTYPE_SS_STATISTICSREP) {
                m_impl->device_config.spike_sorting.statistics = *reinterpret_cast<const cbPKT_SS_STATISTICS*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_FS_BASISREP) {
                auto const* fs_basis = reinterpret_cast<const cbPKT_FS_BASIS*>(buff_bytes + offset);
                if (fs_basis->chan != 0) {  // chan==0 is for a request packet only
                    m_impl->device_config.spike_sorting.basis[fs_basis->chan-1] = *fs_basis;
                }
            }
            else if (header->type == cbPKTTYPE_LNCREP) {
                m_impl->device_config.lnc = *reinterpret_cast<const cbPKT_LNC*>(buff_bytes + offset);
            }
            else if (header->type == cbPKTTYPE_REPFILECFG) {
                auto const* filecfg = reinterpret_cast<const cbPKT_FILECFG*>(buff_bytes + offset);
                if (filecfg->options == cbFILECFG_OPT_REC
                    || filecfg->options == cbFILECFG_OPT_STOP
                    || filecfg->options == cbFILECFG_OPT_TIMEOUT) {
                    m_impl->device_config.fileinfo = *filecfg;
                }
            }
            // else if (header->type == cbPKTTYPE_REPINITIMPEDANCE) {
            //
            // }
            // else if (header->type == cbPKTTYPE_REPPOLL) {
            //
            // }
            // else if (header->type == cbPKTTYPE_SETUNITSELECTION) {
            //
            // }
            else if (header->type == cbPKTTYPE_REPNTRODEINFO) {
                auto const* ntrodeinfo = reinterpret_cast<const cbPKT_NTRODEINFO*>(buff_bytes + offset);
                m_impl->device_config.ntrodeinfo[ntrodeinfo->ntrode-1] = *ntrodeinfo;
            }
            // else if (header->type == cbPKTTYPE_NMREP) {
            //     // NODO: Abandon NM support
            // }
            else if (header->type == cbPKTTYPE_WAVEFORMREP) {
                const auto* waveformrep = reinterpret_cast<const cbPKT_AOUT_WAVEFORM*>(buff_bytes + offset);
                uint16_t chan = waveformrep->chan;
                // Analog out channels start after analog input channels
                if (chan > cbNUM_ANALOG_CHANS && chan <= cbNUM_ANALOG_CHANS + AOUT_NUM_GAIN_CHANS) {
                    uint16_t aout_idx = chan - cbNUM_ANALOG_CHANS - 1;
                    if (waveformrep->trigNum < cbMAX_AOUT_TRIGGER) {
                        m_impl->device_config.waveform[aout_idx][waveformrep->trigNum] = *waveformrep;
                    }
                }
            }
            else if (header->type == cbPKTTYPE_NPLAYREP) {
                // Complete pending clock sync probe from nPlay echo.
                const auto* nplay = reinterpret_cast<const cbPKT_NPLAY*>(buff_bytes + offset);
                std::lock_guard<std::mutex> lock(m_impl->clock_probe_mutex);
                if (m_impl->pending_clock_probe.active) {
                    // New firmware (>=7.9? with clock sync mod) writes fresh
                    // clock_gettime(ptp_clkid) into .etime — zero staleness.
                    // Old firmware echoes the packet unchanged, so .etime stays 0
                    // (we zero-initialize it). Fall back to header->time which is
                    // the stale ptptime from the previous main loop iteration.
                    constexpr uint64_t STALENESS_CORRECTION_NS = 165000;
                    uint64_t device_time_ns;
                    if (nplay->etime != 0) {
                        device_time_ns = nplay->etime;
                    } else {
                        device_time_ns = header->time;
                        if (!m_impl->timestamps_are_nanoseconds && m_impl->ts_convert_den > 1) {
                            device_time_ns = device_time_ns * m_impl->ts_convert_num / m_impl->ts_convert_den;
                        }
                        device_time_ns += STALENESS_CORRECTION_NS;
                    }

                    m_impl->clock_sync.addProbeSample(
                        m_impl->pending_clock_probe.t1_local,
                        device_time_ns,
                        m_impl->last_recv_timestamp);
                    m_impl->pending_clock_probe.active = false;
                }
            }

            std::lock_guard<std::mutex> lock(m_impl->pending_mutex);
            for (auto& pending : m_impl->pending_responses) {
                if (pending->received_count < pending->expected_count && pending->matcher(header)) {
                    std::lock_guard<std::mutex> resp_lock(pending->mutex);
                    pending->received_count++;
                    if (pending->received_count >= pending->expected_count) {
                        pending->cv.notify_all();
                    }
                }
            }
        }

        offset += packet_size;
    }

    // Feed the last data packet's timestamp for fallback clock sync.
    if (last_data_time != 0) {
        m_impl->clock_sync.addDataPacketSample(
            last_data_time, m_impl->last_recv_timestamp);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Receive Thread and Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

CallbackHandle DeviceSession::registerReceiveCallback(ReceiveCallback callback) {
    if (!m_impl || !callback) {
        return 0;  // Invalid
    }

    std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
    CallbackHandle handle = m_impl->next_callback_handle++;
    m_impl->receive_callbacks.push_back({handle, std::move(callback)});
    m_impl->has_callbacks.store(true, std::memory_order_release);
    return handle;
}

CallbackHandle DeviceSession::registerDatagramCompleteCallback(DatagramCompleteCallback callback) {
    if (!m_impl || !callback) {
        return 0;  // Invalid
    }

    std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
    CallbackHandle handle = m_impl->next_callback_handle++;
    m_impl->datagram_callbacks.push_back({handle, std::move(callback)});
    m_impl->has_callbacks.store(true, std::memory_order_release);
    return handle;
}

void DeviceSession::unregisterCallback(CallbackHandle handle) {
    if (!m_impl || handle == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_impl->callback_mutex);

    // Check receive callbacks
    auto& recv_cbs = m_impl->receive_callbacks;
    recv_cbs.erase(
        std::remove_if(recv_cbs.begin(), recv_cbs.end(),
            [handle](const Impl::CallbackRegistration& reg) {
                return reg.handle == handle;
            }),
        recv_cbs.end());

    // Check datagram callbacks
    auto& dg_cbs = m_impl->datagram_callbacks;
    dg_cbs.erase(
        std::remove_if(dg_cbs.begin(), dg_cbs.end(),
            [handle](const Impl::DatagramCallbackRegistration& reg) {
                return reg.handle == handle;
            }),
        dg_cbs.end());

    // Update fast-path flag
    m_impl->has_callbacks.store(
        !recv_cbs.empty() || !dg_cbs.empty(), std::memory_order_release);
}

Result<void> DeviceSession::startReceiveThread() {
    if (!m_impl) {
        return Result<void>::error("Device not initialized");
    }

    if (m_impl->receive_thread_running.load()) {
        return Result<void>::error("Receive thread already running");
    }

    m_impl->receive_thread_stop_requested.store(false);
    m_impl->receive_thread_running.store(true);

    m_impl->receive_thread = std::thread([this]() {
        // Receive buffer with padding. The extra sizeof(cbPKT_GENERIC) bytes ensure
        // that reinterpret_cast<cbPKT_GENERIC*>(&buffer[offset]) always has a full
        // struct's worth of readable memory, even for packets near the end of a
        // datagram. Without this, callbacks that copy the full 1024-byte struct
        // (e.g., SPSCQueue::push) would read past the buffer into unmapped memory.
        uint8_t buffer[cbCER_UDP_SIZE_MAX + sizeof(cbPKT_GENERIC)] = {};

        while (!m_impl->receive_thread_stop_requested.load()) {
            // Receive packets (only fill the actual datagram portion, not the padding)
            auto result = receivePackets(buffer, cbCER_UDP_SIZE_MAX);

            if (result.isError()) {
                // TODO: Could invoke an error callback here
                continue;
            }

            const int bytes_received = result.value();
            if (bytes_received == 0) {
                // No data available - brief sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            // Parse packets and invoke callbacks
            size_t offset = 0;
            while (offset + cbPKT_HEADER_SIZE <= static_cast<size_t>(bytes_received)) {
                const auto* pkt = reinterpret_cast<const cbPKT_GENERIC*>(&buffer[offset]);
                const size_t packet_size = cbPKT_HEADER_SIZE + (pkt->cbpkt_header.dlen * 4);

                // Verify complete packet
                if (offset + packet_size > static_cast<size_t>(bytes_received)) {
                    break;  // Incomplete packet
                }

                // Invoke receive callbacks (skip mutex if no callbacks registered)
                if (m_impl->has_callbacks.load(std::memory_order_acquire)) {
                    std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
                    for (const auto& reg : m_impl->receive_callbacks) {
                        reg.callback(*pkt);
                    }
                }

                offset += packet_size;
            }

            // Invoke datagram complete callbacks (skip mutex if no callbacks registered)
            if (m_impl->has_callbacks.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> lock(m_impl->callback_mutex);
                for (const auto& reg : m_impl->datagram_callbacks) {
                    reg.callback();
                }
            }
        }

        m_impl->receive_thread_running.store(false);
    });

    return Result<void>::ok();
}

void DeviceSession::stopReceiveThread() {
    if (m_impl) {
        m_impl->stopReceiveThreadInternal();
    }
}

bool DeviceSession::isReceiveThreadRunning() const {
    return m_impl && m_impl->receive_thread_running.load();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Response Waiter (General Mechanism)
///////////////////////////////////////////////////////////////////////////////////////////////////

DeviceSession::ResponseWaiter::ResponseWaiter(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl)) {}

DeviceSession::ResponseWaiter::~ResponseWaiter() {
    if (m_impl && m_impl->session && m_impl->session->m_impl) {
        // Remove this waiter from the pending list
        std::lock_guard<std::mutex> lock(m_impl->session->m_impl->pending_mutex);
        auto& vec = m_impl->session->m_impl->pending_responses;
        vec.erase(std::remove(vec.begin(), vec.end(), m_impl->response), vec.end());
    }
    // unique_ptr automatically cleans up m_impl
}

DeviceSession::ResponseWaiter::ResponseWaiter(ResponseWaiter&&) noexcept = default;

DeviceSession::ResponseWaiter& DeviceSession::ResponseWaiter::operator=(ResponseWaiter&&) noexcept = default;

Result<void> DeviceSession::ResponseWaiter::wait(const std::chrono::milliseconds timeout) {
    if (!m_impl || !m_impl->response) {
        return Result<void>::error("Invalid response waiter");
    }

    auto& response = m_impl->response;
    std::unique_lock<std::mutex> lock(response->mutex);

    if (response->received_count >= response->expected_count) {
        return Result<void>::ok();  // Already received all expected packets
    }

    if (response->cv.wait_for(lock, timeout, [&response] {
        return response->received_count >= response->expected_count;
    })) {
        return Result<void>::ok();
    } else {
        return Result<void>::error("Response timeout");
    }
}

DeviceSession::ResponseWaiter DeviceSession::registerResponseWaiter(
    std::function<bool(const cbPKT_HEADER*)> matcher,
        const size_t count) {

    auto response = std::make_shared<Impl::PendingResponse>();
    response->matcher = std::move(matcher);
    response->expected_count = count;

    {
        std::lock_guard<std::mutex> lock(m_impl->pending_mutex);
        m_impl->pending_responses.push_back(response);
    }

    // Create ResponseWaiter::Impl and wrap in unique_ptr
    auto waiter_impl = std::make_unique<ResponseWaiter::Impl>(response, this);
    return ResponseWaiter(std::move(waiter_impl));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Private Helper for Synchronous Operations
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> DeviceSession::sendAndWait(
    const std::function<Result<void>()>& sender,
    std::function<bool(const cbPKT_HEADER*)> matcher,
    const std::chrono::milliseconds timeout,
    const size_t count) {

    // Register waiter BEFORE sending packet (avoids race condition)
    auto waiter = registerResponseWaiter(std::move(matcher), count);

    // Send the request
    auto result = sender();
    if (result.isError()) {
        return result;
    }

    // Wait for response
    return waiter.wait(timeout);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

const char* protocolVersionToString(ProtocolVersion version) {
    switch (version) {
        case ProtocolVersion::UNKNOWN:
            return "Unknown";
        case ProtocolVersion::PROTOCOL_311:
            return "Protocol 3.11 (32-bit timestamps, 8-bit packet types)";
        case ProtocolVersion::PROTOCOL_400:
            return "Protocol 4.0 (64-bit timestamps, 8-bit packet types)";
        case ProtocolVersion::PROTOCOL_410:
            return "Protocol 4.1 (64-bit timestamps, 16-bit packet types)";
        case ProtocolVersion::PROTOCOL_CURRENT:
            return "Protocol 4.2+ (current)";
        default:
            return "Invalid Protocol Version";
    }
}

const char* deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::LEGACY_NSP:
            return "Legacy NSP";
        case DeviceType::NSP:
            return "Gemini NSP";
        case DeviceType::HUB1:
            return "Gemini Hub 1";
        case DeviceType::HUB2:
            return "Gemini Hub 2";
        case DeviceType::HUB3:
            return "Gemini Hub 3";
        case DeviceType::NPLAY:
            return "nPlayServer";
        case DeviceType::CUSTOM:
            return "Custom";
        default:
            return "Invalid Device Type";
    }
}

const char* channelTypeToString(ChannelType type) {
    switch (type) {
        case ChannelType::FRONTEND:
            return "Front-End Analog Input";
        case ChannelType::ANALOG_IN:
            return "Analog Input";
        case ChannelType::ANALOG_OUT:
            return "Analog Output";
        case ChannelType::AUDIO:
            return "Audio Output";
        case ChannelType::DIGITAL_IN:
            return "Digital Input";
        case ChannelType::SERIAL:
            return "Serial Input";
        case ChannelType::DIGITAL_OUT:
            return "Digital Output";
        default:
            return "Invalid Channel Type";
    }
}

const char* deviceRateToString(DeviceRate rate) {
    switch (rate) {
        case DeviceRate::NONE:
            return "None";
        case DeviceRate::SR_500:
            return "500 S/s";
        case DeviceRate::SR_1000:
            return "1000 S/s";
        case DeviceRate::SR_2000:
            return "2000 S/s";
        case DeviceRate::SR_10000:
            return "10000 S/s";
        case DeviceRate::SR_30000:
            return "30000 S/s";
        case DeviceRate::SR_RAW:
            return "Raw Stream";
        default:
            return "Invalid Device Rate";
    }
}

} // namespace cbdev