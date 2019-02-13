// =STS=> cbmex.cpp[4264].aa38   submit   SMID:41 
///////////////////////////////////////////////////////////////////////
//
// Cerebus MATLAB executable SDK
//
// Copyright (C) 2001-2003 Bionic Technologies, Inc.
// (c) Copyright 2003 - 2008 Cyberkinetics, Inc
// (c) Copyright 2008 - 2014 Blackrock Microsystems, LLC
//
// $Workfile: cbmex.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbmex/cbmex.cpp $
//
//
//  Note:
//   Make sure only the SDK is used here, and not cbhwlib directly
//   this will ensure SDK is capable of whatever MATLAB interface can do
//   Do not throw exceptions, catch possible exceptions and handle them the earliest possible in this library
//   Only functions are exported, no data
//
/*! \file cbmex.cpp
    \brief This is the gateway routine for a MATLAB Math/Graphics Library-based C MATLAB MEX File.

*/

#include "StdAfx.h"
#include <algorithm>  // Use C++ default min and max implementation.
#include <map>
#include <string>
#include <vector>
#include "debugmacs.h"

#include "cbmex.h"
#include "cbsdk.h"
#include "cbHwlibHi.h"

#ifdef _DEBUG
// Comment this line not to have a debug console when debugging cbsdk
#define DEBUG_CONSOLE
#endif

bool g_bMexCall = false;    // True if library called from MATLAB mex, False if from SDK


// Ok, this is unusual. I want to have this created once globally, and then
//  I don't want anyone to be able to call this again. To make it "hidden"
//  I put it into an anonymous namespace.
namespace
{
    typedef enum
    {
        CBMEX_FUNCTION_HELP = 0,
        CBMEX_FUNCTION_OPEN,
        CBMEX_FUNCTION_CLOSE,
        CBMEX_FUNCTION_TIME,
        CBMEX_FUNCTION_TRIALCONFIG,
        CBMEX_FUNCTION_CHANLABEL,
        CBMEX_FUNCTION_TRIALDATA,
        CBMEX_FUNCTION_TRIALCOMMENT,
        CBMEX_FUNCTION_TRIALTRACKING,
        CBMEX_FUNCTION_FILECONFIG,
        CBMEX_FUNCTION_DIGITALOUT,
        CBMEX_FUNCTION_ANALOGOUT,
        CBMEX_FUNCTION_MASK,
        CBMEX_FUNCTION_COMMENT,
        CBMEX_FUNCTION_CONFIG,
        CBMEX_FUNCTION_CCF,
        CBMEX_FUNCTION_SYSTEM,
        CBMEX_FUNCTION_SYNCHOUT,
        CBMEX_FUNCTION_EXT,

        CBMEX_FUNCTION_COUNT,  // This must be the last item
    } MexFxnIndex;

    /**
    * This is the look up table for the command to function list.
    */
    typedef void(*PMexFxn)(	
                            int nlhs,              ///< Number of left hand side (output) arguments
                            mxArray *plhs[],       ///< Array of left hand side arguments
                            int nrhs,              ///< Number of right hand side (input) arguments
                            const mxArray *prhs[]);///< Array of right hand side arguments

    /**
    * Contains the Function and Function Index to use.
    */
    typedef std::pair<PMexFxn, MexFxnIndex> NAME_PAIR;

    /**
    * Defines a table entry with NAME,FXN and FXN_IDX to use.
    * \n LUT = Look Up Table
    */
    typedef std::map<std::string, NAME_PAIR> NAME_LUT;

   /**
    * Creates the look-up table for function lists and commands.
    */
    NAME_LUT CreateLut()
    {
        NAME_LUT table;
        table["help"          ] = NAME_PAIR(&::OnHelp,          CBMEX_FUNCTION_HELP);
        table["open"          ] = NAME_PAIR(&::OnOpen,          CBMEX_FUNCTION_OPEN);
        table["close"         ] = NAME_PAIR(&::OnClose,         CBMEX_FUNCTION_CLOSE);
        table["time"          ] = NAME_PAIR(&::OnTime,          CBMEX_FUNCTION_TIME);
        table["trialconfig"   ] = NAME_PAIR(&::OnTrialConfig,   CBMEX_FUNCTION_TRIALCONFIG);
        table["chanlabel"     ] = NAME_PAIR(&::OnChanLabel,     CBMEX_FUNCTION_CHANLABEL);
        table["trialdata"     ] = NAME_PAIR(&::OnTrialData,     CBMEX_FUNCTION_TRIALDATA);
        table["trialcomment"  ] = NAME_PAIR(&::OnTrialComment,  CBMEX_FUNCTION_TRIALCOMMENT);
        table["trialtracking" ] = NAME_PAIR(&::OnTrialTracking, CBMEX_FUNCTION_TRIALTRACKING);
        table["fileconfig"    ] = NAME_PAIR(&::OnFileConfig,    CBMEX_FUNCTION_FILECONFIG);
        table["digitalout"    ] = NAME_PAIR(&::OnDigitalOut,    CBMEX_FUNCTION_DIGITALOUT);
        table["analogout"     ] = NAME_PAIR(&::OnAnalogOut,     CBMEX_FUNCTION_ANALOGOUT);
        table["mask"          ] = NAME_PAIR(&::OnMask,          CBMEX_FUNCTION_MASK);
        table["comment"       ] = NAME_PAIR(&::OnComment,       CBMEX_FUNCTION_COMMENT);
        table["config"        ] = NAME_PAIR(&::OnConfig,        CBMEX_FUNCTION_CONFIG);
        table["ccf"           ] = NAME_PAIR(&::OnCCF,           CBMEX_FUNCTION_CCF);
        table["system"        ] = NAME_PAIR(&::OnSystem,        CBMEX_FUNCTION_SYSTEM);
        table["synchout"      ] = NAME_PAIR(&::OnSynchOut,      CBMEX_FUNCTION_SYNCHOUT);
        table["ext"           ] = NAME_PAIR(&::OnExtCmd,        CBMEX_FUNCTION_EXT);
        return table;
    };
};

/**
* Cleanup function to be called at exit of the extension
*/
static void matexit()
{
    if (g_bMexCall)
    {
        for (int i = 0; i < cbMAXOPEN; ++i)
            cbSdkClose(i);
        mexPrintf("Cerebus interface unloaded\n");
#ifdef DEBUG_CONSOLE
#ifdef WINN32
        FreeConsole();
#endif
#endif
    }
}

// Author & Date:   Ehsan Azar     8 Nov 2012
/**
* Prints the error message for sdk returned values.
* All non-success results are treated as error, and will drop to MATLAB prompt.
* Command should handle special cases themselves.
*
* @param[in] res		Returned result value by SDK
* @param[in] szCustom	Custom error message to print (Optional)
*/
void PrintErrorSDK(cbSdkResult res, const char * szCustom = NULL)
{
    if (szCustom != NULL && res != CBSDKRESULT_SUCCESS)
    {
        mexPrintf(szCustom);
        mexPrintf(":\n");
    }

    switch(res)
    {
    case CBSDKRESULT_WARNCONVERT:
        mexErrMsgTxt("File conversion is needed");
        break;
    case CBSDKRESULT_WARNCLOSED:
        mexErrMsgTxt("Library is already closed");
        break;
    case CBSDKRESULT_WARNOPEN:
        mexErrMsgTxt("Library is already opened");
        break;
    case CBSDKRESULT_SUCCESS:
        // Do nothing
        break;
    case CBSDKRESULT_NOTIMPLEMENTED:
        mexErrMsgTxt("Not implemented");
        break;
    case CBSDKRESULT_UNKNOWN:
        mexErrMsgTxt("Unknown error");
        break;
    case CBSDKRESULT_INVALIDPARAM:
        mexErrMsgTxt("Invalid parameter");
        break;
    case CBSDKRESULT_CLOSED:
        mexErrMsgTxt("Interface is closed cannot do this operation");
        break;
    case CBSDKRESULT_OPEN:
        mexErrMsgTxt("Interface is open cannot do this operation");
        break;
    case CBSDKRESULT_NULLPTR:
        mexErrMsgTxt("Null pointer");
        break;
    case CBSDKRESULT_ERROPENCENTRAL:
        mexErrMsgTxt("Unable to open Central interface");
        break;
    case CBSDKRESULT_ERROPENUDP:
        mexErrMsgTxt("Unable to open UDP interface (might happen if default)");
        break;
    case CBSDKRESULT_ERROPENUDPPORT:
        mexErrMsgTxt("Unable to open UDP port");
        break;
    case CBSDKRESULT_ERRMEMORYTRIAL:
        mexErrMsgTxt("Unable to allocate RAM for trial cache data");
        break;
    case CBSDKRESULT_ERROPENUDPTHREAD:
        mexErrMsgTxt("Unable to open UDP timer thread");
        break;
    case CBSDKRESULT_ERROPENCENTRALTHREAD:
        mexErrMsgTxt("Unable to open Central communication thread");
        break;
    case CBSDKRESULT_INVALIDCHANNEL:
        mexErrMsgTxt("Invalid channel number");
        break;
    case CBSDKRESULT_INVALIDCOMMENT:
        mexErrMsgTxt("Comment too long or invalid");
        break;
    case CBSDKRESULT_INVALIDFILENAME:
        mexErrMsgTxt("Filename too long or invalid");
        break;
    case CBSDKRESULT_INVALIDCALLBACKTYPE:
        mexErrMsgTxt("Invalid callback type");
        break;
    case CBSDKRESULT_CALLBACKREGFAILED:
        mexErrMsgTxt("Callback register/unregister failed");
        break;
    case CBSDKRESULT_ERRCONFIG:
        mexErrMsgTxt("Trying to run an unconfigured method");
        break;
    case CBSDKRESULT_INVALIDTRACKABLE:
        mexErrMsgTxt("Invalid trackable id, or trackable not present");
        break;
    case CBSDKRESULT_INVALIDVIDEOSRC:
        mexErrMsgTxt("Invalid video source id, or video source not present");
        break;
    case CBSDKRESULT_ERROPENFILE:
        mexErrMsgTxt("Cannot open file");
        break;
    case CBSDKRESULT_ERRFORMATFILE:
        mexErrMsgTxt("Wrong file format");
        break;
    case CBSDKRESULT_OPTERRUDP:
        mexErrMsgTxt("Socket option error (possibly permission issue)");
        break;
    case CBSDKRESULT_MEMERRUDP:
        mexErrMsgTxt(ERR_UDP_MESSAGE);
        break;
    case CBSDKRESULT_INVALIDINST:
        mexErrMsgTxt("Invalid range, instrument address or instrument mode");
        break;
    case CBSDKRESULT_ERRMEMORY:
#ifdef __APPLE__
        mexErrMsgTxt("Memory allocation error trying to establish master connection\n"
                "Consider sysctl -w kern.sysv.shmmax=16777216\n"
                "         sysctl -w kern.sysv.shmall=4194304");

#else
        mexErrMsgTxt("Memory allocation error trying to establish master connection");
#endif
        break;
    case CBSDKRESULT_ERRINIT:
        mexErrMsgTxt("Initialization error");
        break;
    case CBSDKRESULT_TIMEOUT:
        mexErrMsgTxt("Connection timeout error");
        break;
    case CBSDKRESULT_BUSY:
        mexErrMsgTxt("Resource is busy");
        break;
    case CBSDKRESULT_ERROFFLINE:
        mexErrMsgTxt("Instrument is offline");
        break;
    default:
        {
            char errstr[128];
            sprintf(errstr, "Unexpected error (%d)", res);
            mexErrMsgTxt(errstr);
        }
        break;
    }
}

// Author & Date:   Ehsan Azar     8 Nov 2012
/**
* Prints the help for given command.
*
* @param[in]	fxnidx		the cbmex command index
* @param[in]	bErr		indicates if it is an error message, will terminate and fall into MATLAB prompt if true
* @param[in]	szCustom	if specified will be printed before the usage message
*/
void PrintHelp(MexFxnIndex fxnidx, bool bErr = false, const char * szCustom = NULL)
{
    if (szCustom != NULL)
        mexPrintf(szCustom);

    if (fxnidx > CBMEX_FUNCTION_COUNT)
        fxnidx = CBMEX_FUNCTION_COUNT;

    const char * szUsage[CBMEX_FUNCTION_COUNT + 1] = {
        CBMEX_USAGE_HELP,
        CBMEX_USAGE_OPEN,
        CBMEX_USAGE_CLOSE,
        CBMEX_USAGE_TIME,
        CBMEX_USAGE_TRIALCONFIG,
        CBMEX_USAGE_CHANLABEL,
        CBMEX_USAGE_TRIALDATA,
        CBMEX_USAGE_TRIALCOMMENT,
        CBMEX_USAGE_TRIALTRACKING,
        CBMEX_USAGE_FILECONFIG,
        CBMEX_USAGE_DIGITALOUT,
        CBMEX_USAGE_ANALOGOUT,
        CBMEX_USAGE_MASK,
        CBMEX_USAGE_COMMENT,
        CBMEX_USAGE_CONFIG,
        CBMEX_USAGE_CCF,
        CBMEX_USAGE_SYSTEM,
        CBMEX_USAGE_SYNCHOUT,
        CBMEX_USAGE_EXTENSION,

        // Keep this in the end
        CBMEX_USAGE_CBMEX
    };
    /**
    * \todo For CBMEX_USAGE_CBMEX, iterate all command names, instead of hard-coded names help string
    */
    if (bErr)
        mexErrMsgTxt(szUsage[fxnidx]);
    else
        mexPrintf(szUsage[fxnidx]);
}

