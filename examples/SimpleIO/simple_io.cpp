///////////////////////////////////////////////////////////////////////
//
// Test IO
//
// Purpose:
//  This extends testcbsdk with explicit testing of data io.
//  This is incomplete and only includes tests that were relevant to debugging specific problems.
//
//  Call with -c to enable continuous data, -e to enable events,
//  and -r <int> to specify a fixed runtime.
//  While running, press Ctrl+C to stop.
//
//  Note:
//   Make sure only the SDK is used here, and not cbhwlib directly
//    this will ensure SDK is capable of whatever test suite can do.
//   Do not throw exceptions, catch possible exceptions and handle them the earliest possible in this library
//

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <cinttypes>

#include <cerelink/cbsdk.h>

#define INST 0

// Global flag to signal shutdown
static std::atomic<bool> g_bShutdown(false);

// Signal handler for Ctrl+C
void signal_handler(const int signal)
{
    if (signal == SIGINT)
    {
        g_bShutdown = true;
        printf("\nShutdown requested...\n");
    }
}

typedef struct cbsdk_config {
    uint32_t nInstance;
    uint32_t bActive;
    uint16_t begchan;
    uint32_t begmask;
    uint32_t begval;
    uint16_t endchan;
    uint32_t endmask;
    uint32_t endval;
    bool bDouble;
    uint32_t uWaveforms;
    uint32_t uConts;
    uint32_t uEvents;
    uint32_t uComments;
    uint32_t uTrackings;
    bool bAbsolute;
} cbsdk_config;

void handleResult(const cbSdkResult res)
{
    switch (res)
    {
    case CBSDKRESULT_SUCCESS:
        break;
    case CBSDKRESULT_NOTIMPLEMENTED:
        printf("Not implemented\n");
        break;
    case CBSDKRESULT_INVALIDPARAM:
        printf("Invalid parameter\n");
        break;
    case CBSDKRESULT_WARNOPEN:
        printf("Real-time interface already initialized\n");
    case CBSDKRESULT_WARNCLOSED:
        printf("Real-time interface already closed\n");
        break;
    case CBSDKRESULT_ERROPENCENTRAL:
        printf("Unable to open library for Central interface\n");
        break;
    case CBSDKRESULT_ERROPENUDP:
        printf("Unable to open library for UDP interface\n");
        break;
    case CBSDKRESULT_ERROPENUDPPORT:
        printf("Unable to open UDP interface\n");
        break;
    case CBSDKRESULT_OPTERRUDP:
        printf("Unable to set UDP interface option\n");
        break;
    case CBSDKRESULT_MEMERRUDP:
        printf("Unable to assign UDP interface memory\n"
            " Consider using sysctl -w net.core.rmem_max=8388608\n"
            " or sysctl -w kern.ipc.maxsockbuf=8388608\n");
        break;
    case CBSDKRESULT_INVALIDINST:
        printf("Invalid UDP interface\n");
        break;
    case CBSDKRESULT_ERRMEMORYTRIAL:
        printf("Unable to allocate RAM for trial cache data\n");
        break;
    case CBSDKRESULT_ERROPENUDPTHREAD:
        printf("Unable to Create UDP thread\n");
        break;
    case CBSDKRESULT_ERROPENCENTRALTHREAD:
        printf("Unable to start Cerebus real-time data thread\n");
        break;
    case CBSDKRESULT_ERRINIT:
        printf("Library initialization error\n");
        break;
    case CBSDKRESULT_ERRMEMORY:
        printf("Library memory allocation error\n");
        break;
    case CBSDKRESULT_TIMEOUT:
        printf("Connection timeout error\n");
        break;
    case CBSDKRESULT_ERROFFLINE:
        printf("Instrument is offline\n");
        break;
    default:
        printf("Unexpected error\n");
        break;
    }
}

