///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   probe_timing.cpp
/// @brief  Measure nPlay clock probe forward/reverse delays using PTP timestamps
///
/// Sends nPlay probe packets to a Gemini Hub and measures the one-way delays
/// using PTP hardware clock timestamps on both sides. Requires:
///   - RPi5 with PTP slave synced to Hub (ptp4l -i eth0 -s -m --priority1=255)
///   - Hub firmware with fresh PTP timestamp in .etime (ProcessPktNplaySet mod)
///
/// All timestamps are in the PTP clock domain, so the deltas are true network delays
/// (assuming PTP sync error is small relative to network delay).
///
/// Usage:
///   ./probe_timing --ptp-device /dev/ptp0 [OPTIONS]
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#include <cbproto/cbproto.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <numeric>
#include <signal.h>
#include <sys/socket.h>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#ifndef FD_TO_CLOCKID
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | 3)
#endif

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int) {
    g_running = 0;
}

static inline int64_t timespec_to_ns(const struct timespec& ts) {
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

static inline int64_t read_phc_ns(clockid_t phc_clockid) {
    struct timespec ts;
    clock_gettime(phc_clockid, &ts);
    return timespec_to_ns(ts);
}

struct Options {
    const char* ptp_device = nullptr;
    const char* device_addr = "192.168.137.200";
    int port = 51002;
    int count = 100;
    int interval_ms = 1000;
    int recv_timeout_ms = 500;
};

struct ProbeResult {
    int64_t t1_ptp;       // PHC time at send
    int64_t t3_etime;     // Fresh device PTP time (.etime)
    int64_t t3_stale;     // Stale device time (header->time)
    int64_t t4_ptp;       // PHC time at receive
    int64_t d1;           // Forward delay: t3_etime - t1_ptp
    int64_t d2;           // Reverse delay: t4_ptp - t3_etime
    int64_t rtt;          // Round trip: t4_ptp - t1_ptp
    int64_t staleness;    // t3_etime - t3_stale
};

static void print_stats(const char* label, const std::vector<int64_t>& values) {
    if (values.empty()) {
        fprintf(stderr, "  %s: no data\n", label);
        return;
    }

    std::vector<int64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    double sum = 0, sum_sq = 0;
    for (auto v : sorted) {
        sum += v;
        sum_sq += static_cast<double>(v) * v;
    }
    double mean = sum / sorted.size();
    double variance = (sum_sq / sorted.size()) - (mean * mean);
    double stddev = std::sqrt(std::max(0.0, variance));

    int64_t min = sorted.front();
    int64_t max = sorted.back();
    int64_t median = sorted[sorted.size() / 2];
    int64_t p5 = sorted[sorted.size() * 5 / 100];
    int64_t p95 = sorted[sorted.size() * 95 / 100];

    fprintf(stderr, "  %-20s  n=%-4zu  min=%6ld  p5=%6ld  median=%6ld  mean=%8.1f  p95=%6ld  max=%6ld  stddev=%7.1f  (ns)\n",
            label, sorted.size(), min, p5, median, mean, p95, max, stddev);
}

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s --ptp-device /dev/ptpN [OPTIONS]\n"
        "\n"
        "Measures nPlay clock probe forward/reverse delays using PTP timestamps.\n"
        "\n"
        "Options:\n"
        "  --ptp-device PATH     PTP hardware clock device (required)\n"
        "  --device-addr ADDR    Hub IP address (default: 192.168.137.200)\n"
        "  --port PORT           Hub UDP port (default: 51002)\n"
        "  --count N             Number of probes to send (default: 100)\n"
        "  --interval MS         Interval between probes in ms (default: 1000)\n"
        "  --timeout MS          Receive timeout per probe in ms (default: 500)\n",
        prog);
}

