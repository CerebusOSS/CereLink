///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   validate_clock_sync.cpp
/// @brief  PTP-based validation of CereLink's ClockSync accuracy
///
/// Compares ClockSync's estimated device-to-host offset against a PTP ground truth.
///
/// Prerequisites (Raspberry Pi 5 connected to Hub1 via Ethernet):
///
///   # Install linuxptp
///   sudo apt install linuxptp
///
///   # Find the Ethernet interface connected to Hub1 (192.168.137.x subnet)
///   ip addr show  # look for the interface with 192.168.137.x
///
///   # Check PTP/timestamping capability and find PHC index
///   ethtool -T eth0  # replace eth0 with actual interface
///   # Look for "PTP Hardware Clock: N" → device is /dev/ptpN
///
///   # Start ptp4l as PTP slave to Hub1 (grandmaster)
///   # Hub1 must be running as PTP master: ptp4l -i eth0 -H -m --priority1 0
///   sudo ptp4l -i eth0 -s -m --ptp_minor_version 0
///   # -i: interface, -s: slave-only, -m: log to stdout
///   # --ptp_minor_version 0: required if RPi5 defaults to PTPv2.1 (Hub1 uses PTPv2.0)
///   # Wait until "rms" in log output drops below 1000 ns
///
/// Usage:
///   ./validate_clock_sync --ptp-device /dev/ptp0 [OPTIONS]
///
/// Options:
///   --ptp-device PATH     PTP hardware clock device (required)
///   --device-type TYPE    Device type: hub1 (default), hub2, hub3, nsp, legacy_nsp, nplay
///   --duration SECS       Duration in seconds (default: 60)
///   --probe-interval MS   Clock probe interval in ms (default: 2000)
///   --sample-interval MS  Sampling interval in ms (default: 100)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#include <cbsdk/sdk_session.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

// Linux-specific: derive clockid_t from PTP device fd
// See kernel docs: Documentation/ptp/ptp.txt
// FD_TO_CLOCKID is defined in linux/posix-timers.h but not always available,
// so we define the standard formula here.
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

struct Options {
    const char* ptp_device = nullptr;
    const char* device_type_str = "hub1";
    int duration_s = 60;
    int probe_interval_ms = 2000;
    int sample_interval_ms = 100;
};

static cbsdk::DeviceType parse_device_type(const char* str) {
    if (strcmp(str, "hub1") == 0)       return cbsdk::DeviceType::HUB1;
    if (strcmp(str, "hub2") == 0)       return cbsdk::DeviceType::HUB2;
    if (strcmp(str, "hub3") == 0)       return cbsdk::DeviceType::HUB3;
    if (strcmp(str, "nsp") == 0)        return cbsdk::DeviceType::NSP;
    if (strcmp(str, "legacy_nsp") == 0) return cbsdk::DeviceType::LEGACY_NSP;
    if (strcmp(str, "nplay") == 0)      return cbsdk::DeviceType::NPLAY;
    fprintf(stderr, "Unknown device type: %s\n", str);
    exit(1);
}

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s --ptp-device /dev/ptpN [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  --ptp-device PATH     PTP hardware clock device (required)\n"
        "  --device-type TYPE    hub1 (default), hub2, hub3, nsp, legacy_nsp, nplay\n"
        "  --duration SECS       Duration in seconds (default: 60)\n"
        "  --probe-interval MS   Clock probe interval in ms (default: 2000)\n"
        "  --sample-interval MS  Sampling interval in ms (default: 100)\n",
        prog);
}

/// Read PTP-vs-CLOCK_MONOTONIC offset using paired clock_gettime reads.
/// Returns the offset (phc_ns - mono_midpoint_ns) and the read uncertainty.
static bool read_ptp_offset(clockid_t phc_clockid,
                            int64_t& offset_ns, int64_t& uncertainty_ns) {
    struct timespec mono1, phc, mono2;

    if (clock_gettime(CLOCK_MONOTONIC, &mono1) != 0) return false;
    if (clock_gettime(phc_clockid, &phc) != 0)       return false;
    if (clock_gettime(CLOCK_MONOTONIC, &mono2) != 0)  return false;

    int64_t mono1_ns = timespec_to_ns(mono1);
    int64_t mono2_ns = timespec_to_ns(mono2);
    int64_t phc_ns = timespec_to_ns(phc);
    int64_t mono_mid = mono1_ns + (mono2_ns - mono1_ns) / 2;

    offset_ns = phc_ns - mono_mid;
    uncertainty_ns = (mono2_ns - mono1_ns) / 2;
    return true;
}

