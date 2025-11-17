///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   protocol_detector.cpp
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Protocol version detection implementation
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbdev/protocol_detector.h>
#include <cbproto/cbproto.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

// Platform-specific networking
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET_ERROR_VALUE -1
    #define INVALID_SOCKET -1
    #define closesocket close
    typedef int SOCKET;
#endif

namespace cbdev {

/// State shared between main thread and receive thread
struct DetectionState {
    std::atomic<bool> done{false};
    std::atomic<ProtocolVersion> detected_version{ProtocolVersion::PROTOCOL_CURRENT};
    SOCKET sock;
};

/// Receive thread function - listens for packets and analyzes protocol version
void receiveThread(DetectionState* state) {
    uint8_t buffer[8192];

    while (!state->done) {
        const int bytes_received = recv(state->sock, (char*)buffer, sizeof(buffer), 0);

        if (bytes_received == SOCKET_ERROR_VALUE) {
            // Timeout or error - thread will be stopped by main thread
            continue;
        }

        // Analyze packet header for protocol version indicators
        // IMPORTANT: We must check protocol version BEFORE interpreting any fields,
        // because the fields are at different offsets! In the table below, values are in bits (b).
        // | version | time | chid | type | dlen | instrument | reserved | sysfreq | spikelen | spikepre | resetque | runlevel | runflags | total |
        // |---------|------|------|------|------|------------|----------|---------|----------|----------|----------|----------|----------|-------|
        // | 3.11    | 32b  | 16b  | 8b   | 8b   | 0b         | 0b       | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 256b  |
        // | 4.0     | 64b  | 16b  | 8b   | 16b  | 8b         | 16b      | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 320b  |
        // | 4.1+    | 64b  | 16b  | 16b  | 16b  | 8b         | 8b       | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 320b  |
        // Expected values:
        //      chid: cbPKTCHAN_CONFIGURATION
        //      type: cbPKTTYPE_SYSREPRUNLEV
        //      dlen: 6 (for SYSREPRUNLEV packets)
        //      sysfreq: 30000 (0x7530)

        if (bytes_received < 32) {
            continue;  // Smaller than minimum SYSREPRUNLEV packet size (3.11; 256b = 32 bytes)
        }

        const auto* pkt = reinterpret_cast<cbPKT_GENERIC *>(buffer);
        const auto* p16 = reinterpret_cast<uint16_t *>(buffer);

        if (
            (p16[2] == cbPKTCHAN_CONFIGURATION)
            && (buffer[6] == cbPKTTYPE_SYSREPRUNLEV)
            && (p16[3] == 6)
        ) {
            state->detected_version = ProtocolVersion::PROTOCOL_311;
            state->done = true;
            return;
        }
        if (
            (pkt->cbpkt_header.chid == cbPKTCHAN_CONFIGURATION)
            && (buffer[10] == cbPKTTYPE_SYSREPRUNLEV)
            // Can't easily check dlen
        ) {
            state->detected_version = ProtocolVersion::PROTOCOL_400;
            state->done = true;
            return;
        }
        if (
            (pkt->cbpkt_header.chid == cbPKTCHAN_CONFIGURATION)
            && (pkt->cbpkt_header.type == cbPKTTYPE_SYSREPRUNLEV)
        ) {
            state->detected_version = ProtocolVersion::PROTOCOL_CURRENT;
            state->done = true;
            return;
        }
    }
}

const char* protocolVersionToString(ProtocolVersion version) {
    switch (version) {
        case ProtocolVersion::UNKNOWN:          return "UNKNOWN";
        case ProtocolVersion::PROTOCOL_311:     return "cbproto 3.11";
        case ProtocolVersion::PROTOCOL_400:     return "cbproto 4.0";
        case ProtocolVersion::PROTOCOL_CURRENT: return "cbproto >= 4.1 (current)";
        default:                                return "INVALID";
    }
}

Result<ProtocolVersion> detectProtocol(const char* device_addr, uint16_t send_port,
                                       const char* client_addr, uint16_t recv_port,
                                       const uint32_t timeout_ms) {
    // Create temporary UDP socket for probing
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return Result<ProtocolVersion>::error("Failed to create probe socket");
    }

    // Bind to client address/port
    sockaddr_in client_sockaddr = {};
    client_sockaddr.sin_family = AF_INET;
    client_sockaddr.sin_port = htons(recv_port);

    if (client_addr && strlen(client_addr) > 0 && strcmp(client_addr, "0.0.0.0") != 0) {
        inet_pton(AF_INET, client_addr, &client_sockaddr.sin_addr);
    } else {
        client_sockaddr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(sock, reinterpret_cast<sockaddr *>(&client_sockaddr), sizeof(client_sockaddr)) == SOCKET_ERROR_VALUE) {
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to bind probe socket");
    }

