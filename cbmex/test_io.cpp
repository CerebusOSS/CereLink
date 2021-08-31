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
#include <string>
#include <vector>
#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#endif //win32
#include <inttypes.h>
#include "debugmacs.h"

#include "cbsdk.h"

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


cbSdkVersion testGetVersion(void)
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
        printf("Initializing Cerebus real-time interface %d.%02d.%02d.%02d (protocol cb%d.%02d)...\n", ver.major, ver.minor, ver.release, ver.beta, ver.majorp, ver.minorp);
    }
    handleResult(res);
    return ver;
}

// Author & Date:   Ehsan Azar    24 Oct 2012
// Purpose: Test openning the library
cbSdkResult testOpen(void)
{
    // Try to get the version. Should be a warning because we are not yet open.
    cbSdkVersion ver = testGetVersion();

    // Open the device using default connection type.
    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    cbSdkResult res = cbSdkOpen(INST, conType);
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
        ver = testGetVersion();

        // Summary results.
        printf("%s real-time interface to %s (%d.%02d.%02d.%02d) successfully initialized\n", strConnection[conType], strInstrument[instType], ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta);
    }

    return res;
}


void testGetConfig(void)
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
}

void testGetTrialConfig(cbsdk_config& cfg)
{
    cbSdkResult res = cbSdkGetTrialConfig(INST, &cfg.bActive, &cfg.begchan, &cfg.begmask, &cfg.begval,
        &cfg.endchan, &cfg.endmask, &cfg.endval, &cfg.bDouble,
        &cfg.uWaveforms, &cfg.uConts, &cfg.uEvents, &cfg.uComments, &cfg.uTrackings, &cfg.bAbsolute);
}

// Author & Date:   Ehsan Azar    25 Oct 2012
// Purpose: Test closing the library
cbSdkResult testClose(void)
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

