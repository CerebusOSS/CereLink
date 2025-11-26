///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   protocol_detector.cpp
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Protocol version detection implementation
///
/// Header layout (values in bits):
///
/// | version | time | chid | type | dlen | instrument | reserved |
/// |---------|------|------|------|------|------------|----------|
/// | 3.11    | 32b  | 16b  | 8b   | 8b   | 0b         | 0b       |
/// | 4.0     | 64b  | 16b  | 8b   | 16b  | 8b         | 16b      |
/// | 4.1+    | 64b  | 16b  | 16b  | 16b  | 8b         | 8b       |
///
/// Then, if it's a SYSREPRUNLEV packet, the payload contains:
///
/// | version | sysfreq | spikelen | spikepre | resetque | runlevel | runflags | total |
/// |---------|---------|----------|----------|----------|----------|----------|-------|
/// | 3.11    | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 256b  |
/// | 4.0     | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 320b  |
/// | 4.1+    | 32b     | 32b      | 32b      | 32b      | 32b      | 32b      | 320b  |
/// Expected values:
///      chid: cbPKTCHAN_CONFIGURATION
///      type: cbPKTTYPE_SYSREPRUNLEV
///      dlen: 6 (for SYSREPRUNLEV packets)
///      sysfreq: 30000 (0x7530)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "protocol_detector.h"
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
    #include <ifaddrs.h>
    #define SOCKET_ERROR_VALUE -1
    #define INVALID_SOCKET -1
    #define closesocket close
    typedef int SOCKET;
#endif