cbSdkVersion getVersion()
{
    // Library version can be read even before library open (return value is a warning)
    //  actual NSP version however needs library to be open
    cbSdkVersion ver;
    const cbSdkResult res = cbSdkGetVersion(INST, &ver);
    if (res != CBSDKRESULT_SUCCESS)
    {
        printf("Unable to determine instrument version\n");
    }
    else {
        printf("Initializing Cerebus real-time interface %d.%02d.%02d.%02d (protocol cb%d.%02d)...\n",
               ver.major, ver.minor, ver.release, ver.beta, ver.majorp, ver.minorp);
    }
    handleResult(res);
    return ver;
}

cbSdkResult open(const LPCSTR inst_ip, const int inst_port, const LPCSTR client_ip)
{
    // Try to get the version. Should be a warning because we are not yet open.
    getVersion();

    // Open the device using default connection type.
    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    auto con = cbSdkConnection();
    con.szOutIP = inst_ip;
    con.nOutPort = inst_port;
    con.szInIP = client_ip;
    con.nInPort = inst_port;
    cbSdkResult res = cbSdkOpen(INST, conType, con);
    if (res != CBSDKRESULT_SUCCESS)
        printf("Unable to open instrument connection.\n");
    handleResult(res);

    cbSdkInstrumentType instType;
    if (res >= 0)
    {
        // Return the actual opened connection
        res = cbSdkGetType(INST, &conType, &instType);
        if (res != CBSDKRESULT_SUCCESS)
            printf("Unable to determine connection type\n");
        handleResult(res);
        // if (instType == CBSDKINSTRUMENT_NPLAY || instType == CBSDKINSTRUMENT_REMOTENPLAY)
        //  	printf("Unable to open UDP interface to nPlay\n");

        if (conType > CBSDKCONNECTION_CLOSED)
            conType = CBSDKCONNECTION_COUNT;
        if (instType > CBSDKINSTRUMENT_COUNT)
            instType = CBSDKINSTRUMENT_COUNT;

        char strConnection[CBSDKCONNECTION_COUNT + 1][8] = {"Default", "Central", "Udp", "Closed", "Unknown"};
        char strInstrument[CBSDKINSTRUMENT_COUNT + 1][13] = {"NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"};

        // Now that we are open, get the version again.
        const cbSdkVersion ver = getVersion();

        // Summary results.
        printf("%s real-time interface to %s (%d.%02d.%02d.%02d) successfully initialized\n",
               strConnection[conType], strInstrument[instType],
               ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta);
    }
    return res;
}