// Author & Date:   Ehsan Azar     8 Nov 2012
/**
* Processing to do with the command "help command-name".
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnHelp(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    static NAME_LUT lut = CreateLut();        // The actual look up table
    if (nrhs == 1)
    {
        PrintHelp(CBMEX_FUNCTION_COUNT);
        return;
    }
    // make sure there is at least one output argument
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_HELP, true, "Too many outputs requested\n");
    if (nrhs > 2)
        PrintHelp(CBMEX_FUNCTION_HELP, true, "Too many inputs provided\n");

    MexFxnIndex fxnidx = CBMEX_FUNCTION_COUNT;

    char cmdstr[128];
    if (mxGetString(prhs[1], cmdstr, 16))
    {
        PrintHelp(CBMEX_FUNCTION_HELP, true, "Invalid command name\n");
    }

    NAME_LUT::iterator it = lut.find(cmdstr);
    if (it == lut.end())
    {
        PrintHelp(CBMEX_FUNCTION_HELP, true, "Invalid command name\n");
    }
    else
    {
        fxnidx = (*it).second.second;
        PrintHelp(fxnidx);
    }
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "interface = open interface".
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnOpen(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 1;
    char szInstIp[16] = "";
    char szCentralIp[16] = "";
    cbSdkConnection con;
    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 1)
                PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid interface parameter\n");
            int type = 3;
            type = (uint32_t)mxGetScalar(prhs[nFirstParam]);
            if (type > 2)
                PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid input interface value\n");
            conType = (cbSdkConnectionType)type;
            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
        PARAM_RECBUFSIZE,
        PARAM_INST_IP,
        PARAM_INST_PORT,
        PARAM_CENTRAL_IP,
        PARAM_CENTRAL_PORT,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 32))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid\n", i);
                PrintHelp(CBMEX_FUNCTION_OPEN, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "receive-buffer-size") == 0)
            {
                param = PARAM_RECBUFSIZE;
            }
            else if (_strcmpi(cmdstr, "inst-addr") == 0)
            {
                param = PARAM_INST_IP;
            }
            else if (_strcmpi(cmdstr, "inst-port") == 0)
            {
                param = PARAM_INST_PORT;
            }
            else if (_strcmpi(cmdstr, "central-addr") == 0)
            {
                param = PARAM_CENTRAL_IP;
            }
            else if (_strcmpi(cmdstr, "central-port") == 0)
            {
                param = PARAM_CENTRAL_PORT;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid\n", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_OPEN, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_RECBUFSIZE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid receive buffer size");
                con.nRecBufSize = (int)mxGetScalar(prhs[i]);
                break;
            case PARAM_INST_IP:
                if (mxGetString(prhs[i], szInstIp, 16))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid instrument ip address");
                con.szOutIP = szInstIp;
                break;
            case PARAM_INST_PORT:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid instrument port number");
                con.nOutPort = (int)mxGetScalar(prhs[i]);
                break;
            case PARAM_CENTRAL_IP:
                if (mxGetString(prhs[i], szCentralIp, 16))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid central ip address");
                con.szInIP = szCentralIp;
                break;
            case PARAM_CENTRAL_PORT:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_OPEN, true, "Invalid central port number");
                con.nInPort = (int)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is mandatory
        PrintHelp(CBMEX_FUNCTION_OPEN, true, "Last parameter requires value");
    }

    cbSdkVersion ver;
    cbSdkResult res = cbSdkGetVersion(nInstance, &ver);

    if (conType == CBSDKCONNECTION_DEFAULT || conType == CBSDKCONNECTION_CENTRAL)
        mexPrintf("Initializing real-time interface %d.%02d.%02d.%02d (protocol cb%d.%02d)...\n", ver.major, ver.minor, ver.release, ver.beta, ver.majorp, ver.minorp);
    else
        mexPrintf("Initializing UDP real-time interface %d.%02d.%02d.%02d (protocol cb%d.%02d)...\n", ver.major, ver.minor, ver.release, ver.beta, ver.majorp, ver.minorp);

    cbSdkInstrumentType instType;
    res = cbSdkOpen(nInstance, conType, con);
    switch(res)
    {
    case CBSDKRESULT_WARNOPEN:
        mexPrintf("Real-time interface already initialized\n");
        return;
    default:
        PrintErrorSDK(res, "cbSdkOpen()");
        break;
    }

    // Return the actual openned connection
    res = cbSdkGetType(nInstance, &conType, &instType);
    PrintErrorSDK(res, "cbSdkGetType()");
    res = cbSdkGetVersion(nInstance, &ver);
    PrintErrorSDK(res, "cbSdkGetVersion()");

    if (conType < 0 || conType > CBSDKCONNECTION_CLOSED)
        conType = CBSDKCONNECTION_COUNT;
    if (instType < 0 || instType > CBSDKINSTRUMENT_COUNT)
        instType = CBSDKINSTRUMENT_COUNT;
    if (nlhs > 0)
    {
        plhs[0] = mxCreateDoubleScalar(conType);
        if (nlhs > 1)
            plhs[1] = mxCreateDoubleScalar(instType);
    }

    char strConnection[CBSDKCONNECTION_COUNT + 1][8] = {"Default", "Central", "Udp", "Closed", "Unknown"};
    char strInstrument[CBSDKINSTRUMENT_COUNT + 1][13] = {"NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"};
    mexPrintf("%s real-time interface to %s (%d.%02d.%02d.%02d hwlib %d.%02d) successfully initialized\n", strConnection[conType], strInstrument[instType], ver.nspmajor, ver.nspminor, ver.nsprelease, ver.nspbeta, ver.nspmajorp, ver.nspminorp);
}


// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "close".
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnClose(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;

    int nFirstParam = 1;
    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_CLOSE, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_CLOSE, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CLOSE, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_CLOSE, true, "Last parameter requires value");
    }

    cbSdkResult res = cbSdkClose(nInstance);
    switch(res)
    {
    case CBSDKRESULT_WARNCLOSED:
        mexPrintf("Real-time interface already closed\n");
        break;
    default:
        PrintErrorSDK(res, "cbSdkClose()");
        break;
    }
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "time".
*
* \n IN MATLAB =>  time = cbmex('time')
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnTime(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;

    int nFirstParam = 1;

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    bool bSamples = false;
    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_TIME, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "samples") == 0)
            {
                bSamples = true;
            }
            else {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_TIME, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TIME, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_TIME, true, "Last parameter requires value");
    }

    uint32_t cbtime;
    cbSdkResult res = cbSdkGetTime(nInstance, &cbtime);
    PrintErrorSDK(res, "cbSdkGetTime()");

    plhs[0] = mxCreateDoubleScalar(bSamples ? cbtime : cbSdk_SECONDS_PER_TICK * cbtime);
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "chanlabel". Set/Get channel label.
*
* \n IN MATLAB =>
* \n   cbmex( 'chanlabel', [channels], [new_label_cell_array])
* \n   label_cell_array = cbmex( 'chanlabel', channels_vector, [new_label_cell_array])
* \n   label_cell_array = cbmex( 'chanlabel', channels_vector)
* \n   label_cell_array = cbmex( 'chanlabel')
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/

void OnChanLabel(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;

    int nFirstParam = 1;
    int idxNewLabels = 0;

    uint16_t channels[cbMAXCHANS];
    uint32_t count = cbMAXCHANS;
    if (nrhs > 1 && mxIsNumeric(prhs[1]))
    {
        nFirstParam++; // skip the optional
        count = (uint32_t)mxGetNumberOfElements(prhs[1]);
        if (nrhs > 2)
        {
            bool bIsString = (mxGetClassID(prhs[2]) == mxCHAR_CLASS);
            bool bIsParamNext = true;
            if (nrhs > 3)
                bIsParamNext = (mxGetClassID(prhs[3]) == mxCHAR_CLASS);
            if (bIsParamNext)
            {
                if (count == 1 && bIsString)
                {
                    // Special case for cbmex('chanlabel', chan, 'newlabel')
                }
                else if (mxGetClassID(prhs[2]) != mxCELL_CLASS)
                {
                    PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, "Wrong format for new_label_cell_array");
                }
                else if (count != mxGetNumberOfElements(prhs[2]))
                {
                    PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, "Number of channels and number of new labels do not match");
                }
                idxNewLabels = 2;
                nFirstParam++; // skip the optional
            }
        }
        if (count > cbMAXCHANS)
            mexErrMsgTxt("Maximum of 156 channels is possible");
        for (uint32_t i = 0; i < count; ++i)
        {
            channels[i] = (uint16_t)(*(mxGetPr(prhs[1]) + i));
            if (channels[i] == 0|| channels[i] > cbMAXCHANS)
                mexErrMsgTxt("Invalid channel number specified");
        }
    }
    else
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            channels[i] = i + 1;
        }
    }

    if (nrhs > 1 && mxGetClassID(prhs[1]) == mxCELL_CLASS && mxGetNumberOfElements(prhs[1]) == cbMAXCHANS)
    {
        // Special case for cbmex('chanlabel', new_label_cell_array)
        //  with labels specified for ALL the channels
        nFirstParam++; // skip the optional
        idxNewLabels = 1;
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_CHANLABEL, true, "Last parameter requires value");
    }

    // if output specifically requested, or if no new channel labels specified
    // build the cell structure to get previous labels
    if (nlhs > 0 || idxNewLabels == 0)
    {
        mxArray *pca = mxCreateCellMatrix(count, 7);
        for (UINT ch = 1; ch <= count; ch++)
        {
            char   label[32];
            uint32_t bValid[cbMAXUNITS + 1];
            cbSdkResult res = cbSdkGetChannelLabel(nInstance, channels[ch - 1], bValid, label, NULL, NULL);
            PrintErrorSDK(res, "cbSdkGetChannelLabel()");

            mxSetCell(pca, ch - 1, mxCreateString(label));
            if (ch <= cbNUM_ANALOG_CHANS)
            {
                for (int i = 0; i < 6; ++i)
                {
                    mxSetCell(pca, ch - 1 + count * (i + 1), mxCreateDoubleScalar(bValid[i]));
                }
            }
            else if ( (ch == MAX_CHANS_DIGITAL_IN) || (ch == MAX_CHANS_SERIAL) )
            {
                mxSetCell(pca, ch - 1 + count, mxCreateDoubleScalar(bValid[0]));
            }
        }
        plhs[0] = pca;
    }

    // Now set new labels
    if (idxNewLabels > 0)
    {
        char label[128];
        // Handle the case for single channel label assignment
        if (count == 1 && mxGetClassID(prhs[idxNewLabels]) == mxCHAR_CLASS)
        {
            if (mxGetString(prhs[idxNewLabels], label, 16))
                mexErrMsgTxt("Invalid channel label");
            cbSdkResult res = cbSdkSetChannelLabel(nInstance, channels[0], label, 0, NULL);
            PrintErrorSDK(res, "cbSdkSetChannelLabel()");
        }
        else
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                const mxArray * cell_element_ptr = mxGetCell(prhs[idxNewLabels], i);
                if (mxGetClassID(cell_element_ptr) != mxCHAR_CLASS || mxGetString(cell_element_ptr, label, 16))
                {
                    sprintf(label, "Invalid label at %d", i + 1);
                    mexErrMsgTxt(label);
                }
                cbSdkResult res = cbSdkSetChannelLabel(nInstance, channels[i], label, 0, NULL);
                PrintErrorSDK(res, "cbSdkSetChannelLabel()");
            }
        }
    }
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "trialconfig".
*
* \n IN MATLAB =>
* \n   [ active, [ begchan begmask begval endchan endmask endval double waveform continuous event ] ] = cbmex('trialconfig')
* \n   cbmex('trialconfig', bActive )
* \n   cbmex('trialconfig', bActive, [ begchan begmask begval endchan endmask endval ] )
* \n   cbmex('trialconfig', bActive, [ begchan begmask begval endchan endmask endval ], 'double')
* \n   cbmex('trialconfig', bActive, [ begchan begmask begval endchan endmask endval ], 'double', 'waveform', 400)
* \n   cbmex('trialconfig', bActive, [ begchan begmask begval endchan endmask endval ], 'double', 'nocontinuous')
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnTrialConfig(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 2;
    // check the number of input arguments
    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "At least one input is required");

    uint32_t bActive;
    // Process first input argument if available
    bActive = ((uint32_t)mxGetScalar(prhs[1]) > 0);

    cbSdkResult res;

    uint16_t uBegChan   = 0;
    uint32_t uBegMask   = 0;
    uint32_t uBegVal    = 0;
    uint16_t uEndChan   = 0;
    uint32_t uEndMask   = 0;
    uint32_t uEndVal    = 0;
    bool   bDouble    = false;
    bool   bAbsolute  = false;
    uint32_t uWaveforms = 0;
    uint32_t uConts     = cbSdk_CONTINUOUS_DATA_SAMPLES;
    uint32_t uEvents    = cbSdk_EVENT_DATA_SAMPLES;
    uint32_t uComments  = 0;
    uint32_t uTrackings = 0;
    uint32_t bWithinTrial = false;

    res = cbSdkGetTrialConfig(nInstance, &bWithinTrial, &uBegChan, &uBegMask, &uBegVal, &uEndChan, &uEndMask, &uEndVal,
            &bDouble, &uWaveforms, &uConts, &uEvents, &uComments, &uTrackings);

    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            // check for proper data structure
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 6)
                PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid config_vector_in");

            // Trim them to 8-bit and 16-bit values
            double * pcfgvals = mxGetPr(prhs[nFirstParam]);
            uBegChan = ((uint32_t)(*(pcfgvals+0))) & 0x00FF;
            uBegMask = ((uint32_t)(*(pcfgvals+1))) & 0xFFFF;
            uBegVal  = ((uint32_t)(*(pcfgvals+2))) & 0xFFFF;
            uEndChan = ((uint32_t)(*(pcfgvals+3))) & 0x00FF;
            uEndMask = ((uint32_t)(*(pcfgvals+4))) & 0xFFFF;
            uEndVal  = ((uint32_t)(*(pcfgvals+5))) & 0xFFFF;

            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_WAVEFORM,
        PARAM_CONTINUOUS,
        PARAM_EVENT,
        PARAM_COMMENT,
        PARAM_TRACKING,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, errstr);
            }
            if (_strcmpi(cmdstr, "double") == 0)
            {
                bDouble = true;
            }
            else if (_strcmpi(cmdstr, "absolute") == 0)
            {
                bAbsolute = true;
            }
            else if (_strcmpi(cmdstr, "nocontinuous") == 0)
            {
                uConts = 0;
            }
            else if (_strcmpi(cmdstr, "noevent") == 0)
            {
                uEvents = 0;
            }
            else if (_strcmpi(cmdstr, "waveform") == 0)
            {
                param = PARAM_WAVEFORM;
            }
            else if (_strcmpi(cmdstr, "continuous") == 0)
            {
                param = PARAM_CONTINUOUS;
            }
            else if (_strcmpi(cmdstr, "event") == 0)
            {
                param = PARAM_EVENT;
            }
            else if (_strcmpi(cmdstr, "comment") == 0)
            {
                param = PARAM_COMMENT;
            }
            else if (_strcmpi(cmdstr, "tracking") == 0)
            {
                param = PARAM_TRACKING;
            }
            else if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_WAVEFORM:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid waveform count");
                uWaveforms = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_CONTINUOUS:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid continuous count");
                uConts = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_EVENT:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid event count");
                uEvents = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_COMMENT:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid comment count");
                uComments = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_TRACKING:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid tracking count");
                uTrackings = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    }// end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_TRIALCONFIG, true, "Last parameter requires value");
    }

    res = cbSdkSetTrialConfig(nInstance, bActive, uBegChan, uBegMask, uBegVal, uEndChan, uEndMask, uEndVal,
            bDouble, uWaveforms, uConts, uEvents, uComments, uTrackings, bAbsolute);
    PrintErrorSDK(res, "cbSdkSetTrialConfig()");

    // process first output argument if available
    if (nlhs > 0)
        plhs[0] = mxCreateDoubleScalar( (double)bWithinTrial );

    // process second output argument if available
    if (nlhs > 1)
    {
        plhs[1] = mxCreateDoubleMatrix(12, 1, mxREAL);
        double *pcfgvals = mxGetPr(plhs[1]);
        *(pcfgvals+0) = uBegChan;
        *(pcfgvals+1) = uBegMask;
        *(pcfgvals+2) = uBegVal;
        *(pcfgvals+3) = uEndChan;
        *(pcfgvals+4) = uEndMask;
        *(pcfgvals+5) = uEndVal;
        *(pcfgvals+6) = bDouble;
        *(pcfgvals+7) = uWaveforms;
        *(pcfgvals+8) = uConts;
        *(pcfgvals+9) = uEvents;
        *(pcfgvals+10) = uComments;
        *(pcfgvals+11) = uTrackings;
    }
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "trialdata".
*
* \n IN MATLAB =>
* \n timestamps_cell_array = cbmex('trialdata')
* \n timestamps_cell_array = cbmex('trialdata', 1)
* \n [timestamps_cell_array, time, continuous_cell_array] = cbmex('trialdata')
* \n [timestamps_cell_array, time, continuous_cell_array] = cbmex('trialdata',  1)
* \n [time, continuous_cell_array] = cbmex('trialdata')
* \n [time, continuous_cell_array] = cbmex('trialdata',  1)
* \n
* \n Inputs in Matlab:
* \n the 2nd parameter == 0 means to NOT flush the buffer
* \n                  == 1 means to flush the buffer
* \n
* \n Outputs in Matlab:
* \n timestamps_cell_array =
* \n   for neural channel rows 1 - 144,
* \n   { 'label' u0ts u1ts u2ts u3ts u4ts u5ts [waveform]} where u0ts = unit0 timestamps, etc.
* \n   for channels 151 and 152, the digital channels, each row is defined as
* \n   { 'label'  timestamps  values  [empty] [empty] [empty] [empty] }
* \n
* \n continuous_cell_array =
* \n    for each channel with continuous recordings
* \n    { channel_number, sample rate, data_points[] }
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments*
*/