/////////////////////////////////////////////////////////////////////////////
// The test suit main entry
int main(int argc, char *argv[])
{
    // Parse command line arguments.
    bool bCont = false;
    bool bEv = false;
    bool bComm = false;
    uint32_t runtime = 30000;
    size_t optind;
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++)
    {
        switch (argv[optind][1]) {
        case 'c': bCont = true; break;
        case 'e': bEv = true; break;
        case 'r': runtime = 30000 * (argv[optind][2] - '0');
        }
    }

    cbSdkResult res = testOpen();
    if (res < 0)
        printf("testOpen failed (%d)!\n", res);
    else
        printf("testOpen succeeded\n");
    
    testGetConfig();

    cbsdk_config cfg;
    testGetTrialConfig(cfg);
    cfg.bActive = 1;

    cfg.uConts = bCont ? cbSdk_CONTINUOUS_DATA_SAMPLES : 0;
    cfg.uEvents = bEv ? cbSdk_EVENT_DATA_SAMPLES : 0;
    res = cbSdkSetTrialConfig(INST, cfg.bActive, cfg.begchan, cfg.begmask, cfg.begval, cfg.endchan, cfg.endmask, cfg.endval,
        cfg.bDouble, cfg.uWaveforms, cfg.uConts, cfg.uEvents, cfg.uComments, cfg.uTrackings, cfg.bAbsolute);

    uint32_t start_time;
    uint32_t elapsed_time = 0;
    res = cbSdkGetTime(INST, &start_time);
    char kb_ch = '1';
    while ((kb_ch != 27) && ((runtime == 0) || (elapsed_time < runtime)))
    {
        res = cbSdkGetTime(INST, &elapsed_time);
        elapsed_time -= start_time;

        // cbSdkInitTrialData to determine how many samples are available
        cbSdkTrialEvent* ptrialevent = new cbSdkTrialEvent;
        if (!bEv) {
            delete ptrialevent;
            ptrialevent = nullptr;
        }
        cbSdkTrialCont* ptrialcont = new cbSdkTrialCont;
        if (!bCont) {
            delete ptrialcont;
            ptrialcont = nullptr;
        }
        cbSdkTrialComment* ptrialcomment = new cbSdkTrialComment;
        if (!bComm) {
            delete ptrialcomment;
            ptrialcomment = nullptr;
        }
        res = cbSdkInitTrialData(INST, 0, ptrialevent, ptrialcont, ptrialcomment, 0);

        // allocate memory
        if (ptrialevent && (ptrialevent->count > 0))
        {
            for (size_t ev_ix = 0; ev_ix < ptrialevent->count; ev_ix++)
            {
                uint16_t chan_id = ptrialevent->chan[ev_ix];  // 1-based

                // Every event channel, regardless of type (spike, digital, serial), gets an array of timestamps.
                for (size_t un_ix = 0; un_ix < cbMAXUNITS + 1; un_ix++)
                {
                    uint32_t n_samples = ptrialevent->num_samples[ev_ix][un_ix];
                    if (n_samples)
                    {
                        // alloc num_samples uint32_t
                        ptrialevent->timestamps[ev_ix][un_ix] = malloc(n_samples * sizeof(uint32_t));
                    }
                    else {
                        ptrialevent->timestamps[ev_ix][un_ix] = NULL;
                    }
                }

                uint8_t chanType;
                cbSdkGetChannelType(INST, chan_id, &chanType);
                if ((chanType == cbCHANTYPE_DIGIN) || (chanType == cbCHANTYPE_SERIAL))
                {
                    // alloc waveform data
                    uint32_t n_samples = ptrialevent->num_samples[ev_ix][0];
                    ptrialevent->waveforms[ev_ix] = malloc(n_samples * sizeof(uint16_t));
                    // TODO: If double then allocate sizeof(double)
                }
            }
        }

        if (ptrialcont && (ptrialcont->count > 0))
        {
            // TODO: Allocate continuous data
        }
        
        // cbSdkGetTrialData to fetch the data
        res = cbSdkGetTrialData(INST, cfg.bActive, ptrialevent, ptrialcont, ptrialcomment, 0);

        // Print something to do with the data.
        if (ptrialevent && (ptrialevent->count > 0))
        {
            for (size_t ev_ix = 0; ev_ix < ptrialevent->count; ev_ix++)
            {
                for (size_t un_ix = 0; un_ix < cbMAXUNITS + 1; un_ix++)
                {
                    if (ptrialevent->num_samples[ev_ix][un_ix])
                    {
                        uint32_t* ts = (uint32_t *)ptrialevent->timestamps[ev_ix][un_ix];
                        // printf(" %" PRIu32, *ts);
                    }
                }
                // printf("\n");
                
                uint16_t chan_id = ptrialevent->chan[ev_ix];
                uint8_t chanType;
                cbSdkGetChannelType(INST, chan_id, &chanType);
                if ((chanType == cbCHANTYPE_DIGIN) || (chanType == cbCHANTYPE_SERIAL))
                {
                    uint32_t n_samples = ptrialevent->num_samples[ev_ix][0];
                    if (n_samples > 0)
                    {
                        uint32_t* ts = (uint32_t *)ptrialevent->timestamps[ev_ix][0];
                        printf("%" PRIu32 ":", *ts);
                    }
                    uint16_t* wf = (uint16_t*)ptrialevent->waveforms[ev_ix];
                    for (uint32_t sample_ix = 0; sample_ix < n_samples; sample_ix++)
                    {
                        printf(" %" PRIu16, wf[sample_ix]);
                    }
                    if (n_samples > 0) printf("\n");
                }
            }
        }

        if (ptrialcont && (ptrialcont->count > 0))
        {
            // TODO: Print continous data
        }

        // Free allocated memory.
        if (ptrialevent)
        {
            if (ptrialevent->count > 0)
            {
                for (size_t ev_ix = 0; ev_ix < ptrialevent->count; ev_ix++)
                {
                    for (size_t un_ix = 0; un_ix < cbMAXUNITS + 1; un_ix++)
                    {
                        if (ptrialevent->num_samples[ev_ix][un_ix])
                        {
                            free(ptrialevent->timestamps[ev_ix][un_ix]);
                            ptrialevent->timestamps[ev_ix][un_ix] = NULL;
                        }
                    }

                    uint16_t chan_id = ptrialevent->chan[ev_ix];
                    uint8_t chanType;
                    cbSdkGetChannelType(INST, chan_id + 1, &chanType);
                    if ((chanType == cbCHANTYPE_DIGIN) || (chanType == cbCHANTYPE_SERIAL))
                    {
                        free(ptrialevent->waveforms[ev_ix]);
                        ptrialevent->waveforms[ev_ix] = NULL;
                    }
                }
            }
            delete ptrialevent;
            ptrialevent = nullptr;
        }

        if (ptrialcont)
        {
            if (ptrialcont->count > 0)
            {
                // TODO: Free continous data
            }
            delete ptrialcont;
            ptrialcont = nullptr;
        }

#ifdef WIN32
        if (_kbhit())
        {
            kb_ch = _getch();
        }
#endif
    }

    res = testClose();
    if (res < 0)
        printf("testClose failed (%d)!\n", res);
    else
        printf("testClose succeeded\n");

    return 0;
}