void setConfig(const bool bCont, bool bEv)
{
    uint32_t proc = 1;
    uint32_t nChansInGroup;
    uint16_t pGroupList[cbNUM_ANALOG_CHANS];
    for (uint32_t group_ix = 1; group_ix < 7; group_ix++)
    {
        const cbSdkResult res = cbSdkGetSampleGroupList(INST, proc, group_ix, &nChansInGroup, pGroupList);
        if (res == CBSDKRESULT_SUCCESS)
        {
            printf("In sampling group %d, found %d channels.\n", group_ix, nChansInGroup);
        }
        handleResult(res);
    }
    if (bCont)
    {
        // Set sample group 3 and filter 7 on the first few channels.
        // Also disable spiking.
        for (int chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; ++chan_ix)
        {
            cbSdkSetAinpSampling(INST, chan_ix + 1, 7, chan_ix < 2 ? 3 : 0);
            cbSdkSetAinpSpikeOptions(INST, chan_ix + 1, 0, 2);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

cbSdkResult close()
{
    const cbSdkResult res = cbSdkClose(INST);
    if (res == CBSDKRESULT_SUCCESS)
    {
        printf("Interface closed successfully\n");
    }
    else
    {
        printf("Unable to close interface.\n");
        handleResult(res);
    }
    return res;
}

int main(const int argc, char *argv[])
{
    auto inst_ip = "";
    int inst_port = cbNET_UDP_PORT_CNT;
    auto client_ip = "";
    bool bCont = false;
    bool bEv = false;
    bool bComm = false;
    uint32_t runtime = 30000;

    // Parse command line arguments.
    {
        if (argc > 1 && argv[1][0] != '-') {inst_ip = argv[1];}
        if (argc > 2 && argv[2][0] != '-') {
            try {
                inst_port = std::stoi(argv[2]);
            } catch (const std::exception&) {
                printf("Error: Invalid port number '%s'\n", argv[2]);
                return 1;
            }
        }
        if (argc > 3 && argv[3][0] != '-') { client_ip = argv[3]; }

        for (size_t optind = 1; optind < argc; optind++)
        {
            if (argv[optind][0] == '-')
            {
                switch (const char option = argv[optind][1]) {
                    case 'c': bCont = true; break;
                    case 'e': bEv = true; break;
                    case 'i': bComm = true; break;
                    case 'r': runtime = 30000 * (argv[optind][2] - '0'); break;
                    case '\0':
                        printf("Error: Invalid option '-' without a flag character\n");
                        printf("Usage: %s [inst_ip] [inst_port] [client_ip] [-c] [-e] [-i] [-rN]\n", argv[0]);
                        printf("  -c: Enable continuous data\n");
                        printf("  -e: Enable event data\n");
                        printf("  -i: Enable comment data\n");
                        printf("  -rN: Set runtime (N * 30000 ticks)\n");
                        return 1;
                    default:
                        printf("Error: Unrecognized option '-%c'\n", option);
                        printf("Usage: %s [inst_ip] [inst_port] [client_ip] [-c] [-e] [-i] [-rN]\n", argv[0]);
                        printf("  -c: Enable continuous data\n");
                        printf("  -e: Enable event data\n");
                        printf("  -i: Enable comment data\n");
                        printf("  -rN: Set runtime (N * 30000 ticks)\n");
                        return 1;
                }
            }
        }
    }

    // Install signal handler for Ctrl+C
    std::signal(SIGINT, signal_handler);

    // Connect to the device.
    cbSdkResult res = open(inst_ip, inst_port, client_ip);
    if (res < 0)
    {
        printf("open failed (%d)!\n", res);
        return -1;
    }
    printf("open succeeded\n");

    setConfig(bCont, bEv);

    cbsdk_config cfg;
    cbSdkGetTrialConfig(
        INST,
        &cfg.bActive,
        &cfg.begchan, &cfg.begmask, &cfg.begval,
        &cfg.endchan, &cfg.endmask, &cfg.endval,
        &cfg.uWaveforms,
        &cfg.uConts,
        &cfg.uEvents,
        &cfg.uComments,
        &cfg.uTrackings
    );
    cbSdkSetTrialConfig(
        INST,
        1,
        0, 0, 0,
        0, 0, 0,
        cfg.uWaveforms,
        bCont ? cbSdk_CONTINUOUS_DATA_SAMPLES : 0,
        bEv ? cbSdk_EVENT_DATA_SAMPLES : 0,
        cfg.uComments,
        cfg.uTrackings
    );

    auto trialEvent = std::unique_ptr<cbSdkTrialEvent>(nullptr);
    if (bEv)
        trialEvent = std::make_unique<cbSdkTrialEvent>();
    auto trialCont = std::unique_ptr<cbSdkTrialCont>(nullptr);
    if (bCont) {
        trialCont = std::make_unique<cbSdkTrialCont>();
        trialCont->group = 3;
    }
    auto trialComm = std::unique_ptr<cbSdkTrialComment>(nullptr);
    if (bComm)
        trialComm = std::make_unique<cbSdkTrialComment>();

    // Continuous data: single contiguous buffer for all channels and samples
    std::vector<int16_t> cont_samples;     // [num_samples * count] contiguous array
    std::vector<PROCTIME> cont_timestamps; // [num_samples] timestamps
    // Event data
    std::vector<PROCTIME> event_ts;       // [num_events] flat timestamp array
    std::vector<uint16_t> event_channels; // [num_events] flat channel array
    std::vector<uint16_t> event_units;    // [num_events] flat unit array

    PROCTIME start_time;
    PROCTIME elapsed_time = 0;
    cbSdkGetTime(INST, &start_time);
    while (!g_bShutdown && ((runtime == 0) || (elapsed_time < runtime)))
    {
        cbSdkGetTime(INST, &elapsed_time);
        elapsed_time -= start_time;

        // cbSdkInitTrialData to determine how many samples are available
        cbSdkInitTrialData(
            INST,
            0,
            trialEvent.get(),
            trialCont.get(),
            trialComm.get(),
            nullptr
        );

        // allocate memory for flat event arrays
        if (trialEvent && trialEvent->num_events)
        {
            // Allocate flat arrays for all events
            event_ts.resize(trialEvent->num_events);
            event_channels.resize(trialEvent->num_events);
            event_units.resize(trialEvent->num_events);

            // Assign pointers to trialEvent structure
            trialEvent->timestamps = event_ts.data();
            trialEvent->channels = event_channels.data();
            trialEvent->units = event_units.data();
            trialEvent->waveforms = nullptr;  // Not using waveforms in this example
        }

        if (trialCont && (trialCont->count > 0))
        {
            // Allocate memory for continuous data.
            // Data layout is [num_samples][count] - contiguous array of num_samples * count elements
            const uint32_t n_samples = trialCont->num_samples;
            const uint32_t n_channels = trialCont->count;

            // Allocate contiguous buffer for all samples and channels
            cont_samples.assign(n_samples * n_channels, 0);
            trialCont->samples = cont_samples.data();

            // Allocate timestamps array
            cont_timestamps.assign(n_samples, 0);
            trialCont->timestamps = cont_timestamps.data();
        }

        // TODO: Allocate memory for comment data

        // cbSdkGetTrialData to fetch the data
        cbSdkGetTrialData(
            INST,
            1,
            trialEvent.get(),
            trialCont.get(),
            trialComm.get(),
            nullptr
        );

        // Do something with the data.
        if (trialEvent && trialEvent->num_events)
        {
            // Print first few events as example
            const size_t n_to_print = std::min(static_cast<size_t>(10), static_cast<size_t>(trialEvent->num_events));
            for (size_t ev_ix = 0; ev_ix < n_to_print; ev_ix++)
            {
                printf("Event %zu: ch=%u unit=%u ts=%" PRIu64 "\n",
                       ev_ix,
                       event_channels[ev_ix],
                       event_units[ev_ix],
                       static_cast<uint64_t>(event_ts[ev_ix]));
            }
            if (trialEvent->num_events > n_to_print)
                printf("... and %u more events\n", trialEvent->num_events - static_cast<uint32_t>(n_to_print));
        }
        if (trialCont && trialCont->count)
        {
            const uint32_t n_samples = trialCont->num_samples;
            const uint32_t n_channels = trialCont->count;
            if (n_samples > 0)
            {
                for (size_t chan_ix = 0; chan_ix < n_channels; chan_ix++)
                {
                    // Extract min/max for this channel from the contiguous [sample][channel] array
                    // Data for channel chan_ix at sample i is at: cont_samples[i * n_channels + chan_ix]
                    int16_t min_val = cont_samples[chan_ix];  // First sample for this channel
                    int16_t max_val = cont_samples[chan_ix];
                    for (size_t sample_ix = 0; sample_ix < n_samples; sample_ix++)
                    {
                        const int16_t val = cont_samples[sample_ix * n_channels + chan_ix];
                        if (val < min_val) min_val = val;
                        if (val > max_val) max_val = val;
                    }
                    std::cout << "chan = " << trialCont->chan[chan_ix]
                             << ", nsamps = " << n_samples
                             << ", min = " << min_val
                             << ", max = " << max_val
                             << " (t " << cont_timestamps[0] << " - " << cont_timestamps[n_samples - 1] << ")"
                             << '\n';
                }
            }
        }
    }

    printf("Shutting down...\n");
    res = close();
    if (res < 0)
        printf("close failed (%d)!\n", res);
    else
        printf("close succeeded\n");

    // No need to clear trialCont, trialEvent and trialComm because they are smart pointers and will dealloc.

    return 0;
}