void OnTrialData(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 1;
    bool bFlushBuffer = false;
    cbSdkResult res;

    // make sure there is at least one output argument
    if (nlhs > 3)
        PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, "Too many outputs requested");

    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            // check for proper data structure
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 1)
                PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, "Invalid active parameter");

            // set restartTrialFlag
            if (mxGetScalar(prhs[1]) != 0.0)
                bFlushBuffer = true;

            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_TRIALDATA, true, "Last parameter requires value");
    }

    cbSdkTrialEvent trialevent;
    trialevent.count = 0;
    cbSdkTrialCont trialcont;
    trialcont.count = 0;

    // 1 - Get how many samples are waiting

    bool   bTrialDouble    = false;
    uint32_t uEvents, uConts;

    res = cbSdkGetTrialConfig(nInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bTrialDouble, NULL, &uConts, &uEvents);

    res = cbSdkInitTrialData(nInstance, bFlushBuffer, (nlhs == 2 || !uEvents) ? NULL : &trialevent, (nlhs >= 2 && uConts) ? &trialcont : NULL, NULL, NULL);
    PrintErrorSDK(res, "cbSdkInitTrialData()");


    // 2 - Allocate buffers for samples

    // Does the user want event data?
    if (nlhs != 2)
    {
        // For back-ward compatibility all channels are returned no matter if empty or not
        mxArray *pca = mxCreateCellMatrix(152, 7);
        plhs[0] = pca;
        for (uint32_t channel = 0; channel < 152; channel++)
        {
            char label[32] = {0};
            cbSdkGetChannelLabel(nInstance, channel + 1, NULL, label, NULL, NULL);
            mxSetCell(pca, channel, mxCreateString(label));
        }

        for (uint32_t channel = 0; channel < trialevent.count; channel++)
        {
            uint16_t ch = trialevent.chan[channel]; // Actual channel number
            // Fill timestamps for non-empty channels
            for(UINT u = 0; u <= cbMAXUNITS; u++)
            {
                trialevent.timestamps[channel][u] = NULL;
                uint32_t num_samples = trialevent.num_samples[channel][u];
                if (num_samples)
                {
                    mxArray *mxa;
                    if (bTrialDouble)
                        mxa = mxCreateDoubleMatrix(num_samples, 1, mxREAL);
                    else
                        mxa = mxCreateNumericMatrix(num_samples, 1, mxUINT32_CLASS, mxREAL);
                    trialevent.timestamps[channel][u] = mxGetData(mxa);
                    mxSetCell(pca, (ch - 1) + 152 * (u + 1), mxa);
                }
            }
            // Fill values for non-empty digital or serial channels
            if (ch == MAX_CHANS_DIGITAL_IN || ch == MAX_CHANS_SERIAL)
            {
                uint32_t num_samples = trialevent.num_samples[channel][0];
                if (num_samples)
                {
                    mxArray *mxa;
                    if (bTrialDouble)
                        mxa = mxCreateDoubleMatrix(num_samples, 1, mxREAL);
                    else
                        mxa = mxCreateNumericMatrix(num_samples, 1, mxUINT16_CLASS, mxREAL);
                    trialevent.waveforms[channel] = mxGetData(mxa);
                    mxSetCell(pca, (ch - 1) + 152 * (1 + 1), mxa);
                }
            }
        }
    }

    // Does the user want continuous data?
    if (nlhs >= 2)
    {
        // Output format for continuous data:
        //
        // Each row contains:
        //
        // [channel] [sample rate] [n x 1 data array]

        mxArray *pca = mxCreateCellMatrix(trialcont.count, 3);
        plhs[nlhs - 1] = pca;
        for (uint32_t channel = 0; channel < trialcont.count; channel++)
        {
            trialcont.samples[channel] = NULL;
            uint32_t num_samples = trialcont.num_samples[channel];
            mxSetCell(pca, channel, mxCreateDoubleScalar(trialcont.chan[channel]));
            mxSetCell(pca, channel + trialcont.count, mxCreateDoubleScalar(trialcont.sample_rates[channel]));
            mxArray *mxa;
            if (bTrialDouble)
                mxa = mxCreateDoubleMatrix(num_samples, 1, mxREAL);
            else
                mxa = mxCreateNumericMatrix(num_samples, 1, mxINT16_CLASS, mxREAL);
            trialcont.samples[channel] = mxGetData(mxa);
            mxSetCell(pca, channel + trialcont.count * 2, mxa);
        }
    }

    // 3 - Now get buffered data

    res = cbSdkGetTrialData(nInstance, bFlushBuffer, (nlhs == 2 || !uEvents) ? NULL : &trialevent, (nlhs >= 2 && uConts) ? &trialcont : NULL, NULL, NULL);
    PrintErrorSDK(res, "cbSdkGetTrialData()");

    // Does the user want event data?
    if (nlhs != 2)
    {
        // Buffers are already filled
    }

    // Does the user want continuous data?
    if (nlhs >= 2)
    {
        // Buffers are already filled
        plhs[nlhs - 2] = mxCreateDoubleScalar(cbSdk_SECONDS_PER_TICK * trialcont.time);
    }
}