    // Set socket timeout for receive (for the thread)
    // Use a short timeout so the thread can check 'done' flag frequently
#ifdef _WIN32
    DWORD recv_timeout = 100;  // 100ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recv_timeout, sizeof(recv_timeout));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // Prepare device address for sending
    sockaddr_in device_sockaddr = {};
    device_sockaddr.sin_family = AF_INET;
    device_sockaddr.sin_port = htons(send_port);
    inet_pton(AF_INET, device_addr, &device_sockaddr.sin_addr);

    // Create detection state and start receive thread BEFORE sending probes
    DetectionState state;
    state.sock = sock;
    state.done = false;
    state.detected_version = ProtocolVersion::PROTOCOL_CURRENT;

    // Prepare probe packet in protocol 4.1 format (cbPKTTYPE_SYSSETRUNLEV with cbRUNLEVEL_RUNNING)
    // Note: We'd rather send REQCONFIGALL because the first packet in the response has explicit protocol version info,
    // that packet is not replied to by devices in standby mode, and we can't take it out of standby mode without
    // first establishing the protocol!
    cbPKT_SYSINFO probe_41 = {};
    probe_41.cbpkt_header.time = 1;
    probe_41.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    probe_41.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
    probe_41.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;
    probe_41.cbpkt_header.instrument = 0;
    probe_41.runlevel = cbRUNLEVEL_RUNNING;
    probe_41.resetque = 0;
    probe_41.runflags = 0;

    // Prepare probe packet in protocol 4.0 format (64-bit timestamp, 8-bit type)
    // See table in receiveThread for detailed differences.
    // Protocol 4.0 layout: time(64b) chid(16b) type(8b) dlen(16b) instrument(8b) reserved(16b) payload...
    constexpr int HEADER_SIZE_400 = 16;
    constexpr int PAYLOAD_SIZE = 24;
    uint8_t probe_400[HEADER_SIZE_400 + PAYLOAD_SIZE] = {};
    *reinterpret_cast<uint64_t*>(&probe_400[0]) = 1;                           // time (64-bit) at byte 0
    *reinterpret_cast<uint16_t*>(&probe_400[8]) = cbPKTCHAN_CONFIGURATION;     // chid (16-bit) at byte 8
    probe_400[10] = cbPKTTYPE_SYSSETRUNLEV;                                    // type (8-bit) at byte 10
    *reinterpret_cast<uint16_t*>(&probe_400[11]) = PAYLOAD_SIZE / 4;           // dlen (16-bit) at byte 11
    // Add SYSINFO payload (all zeros: sysfreq, spikelen, spikepre, resetque)
    *reinterpret_cast<uint32_t*>(&probe_400[36]) = cbRUNLEVEL_RUNNING;         // runlevel (32-bit) at byte 36

    // Prepare probe packet in protocol 3.11 format (32-bit timestamp, 8-bit type)
    // See table in receiveThread for detailed differences.
    // Protocol 3.11 layout: time(32b) chid(16b) type(8b) dlen(8b) payload...
    constexpr int HEADER_SIZE_311 = 8;
    uint8_t probe_311[HEADER_SIZE_311 + PAYLOAD_SIZE] = {};
    *reinterpret_cast<uint32_t*>(&probe_311[0]) = 1;                           // time (32-bit) at byte 0
    *reinterpret_cast<uint16_t*>(&probe_311[4]) = cbPKTCHAN_CONFIGURATION;     // chid (16-bit) at byte 4
    probe_311[6] = cbPKTTYPE_SYSSETRUNLEV;                                     // type (8-bit) at byte 6
    probe_311[7] = PAYLOAD_SIZE / 4;                                           // dlen (8-bit) at byte 7
    // Add SYSINFO payload (all zeros: sysfreq, spikelen, spikepre, resetque)
    *reinterpret_cast<uint32_t*>(&probe_311[24]) = cbRUNLEVEL_RUNNING;         // runlevel (32-bit) at byte 24

    // Start the receive thread before sending the probe packets.
    std::thread recv_thread(receiveThread, &state);

    // Send the probe packets
    if (sendto(sock, (const char*)&probe_41, sizeof(cbPKT_SYSINFO), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 4.1 probe packet");
    }

    if (sendto(sock, probe_400, sizeof(probe_400), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 4.0 probe packet");
               }

    if (sendto(sock, probe_311, sizeof(probe_311), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 3.11 probe packet");
    }

    // Wait for receive thread to detect protocol or timeout
    const auto start_time = std::chrono::steady_clock::now();
    while (!state.done) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        if (elapsed >= timeout_ms) {
            // Timeout - stop thread and return default (current protocol)
            break;
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Stop receive thread
    state.done = true;
    recv_thread.join();
    closesocket(sock);

    return Result<ProtocolVersion>::ok(state.detected_version.load());
}

} // namespace cbdev