namespace cbdev {

/// State shared between main thread and receive thread
struct DetectionState {
    std::atomic<bool> send_config{false};
    std::atomic<bool> done{false};
    // TODO: State machine to create REQCONFIGALL and capture the first packet in the response
    std::atomic<ProtocolVersion> detected_version{ProtocolVersion::PROTOCOL_CURRENT};
    SOCKET sock = INVALID_SOCKET;
    // Debug counters
    std::atomic<size_t> packets_seen{0};
    std::atomic<size_t> bytes_received{0};
    std::atomic<size_t> config_packets_seen{0};
    std::atomic<size_t> procrep_seen{0};
};

/// @brief Guess packet size when protocol version is ambiguous
/// @param buffer Pointer to start of packet (at least 16 bytes available)
/// @param buffer_size Remaining bytes in buffer
/// @return Number of bytes to advance, or 0 if unable to determine
///
/// Reads all three possible dlen locations (3.11, 4.0, 4.1+) and uses heuristics
/// to determine the most likely packet size.
static size_t guessPacketSize(const uint8_t* buffer, size_t buffer_size) {
    if (buffer_size < 16) {
        return 0;  // Not enough data to analyze
    }

    // Read all three possible dlen interpretations
    constexpr size_t HEADER_311 = 8;
    constexpr size_t HEADER_400 = 16;
    constexpr size_t HEADER_410 = 16;

    uint8_t dlen_311 = buffer[7];                                          // 3.11: byte 7
    uint16_t dlen_400 = *reinterpret_cast<const uint16_t*>(&buffer[11]);  // 4.0: bytes 11-12
    uint16_t dlen_410 = *reinterpret_cast<const uint16_t*>(&buffer[12]);  // 4.1+: bytes 12-13

    // Calculate potential packet sizes
    size_t size_311 = HEADER_311 + dlen_311 * 4;
    size_t size_400 = HEADER_400 + dlen_400 * 4;
    size_t size_410 = HEADER_410 + dlen_410 * 4;

    // Validate against maximum packet size
    constexpr size_t MAX_SIZE = 1024;
    bool valid_311 = (size_311 <= MAX_SIZE);
    bool valid_400 = (size_400 <= MAX_SIZE);
    bool valid_410 = (size_410 <= MAX_SIZE);

    // Count valid interpretations
    int valid_count = valid_311 + valid_400 + valid_410;

    if (valid_count == 0) {
        return 0;  // All invalid - corrupted data
    }

    if (valid_count == 1) {
        // Only one valid interpretation - use it
        if (valid_311) return size_311;
        if (valid_400) return size_400;
        if (valid_410) return size_410;
    }

    // Multiple valid interpretations - use heuristics to disambiguate

    // Heuristic 1: Timestamp magnitude
    // 4.0/4.1+ use 64-bit timestamps (often nanoseconds, which are huge numbers)
    // 3.11 first 8 bytes are: time(32) | chid(16) | type(8) | dlen(8)
    uint64_t first_8_bytes = *reinterpret_cast<const uint64_t*>(&buffer[0]);
    if (first_8_bytes > (1ULL << 40)) {  // > ~1 trillion
        // Almost certainly a real 64-bit timestamp (4.0 or 4.1+)
        // Prefer 4.1+ over 4.0 (newer protocol)
        if (valid_410) return size_410;
        if (valid_400) return size_400;
    }

    // Heuristic 2: Channel validity
    // Channels have known valid ranges
    uint16_t chid_311 = *reinterpret_cast<const uint16_t*>(&buffer[4]);
    uint16_t chid_400 = *reinterpret_cast<const uint16_t*>(&buffer[8]);

    auto is_valid_chid = [](uint16_t chid) {
        return (chid == 0) ||                          // Special channel
               (chid >= 1 && chid <= 768) ||           // Front-end channels (Gemini max)
               (chid >= 0x8000 && chid <= 0x8FFF);     // Configuration channels
    };

    bool valid_chid_311 = is_valid_chid(chid_311);
    bool valid_chid_400 = is_valid_chid(chid_400);  // Same for 4.0 and 4.1+

    if (valid_chid_400 && !valid_chid_311) {
        // 4.0 or 4.1+ more likely
        if (valid_410) return size_410;
        if (valid_400) return size_400;
    } else if (valid_chid_311 && !valid_chid_400) {
        if (valid_311) return size_311;
    }

    // Heuristic 3: Type validity
    // Check if packet type is in known ranges
    uint8_t type_311 = buffer[6];
    uint8_t type_400 = buffer[10];
    uint16_t type_410 = *reinterpret_cast<const uint16_t*>(&buffer[10]);

    auto is_valid_type = [](uint16_t type) {
        // Known packet type ranges (see cbproto/types.h)
        return (type >= 0x01 && type <= 0x10) ||   // System types
               (type >= 0x20 && type <= 0x30) ||   // Channel info types
               (type >= 0x40 && type <= 0x5F) ||   // Data/preview types
               (type >= 0x81 && type <= 0x90) ||   // SET types
               (type >= 0xA1 && type <= 0xCF) ||   // REP types
               (type >= 0xD0 && type <= 0xEF);     // Config/file types
    };

    int valid_type_count = is_valid_type(type_311) +
                          is_valid_type(type_400) +
                          is_valid_type(type_410);

    if (valid_type_count == 1) {
        if (is_valid_type(type_311) && valid_311) return size_311;
        if (is_valid_type(type_400) && valid_400) return size_400;
        if (is_valid_type(type_410) && valid_410) return size_410;
    }

    // Heuristic 4: Default to most recent protocol if still ambiguous
    // This is a reasonable assumption for newer devices
    if (valid_410) return size_410;
    if (valid_400) return size_400;
    if (valid_311) return size_311;

    return 0;  // Should never reach here
}

/// Receive thread function - listens for packets and analyzes protocol version
void receiveThread(DetectionState* state) {
    while (!state->done) {
        uint8_t buffer[cbPKT_MAX_SIZE * 1024];
        const int bytes_received = recv(state->sock, reinterpret_cast<char *>(buffer), sizeof(buffer), 0);

        if (bytes_received == SOCKET_ERROR_VALUE) {
            // Timeout or error - thread will be stopped by main thread
            continue;
        }
        state->bytes_received += bytes_received;

        size_t offset = 0;
        while (offset < bytes_received && bytes_received - offset >= 32) {
            ++state->packets_seen;

            // Remaining bytes in buffer are at least as large as 3.11 SYSINFO packet.
            // This filters out some smaller packets we might want to ignore.
            // Try 3.11 SYSREPRUNLEV first
            if (
                (*reinterpret_cast<uint16_t*>(buffer + offset + 4) == cbPKTCHAN_CONFIGURATION)
                && (buffer[offset + 6] == cbPKTTYPE_SYSREPRUNLEV)
                && (buffer[offset + 8] == cbPKTDLEN_SYSINFO)
            ) {
                state->detected_version = ProtocolVersion::PROTOCOL_311;
                state->done = true;
                return;
            }

            // If remaining bytes aren't big enough for a 4.0+ SYSINFO packet, break.
            if (bytes_received - offset < cbPKTDLEN_SYSINFO * 4 + cbPKT_HEADER_SIZE) {
                break;
            }

            // Try 4.0 SYSREPRUNLEV-type SYSINFO packet.
            if (
                (*reinterpret_cast<uint16_t*>(buffer + offset + 8) == cbPKTCHAN_CONFIGURATION)
                && (buffer[offset + 10] == cbPKTTYPE_SYSREPRUNLEV)
                && (*reinterpret_cast<uint16_t*>(buffer + offset + 11) == cbPKTDLEN_SYSINFO)
            ) {
                    state->detected_version = ProtocolVersion::PROTOCOL_400;
                    state->done = true;
                    return;
            }

            // Try 4.1+ packets.
            const auto* pkt_header = reinterpret_cast<const cbPKT_HEADER *>(buffer + offset);
            if ((pkt_header->chid == cbPKTCHAN_CONFIGURATION)) {
                ++state->config_packets_seen;
                if (pkt_header->type == cbPKTTYPE_SYSREPRUNLEV) {
                    // We cannot distinguish between 4.1 and 4.2 based solely on SYSREPRUNLEV
                    // Send a REQCONFIGALL and await the response.
                    state->send_config = true;
                }
                else if (pkt_header->type == cbPKTTYPE_PROCREP) {
                    ++state->procrep_seen;

                    // If we received PROCREP then we can inspect it to distinguish between 4.1 and 4.2+.
                    const auto* pkt_procinfo = reinterpret_cast<const cbPKT_PROCINFO*>(buffer + offset);

                    // Extract major and minor version from packed uint32_t
                    // Undo MAKELONG(cbVERSION_MINOR, cbVERSION_MAJOR)
                    // Where #define MAKELONG(a, b)      ((a & 0xffff) | ((b & 0xffff) << 16))
                    uint32_t version = pkt_procinfo->version;
                    uint16_t minor_version = version & 0xFFFF;         // Lower 16 bits
                    uint16_t major_version = (version >> 16) & 0xFFFF; // Upper 16 bits

                    // Distinguish between 4.1 and 4.2+
                    if (major_version == 4) {
                        if (minor_version == 1) {
                            state->detected_version = ProtocolVersion::PROTOCOL_410;
                        } else if (minor_version >= 2) {
                            state->detected_version = ProtocolVersion::PROTOCOL_CURRENT;
                        } else {
                            // 4.0 or earlier - shouldn't happen if we got PROCREP
                            state->detected_version = ProtocolVersion::PROTOCOL_400;
                        }
                    } else if (major_version > 4) {
                        // Future protocol version - treat as current
                        state->detected_version = ProtocolVersion::PROTOCOL_CURRENT;
                    } else {
                        // Older major version
                        state->detected_version = ProtocolVersion::PROTOCOL_400;
                    }

                    state->done = true;
                    return;
                }
            }

            // If we didn't match any specific protocol check above, we need to guess
            // the packet size to properly advance the offset
            size_t pkt_size = guessPacketSize(buffer + offset, bytes_received - offset);
            if (pkt_size == 0) {
                // Unable to determine packet size - skip to next datagram
                break;
            }
            offset += pkt_size;
        }
    }
}
Result<ProtocolVersion> detectProtocol(const char* device_addr, uint16_t send_port,
                                       const char* client_addr, uint16_t recv_port,
                                       const uint32_t timeout_ms) {
#ifdef _WIN32
    // Initialize Winsock on Windows before using socket APIs
    WSADATA wsaData;
    int wsaInit = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaInit != 0) {
        return Result<ProtocolVersion>::error("WSAStartup failed");
    }
#endif
    // Create temporary UDP socket for probing
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<ProtocolVersion>::error("Failed to create probe socket");
    }

    // Set socket options for broadcast
    int opt_one = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt_one), sizeof(opt_one));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_one), sizeof(opt_one));

    // Bind to client address/port
    sockaddr_in client_sockaddr = {};
    client_sockaddr.sin_family = AF_INET;
    client_sockaddr.sin_port = htons(recv_port);

    if (client_addr && strlen(client_addr) > 0 && strcmp(client_addr, "0.0.0.0") != 0) {
        inet_pton(AF_INET, client_addr, &client_sockaddr.sin_addr);
    } else {
#if defined(__linux__)
        // Linux: To receive UDP broadcast packets, we must bind to the broadcast address.
        // First check if we have a 192.168.137.x interface, if so use its broadcast address.
        bool found_cerebus_interface = false;
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) != -1) {
            for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;
                struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                uint32_t ip = ntohl(addr->sin_addr.s_addr);
                // Check if this is a 192.168.137.x address (0xC0A889xx)
                if ((ip & 0xFFFFFF00) == 0xC0A88900) {
                    // Use the broadcast address for this subnet
                    inet_pton(AF_INET, cbNET_UDP_ADDR_BCAST, &client_sockaddr.sin_addr);
                    found_cerebus_interface = true;
                    break;
                }
            }
            freeifaddrs(ifaddr);
        }
        if (!found_cerebus_interface) {
            client_sockaddr.sin_addr.s_addr = INADDR_ANY;
        }