// Author & Date:   Ehsan Azar     26 Oct 2011
/**
* Processing to do with the command "trialcomment".
*
* \n IN MATLAB =>
* \n [comments_cell_array ts] = cbmex('trialcomment')
* \n [comments_cell_array ts] = cbmex('trialcomment', 1)
* \n
* \n Inputs:
* \n the 2nd parameter == 0 means to NOT flush the buffer
* \n                   == 1 means to flush the buffer
* \n
* \n Outputs:
* \n comments_cell_array: is an array of the variable-sized comments, each row is a comment
* \n ts: is the timestamps
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/

void OnTrialComment(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 1;
    bool bFlushBuffer = false;
    cbSdkResult res;

    // make sure there is at least one output argument
    if (nlhs > 4)
        PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, "Too many outputs requested");

    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            // check for proper data structure
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 1)
                PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, "Invalid active parameter");

            // set restartTrialFlag
            if (mxGetScalar(prhs[1]) != 0.0)
                bFlushBuffer = true;

            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_TRIALCOMMENT, true, "Last parameter requires value");
    }

    cbSdkTrialComment trialcomment;

    bool   bTrialDouble    = false;
    res = cbSdkGetTrialConfig(nInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            &bTrialDouble);

    // 1 - Get how many samples are waiting

    res = cbSdkInitTrialData(nInstance, bFlushBuffer, NULL, NULL, &trialcomment, NULL);
    PrintErrorSDK(res, "cbSdkInitTrialData()");

    trialcomment.comments = NULL;
    // 2 - Allocate buffers
    {
        mxArray *pca = mxCreateCellMatrix(trialcomment.num_samples, 1);
        plhs[0] = pca;
        if (trialcomment.num_samples)
        {
            trialcomment.comments = (uint8_t * *)mxMalloc(trialcomment.num_samples * sizeof(uint8_t *));
            for (int i = 0; i < trialcomment.num_samples; ++i)
            {
                const mwSize dims[2] = {1, cbMAX_COMMENT + 1};
                mxArray *mxa = mxCreateCharArray(2, dims);
                mxSetCell(pca, i, mxa);
                trialcomment.comments[i] = (uint8_t *)mxGetData(mxa);
            }
        }
    }
    trialcomment.timestamps = NULL;
    if (nlhs > 1)
    {
        mxArray *mxa;
        if (bTrialDouble)
            mxa = mxCreateDoubleMatrix(trialcomment.num_samples, 1, mxREAL);
        else
            mxa = mxCreateNumericMatrix(trialcomment.num_samples, 1, mxUINT32_CLASS, mxREAL);
        plhs[1] = mxa;
        trialcomment.timestamps = mxGetData(mxa);
    }
    trialcomment.rgbas = NULL;
    if (nlhs > 2)
    {
        mxArray *mxa;
        mxa = mxCreateNumericMatrix(trialcomment.num_samples, 1, mxUINT32_CLASS, mxREAL);
        plhs[2] = mxa;
        trialcomment.rgbas = (uint32_t *)mxGetData(mxa);
    }
    trialcomment.charsets = NULL;
    if (nlhs > 3)
    {
        mxArray *mxa;
        mxa = mxCreateNumericMatrix(trialcomment.num_samples, 1, mxUINT8_CLASS, mxREAL);
        plhs[3] = mxa;
        trialcomment.charsets = (uint8_t *)mxGetData(mxa);
    }

    // 3 - Now get buffered data

    res = cbSdkGetTrialData(nInstance, bFlushBuffer, NULL, NULL, &trialcomment, NULL);
    PrintErrorSDK(res, "cbSdkGetTrialData()");

    // NSP returns strings as char, but Matlab wants wide characters.  The following function
    // converts the char string to wchar_t string to return to Matlab
    wchar_t szTempWcString[cbMAX_COMMENT];
    for (unsigned int i = 0; i < trialcomment.num_samples; ++i)
    {
        memset(szTempWcString, 0, sizeof(szTempWcString));
        for (unsigned int j = 0; j < strlen((char*)trialcomment.comments[i]); ++j)
        {
            szTempWcString[j] = trialcomment.comments[i][j];
        }
        wcscpy((wchar_t*)trialcomment.comments[i], szTempWcString);
    }

    // free memory
    if (trialcomment.comments)
        mxFree(trialcomment.comments);
}

// Author & Date:   Ehsan Azar     26 Oct 2011
/**
* Processing to do with the command "trialtracking".
*
* \n IN MATLAB =>
* \n tracking_cell_array = cbmex('trialtracking')
* \n tracking_cell_array = cbmex('trialtracking', 1)
* \n
* \n Inputs:
* \n the 2nd parameter == 0 means to NOT flush the buffer
* \n                  == 1 means to flush the buffer
* \n
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnTrialTracking(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 1;
    bool bFlushBuffer = false;
    cbSdkResult res;

    if (nlhs > 1)
        PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, "Too many outputs requested");

    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            // check for proper data structure
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 1)
                PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, "Invalid active parameter");

            // set restartTrialFlag
            if (mxGetScalar(prhs[1]) != 0.0)
                bFlushBuffer = true;

            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_TRIALTRACKING, true, "Last parameter requires value");
    }

    cbSdkTrialTracking trialtracking;

    // 1 - Get how many samples are waiting

    res = cbSdkInitTrialData(nInstance, bFlushBuffer, NULL, NULL, NULL, &trialtracking);
    PrintErrorSDK(res, "cbSdkInitTrialData()");

    bool   bTrialDouble    = false;
    res = cbSdkGetTrialConfig(nInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            &bTrialDouble);

    // 2 - Allocate buffers
    mxArray *pca_rb[cbMAXTRACKOBJ] = {NULL};
    mxArray *pca = mxCreateCellMatrix(trialtracking.count, 6);
    plhs[0] = pca;
    for (int i = 0; i < trialtracking.count; ++i)
    {
        mxSetCell(pca, i * 6 + 0, mxCreateString((char *)trialtracking.names[i]));
        mxArray *mxa;
        mxa = mxCreateNumericMatrix(3, 1, mxUINT16_CLASS, mxREAL);
        mxSetCell(pca, i * 6 + 1, mxa);
        uint16_t * pvals = (uint16_t *)mxGetData(mxa);
        *(pvals + 0) = trialtracking.types[i];
        *(pvals + 1) = trialtracking.ids[i];
        *(pvals + 2) = trialtracking.max_point_counts[i];
        if (bTrialDouble)
            mxa = mxCreateDoubleMatrix(trialtracking.num_samples[i], 1, mxREAL);
        else
            mxa = mxCreateNumericMatrix(trialtracking.num_samples[i], 1, mxUINT32_CLASS, mxREAL);
        mxSetCell(pca, i * 6 + 2, mxa);
        trialtracking.timestamps[i] = mxGetData(mxa);

        mxa = mxCreateNumericMatrix(trialtracking.num_samples[i], 1, mxUINT32_CLASS, mxREAL);
        mxSetCell(pca, i * 6 + 3, mxa);
        trialtracking.synch_timestamps[i] = (uint32_t *)mxGetData(mxa);

        mxa = mxCreateNumericMatrix(trialtracking.num_samples[i], 1, mxUINT32_CLASS, mxREAL);
        mxSetCell(pca, i * 5 + 4, mxa);
        trialtracking.synch_frame_numbers[i]= (uint32_t *)mxGetData(mxa);

        trialtracking.point_counts[i] = (uint16_t *)mxMalloc(trialtracking.num_samples[i] * sizeof(uint16_t));

        bool bWordData = false; // if data is of word-length
        int dim_count = 2; // number of dimensionf for each point
        switch(trialtracking.types[i])
        {
        case cbTRACKOBJ_TYPE_2DMARKERS:
        case cbTRACKOBJ_TYPE_2DBLOB:
        case cbTRACKOBJ_TYPE_2DBOUNDARY:
            dim_count = 2;
            break;
        case cbTRACKOBJ_TYPE_1DSIZE:
            bWordData = true;
            dim_count = 1;
            break;
        default:
            dim_count = 3;
            break;
        }

        if (bWordData)
            trialtracking.coords[i] = (void * *)mxMalloc(trialtracking.num_samples[i] * sizeof(uint32_t *));
        else
            trialtracking.coords[i] = (void * *)mxMalloc(trialtracking.num_samples[i] * sizeof(uint16_t *));

        // Rigid-body cell array
        pca_rb[i] = mxCreateCellMatrix(trialtracking.num_samples[i], 1);
        mxSetCell(pca, i * 5 + 5, pca_rb[i]);

        // We allocate for the maximum number of points, later we reduce dimension
        for (int j = 0; j < trialtracking.num_samples[i]; ++j)
        {
            if (bWordData)
            {
                mxa = mxCreateNumericMatrix(trialtracking.max_point_counts[i], dim_count, mxUINT32_CLASS, mxREAL);
                trialtracking.coords[i][j] = (uint32_t *)mxGetData(mxa);
            }
            else
            {
                mxa = mxCreateNumericMatrix(trialtracking.max_point_counts[i], dim_count, mxUINT16_CLASS, mxREAL);
                trialtracking.coords[i][j] = (uint16_t *)mxGetData(mxa);
            }
            mxSetCell(pca_rb[i], j, mxa);
        } //end for (int j
    } //end for (int i

    // 3 - Now get buffered data
    res = cbSdkGetTrialData(nInstance, bFlushBuffer, NULL, NULL, NULL, &trialtracking);
    PrintErrorSDK(res, "cbSdkGetTrialData()");

    // Reduce dimensions if needed,
    //  and free memory
    for (int i = 0; i < trialtracking.count; ++i)
    {
        for (int j = 0; j < trialtracking.num_samples[i]; ++j)
        {
            mxArray *mxa = mxGetCell(pca_rb[i], j);
            mxSetM(mxa, trialtracking.point_counts[i][j]);
        }
        mxFree(trialtracking.point_counts[i]);
        mxFree(trialtracking.coords[i]);
    }
}

// Author & Date:   Kirk Korver     13 Jun 2005
/**
* Processing to do with the command "fileconfig".
*
* \n In MATLAB use =>
* \n      cbmex('fileconfig', filename, comments, 1)      to start recording
* \n         -or-
* \n     cbmex('fileconfig', filename, comments, 0)      to stop recording
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnFileConfig(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    bool bInstanceFound = false;
    int nFirstParam = 4;
    bool bGetFileInfo = false;
    cbSdkResult res;

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
        PARAM_OPTION,
    } param = PARAM_NONE;

    // Do a quick look at the options just to find the instance if specified
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        
        if (param == PARAM_NONE)
        {

            char cmdstr[255];
            if (mxGetString(prhs[i], cmdstr, 255))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "option") == 0)
            {
                param = PARAM_OPTION;
            }
            else
            {
                param = PARAM_NONE; // Just something valid but instance
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                bInstanceFound = true;
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam

    if ((nlhs > 0) && ((!bInstanceFound && (nrhs == 1)) || (bInstanceFound && (nrhs == 3))))
    {
        if (nlhs > 3)
            PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Too many outputs requested");
        bGetFileInfo = true;
    }
    else
    {
        if (nlhs > 0)
            PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Too many outputs requested");
    }

    if (bGetFileInfo)
    {
        char filename[256] = {'\0'};
        char username[256] = {'\0'};
        bool bRecording = false;
        res = cbSdkGetFileConfig(nInstance, filename, username, &bRecording);
        PrintErrorSDK(res, "cbSdkGetFileConfig()");
        plhs[0] = mxCreateDoubleScalar(bRecording);
        if (nlhs > 1)
            plhs[1] = mxCreateString(filename);
        if (nlhs > 2)
            plhs[2] = mxCreateString(username);
        // If no inputs given, nothing else to do
        if ((!bInstanceFound && (nrhs == 1)) || (bInstanceFound && (nrhs == 3)))
            return;
    }

    if (nrhs < 4)
        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Too few inputs provided");

    // declare the packet that will be sent
    cbPKT_FILECFG fcpkt;

    // fill in the filename string
    if (mxGetString(prhs[1], fcpkt.filename, 255))
        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid file name specified");

    // fill in the comment string
    if (mxGetString(prhs[2], fcpkt.comment, 255))
        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid comment specified");

    if (!mxIsNumeric(prhs[3]) || mxGetNumberOfElements(prhs[3]) != 1)
        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid action parameter");

    uint32_t bStart = (uint32_t) mxGetScalar(prhs[3]);
    uint32_t options = cbFILECFG_OPT_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "option") == 0)
            {
                param = PARAM_OPTION;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_OPTION:
                {
                    char cmdstr[128];
                    // check for proper data structure
                    if (mxGetClassID(prhs[i]) != mxCHAR_CLASS || mxGetString(prhs[i], cmdstr, 10))
                        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid option parameter");
                    if (_strcmpi(cmdstr, "none") == 0)
                    {
                        options = cbFILECFG_OPT_NONE;
                    }
                    else if (_strcmpi(cmdstr, "close") == 0)
                    {
                        options = cbFILECFG_OPT_CLOSE;
                    }
                    else if (_strcmpi(cmdstr, "open") == 0)
                    {
                        options = cbFILECFG_OPT_OPEN;
                    }
                    else
                    {
                        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Invalid option parameter");
                    }
                }
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_FILECONFIG, true, "Last parameter requires value");
    }

    res = cbSdkSetFileConfig(nInstance, fcpkt.filename, fcpkt.comment, bStart, options);
    PrintErrorSDK(res, "cbSdkSetFileConfig()");
}


// Author & Date:   Hyrum L. Sessions       1 Apr 2008
/**
* Processing to do with the command "digitalout".
*
* \n In MATLAB use =>
* \n      cbmex('digitalout', channel, 1)      to set digital out
* \n        -or-
* \n      cbmex('digitalout', channel, 0)      to clear digital out
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnDigitalOut(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 3;
    uint16_t nChannel = 0;
    uint16_t nValue = 99;
    bool bHasNewParams = false;
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    cbPKT_CHANINFO chaninfo;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Too few inputs provided");
    if (nlhs > 1)
        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Too many outputs requested");

    if (!mxIsNumeric(prhs[1]) || mxGetNumberOfElements(prhs[1]) != 1)
        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid channel parameter");
    nChannel = (uint16_t) mxGetScalar(prhs[1]);

    if (2 < nrhs)
    {
        if (mxGetNumberOfElements(prhs[2]) != 1)
            nFirstParam = 2;
        else
            nValue = (uint16_t) mxGetScalar(prhs[2]);

        enum
        {
            PARAM_NONE,
            PARAM_MONITOR,
            PARAM_TRACK,
            PARAM_TRIGGER,
            PARAM_TIMED,
            PARAM_OFFSET,
            PARAM_VALUE,
            PARAM_INPUT,
            PARAM_INSTANCE,
        } param = PARAM_NONE;

        bool bDisable = false;
        int nIdxTimed = 0;
        int nIdxMonitor = 0;
        int nIdxTrigger = 0;
        uint16_t  nTrigChan = 0;
        uint16_t  nTrigValue = 0;
        uint32_t  nOffset = 0;

        // Do a quick look at the options just to find the instance if specified
        for (int i = nFirstParam; i < nrhs; ++i)
        {
            if (param == PARAM_NONE)
            {
                char cmdstr[128];
                if (mxGetString(prhs[i], cmdstr, 16))
                {
                    char errstr[128];
                    sprintf(errstr, "Parameter %d is invalid", i);
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, errstr);
                }
                if (_strcmpi(cmdstr, "instance") == 0)
                {
                    param = PARAM_INSTANCE;
                }
                else if (_strcmpi(cmdstr, "disable") == 0)
                {
                    param = PARAM_NONE;
                }
                else if (_strcmpi(cmdstr, "track") == 0)
                {
                    param = PARAM_NONE;
                }
                else
                {
                    param = PARAM_MONITOR; // Just something valid but instance
                }
            }
            else
            {
                switch(param)
                {
                case PARAM_INSTANCE:
                    if (!mxIsNumeric(prhs[i]))
                        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid instance number");
                    nInstance = (uint32_t)mxGetScalar(prhs[i]);
                    break;
                default:
                    break;
                }
                param = PARAM_NONE;
            }
        } // end for (int i = nFirstParam
        if (param != PARAM_NONE)
        {
            // Some parameter did not have a value, and value is non-optional
            PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Last parameter requires value");
        }

        // Get old config, so that we would only change the bits requested
        res = cbSdkGetChannelConfig(nInstance, nChannel, &chaninfo);
        PrintErrorSDK(res, "cbSdkGetChannelConfig()");

        // Process remaining input arguments if available
        for (int i = nFirstParam; i < nrhs; ++i)
        {
            if (param == PARAM_NONE)
            {
                char cmdstr[128];
                if (mxGetString(prhs[i], cmdstr, 16))
                {
                    char errstr[128];
                    sprintf(errstr, "Parameter %d is invalid", i);
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, errstr);
                }
                else if (_strcmpi(cmdstr, "monitor") == 0)
                {
                    param = PARAM_MONITOR;
                }
                else if (_strcmpi(cmdstr, "track") == 0)
                {
                    if (nIdxMonitor == 0)
                        mexErrMsgTxt("Cannot track with no monitor channel specified");
                    chaninfo.doutopts |= cbDOUT_TRACK;
                    bHasNewParams = true;
                }
                else if (_strcmpi(cmdstr, "trigger") == 0)
                {
                    param = PARAM_TRIGGER;
                }
                else if (_strcmpi(cmdstr, "value") == 0)
                {
                    param = PARAM_VALUE;
                }
                else if (_strcmpi(cmdstr, "input") == 0)
                {
                    param = PARAM_INPUT;
                }
                else if (_strcmpi(cmdstr, "disable") == 0)
                {
                    bDisable = true;
                    if (i != 2)
                        mexErrMsgTxt("Cannot specify any other parameters with 'disable' command");
                    chaninfo.doutopts &= ~(cbDOUT_FREQUENCY | cbDOUT_TRIGGERED | cbDOUT_TRACK);
                    chaninfo.monsource = 0;
                    bHasNewParams = true;
                }
                else if (_strcmpi(cmdstr, "timed") == 0)
                {
                    param = PARAM_TIMED;
                }
                else if (_strcmpi(cmdstr, "offset") == 0)
                {
                    param = PARAM_OFFSET;
                }
                else if (_strcmpi(cmdstr, "instance") == 0)
                {
                    param = PARAM_INSTANCE;
                }
                else
                {
                    char errstr[128];
                    sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, errstr);
                }
            }
            else
            {
                switch(param)
                {
                case PARAM_MONITOR:
                    if (nIdxMonitor)
                        mexErrMsgTxt("Cannot specify multiple monitor commands");
                    if (nIdxTimed)
                        mexErrMsgTxt("Cannot specify monitor and timed commands together");
                    if (nIdxTrigger)
                        mexErrMsgTxt("Cannot specify monitor and trigger commands together");
                    nIdxMonitor = i;
                    chaninfo.doutopts &= ~(cbDOUT_FREQUENCY | cbDOUT_TRIGGERED);
                    bHasNewParams = true;
                    break;
                case PARAM_TRIGGER:
                    if (nIdxTrigger)
                        mexErrMsgTxt("Cannot specify multiple waveform triggers");
                    if (nIdxMonitor)
                        mexErrMsgTxt("Cannot specify trigger and monitor commands together");
                    if (nIdxTimed)
                        mexErrMsgTxt("Cannot specify trigger and timed commands together");
                    nIdxTrigger = i;
                    chaninfo.doutopts &= ~(cbDOUT_FREQUENCY | cbDOUT_TRACK);
                    chaninfo.doutopts |= cbDOUT_TRIGGERED;
                    bHasNewParams = true;
                    break;
                case PARAM_TIMED:
                    if (nIdxTimed)
                        mexErrMsgTxt("Cannot specify multiple timed commands");
                    if (nIdxMonitor)
                        mexErrMsgTxt("Cannot specify timed and monitor commands together");
                    if (nIdxTrigger)
                        mexErrMsgTxt("Cannot specify timed and trigger commands together");
                    nIdxTimed = i;
                    chaninfo.doutopts &= ~(cbDOUT_TRACK | cbDOUT_TRIGGERED);
                    chaninfo.doutopts |= cbDOUT_FREQUENCY;
                    bHasNewParams = true;
                    break;
                case PARAM_OFFSET:
                    if (!mxIsNumeric(prhs[i]))
                        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid offset number");
                    nOffset = (uint16_t)(*mxGetPr(prhs[i]));
                    bHasNewParams = true;
                    break;
                case PARAM_VALUE:
                    if (!mxIsNumeric(prhs[i]))
                        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid value number");
                    nTrigValue = (uint16_t)(*mxGetPr(prhs[i]));
                    bHasNewParams = true;
                    break;
                case PARAM_INPUT:
                    if (!mxIsNumeric(prhs[i]))
                        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid input number");
                    nTrigChan = (uint16_t)(*mxGetPr(prhs[i]));
                    bHasNewParams = true;
                    break;
                case PARAM_INSTANCE:
                    if (!mxIsNumeric(prhs[i]))
                        PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid instance number");
                    nInstance = (uint32_t)mxGetScalar(prhs[i]);
                    break;
                default:
                    break;
                }
                param = PARAM_NONE;
            }
        } // end for (int i = nFirstParam

        if (param != PARAM_NONE)
        {
            // Some parameter did not have a value, and value is non-optional
            PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Last parameter requires value");
        }

        if (nIdxTrigger == 0 && nIdxMonitor == 0 && !nIdxTimed == 0 && !bDisable && !nValue)
        {
            PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true,
                "No action specified\n"
                "specify monitor, trigger, timed or disable");
        }

        // If any trigger is specified, parse it
        if (nIdxTimed)
        {
            char cmdstr[128];

            // check for proper data structure
            if (mxGetClassID(prhs[nIdxTimed]) != mxCHAR_CLASS || mxGetString(prhs[nIdxTimed], cmdstr, 10))
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid timed parameters");

            if (_strcmpi(cmdstr, "frequency") == 0)
            {
                if (nTrigChan == 0 || nTrigChan > 15000)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid timer frequency");
                if (nTrigValue == 0 || nTrigValue > 99)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid timer duty cycle");
                if (nOffset > 30000)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid timer offset");
                int nTotalSamples = 30000 / nTrigChan;
                chaninfo.highsamples = std::max(1, (int)((float)nTotalSamples * (nTrigValue / 100.0)));
                chaninfo.lowsamples = nTotalSamples - chaninfo.highsamples;
                chaninfo.offset = nOffset;
            }
            if (_strcmpi(cmdstr, "samples") == 0)
            {
                if (nTrigChan == 0 || nTrigChan > 15000)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid high sample value");
                if (nTrigValue == 0 || nTrigValue > 15000)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid low sample value");
                chaninfo.lowsamples = nTrigValue;
                chaninfo.highsamples = nTrigChan;
                if (nTrigValue > 30000)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid timer offset");
                chaninfo.offset = nOffset;
            }
        }

        // If any trigger is specified, parse it
        if (nIdxTrigger)
        {
            char cmdstr[128];
            // check for proper data structure
            if (mxGetClassID(prhs[nIdxTrigger]) != mxCHAR_CLASS || mxGetString(prhs[nIdxTrigger], cmdstr, 10))
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid trigger");

            if (_strcmpi(cmdstr, "dinrise") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_DINPREG;
                if (nTrigChan == 0 || nTrigChan > 16)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Only bits 1-16 (first digital input) trigger is supported");
                chaninfo.trigchan = nTrigChan;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "dinfall") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_DINPFEG;
                if (nTrigChan == 0 || nTrigChan > 16)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Only bits 1-16 (first digital input) trigger is supported");
                chaninfo.trigchan = nTrigChan;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "spike") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_SPIKEUNIT;
                if (nTrigChan == 0 || nTrigChan > cbMAXCHANS)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid channel number");
                chaninfo.trigchan = nTrigChan;
                if (nTrigValue == 0 || nTrigValue > cbMAXUNITS)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid unit number");
                chaninfo.trigval = nTrigValue;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "roi") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_COMMENTCOLOR;
                if (nTrigChan == 0 || nTrigChan > 4)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid ROI");
                chaninfo.trigval = nTrigChan;
                if (nTrigValue == 0 || nTrigValue > 2)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid NeuroMotive Event");
                    chaninfo.trigval += nTrigValue * 256;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "cmtcolor") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_COMMENTCOLOR;
                if (nTrigChan == 0 || nTrigChan > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid color");
                chaninfo.trigchan = nTrigChan;
                if (nTrigValue == 0 || nTrigValue > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid color");
                chaninfo.trigval = nTrigValue;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "softreset") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_SOFTRESET;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else if (_strcmpi(cmdstr, "extension") == 0)
            {
                chaninfo.trigtype = cbSdkWaveformTrigger_EXTENSION;
                if (nOffset  == 0 || nOffset > 65535)
                    PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid samples high");
                chaninfo.highsamples = nOffset;
            }
            else
            {
                PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid trigger");
            }
        }

        if (nIdxMonitor)
        {
            char cmdstr[128];
            // check for proper data structure
            chaninfo.monsource = nTrigChan;
            chaninfo.doutopts &= ~cbDOUT_MONITOR_UNIT_ALL;
            if (mxGetClassID(prhs[nIdxMonitor]) != mxCHAR_CLASS || mxGetString(prhs[nIdxMonitor], cmdstr, sizeof(cmdstr)))
                PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid monitor");
            if (strstr(cmdstr, "unclass") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT0;
            }
            if (strstr(cmdstr, "unit1") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT1;
            }
            if (strstr(cmdstr, "unit2") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT2;
            }
            if (strstr(cmdstr, "unit3") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT3;
            }
            if (strstr(cmdstr, "unit4") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT4;
            }
            if (strstr(cmdstr, "unit5") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT5;
            }
            if (strstr(cmdstr, "all") != 0)
            {
                chaninfo.doutopts |= cbDOUT_MONITOR_UNIT_ALL;
            }
            if (0 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT_ALL))
            {
                PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid monitor unit");
            }
            if (chaninfo.monsource == 0 || chaninfo.monsource > cbNUM_ANALOG_CHANS)
                PrintHelp(CBMEX_FUNCTION_DIGITALOUT, true, "Invalid input channel number");
        }
    }

    // if output specifically requested, or if no new configuration specified
    // build the cell structure to get previous values
    if (nlhs > 0 || ! bHasNewParams)
    {
        int count = 4; // number of parameters
        char   string[128];
        mxArray *pca = mxCreateCellMatrix(count, 2);
        plhs[0] = pca;

        // describe the monitoring mode and parameters
        mxSetCell(pca, 0, mxCreateString("monitor"));
        if ((0 == (cbDOUT_FREQUENCY & chaninfo.doutopts)) && (0 == (cbDOUT_TRIGGERED & chaninfo.doutopts)) && (0 != chaninfo.monsource))
        {
            sprintf(string, "monitor chan %d", chaninfo.monsource);
            if (cbDOUT_MONITOR_UNIT_ALL == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT_ALL))
                strncat(string, " all units", sizeof(string) - strlen(string) - 1);
            else
            {
                if (cbDOUT_MONITOR_UNIT0 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT0))
                    strncat(string, " unclass ", sizeof(string) - strlen(string) - 1);
                if (cbDOUT_MONITOR_UNIT1 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT1))
                    strncat(string, " unit1 ", sizeof(string) - strlen(string) - 1);
                if (cbDOUT_MONITOR_UNIT2 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT2))
                    strncat(string, " unit2 ", sizeof(string) - strlen(string) - 1);
                if (cbDOUT_MONITOR_UNIT3 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT3))
                    strncat(string, " unit3 ", sizeof(string) - strlen(string) - 1);
                if (cbDOUT_MONITOR_UNIT4 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT4))
                    strncat(string, " unit4 ", sizeof(string) - strlen(string) - 1);
                if (cbDOUT_MONITOR_UNIT5 == (chaninfo.doutopts & cbDOUT_MONITOR_UNIT5))
                    strncat(string, " unit5 ", sizeof(string) - strlen(string) - 1);
            }
        }
        else
            strncpy(string, "disable", sizeof(string));
        mxSetCell(pca, count + 0, mxCreateString(string));

        // describe if track recently selected channel is on of off
        mxSetCell(pca, 1, mxCreateString("track"));
        if (cbDOUT_TRACK == (chaninfo.doutopts & cbDOUT_TRACK))
            strncpy(string, "yes", sizeof(string));
        else
            strncpy(string, "no", sizeof(string));
        mxSetCell(pca, count + 1, mxCreateString(string));

        // describe the timed mode and parameters
        mxSetCell(pca, 2, mxCreateString("timed"));
        memset(string, 0, sizeof(string));
        if (cbDOUT_FREQUENCY == (chaninfo.doutopts & cbDOUT_FREQUENCY))
            sprintf(string, "Timed %dHz %d%% Duty Cycle - Samples High %d, Low %d, Offset %d", 
                    (int)(30000.0 / ((float)chaninfo.highsamples + (float)chaninfo.lowsamples) + 0.5),
                    (int)((float)chaninfo.highsamples / (float)(chaninfo.highsamples + chaninfo.lowsamples) * 100.0),
                    chaninfo.highsamples, chaninfo.lowsamples, chaninfo.offset);
        else
            sprintf(string, "disable");
        mxSetCell(pca, count + 2, mxCreateString(string));

        // describe the trigger mode and parameters
        mxSetCell(pca, 3, mxCreateString("trigger"));
        memset(string, 0, sizeof(string));
        if (cbDOUT_TRIGGERED == (chaninfo.doutopts & cbDOUT_TRIGGERED))
        {
            switch(chaninfo.trigtype)
            {
            case cbSdkWaveformTrigger_DINPREG:
                sprintf(string, "dinrise bit %d for %d samples", chaninfo.trigchan, chaninfo.highsamples);
                break;
            case cbSdkWaveformTrigger_DINPFEG:
                sprintf(string, "dinfall bit %d for %d samples", chaninfo.trigchan, chaninfo.highsamples);
                break;
            case cbSdkWaveformTrigger_SPIKEUNIT:
                sprintf(string, "spike on chan %d unit %d for %d samples", chaninfo.trigchan, chaninfo.trigval, chaninfo.highsamples);
                break;
            case cbSdkWaveformTrigger_COMMENTCOLOR:
                sprintf(string, "roi ROI %d %s for %d samples", chaninfo.trigval & 0xFF, (1 == chaninfo.trigval / 256) ? "Enter" : "Exit", chaninfo.highsamples);
                break;
            case cbSdkWaveformTrigger_SOFTRESET:
                sprintf(string, "softreset for %d samples", chaninfo.highsamples);
                break;
            case cbSdkWaveformTrigger_EXTENSION:
                sprintf(string, "extension for %d samples", chaninfo.highsamples);
                break;
            default:
                sprintf(string, "off");
            }
        }
        else
            sprintf(string, "disable");
        mxSetCell(pca, count + 3, mxCreateString(string));
    }

    // if new configuration to send
    if (bHasNewParams)
    {
        res = cbSdkSetChannelConfig(nInstance, nChannel, &chaninfo);
        PrintErrorSDK(res, "cbSdkSetChannelConfig()");
    }

    // set the output value if specified
    if (99 != nValue)
    {
        res = cbSdkSetDigitalOutput(nInstance, nChannel, nValue);
        PrintErrorSDK(res, "cbSdkSetDigitalOutput()");
    }
}

// Author & Date:   Ehsan Azar       26 Oct 2011
/**
* Processing to do with the command "analogout", to set specified waveform.
*
* \n In MATLAB use =>
* \n      cbmex('analogout', channel, [<parameter>, [value]])
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnAnalogOut(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 2;

    if (nrhs < 3)
        PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Too many outputs requested");

    enum
    {
        PARAM_NONE,
        PARAM_PULSES,
        PARAM_SEQUENCE,
        PARAM_SINUSOID,
        PARAM_OFFSET,
        PARAM_REPEATS,
        PARAM_TRIGGER,
        PARAM_VALUE,
        PARAM_INDEX,
        PARAM_INPUT,
        PARAM_MONITOR,
        PARAM_TRACK,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    bool bUnitMv = false; // mv voltage units
    bool bUnitMs = false; // ms interval units
    bool bPulses = false;
    bool bDisable = false;
    double dOffset = 0;
    uint16_t duration[cbMAX_WAVEFORM_PHASES];
    int16_t  amplitude[cbMAX_WAVEFORM_PHASES];

    cbSdkAoutMon mon;
    memset(&mon, 0, sizeof(mon));

    cbSdkWaveformData wf;
    wf.type = cbSdkWaveform_NONE;
    wf.repeats = 0;
    wf.trig = cbSdkWaveformTrigger_NONE;
    wf.trigChan = 0;
    wf.trigValue = 0;
    wf.trigNum = 0;
    wf.offset = 0;
    wf.duration = duration;
    wf.amplitude = amplitude;
    int nIdxWave = 0;
    int nIdxTrigger = 0;
    int nIdxMonitor = 0;

    // Channel number
    uint16_t channel = (uint16_t)mxGetScalar(prhs[1]);

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        char cmdstr[128];
        if (param == PARAM_NONE)
        {
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, errstr);
            }
            if (_strcmpi(cmdstr, "pulses") == 0)
            {
                param = PARAM_PULSES;
            }
            else if (_strcmpi(cmdstr, "sequence") == 0)
            {
                param = PARAM_SEQUENCE;
            }
            else if (_strcmpi(cmdstr, "sinusoid") == 0)
            {
                param = PARAM_SINUSOID;
            }
            else if (_strcmpi(cmdstr, "offset") == 0)
            {
                param = PARAM_OFFSET;
            }
            else if (_strcmpi(cmdstr, "repeats") == 0)
            {
                param = PARAM_REPEATS;
            }
            else if (_strcmpi(cmdstr, "trigger") == 0)
            {
                param = PARAM_TRIGGER;
            }
            else if (_strcmpi(cmdstr, "value") == 0)
            {
                param = PARAM_VALUE;
            }
            else if (_strcmpi(cmdstr, "index") == 0)
            {
                param = PARAM_INDEX;
            }
            else if (_strcmpi(cmdstr, "input") == 0)
            {
                param = PARAM_INPUT;
            }
            else if (_strcmpi(cmdstr, "monitor") == 0)
            {
                param = PARAM_MONITOR;
            }
            else if (_strcmpi(cmdstr, "track") == 0)
            {
                mon.bTrack = TRUE;
                if (nIdxMonitor == 0)
                    mexErrMsgTxt("Cannot track with no monitor channel specified");
            }
            else if (_strcmpi(cmdstr, "disable") == 0)
            {
                bDisable = true;
                if (i != 2)
                    mexErrMsgTxt("Cannot specify any other parameters with 'disable' command");
            }
            else if (_strcmpi(cmdstr, "mv") == 0)
            {
                bUnitMv = true;
            }
            else if (_strcmpi(cmdstr, "ms") == 0)
            {
                bUnitMs = true;
            }
            else if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                mexErrMsgTxt("Invalid parameter");
            }
        }
        else
        {
            // parameters are checked here
            switch (param)
            {
            case PARAM_PULSES:
                if (nIdxWave)
                    mexErrMsgTxt("Cannot specify multiple waveform commands");
                if (nIdxMonitor)
                    mexErrMsgTxt("Cannot specify monitor and waveform commands together");
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid pulses waveform");
                bPulses = true;
                wf.type = cbSdkWaveform_PARAMETERS;
                nIdxWave = i;
                break;
            case PARAM_SEQUENCE:
                if (nIdxWave)
                    mexErrMsgTxt("Cannot specify multiple waveform commands");
                if (nIdxMonitor)
                    mexErrMsgTxt("Cannot specify monitor and waveform commands together");
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid sequence waveform");
                wf.type = cbSdkWaveform_PARAMETERS;
                nIdxWave = i;
                break;
            case PARAM_MONITOR:
                if (nIdxMonitor)
                    mexErrMsgTxt("Cannot specify multiple monitor commands");
                if (nIdxWave)
                    mexErrMsgTxt("Cannot specify monitor and waveform commands together");
                nIdxMonitor = i;
                break;
            case PARAM_SINUSOID:
                if (nIdxWave)
                    mexErrMsgTxt("Cannot specify multiple waveform commands");
                if (nIdxMonitor)
                    mexErrMsgTxt("Cannot specify monitor and waveform commands together");
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid sinusoid waveform");
                wf.type = cbSdkWaveform_SINE;
                nIdxWave = i;
                break;
            case PARAM_OFFSET:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid offset number");
                dOffset = *mxGetPr(prhs[i]);
                break;
            case PARAM_REPEATS:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid repeats number");
                wf.repeats = (uint32_t)(*mxGetPr(prhs[i]));
                break;
            case PARAM_VALUE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid value number");
                wf.trigValue = (uint16_t)(*mxGetPr(prhs[i]));
                break;
            case PARAM_INDEX:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid index number");
                wf.trigNum = (uint8_t)(*mxGetPr(prhs[i]));
                break;
            case PARAM_INPUT:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid input number");
                wf.trigChan = (uint16_t)(*mxGetPr(prhs[i]));
                mon.chan = wf.trigChan;
                break;
            case PARAM_TRIGGER:
                if (nIdxTrigger)
                    mexErrMsgTxt("Cannot specify multiple waveform triggers");
                if (nIdxMonitor)
                    mexErrMsgTxt("Cannot specify monitor and waveform commands together");
                nIdxTrigger = i;
                break;
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Last parameter requires value");
    }

    if (nIdxWave == 0 && nIdxMonitor == 0 && !bDisable)
    {
        PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true,
            "No action specified\n"
            "specify waveform, monitoring or disable");
    }

    if (nIdxWave)
    {
        if (wf.type == cbSdkWaveform_PARAMETERS)
        {
            double dDuration[cbMAX_WAVEFORM_PHASES];
            double dAmplitude[cbMAX_WAVEFORM_PHASES];
            if (bPulses)
            {
                // check for proper data structure
                if (mxGetClassID(prhs[nIdxWave]) != mxDOUBLE_CLASS || mxGetNumberOfElements(prhs[nIdxWave]) != 6)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid pulses waveform");

                double *pcfgvals = mxGetPr(prhs[nIdxWave]);
                double dPhase1Duration  = *(pcfgvals+0);
                double dPhase1Amplitude = *(pcfgvals+1);
                double dInterPhaseDelay = *(pcfgvals+2);
                double dPhase2Duration  = *(pcfgvals+3);
                double dPhase2Amplitude = *(pcfgvals+4);
                double dInterPulseDelay = *(pcfgvals+5);
                wf.phases = 4;
                dDuration[0] = dPhase1Duration;
                dDuration[1] = dInterPhaseDelay;
                dDuration[2] = dPhase2Duration;
                dDuration[3] = dInterPulseDelay;
                dAmplitude[0] = dPhase1Amplitude;
                dAmplitude[1] = 0;
                dAmplitude[2] = dPhase2Amplitude;
                dAmplitude[3] = 0;
            }
            else
            {
                uint32_t count = (uint32_t)mxGetNumberOfElements(prhs[nIdxWave]);
                // check for proper data structure
                if (mxGetClassID(prhs[nIdxWave]) != mxDOUBLE_CLASS || count < 2 || (count & 0x01))
                {
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid sequence waveform");
                }
                if (count > (2 * cbMAX_WAVEFORM_PHASES))
                {
                    char cmdstr[256];
                    sprintf(cmdstr, "Maximum of %u phases can be specified for each sequence", (uint32_t)cbMAX_WAVEFORM_PHASES);
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, cmdstr);
                }
                /// \todo use sequence to pump larger number of phases in NSP1.5
                wf.phases = count / 2;
                double *pcfgvals = mxGetPr(prhs[nIdxWave]);
                for (uint16_t i = 0; i < wf.phases; ++i)
                {
                    dDuration[i] = *(pcfgvals + i * 2 + 0);
                    dAmplitude[i] = *(pcfgvals + i * 2 + 1);
                }
            }

            if (bUnitMv)
            {
                // FIXME: add to SDK
                cbSCALING isScaleOut;
                ::cbGetAoutCaps(channel, NULL, &isScaleOut, NULL, nInstance);

                int nAnaAmplitude = isScaleOut.anamax;
                int nDigiAmplitude = isScaleOut.digmax;
                for (uint16_t i = 0; i < wf.phases; ++i)
                {
                    dAmplitude[i] = (dAmplitude[i] * nDigiAmplitude) / nAnaAmplitude;
                    if (dAmplitude[i] > MAX_int16_T)
                        dAmplitude[i] = MAX_int16_T;
                    else if (dAmplitude[i] < MIN_int16_T)
                        dAmplitude[i] = MIN_int16_T;
                }
                dOffset = (dOffset * nDigiAmplitude) / nAnaAmplitude;
            }
            for (uint16_t i = 0; i < wf.phases; ++i)
                wf.amplitude[i] = (int16_t)dAmplitude[i];

            if (bUnitMs)
            {
                for (uint16_t i = 0; i < wf.phases; ++i)
                {
                    dDuration[i] *= 30;
                    if (dDuration[i] > MAX_uint16_T)
                        dDuration[i] = MAX_uint16_T;
                }
            }
            for (uint16_t i = 0; i < wf.phases; ++i)
            {
                // Zero is, but let's not allow it with high level library, because it wastes NSP CPU cycles
                if (dDuration[i] <= 0)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Negative or zero duration is not valid");
                wf.duration[i] = (uint16_t)dDuration[i];
            }
            wf.offset = (int16_t)dOffset;
        }
        else if (wf.type == cbSdkWaveform_SINE)
        {
            if (mxGetClassID(prhs[nIdxWave]) != mxDOUBLE_CLASS || mxGetNumberOfElements(prhs[nIdxWave]) != 2)
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid sinusoid waveform");

            double *pcfgvals = mxGetPr(prhs[nIdxWave]);
            wf.sineFrequency  = (int32_t)(*(pcfgvals + 0));
            double dAmplitude = *(pcfgvals + 1);
            if (bUnitMv)
            {
                // FIXME: add to SDK
                cbSCALING isScaleOut;
                ::cbGetAoutCaps(channel, NULL, &isScaleOut, NULL, nInstance);

                int nAnaAmplitude = isScaleOut.anamax;
                int nDigiAmplitude = isScaleOut.digmax;

                dAmplitude = (dAmplitude * nDigiAmplitude) / nAnaAmplitude;
                dOffset = (dOffset * nDigiAmplitude) / nAnaAmplitude;
            }
            wf.sineAmplitude = (int16_t)dAmplitude;
            wf.offset = (int16_t)dOffset;
        }

        // If any trigger is specified, parse it
        if (nIdxTrigger)
        {
            char cmdstr[128];
            // check for proper data structure
            if (mxGetClassID(prhs[nIdxTrigger]) != mxCHAR_CLASS || mxGetString(prhs[nIdxTrigger], cmdstr, 10))
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid trigger");

            if (_strcmpi(cmdstr, "instant") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_NONE;
            }
            else if (_strcmpi(cmdstr, "dinrise") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_DINPREG;
                if (wf.trigChan == 0 || wf.trigChan > 16)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Only bits 1-16 (first digital input) trigger is supported");
            }
            else if (_strcmpi(cmdstr, "dinfall") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_DINPFEG;
                if (wf.trigChan == 0 || wf.trigChan > 16)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Only bits 1-16 (first digital input) trigger is supported");
            }
            else if (_strcmpi(cmdstr, "spike") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_SPIKEUNIT;
                if (wf.trigChan == 0 || wf.trigChan > cbMAXCHANS)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid input channel number");
            }
            else if (_strcmpi(cmdstr, "cmtcolor") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_COMMENTCOLOR;
            }
            else if (_strcmpi(cmdstr, "softreset") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_SOFTRESET;
            }
            else if (_strcmpi(cmdstr, "extension") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_EXTENSION;
            }
            else if (_strcmpi(cmdstr, "off") == 0)
            {
                wf.trig = cbSdkWaveformTrigger_NONE;
                if (wf.type != cbSdkWaveform_NONE)
                    PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Trigger off is incompatible with specifying a waveform");
            }
            else
            {
                PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid trigger");
            }
        }
    }

    if (nIdxMonitor)
    {
        char cmdstr[128];
        // check for proper data structure
        if (mxGetClassID(prhs[nIdxMonitor]) != mxCHAR_CLASS || mxGetString(prhs[nIdxMonitor], cmdstr, 10))
            PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid monitor");
        if (_strcmpi(cmdstr, "spike") == 0)
        {
            mon.bSpike = TRUE;
        }
        else if (_strcmpi(cmdstr, "continuous") == 0)
        {
            mon.bSpike = FALSE;
        }
        else
        {
            PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid monitor");
        }
        if (mon.chan == 0 || mon.chan > cbMAXCHANS)
            PrintHelp(CBMEX_FUNCTION_ANALOGOUT, true, "Invalid input channel number");
    }

    cbSdkResult res = cbSdkSetAnalogOutput(nInstance, channel, nIdxWave ? &wf : NULL, nIdxMonitor ? &mon : NULL);
    PrintErrorSDK(res, "cbSdkSetAnalogOutput()");
}

// Author & Date:   Ehsan Azar       11 March 2011
/**
* Set a channel mask.
*
* \n In MATLAB use =>
* \n      cbmex('mask', channel, [bActive])
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnMask(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 2;
    uint32_t bActive = 1;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_MASK, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_MASK, true, "Too many outputs requested");

    if (!mxIsNumeric(prhs[1]) || mxGetNumberOfElements(prhs[1]) != 1)
        PrintHelp(CBMEX_FUNCTION_MASK, true, "Invalid channel parameter");

    uint16_t nChannel = (uint16_t) mxGetScalar(prhs[1]);

    if (nFirstParam < nrhs)
    {
        if (mxIsNumeric(prhs[nFirstParam]))
        {
            // check for proper data structure
            if (mxGetNumberOfElements(prhs[nFirstParam]) != 1)
                PrintHelp(CBMEX_FUNCTION_MASK, true, "Invalid active parameter");

            bActive = (uint32_t)mxGetScalar(prhs[nFirstParam]);

            nFirstParam++; // skip the optional
        }
    }

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_MASK, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_MASK, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_MASK, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_MASK, true, "Last parameter requires value");
    }

    cbSdkResult res = cbSdkSetChannelMask(nInstance, nChannel, bActive);
    PrintErrorSDK(res, "cbSdkSetChannelMask()");
}

// Author & Date:   Ehsan Azar       25 Feb 2011
/**
* Send a comment or custom event.
*
* \n In MATLAB use =>
* \n     cbmex('comment', rgba, charset, comment)       to send a comment
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnComment(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 4;

    if (nrhs < 4)
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Too many outputs requested");

    if (!mxIsNumeric(prhs[1]) || mxGetNumberOfElements(prhs[1]) != 1)
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Invalid rgba parameter");

    if (!mxIsNumeric(prhs[2]) || mxGetNumberOfElements(prhs[2]) != 1)
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Invalid charset parameter");

    uint32_t rgba = (uint32_t) mxGetScalar(prhs[1]);
    uint8_t charset = (uint8_t) mxGetScalar(prhs[2]);

    char  cmt[cbMAX_COMMENT] = {0};
    // fill in the comment string
    if (mxGetString(prhs[3], cmt, cbMAX_COMMENT))
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Invalid comment or comment is too long");

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_COMMENT, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_COMMENT, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_COMMENT, true, "Last parameter requires value");
    }

    cbSdkResult res = cbSdkSetComment(nInstance, rgba, charset, cmt);
    PrintErrorSDK(res, "cbSdkSetComment()");
}

// Author & Date:   Ehsan Azar       25 Feb 2011
/**
* Send a channel configuration, and return values.
*
* \n In MATLAB use =>
* \n      [config_cell_aray] = cbmex('config', channel, [<parameter>, value], ...)      to set one or more parameters
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnConfig(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 2;
    cbSdkResult res;
    bool bHasNewParams = false;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Too few inputs provided");
    if (nlhs > 1)
        PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Too many outputs requested");

    if (!mxIsNumeric(prhs[1]) || mxGetNumberOfElements(prhs[1]) != 1)
        PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid channel parameter");

    uint16_t channel = (uint16_t)mxGetScalar(prhs[1]);

    enum
    {
        PARAM_NONE,
        PARAM_USERFLAGS,
        PARAM_SMPFILTER,
        PARAM_SMPGROUP,
        PARAM_SPKFILTER,
        PARAM_SPKGROUP,
        PARAM_SPKTHRLEVEL,
        PARAM_AMPLREJPOS,
        PARAM_AMPLREJNEG,
        PARAM_REFELECCHAN,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Do a quick look at the options just to find the instance if specified
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_CONFIG, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                param = PARAM_USERFLAGS; // Just something valid but instance
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Last parameter requires value");
    }

    // Get old config, so that we would only change the bits requested
    cbPKT_CHANINFO chaninfo;
    res = cbSdkGetChannelConfig(nInstance, channel, &chaninfo);
    PrintErrorSDK(res, "cbSdkGetChannelConfig()");

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_CONFIG, true, errstr);
            }
            if (_strcmpi(cmdstr, "userflags") == 0)
            {
                param = PARAM_USERFLAGS;
            }
            else if (_strcmpi(cmdstr, "smpfilter") == 0)
            {
                param = PARAM_SMPFILTER;
            }
            else if (_strcmpi(cmdstr, "smpgroup") == 0)
            {
                param = PARAM_SMPGROUP;
            }
            else if (_strcmpi(cmdstr, "spkfilter") == 0)
            {
                param = PARAM_SPKFILTER;
            }
            else if (_strcmpi(cmdstr, "spkthrlevel") == 0)
            {
                param = PARAM_SPKTHRLEVEL;
            }
            else if (_strcmpi(cmdstr, "spkgroup") == 0)
            {
                param = PARAM_SPKGROUP;
            }
            else if (_strcmpi(cmdstr, "amplrejpos") == 0)
            {
                param = PARAM_AMPLREJPOS;
            }
            else if (_strcmpi(cmdstr, "amplrejneg") == 0)
            {
                param = PARAM_AMPLREJNEG;
            }
            else if (_strcmpi(cmdstr, "refelecchan") == 0)
            {
                param = PARAM_REFELECCHAN;
            }
            else if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_CONFIG, true, errstr);
            }
        }
        else
        {
            char cmdstr[128];
            switch(param)
            {
            case PARAM_USERFLAGS:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid userflags number");
                chaninfo.userflags = (uint32_t)mxGetScalar(prhs[i]);
                bHasNewParams = true;
                break;
            case PARAM_SMPFILTER:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid smpfilter number");
                chaninfo.smpfilter = (uint32_t)mxGetScalar(prhs[i]);
                if (chaninfo.smpfilter >= (cbFIRST_DIGITAL_FILTER + cbNUM_DIGITAL_FILTERS))
                    mexErrMsgTxt("Invalid continuous filter number");
                bHasNewParams = true;
                break;
            case PARAM_SMPGROUP:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid smpgroup number");
                chaninfo.smpgroup = (uint32_t)mxGetScalar(prhs[i]);
                if (chaninfo.smpgroup >= cbMAXGROUPS)
                    mexErrMsgTxt("Invalid sampling group number");
                bHasNewParams = true;
                break;
            case PARAM_SPKFILTER:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid spkfilter number");
                chaninfo.spkfilter = (uint32_t)mxGetScalar(prhs[i]);
                if (chaninfo.spkfilter >= (cbFIRST_DIGITAL_FILTER + cbNUM_DIGITAL_FILTERS))
                    mexErrMsgTxt("Invalid spike filter number");
                bHasNewParams = true;
                break;
            case PARAM_SPKTHRLEVEL:
                if (!mxGetString(prhs[i], cmdstr, 16))
                {
                    int32_t nValue = 0;
                    res = cbSdkAnalogToDigital(nInstance, channel, cmdstr, &nValue);
                    PrintErrorSDK(res, "cbSdkAnalogToDigital()");
                    chaninfo.spkthrlevel = nValue;
                }
                else if (mxIsNumeric(prhs[i]))
                {
                    chaninfo.spkthrlevel = (uint32_t)mxGetScalar(prhs[i]);
                }
                else
                {
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid spkthrlevel value");
                }
                bHasNewParams = true;
                break;
            case PARAM_SPKGROUP:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid spkgroup number");
                chaninfo.spkgroup = (uint32_t)mxGetScalar(prhs[i]);
                bHasNewParams = true;
                break;
            case PARAM_AMPLREJPOS:
                if (!mxGetString(prhs[i], cmdstr, 16))
                {
                    int32_t nValue = 0;
                    res = cbSdkAnalogToDigital(nInstance, channel, cmdstr, &nValue);
                    PrintErrorSDK(res, "cbSdkAnalogToDigital()");
                    chaninfo.amplrejpos = nValue;
                }
                else if (mxIsNumeric(prhs[i]))
                {
                    chaninfo.amplrejpos = (uint32_t)mxGetScalar(prhs[i]);
                }
                else
                {
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid amprejpos number");
                }
                bHasNewParams = true;
                break;
            case PARAM_AMPLREJNEG:
                if (!mxGetString(prhs[i], cmdstr, 16))
                {
                    int32_t nValue = 0;
                    res = cbSdkAnalogToDigital(nInstance, channel, cmdstr, &nValue);
                    PrintErrorSDK(res, "cbSdkAnalogToDigital()");
                    chaninfo.amplrejneg = nValue;
                }
                else if (mxIsNumeric(prhs[i]))
                {
                    chaninfo.amplrejneg = (uint32_t)mxGetScalar(prhs[i]);
                }
                else
                {
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid amprejneg number");
                }
                bHasNewParams = true;
                break;
            case PARAM_REFELECCHAN:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Invalid refelecchan number");
                chaninfo.refelecchan = (uint32_t)mxGetScalar(prhs[i]);
                bHasNewParams = true;
                break;
            case PARAM_INSTANCE:
                // Done before
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_CONFIG, true, "Last parameter requires value");
    }


    /// \todo use map for parameter names
    /// \todo add more parameters

    // if output specifically requested, or if no new configuration specified
    // build the cell structure to get previous values
    if (nlhs > 0 || ! bHasNewParams)
    {
        int count = 14; // number of parameters
        mxArray *pca = mxCreateCellMatrix(count, 2);
        plhs[0] = pca;
        mxSetCell(pca, 0, mxCreateString("userflags"));
        mxSetCell(pca, count + 0, mxCreateDoubleScalar(chaninfo.userflags));

        mxSetCell(pca, 1, mxCreateString("smpfilter"));
        mxSetCell(pca, count + 1, mxCreateDoubleScalar(chaninfo.smpfilter));

        mxSetCell(pca, 2, mxCreateString("smpgroup"));
        mxSetCell(pca, count + 2, mxCreateDoubleScalar(chaninfo.smpgroup));

        mxSetCell(pca, 3, mxCreateString("spkfilter"));
        mxSetCell(pca, count + 3, mxCreateDoubleScalar(chaninfo.spkfilter));

        mxSetCell(pca, 4, mxCreateString("spkgroup"));
        mxSetCell(pca, count + 4, mxCreateDoubleScalar(chaninfo.spkgroup));

        mxSetCell(pca, 5, mxCreateString("spkthrlevel"));
        mxSetCell(pca, count + 5, mxCreateDoubleScalar(chaninfo.spkthrlevel));

        mxSetCell(pca, 6, mxCreateString("amplrejpos"));
        mxSetCell(pca, count + 6, mxCreateDoubleScalar(chaninfo.amplrejpos));

        mxSetCell(pca, 7, mxCreateString("amplrejneg"));
        mxSetCell(pca, count + 7, mxCreateDoubleScalar(chaninfo.amplrejneg));

        mxSetCell(pca, 8, mxCreateString("refelecchan"));
        mxSetCell(pca, count + 8, mxCreateDoubleScalar(chaninfo.refelecchan));

        mxSetCell(pca, 9, mxCreateString("analog_unit"));
        mxSetCell(pca, count + 9, mxCreateString(chaninfo.physcalin.anaunit));

        mxSetCell(pca, 10, mxCreateString("max_analog"));
        mxSetCell(pca, count + 10, mxCreateDoubleScalar(chaninfo.physcalin.anamax));

        mxSetCell(pca, 11, mxCreateString("max_digital"));
        mxSetCell(pca, count + 11, mxCreateDoubleScalar(chaninfo.physcalin.digmax));

        mxSetCell(pca, 12, mxCreateString("min_analog"));
        mxSetCell(pca, count + 12, mxCreateDoubleScalar(chaninfo.physcalin.anamin));

        mxSetCell(pca, 13, mxCreateString("min_digital"));
        mxSetCell(pca, count + 13, mxCreateDoubleScalar(chaninfo.physcalin.digmin));
    }

    // if new configuration to send
    if (bHasNewParams)
    {
        res = cbSdkSetChannelConfig(nInstance, channel, &chaninfo);
        PrintErrorSDK(res, "cbSdkSetChannelConfig()");
    }
}

// Author & Date:   Ehsan Azar       13 April 2012
/**
* Read or convert CCF - Cerebus Configuration File.
*
* \n In MATLAB use =>
* \n      cbmex('ccf', filename, [<parameter>, value], ...)
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnCCF(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 1;
    cbSdkResult res = CBSDKRESULT_SUCCESS;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_CCF, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_CCF, true, "Too many outputs requested");

    char source_file[256] = {0};
    char destination_file[256] = {0};
    cbSdkCCF ccf;

    enum
    {
        PARAM_NONE,
        PARAM_SEND,
        PARAM_CONVERT,
        PARAM_LOAD,
        PARAM_SAVE,
        PARAM_INSTANCE,
    } param = PARAM_NONE, command = PARAM_NONE;

    bool bThreaded = false;
    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_CCF, true, errstr);
            }
            if (_strcmpi(cmdstr, "send") == 0)
            {
                param = PARAM_SEND;
            }
            else if (_strcmpi(cmdstr, "threaded") == 0)
            {
                bThreaded = true;
            }
            else if (_strcmpi(cmdstr, "load") == 0)
            {
                param = PARAM_LOAD;
            }
            else if (_strcmpi(cmdstr, "convert") == 0)
            {
                param = PARAM_CONVERT;
            }
            else if (_strcmpi(cmdstr, "save") == 0)
            {
                param = PARAM_SAVE;
            }
            else if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_CCF, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_SEND:
                if (source_file[0] != 0 || command != PARAM_NONE)
                    mexErrMsgTxt("Cannot specify multiple commands");
                if (mxGetString(prhs[i], source_file, sizeof(source_file)))
                    mexErrMsgTxt("Invalid source CCF filename");
                command = PARAM_SEND;
                break;
            case PARAM_LOAD:
                if (source_file[0] != 0)
                    mexErrMsgTxt("Cannot specify multiple commands");
                if (mxGetString(prhs[i], source_file, sizeof(source_file)))
                    mexErrMsgTxt("Invalid source CCF filename");
                break;
            case PARAM_CONVERT:
                if (destination_file[0] != 0 || command != PARAM_NONE)
                    mexErrMsgTxt("Cannot specify multiple commands");
                if (mxGetString(prhs[i], destination_file, sizeof(destination_file)))
                    mexErrMsgTxt("Invalid destination CCF filename");
                command = PARAM_CONVERT;
                break;
            case PARAM_SAVE:
                if (destination_file[0] != 0 || command != PARAM_NONE)
                    mexErrMsgTxt("Cannot specify multiple commands");
                if (mxGetString(prhs[i], destination_file, sizeof(destination_file)))
                    mexErrMsgTxt("Invalid destination CCF filename");
                command = PARAM_SAVE;
                break;
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_CCF, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_CCF, true, "Last parameter requires value");
    }

    switch (command)
    {
    case PARAM_SEND:
        res = cbSdkReadCCF(nInstance, &ccf, NULL, true, true, bThreaded);
        break;
    case PARAM_SAVE:
        res = cbSdkReadCCF(nInstance, &ccf, NULL, true, false, false);
        break;
    case PARAM_CONVERT:
        if (source_file[0] == 0)
            PrintHelp(CBMEX_FUNCTION_CCF, true, "source file not specified for 'convert' command");
        res = cbSdkReadCCF(nInstance, &ccf, source_file, true, false, false);
        break;
    default:
        // Never should happen
        break;
    }
    // Check if reading was successful
    PrintErrorSDK(res, "cbSdkReadCCF()");

    if (command == PARAM_SAVE || command == PARAM_CONVERT)
    {
        res = cbSdkWriteCCF(nInstance, &ccf, destination_file, bThreaded);
        PrintErrorSDK(res, "cbSdkWriteCCF()");
    }
}
// Author & Date:   Ehsan Azar       11 May 2012
/**
* Send a system runlevel command.
*
* \n In MATLAB use =>
* \n      cbmex('system', <command>)      to send a runelvel command
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnSystem(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    int nFirstParam = 2;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Too many outputs requested");

    char cmdstr[128];

    // check the input argument count
    if (mxGetString(prhs[1], cmdstr, 16))
        PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Invalid system command");

    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = nFirstParam; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_SYSTEM, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_SYSTEM, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Last parameter requires value");
    }

    cbSdkSystemType cmd = cbSdkSystem_RESET;
    if (_strcmpi(cmdstr, "reset") == 0)
        cmd = cbSdkSystem_RESET;
    else if (_strcmpi(cmdstr, "shutdown") == 0)
        cmd = cbSdkSystem_SHUTDOWN;
    else if (_strcmpi(cmdstr, "standby") == 0)
        cmd = cbSdkSystem_STANDBY;
    else
        PrintHelp(CBMEX_FUNCTION_SYSTEM, true, "Invalid system command");

    cbSdkResult res = cbSdkSystem(nInstance, cmd);
    PrintErrorSDK(res, "cbSdkSystem()");
}

// Author & Date:   Ehsan Azar       23 April 2013
/**
* Send a synch output command.
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnSynchOut(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    uint32_t nFreq = 0;
    uint32_t nRepeats = 0;
    uint32_t nChannel = 1;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Too many outputs requested");


    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
        PARAM_FREQ,
        PARAM_REPEATS,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = 1; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "freq") == 0)
            {
                param = PARAM_FREQ;
            }
            else if (_strcmpi(cmdstr, "repeats") == 0)
            {
                param = PARAM_REPEATS;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, errstr);
            }
        }
        else
        {
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_FREQ:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Invalid frequency value");
                nFreq = (uint32_t)(mxGetScalar(prhs[i]) * 1000);
                break;
            case PARAM_REPEATS:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Invalid repeats value");
                nRepeats = (uint32_t)mxGetScalar(prhs[i]);
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam
    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Last parameter requires value");
    }

    cbSdkResult res = cbSdkSetSynchOutput(nInstance, nChannel, nFreq, nRepeats);
    if (res == CBSDKRESULT_NOTIMPLEMENTED)
        PrintErrorSDK(res, "cbSdkSynchOut(): Only CerePlex currently supports this command");
    PrintErrorSDK(res, "cbSdkSynchOut()");
}

// Author & Date:   Ehsan Azar       14 May 2013
/**
* Send an extension command.
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/
void OnExtCmd(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
    uint32_t nInstance = 0;
    cbSdkResult res = CBSDKRESULT_SUCCESS;
    cbSdkExtCmd extCmd;

    if (nrhs < 2)
        PrintHelp(CBMEX_FUNCTION_EXT, true, "Too few inputs provided");
    if (nlhs > 0)
        PrintHelp(CBMEX_FUNCTION_EXT, true, "Too many outputs requested");


    enum
    {
        PARAM_NONE,
        PARAM_INSTANCE,
        PARAM_CMD,
        PARAM_UPLOAD,
        PARAM_INPUT,
    } param = PARAM_NONE;

    // Process remaining input arguments if available
    for (int i = 1; i < nrhs; ++i)
    {
        if (param == PARAM_NONE)
        {
            char cmdstr[128];
            if (mxGetString(prhs[i], cmdstr, 16))
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d is invalid", i);
                PrintHelp(CBMEX_FUNCTION_EXT, true, errstr);
            }
            if (_strcmpi(cmdstr, "instance") == 0)
            {
                param = PARAM_INSTANCE;
            }
            else if (_strcmpi(cmdstr, "upload") == 0)
            {
                param = PARAM_UPLOAD;
            }
            else if (_strcmpi(cmdstr, "command") == 0)
            {
                param = PARAM_CMD;
            }
            else if (_strcmpi(cmdstr, "input") == 0)
            {
                param = PARAM_INPUT;
            }
            else if (_strcmpi(cmdstr, "terminate") == 0)
            {
                // Send a kill request
                extCmd.cmd = cbSdkExtCmd_TERMINATE;
                res = cbSdkExtDoCommand(nInstance, &extCmd);
                // On error just get out of the for-loop
                if (res != CBSDKRESULT_SUCCESS)
                    break;
            }
            else
            {
                char errstr[128];
                sprintf(errstr, "Parameter %d (%s) is invalid", i, cmdstr);
                PrintHelp(CBMEX_FUNCTION_EXT, true, errstr);
            }
        }
        else
        {
            char cmdstr[cbMAX_LOG] = {'\0'};
            switch(param)
            {
            case PARAM_INSTANCE:
                if (!mxIsNumeric(prhs[i]))
                    PrintHelp(CBMEX_FUNCTION_SYNCHOUT, true, "Invalid instance number");
                nInstance = (uint32_t)mxGetScalar(prhs[i]);
                break;
            case PARAM_UPLOAD:
                if (mxGetString(prhs[i], cmdstr, cbMAX_LOG - 1))
                    PrintHelp(CBMEX_FUNCTION_EXT, true, "Invalid filename");
                extCmd.cmd = cbSdkExtCmd_UPLOAD;
                strncpy(extCmd.szCmd, cmdstr, sizeof(extCmd.szCmd));
                res = cbSdkExtDoCommand(nInstance, &extCmd);
                // On error just get out of the for-loop without goto
                if (res != CBSDKRESULT_SUCCESS)
                    i = nrhs;
                break;
            case PARAM_CMD:
                if (mxGetString(prhs[i], cmdstr, cbMAX_LOG - 1))
                    PrintHelp(CBMEX_FUNCTION_EXT, true, "Invalid command string");
                extCmd.cmd = cbSdkExtCmd_RPC;
                strncpy(extCmd.szCmd, cmdstr, sizeof(extCmd.szCmd));
                res = cbSdkExtDoCommand(nInstance, &extCmd);
                // On error just get out of the for-loop without goto
                if (res != CBSDKRESULT_SUCCESS)
                    i = nrhs;
                break;
            case PARAM_INPUT:
                if (mxGetString(prhs[i], cmdstr, cbMAX_LOG - 1))
                    PrintHelp(CBMEX_FUNCTION_EXT, true, "Invalid input string");
                extCmd.cmd = cbSdkExtCmd_INPUT;
                strncpy(extCmd.szCmd, cmdstr, sizeof(extCmd.szCmd));
                res = cbSdkExtDoCommand(nInstance, &extCmd);
                // On error just get out of the for-loop without goto
                if (res != CBSDKRESULT_SUCCESS)
                    i = nrhs;
                break;
            default:
                break;
            }
            param = PARAM_NONE;
        }
    } // end for (int i = nFirstParam

    if (param != PARAM_NONE)
    {
        // Some parameter did not have a value, and value is non-optional
        PrintHelp(CBMEX_FUNCTION_EXT, true, "Last parameter requires value");
    }

    if (res == CBSDKRESULT_NOTIMPLEMENTED)
        PrintErrorSDK(res, "cbSdkExtCmd(): NSP1 does not support this");
    PrintErrorSDK(res, "cbSdkExtCmd()");
}

#ifdef WIN32
#define MEX_EXPORT
#else
#define MEX_EXPORT CBSDKAPI
#endif

/////////////////////////////////////////////////////////////////////////////
/**
* The one and only mexFunction.
*
* @param[in] nlhs	Number of left hand side (output) arguments
* @param[in] plhs	Array of left hand side arguments
* @param[in] nrhs	Number of right hand side (input) arguments
* @param[in] prhs	Array of right hand side arguments
*/

