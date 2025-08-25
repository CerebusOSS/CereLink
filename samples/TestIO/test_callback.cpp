#include <iostream>
#include <chrono>
#include <thread>
#include "cbsdk.h"


#define INST 0


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


void chaninfo_callback(uint32_t nInstance, const cbSdkPktType type, const void* pEventData, void* pCallbackData)
{
    if (type == cbSdkPkt_CHANINFO) {
        auto chan_info = *(static_cast<const cbPKT_CHANINFO *>(pEventData));
        auto chan_label = static_cast<std::pair<uint32_t, std::string>*>(pCallbackData);
        chan_label->first = chan_info.chan;
        chan_label->second = chan_info.label;
        printf("chaninfo_callback received CHANINFO for %u:%s\n",
               chan_label->first, chan_label->second.c_str());
    }
}


int main(int argc, char *argv[]) {
    LPCSTR inst_ip = "";
    int inst_port = cbNET_UDP_PORT_CNT;
    LPCSTR client_ip = "";
    // Parse command line arguments.
    {
        if (argc > 1 && argv[1][0] != '-') {inst_ip = argv[1];}
        if (argc > 2 && argv[2][0] != '-') {inst_port = strtol(argv[2], NULL, 10);}
        if (argc > 3 && argv[3][0] != '-') { client_ip = argv[3]; }
    }

    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    cbSdkConnection con = cbSdkConnection();
    con.szOutIP = inst_ip;
    con.nOutPort = inst_port;
    con.szInIP = client_ip;
    con.nInPort = inst_port;
    cbSdkResult res = cbSdkOpen(INST, conType, con);
    handleResult(res);
    if (res == CBSDKRESULT_SUCCESS) {
        printf("cbSdkOpen succeeded\n");
    }
    else {
        printf("Unable to open instrument connection.\n");
        return -1;
    }

    std::pair<uint32_t, std::string> chan_label_pair(0, "");
    res = cbSdkRegisterCallback(INST, cbSdkCallbackType::CBSDKCALLBACK_CHANINFO,
                                &chaninfo_callback,
                                &chan_label_pair);
    std::cout << "Please use central to modify a channel label." << std::flush;
    while (chan_label_pair.first == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Even if we reduce the sleep_for to a very small number, we probably still get 2 callback calls
    //  before we have a chance to unregister.
    cbSdkUnRegisterCallback(INST, cbSdkCallbackType::CBSDKCALLBACK_CHANINFO);
    printf("caller received chan_label for %u:%s\n",
           chan_label_pair.first, chan_label_pair.second.c_str());
    res = cbSdkClose(INST);
    if (res < 0)
        printf("cbSdkClose failed (%d)!\n", res);
    else
        printf("cbSdkClose succeeded\n");
    handleResult(res);

}