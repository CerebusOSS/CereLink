///////////////////////////////////////////////////////////////////////
//
// Test IO
//
// Purpose:
//  This extends testcbsdk with explicit testing of data io.
//  This is incomplete and only includes tests that were relevant to debugging specific problems.
//
//  Call with -c to enable continuous data (not tested), -e to enable events (works ok),
//  and -r <int> to specify a fixed runtime (?not working).
//  while running, press Esc key to stop.
//
//  Note:
//   Make sure only the SDK is used here, and not cbhwlib directly
//    this will ensure SDK is capable of whatever test suite can do
//   Do not throw exceptions, catch possible exceptions and handle them the earliest possible in this library
//

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <thread>         // std::this_thread::sleep_for
#ifdef WIN32
#include <conio.h>
#endif //win32
#include <memory>
#include <inttypes.h>

#include <cerelink/cbsdk.h>

#define INST 0

typedef struct _cbsdk_config {
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

void handleResult(cbSdkResult res)
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

cbSdkVersion getVersion(void)
{
    // Library version can be read even before library open (return value is a warning)
    //  actual NSP version however needs library to be open
    cbSdkVersion ver;
    cbSdkResult res = cbSdkGetVersion(INST, &ver);
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

cbSdkResult open(LPCSTR inst_ip, int inst_port, LPCSTR client_ip)
{
    // Try to get the version. Should be a warning because we are not yet open.
    cbSdkVersion ver = getVersion();

    // Open the device using default connection type.
    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    cbSdkConnection con = cbSdkConnection();
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


        if (conType < 0 || conType > CBSDKCONNECTION_CLOSED)
            conType = CBSDKCONNECTION_COUNT;
        if (instType < 0 || instType > CBSDKINSTRUMENT_COUNT)
            instType = CBSDKINSTRUMENT_COUNT;

        char strConnection[CBSDKCONNECTION_COUNT + 1][8] = {"Default", "Central", "Udp", "Closed", "Unknown"};
        char strInstrument[CBSDKINSTRUMENT_COUNT + 1][13] = {"NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"};

        // Now that we are open, get the version again.
        ver = getVersion();

        // Summary results.
        printf("%s real-time interface to %s (%d.%02d.%02d.%02d) successfully initialized\n",
               strConnection[conType], strInstrument[instType],
               ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta);
    }

    return res;
}

void setConfig(bool bCont, bool bEv)
{
    uint32_t proc = 1;
    uint32_t nChansInGroup;
    uint16_t pGroupList[cbNUM_ANALOG_CHANS];
    for (uint32_t group_ix = 1; group_ix < 7; group_ix++)
    {
        cbSdkResult res = cbSdkGetSampleGroupList(INST, proc, group_ix, &nChansInGroup, pGroupList);
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
        cbPKT_CHANINFO ch_info;
        cbSdkResult res;
        for (int chan_ix = 0; chan_ix < cbNUM_ANALOG_CHANS; ++chan_ix) {
            res = cbSdkSetAinpSampling(INST, chan_ix + 1, 7, chan_ix < 2 ? 3 : 0);
            res = cbSdkSetAinpSpikeOptions(INST, chan_ix + 1, 0, 2);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

cbSdkResult close(void)
{
    cbSdkResult res = cbSdkClose(INST);
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

int main(int argc, char *argv[])
{
    LPCSTR inst_ip = "";
    int inst_port = cbNET_UDP_PORT_CNT;
    LPCSTR client_ip = "";
    bool bCont = false;
    bool bEv = false;
    bool bComm = false;
    uint32_t runtime = 30000;

    // Parse command line arguments.
    {
        if (argc > 1 && argv[1][0] != '-') {inst_ip = argv[1];}
        if (argc > 2 && argv[2][0] != '-') {inst_port = strtol(argv[2], NULL, 10);}
        if (argc > 3 && argv[3][0] != '-') { client_ip = argv[3]; }

        size_t optind;
        for (optind = 1; optind < argc; optind++)
        {
            if (argv[optind][0] == '-')
            {
                switch (argv[optind][1]) {
                    case 'c': bCont = true; break;
                    case 'e': bEv = true; break;
                    case 'r': runtime = 30000 * (argv[optind][2] - '0'); break;
                }
            }
        }
    }

    // Connect to the device.
    cbSdkResult res = open(inst_ip, inst_port, client_ip);
    if (res < 0)
    {
        printf("open failed (%d)!\n", res);
        return -1;
    }
    else
        printf("open succeeded\n");
    
    setConfig(bCont, bEv);

    cbsdk_config cfg;
    res = cbSdkGetTrialConfig(INST, &cfg.bActive,
                              &cfg.begchan, &cfg.begmask, &cfg.begval,
                              &cfg.endchan, &cfg.endmask, &cfg.endval,
                              &cfg.bDouble, &cfg.uWaveforms,
                              &cfg.uConts, &cfg.uEvents, &cfg.uComments, &cfg.uTrackings,
                              &cfg.bAbsolute);
    cfg.bActive = 1;
    cfg.uConts = bCont ? cbSdk_CONTINUOUS_DATA_SAMPLES : 0;
    cfg.uEvents = bEv ? cbSdk_EVENT_DATA_SAMPLES : 0;
    res = cbSdkSetTrialConfig(INST, cfg.bActive,
                              cfg.begchan, cfg.begmask, cfg.begval,
                              cfg.endchan, cfg.endmask, cfg.endval,
                              cfg.bDouble, cfg.uWaveforms,
                              cfg.uConts, cfg.uEvents, cfg.uComments, cfg.uTrackings,
                              cfg.bAbsolute);

    std::unique_ptr<cbSdkTrialEvent> trialEvent = std::unique_ptr<cbSdkTrialEvent>(nullptr);
    if (bEv)
        trialEvent = std::make_unique<cbSdkTrialEvent>();
    std::unique_ptr<cbSdkTrialCont> trialCont = std::unique_ptr<cbSdkTrialCont>(nullptr);
    if (bCont)
        trialCont = std::make_unique<cbSdkTrialCont>();
    std::unique_ptr<cbSdkTrialComment> trialComm = std::unique_ptr<cbSdkTrialComment>(nullptr);
    if (bComm)
        trialComm = std::make_unique<cbSdkTrialComment>();

    // We allocate a bunch of std::vectors now, and we can grow them if the number of samples to be retrieved
    //  exceeds the allocated space.
    std::vector<int16_t> cont_short[cbMAXCHANS];
    std::vector<double> cont_double[cbMAXCHANS];
    std::vector<PROCTIME> event_ts[cbMAXCHANS][cbMAXUNITS + 1];
    std::vector<int16_t> event_wfs_short[cbMAXCHANS];
    std::vector<double> event_wfs_double[cbMAXCHANS];

    PROCTIME start_time;
    PROCTIME elapsed_time = 0;
    res = cbSdkGetTime(INST, &start_time);
    char kb_ch = '1';  // Will monitor for Esc key.
    while ((kb_ch != 27) && ((runtime == 0) || (elapsed_time < runtime)))
    {
        res = cbSdkGetTime(INST, &elapsed_time);
        elapsed_time -= start_time;

        // cbSdkInitTrialData to determine how many samples are available
        res = cbSdkInitTrialData(INST, 0,
                                 trialEvent.get(), trialCont.get(), trialComm.get(),
                                 nullptr);

        // allocate memory
        if (trialEvent && trialEvent->count)
        {
            for (size_t ev_ix = 0; ev_ix < trialEvent->count; ev_ix++)
            {
                // Every event channel, regardless of type (spike, digital, serial), gets an array of timestamps.
                for (size_t un_ix = 0; un_ix < cbMAXUNITS + 1; un_ix++)
                {
                    const uint32_t n_samples = trialEvent->num_samples[ev_ix][un_ix];
                    event_ts[ev_ix][un_ix].resize(n_samples);
                    std::fill(event_ts[ev_ix][un_ix].begin(), event_ts[ev_ix][un_ix].end(), 0);
                    trialEvent->timestamps[ev_ix][un_ix] = event_ts[ev_ix][un_ix].data();
                }

                uint32_t bIsDig = false;
                cbSdkIsChanAnyDigIn(INST, trialEvent->chan[ev_ix], &bIsDig);
                if (bIsDig)
                {
                    // alloc waveform data
                    uint32_t n_samples = trialEvent->num_samples[ev_ix][0];
                    if (cfg.bDouble)
                    {
                        event_wfs_short[ev_ix].resize(n_samples);
                        std::fill(event_wfs_short[ev_ix].begin(), event_wfs_short[ev_ix].end(), 0);
                        trialEvent->waveforms[ev_ix] = event_wfs_short[ev_ix].data();
                    } else {
                        event_wfs_double[ev_ix].resize(n_samples);
                        std::fill(event_wfs_double[ev_ix].begin(), event_wfs_double[ev_ix].end(), 0);
                        trialEvent->waveforms[ev_ix] = event_wfs_double[ev_ix].data();
                    }
                }
            }
        }

        if (trialCont && (trialCont->count > 0))
        {
            // Allocate memory for continuous data
            for (int chan_ix = 0; chan_ix < trialCont->count; ++chan_ix) {
                uint32_t n_samples = trialCont->num_samples[chan_ix];
                if (cfg.bDouble)
                {
                    cont_double[chan_ix].resize(n_samples);
                    std::fill(cont_double[chan_ix].begin(), cont_double[chan_ix].end(), 0);
                    trialCont->samples[chan_ix] = cont_double[chan_ix].data();  // Technically only needed if resize grows the vector.
                } else {
                    cont_short[chan_ix].resize(n_samples);
                    std::fill(cont_short[chan_ix].begin(), cont_short[chan_ix].end(), 0);
                    trialCont->samples[chan_ix] = cont_short[chan_ix].data();  // Technically only needed if resize grows the vector.
                }
            }
        }
        
        // cbSdkGetTrialData to fetch the data
        res = cbSdkGetTrialData(INST, cfg.bActive,
                                trialEvent.get(), trialCont.get(), trialComm.get(),
                                0);

        // Print something to do with the data.
        if (trialEvent && trialEvent->count)
        {
            for (size_t ev_ix = 0; ev_ix < trialEvent->count; ev_ix++)
            {
                for (size_t un_ix = 0; un_ix < cbMAXUNITS + 1; un_ix++)
                {
                    if (trialEvent->num_samples[ev_ix][un_ix])
                    {
                        printf("%" PRIu64, static_cast<uint64_t>(event_ts[ev_ix][un_ix][0]));
                    }
                }
                printf("\n");
                
                uint16_t chan_id = trialEvent->chan[ev_ix];
                uint32_t bIsDig = false;
                cbSdkIsChanAnyDigIn(INST, chan_id, &bIsDig);
                if (bIsDig)
                {
                    uint32_t n_samples = trialEvent->num_samples[ev_ix][0];
                    if (n_samples > 0)
                    {
                        printf("%" PRIu64 ":", static_cast<uint64_t>(event_ts[ev_ix][0][0]));
                    }
                    if (cfg.bDouble) {
                        for (uint32_t sample_ix = 0; sample_ix < n_samples; sample_ix++)
                            printf(" %f", event_wfs_double[ev_ix][sample_ix]);
                    } else {
                        for (uint32_t sample_ix = 0; sample_ix < n_samples; sample_ix++)
                            printf(" %" PRIu16, event_wfs_short[ev_ix][sample_ix]);
                    }
                    if (n_samples > 0) printf("\n");
                }
            }
        }

        if (trialCont && trialCont->count)
        {
            for (size_t chan_ix = 0; chan_ix < trialCont->count; chan_ix++)
            {
                uint32_t n_samples = trialCont->num_samples[chan_ix];
                if (cfg.bDouble) {
                    const auto [min, max] = std::minmax_element(begin(cont_double[chan_ix]), end(cont_double[chan_ix]));
                    std::cout << "chan = " << trialCont->chan[chan_ix] << ", nsamps = " << n_samples << ", min = " << *min << ", max = " << *max << '\n';
                } else {
                    const auto [min, max] = std::minmax_element(begin(cont_short[chan_ix]), end(cont_short[chan_ix]));
                    std::cout << "chan = " << trialCont->chan[chan_ix] << ", nsamps = " << n_samples << ", min = " << *min << ", max = " << *max << '\n';
                }
            }
        }

#ifdef WIN32
        if (_kbhit())
        {
            kb_ch = _getch();
        }
#endif
    }

    res = close();
    if (res < 0)
        printf("close failed (%d)!\n", res);
    else
        printf("close succeeded\n");

    // No need to clear trialCont, trialEvent and trialComm because they are smart pointers and will dealloc.

    return 0;
}