#else
        client_sockaddr.sin_addr.s_addr = INADDR_ANY;
#endif
    }

    if (bind(sock, reinterpret_cast<sockaddr *>(&client_sockaddr), sizeof(client_sockaddr)) == SOCKET_ERROR_VALUE) {
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return Result<ProtocolVersion>::error("Failed to bind probe socket");
    }

    // Set socket timeout for receive (for the thread)
    // Use a short timeout so the thread can check 'done' flag frequently
#ifdef _WIN32
    DWORD recv_timeout = 500;  // 500ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&recv_timeout), sizeof(recv_timeout));
    // Increase socket buffers to handle bursts
    int bufSize = 8 * 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;  // 500ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    // Increase socket buffers to handle bursts
    int bufSize = 8 * 1024 * 1024; // 8 MB
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
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
    state.send_config = false;
    state.detected_version = ProtocolVersion::PROTOCOL_CURRENT;

    // Prepare probe packet in current protocol (cbPKTTYPE_SYSSETRUNLEV with cbRUNLEVEL_RUNNING)
    // Note: We'd rather send REQCONFIGALL because the first packet in the response has explicit protocol version info,
    // that packet is not replied to by devices in standby mode, and we can't take it out of standby mode without
    // first establishing the protocol!
    cbPKT_SYSINFO runlev = {};
    runlev.cbpkt_header.time = 1;
    runlev.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    runlev.cbpkt_header.type = cbPKTTYPE_SYSSETRUNLEV;
    runlev.cbpkt_header.dlen = cbPKTDLEN_SYSINFO;
    runlev.cbpkt_header.instrument = 0;
    runlev.runlevel = cbRUNLEVEL_RUNNING;
    runlev.resetque = 0;
    runlev.runflags = 0;

    // cbPKT_GENERIC pktgeneric;
    // pktgeneric.cbpkt_header.time = 1;
    // pktgeneric.cbpkt_header.chid = 0x8000;
    // pktgeneric.cbpkt_header.type = cbPKTTYPE_REQCONFIGALL;
    // pktgeneric.cbpkt_header.dlen = 0;
    // pktgeneric.cbpkt_header.instrument = 0;

    // Prepare runlev packet in protocol 4.0 format (64-bit timestamp, 8-bit type)
    // See table in receiveThread for detailed differences.
    // Protocol 4.0 layout: time(64b) chid(16b) type(8b) dlen(16b) instrument(8b) reserved(16b) payload...
    constexpr int HEADER_SIZE_400 = 16;
    constexpr int PAYLOAD_SIZE = 24;
    uint8_t runlev_400[HEADER_SIZE_400 + PAYLOAD_SIZE] = {};
    *reinterpret_cast<uint64_t*>(&runlev_400[0]) = 1;                           // time (64-bit) at byte 0
    *reinterpret_cast<uint16_t*>(&runlev_400[8]) = cbPKTCHAN_CONFIGURATION;     // chid (16-bit) at byte 8
    runlev_400[10] = cbPKTTYPE_SYSSETRUNLEV;                                    // type (8-bit) at byte 10
    *reinterpret_cast<uint16_t*>(&runlev_400[11]) = PAYLOAD_SIZE / 4;           // dlen (16-bit) at byte 11
    // Add SYSINFO payload (all zeros: sysfreq, spikelen, spikepre, resetque)
    *reinterpret_cast<uint32_t*>(&runlev_400[36]) = cbRUNLEVEL_RUNNING;         // runlevel (32-bit) at byte 36

    // Prepare runlev packet in protocol 3.11 format (32-bit timestamp, 8-bit type)
    // See table in receiveThread for detailed differences.
    // Protocol 3.11 layout: time(32b) chid(16b) type(8b) dlen(8b) payload...
    constexpr int HEADER_SIZE_311 = 8;
    uint8_t runlev_311[HEADER_SIZE_311 + PAYLOAD_SIZE] = {};
    *reinterpret_cast<uint32_t*>(&runlev_311[0]) = 1;                           // time (32-bit) at byte 0
    *reinterpret_cast<uint16_t*>(&runlev_311[4]) = cbPKTCHAN_CONFIGURATION;     // chid (16-bit) at byte 4
    runlev_311[6] = cbPKTTYPE_SYSSETRUNLEV;                                     // type (8-bit) at byte 6
    runlev_311[7] = PAYLOAD_SIZE / 4;                                           // dlen (8-bit) at byte 7
    // Add SYSINFO payload (all zeros: sysfreq, spikelen, spikepre, resetque)
    *reinterpret_cast<uint32_t*>(&runlev_311[24]) = cbRUNLEVEL_RUNNING;         // runlevel (32-bit) at byte 24

    // Start the receive thread before sending the runlev packets.
    std::thread recv_thread(receiveThread, &state);

    // Send the runlev packets
    if (sendto(sock, reinterpret_cast<const char *>(&runlev), sizeof(cbPKT_SYSINFO), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 4.1 runlev packet");
    }

    std::this_thread::sleep_for(std::chrono::microseconds(50));
    if (sendto(sock, reinterpret_cast<const char *>(runlev_400), sizeof(runlev_400), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 4.0 runlev packet");
    }

    std::this_thread::sleep_for(std::chrono::microseconds(50));
    if (sendto(sock, reinterpret_cast<const char *>(runlev_311), sizeof(runlev_311), 0,
               reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr)) == SOCKET_ERROR_VALUE) {
        state.done = true;
        recv_thread.join();
        closesocket(sock);
        return Result<ProtocolVersion>::error("Failed to send 3.11 runlev packet");
    }

    // Wait for receive thread to detect protocol or timeout
    const auto start_time = std::chrono::steady_clock::now();
    bool timed_out = false;
    while (!state.done) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        if (elapsed >= timeout_ms) {
            // Timeout - stop thread and return default (current protocol)
            timed_out = true;
            break;
        }
        if (state.send_config.load()) {
            // Send REQCONFIGALL packet to elicit PROCINFO packet.
            cbPKT_GENERIC pkt;
            pkt.cbpkt_header.time = 1;
            pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
            pkt.cbpkt_header.type = cbPKTTYPE_REQCONFIGALL;
            pkt.cbpkt_header.dlen = 0;
            pkt.cbpkt_header.instrument = 0;

            sendto(sock, reinterpret_cast<const char *>(&pkt), cbPKT_HEADER_SIZE, 0,
                   reinterpret_cast<sockaddr *>(&device_sockaddr), sizeof(device_sockaddr));

            state.send_config = false;
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Stop receive thread
    state.done = true;
    recv_thread.join();
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    if (timed_out) {
        // Provide debug info on what we observed
        char msg[256];
        snprintf(msg, sizeof(msg), "Timed out: packets=%llu, config=%llu, procrep=%llu, bytes=%llu",
                 (unsigned long long)state.packets_seen.load(),
                 (unsigned long long)state.config_packets_seen.load(),
                 (unsigned long long)state.procrep_seen.load(),
                 (unsigned long long)state.bytes_received.load());
        return Result<ProtocolVersion>::error(msg);
    }
    return Result<ProtocolVersion>::ok(state.detected_version.load());
}

} // namespace cbdev