int main(int argc, char* argv[]) {
    Options opts;

    static struct option long_options[] = {
        {"ptp-device",  required_argument, nullptr, 'p'},
        {"device-addr", required_argument, nullptr, 'a'},
        {"port",        required_argument, nullptr, 'P'},
        {"count",       required_argument, nullptr, 'n'},
        {"interval",    required_argument, nullptr, 'i'},
        {"timeout",     required_argument, nullptr, 't'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:a:P:n:i:t:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'p': opts.ptp_device = optarg; break;
            case 'a': opts.device_addr = optarg; break;
            case 'P': opts.port = atoi(optarg); break;
            case 'n': opts.count = atoi(optarg); break;
            case 'i': opts.interval_ms = atoi(optarg); break;
            case 't': opts.recv_timeout_ms = atoi(optarg); break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (!opts.ptp_device) {
        fprintf(stderr, "Error: --ptp-device is required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    // Open PTP hardware clock
    int phc_fd = open(opts.ptp_device, O_RDONLY);
    if (phc_fd < 0) {
        perror("Failed to open PTP device");
        return 1;
    }
    clockid_t phc_clockid = FD_TO_CLOCKID(phc_fd);

    // Verify PHC is readable
    {
        int64_t test_ns = read_phc_ns(phc_clockid);
        if (test_ns == 0) {
            fprintf(stderr, "Failed to read PTP clock\n");
            close(phc_fd);
            return 1;
        }
        fprintf(stderr, "PHC opened: %s (current time: %ld ns)\n", opts.ptp_device, test_ns);
    }

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Failed to create socket");
        close(phc_fd);
        return 1;
    }

    // Set SO_REUSEADDR
    int opt_one = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_one, sizeof(opt_one));

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = opts.recv_timeout_ms / 1000;
    tv.tv_usec = (opts.recv_timeout_ms % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Set receive buffer size
    int rcvbuf = 6000000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    // Bind to receive port
    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(opts.port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0) {
        perror("Failed to bind socket");
        fprintf(stderr, "  Port %d may be in use — ensure no other CereLink client is running\n", opts.port);
        close(sockfd);
        close(phc_fd);
        return 1;
    }

    // Set up send address
    struct sockaddr_in send_addr{};
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(opts.port);
    send_addr.sin_addr.s_addr = inet_addr(opts.device_addr);

    fprintf(stderr, "Connected to %s:%d\n", opts.device_addr, opts.port);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Collect results
    std::vector<ProbeResult> results;
    results.reserve(opts.count);

    // Print TSV header
    printf("#\tt1_ptp\t\t\t\tt3_etime\t\t\tt3_stale\t\t\tt4_ptp\t\t\t\td1_ns\td2_ns\t\trtt_ns\tstaleness_ns\n");
    fflush(stdout);

    uint8_t recv_buf[65536];

    for (int i = 0; i < opts.count && g_running; i++) {
        // Build nPlay probe packet
        cbPKT_NPLAY pkt{};
        pkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
        pkt.cbpkt_header.type = cbPKTTYPE_NPLAYSET;
        pkt.cbpkt_header.dlen = cbPKTDLEN_NPLAY;

        // Read PHC just before sending → T1
        int64_t t1_ptp = read_phc_ns(phc_clockid);
        pkt.stime = static_cast<uint64_t>(t1_ptp);

        // Send
        size_t pkt_size = cbPKT_HEADER_SIZE + (pkt.cbpkt_header.dlen * 4);
        ssize_t sent = sendto(sockfd, &pkt, pkt_size, 0,
                              reinterpret_cast<struct sockaddr*>(&send_addr), sizeof(send_addr));
        if (sent < 0) {
            fprintf(stderr, "Probe %d: sendto failed\n", i);
            continue;
        }

        // Receive loop — wait for NPLAYREP, skip other packets
        bool got_reply = false;
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(opts.recv_timeout_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            ssize_t n = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, nullptr, nullptr);
            if (n < 0) {
                break;  // timeout or error
            }

            // Read PHC immediately after recvfrom → T4 candidate
            int64_t t4_ptp = read_phc_ns(phc_clockid);

            // Parse — may contain multiple packets in one datagram
            size_t offset = 0;
            while (offset + cbPKT_HEADER_SIZE <= static_cast<size_t>(n)) {
                const auto* hdr = reinterpret_cast<const cbPKT_HEADER*>(recv_buf + offset);
                size_t pkt_len = cbPKT_HEADER_SIZE + (hdr->dlen * 4);
                if (offset + pkt_len > static_cast<size_t>(n)) break;

                if (hdr->type == cbPKTTYPE_NPLAYREP &&
                    (hdr->chid & cbPKTCHAN_CONFIGURATION) == cbPKTCHAN_CONFIGURATION) {

                    const auto* nplay = reinterpret_cast<const cbPKT_NPLAY*>(recv_buf + offset);

                    ProbeResult r;
                    r.t1_ptp = t1_ptp;
                    r.t3_etime = static_cast<int64_t>(nplay->etime);
                    r.t3_stale = static_cast<int64_t>(hdr->time);
                    r.t4_ptp = t4_ptp;
                    r.d1 = r.t3_etime - r.t1_ptp;
                    r.d2 = r.t4_ptp - r.t3_etime;
                    r.rtt = r.t4_ptp - r.t1_ptp;
                    r.staleness = r.t3_etime - r.t3_stale;

                    printf("%d\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t\t%ld\t%ld\n",
                           i, r.t1_ptp, r.t3_etime, r.t3_stale, r.t4_ptp,
                           r.d1, r.d2, r.rtt, r.staleness);
                    fflush(stdout);

                    results.push_back(r);
                    got_reply = true;
                    break;
                }

                offset += pkt_len;
            }

            if (got_reply) break;
        }

        if (!got_reply) {
            fprintf(stderr, "Probe %d: no NPLAYREP received (timeout)\n", i);
        }

        // Wait for next probe
        if (i + 1 < opts.count && g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(opts.interval_ms));
        }
    }

    // Print statistics
    fprintf(stderr, "\n=== Probe Timing Statistics (%zu/%d probes received) ===\n",
            results.size(), opts.count);

    if (!results.empty()) {
        std::vector<int64_t> d1_vals, d2_vals, rtt_vals, stale_vals;
        for (const auto& r : results) {
            d1_vals.push_back(r.d1);
            d2_vals.push_back(r.d2);
            rtt_vals.push_back(r.rtt);
            stale_vals.push_back(r.staleness);
        }

        print_stats("D1 (forward)", d1_vals);
        print_stats("D2 (reverse)", d2_vals);
        print_stats("RTT", rtt_vals);
        print_stats("Staleness", stale_vals);
    }

    close(sockfd);
    close(phc_fd);
    return 0;
}

#else // !__linux__

#include <cstdio>

int main() {
    fprintf(stderr, "probe_timing requires Linux (PTP hardware clock support)\n");
    return 1;
}

#endif // __linux__