int main(int argc, char* argv[]) {
    Options opts;

    static struct option long_options[] = {
        {"ptp-device",      required_argument, nullptr, 'p'},
        {"device-type",     required_argument, nullptr, 't'},
        {"duration",        required_argument, nullptr, 'd'},
        {"probe-interval",  required_argument, nullptr, 'P'},
        {"sample-interval", required_argument, nullptr, 's'},
        {"help",            no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:t:d:P:s:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'p': opts.ptp_device = optarg; break;
            case 't': opts.device_type_str = optarg; break;
            case 'd': opts.duration_s = atoi(optarg); break;
            case 'P': opts.probe_interval_ms = atoi(optarg); break;
            case 's': opts.sample_interval_ms = atoi(optarg); break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
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
        fprintf(stderr, "  Device: %s\n", opts.ptp_device);
        fprintf(stderr, "  Hint: run with sudo, or add user to 'ptp' group\n");
        return 1;
    }
    clockid_t phc_clockid = FD_TO_CLOCKID(phc_fd);

    // Verify PHC is readable
    {
        int64_t test_offset, test_unc;
        if (!read_ptp_offset(phc_clockid, test_offset, test_unc)) {
            fprintf(stderr, "Failed to read PTP clock. Is ptp4l running?\n");
            close(phc_fd);
            return 1;
        }
        fprintf(stderr, "PHC opened: %s (initial offset: %ld ns, read uncertainty: %ld ns)\n",
                opts.ptp_device, test_offset, test_unc);
    }

    // Connect to device via SdkSession
    cbsdk::SdkConfig config;
    config.device_type = parse_device_type(opts.device_type_str);

    fprintf(stderr, "Connecting to device (type: %s)...\n", opts.device_type_str);
    auto result = cbsdk::SdkSession::create(config);
    if (result.isError()) {
        fprintf(stderr, "Failed to create SDK session: %s\n", result.error().c_str());
        close(phc_fd);
        return 1;
    }
    auto& session = result.value();
    fprintf(stderr, "Connected. Waiting for clock sync data...\n");

    // Wait briefly for one-way samples to arrive (SYSPROTOCOLMONITOR)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Install signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Print TSV header
    printf("# elapsed_s\tptp_offset_ns\tcs_offset_ns\tcs_uncertainty_ns\terror_ns\tphc_read_uncertainty_ns\n");
    fflush(stdout);

    auto start_time = std::chrono::steady_clock::now();
    auto last_probe_time = start_time;
    auto end_time = start_time + std::chrono::seconds(opts.duration_s);
    auto sample_interval = std::chrono::milliseconds(opts.sample_interval_ms);
    auto probe_interval = std::chrono::milliseconds(opts.probe_interval_ms);

    // Send initial clock probe
    session.sendClockProbe();

    while (g_running && std::chrono::steady_clock::now() < end_time) {
        auto now = std::chrono::steady_clock::now();

        // Send periodic clock probes
        if (now - last_probe_time >= probe_interval) {
            auto probe_result = session.sendClockProbe();
            if (probe_result.isError()) {
                fprintf(stderr, "Warning: sendClockProbe failed: %s\n",
                        probe_result.error().c_str());
            }
            last_probe_time = now;
        }

        // Read PTP offset (ground truth)
        int64_t ptp_offset_ns = 0;
        int64_t phc_read_uncertainty_ns = 0;
        if (!read_ptp_offset(phc_clockid, ptp_offset_ns, phc_read_uncertainty_ns)) {
            fprintf(stderr, "Warning: failed to read PHC\n");
            std::this_thread::sleep_for(sample_interval);
            continue;
        }

        // Read ClockSync offset
        auto cs_offset = session.getClockOffsetNs();
        auto cs_uncertainty = session.getClockUncertaintyNs();

        if (cs_offset.has_value()) {
            int64_t error_ns = cs_offset.value() - ptp_offset_ns;
            double elapsed_s = std::chrono::duration<double>(now - start_time).count();

            printf("%.3f\t%ld\t%ld\t%ld\t%ld\t%ld\n",
                   elapsed_s,
                   ptp_offset_ns,
                   cs_offset.value(),
                   cs_uncertainty.value_or(0),
                   error_ns,
                   phc_read_uncertainty_ns);
            fflush(stdout);
        } else {
            double elapsed_s = std::chrono::duration<double>(now - start_time).count();
            fprintf(stderr, "%.3f: no clock sync data yet\n", elapsed_s);
        }

        // Sleep until next sample
        std::this_thread::sleep_for(sample_interval);
    }

    fprintf(stderr, "Done. Shutting down...\n");
    session.stop();
    close(phc_fd);
    return 0;
}

#else // !__linux__

#include <cstdio>

int main() {
    fprintf(stderr, "validate_clock_sync requires Linux (PTP hardware clock support)\n");
    return 1;
}

#endif // __linux__