extern "C" MEX_EXPORT void mexFunction(
    int nlhs,
    mxArray *plhs[],
    int nrhs,
    const mxArray *prhs[] )
{
#ifdef DEBUG_CONSOLE
#ifdef WIN32
    AllocConsole();
#endif
#endif

    static NAME_LUT lut = CreateLut();        // The actual look up table

    g_bMexCall = true;

    // assign the atexit function to close the library and deallocate everything if
    //  the mex DLL is closed for any reason before the 'close' command is called
    mexAtExit(matexit);

    // test for minimum number of arguments
    if (nrhs < 1)
        PrintHelp(CBMEX_FUNCTION_COUNT, true);

    // get the command string and process the command
    char cmdstr[16];
    if (mxGetString(prhs[0], cmdstr, 16))
        PrintHelp(CBMEX_FUNCTION_COUNT, true);

    // Find and then call the correct function
    NAME_LUT::iterator it = lut.find(cmdstr);
    if (it == lut.end())
    {
        // Not found
        PrintHelp(CBMEX_FUNCTION_COUNT, true);
    }
    else
    {
        // The map triple is     "command name", "function pointer", "function index"
        //  so call the function pointer (2nd value of pair)
        (*it).second.first(nlhs, plhs, nrhs, prhs);
    }
}
