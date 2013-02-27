///////////////////////////////////////////////////////////////////////
//
// Cerebus python port of SDK
//
// (c) Copyright Blackrock Microsystems
// (c) Copyright Ehsan Azarnasab
//
// $Workfile: cbpy.cpp $
// $Archive: /Cerebus/Human/LinuxApps/cbmex/cbpy.cpp $
// $Revision: 1 $
// $Date: 04/29/12 11:06a $
// $Author: Ehsan $
//
// Notes:
//   Do type checking wherever/however possible (although not recommended),
//    Avoid implicit conversions, it is hard to find bugs related to relaxed types
//   Do not throw cpp exceptions, catch possible exceptions and handle them the earliest possible in this library
//   Only functions are exported, no data
//

#include "StdAfx.h"
#include "cbpy.h"
#define NPY_NO_DEPRECATED_API  NPY_1_7_API_VERSION
#include "numpy/arrayobject.h"
#include <vector>
#include <string>
#include <map>

static PyObject * g_cbpyError; // cbpy exception
static PyObject * g_callback[CBSDKCALLBACK_COUNT] = {NULL};
static PyGILState_STATE g_gilState; // Python global interpreter lock state

typedef enum {
    CHANLABEL_OUTPUTS_NONE          = 0,
    CHANLABEL_OUTPUTS_LABEL         = 1,
    CHANLABEL_OUTPUTS_UNIT_VALID    = 2,
    CHANLABEL_OUTPUTS_ENABLED       = 4,
} CHANLABEL_OUTPUTS;
typedef std::map<std::string, CHANLABEL_OUTPUTS> LUT_CHANLABEL_OUTPUTS;
LUT_CHANLABEL_OUTPUTS g_lutChanLabelOutputs;

typedef std::map<std::string, cbSdkSystemType> LUT_SYSTEM;
LUT_SYSTEM g_lutSystem;

typedef enum {
    FILECFG_CMD_INFO        = 0,
    FILECFG_CMD_OPEN        = 1,
    FILECFG_CMD_CLOSE       = 2,
    FILECFG_CMD_START       = 3,
    FILECFG_CMD_STOP        = 4,
} FILECFG_CMD;
typedef std::map<std::string, FILECFG_CMD> LUT_FILECFG_CMD;
LUT_FILECFG_CMD g_lutFileCfgCmd;

typedef enum {
    DIGOUT_CMD_SETVALUE   = 0,
} DIGOUT_CMD;
typedef std::map<std::string, DIGOUT_CMD> LUT_DIGOUT_CMD;
LUT_DIGOUT_CMD g_lutDigOutCmd;


typedef enum {
    MASK_CMD_OFF  = 0,
    MASK_CMD_ON   = 1,
} MASK_CMD;
typedef std::map<std::string, MASK_CMD> LUT_MASK_CMD;
LUT_MASK_CMD g_lutMaskCmd;

typedef enum {
    CCF_CMD_SAVE      = 0,
    CCF_CMD_SEND      = 1,
    CCF_CMD_CONVERT   = 2,
} CCF_CMD;
typedef std::map<std::string, CCF_CMD> LUT_CCF_CMD;
LUT_CCF_CMD g_lutCCFCmd;

typedef enum {
    CONFIG_CMD_NONE            = 0x0000,
    CONFIG_CMD_USERFLAGS       = 0x0001,
    CONFIG_CMD_SMPFILTER       = 0x0002,
    CONFIG_CMD_SMPGROUP        = 0x0004,
    CONFIG_CMD_SPKFILTER       = 0x0008,
    CONFIG_CMD_SPKGROUP        = 0x0010,
    CONFIG_CMD_SPKTHRLEVEL     = 0x0020,
    CONFIG_CMD_AMPLREJPOS      = 0x0040,
    CONFIG_CMD_AMPLREJNEG      = 0x0080,
    CONFIG_CMD_REFELECCHAN     = 0x0100,
    CONFIG_CMD_SACLE           = 0x0200,

    CONFIG_CMD_ALL             = 0xFFFF,
} CONFIG_CMD;
typedef std::map<std::string, CONFIG_CMD> LUT_CONFIG_INPUTS_CMD;
LUT_CONFIG_INPUTS_CMD g_lutConfigInputsCmd;
typedef std::map<std::string, CONFIG_CMD> LUT_CONFIG_OUTPUTS_CMD;
LUT_CONFIG_OUTPUTS_CMD g_lutConfigOutputsCmd;


typedef enum {
    CONNECTION_PARAM_INST_ADDR      = 0,
    CONNECTION_PARAM_INST_PORT      = 1,
    CONNECTION_PARAM_CLIENT_ADDR    = 2,
    CONNECTION_PARAM_CLIENT_PORT    = 3,
    CONNECTION_PARAM_RECBUFSIZE     = 4,
} CONNECTION_PARAM;
typedef std::map<std::string, CONNECTION_PARAM> LUT_CONNECTION_PARAM;
LUT_CONNECTION_PARAM g_lutConnectionParam;

// Author & Date: Ehsan Azar       4 July 2012
// Purpose: Create all lookup tables
//          Note: All parameter names start with lower letter and follow under_score notation
//                 no type qualifier prefix is used in the name
// Outputs:
//   Returns 0 on success, error code otherwise
static int CreateLUTs()
{
    // Create ChanLabel outputs LUT
    g_lutChanLabelOutputs["none"         ] = CHANLABEL_OUTPUTS_NONE;
    g_lutChanLabelOutputs["label"        ] = CHANLABEL_OUTPUTS_LABEL;
    g_lutChanLabelOutputs["enabled"      ] = CHANLABEL_OUTPUTS_ENABLED;
    g_lutChanLabelOutputs["valid_unit"   ] = CHANLABEL_OUTPUTS_UNIT_VALID;
    // Create System commands LUT
    g_lutSystem["reset"    ] = cbSdkSystem_RESET;
    g_lutSystem["shutdown" ] = cbSdkSystem_SHUTDOWN;
    g_lutSystem["standby"  ] = cbSdkSystem_STANDBY;
    // Create file config commands LUT
    g_lutFileCfgCmd["info"     ] = FILECFG_CMD_INFO;
    g_lutFileCfgCmd["open"     ] = FILECFG_CMD_OPEN;
    g_lutFileCfgCmd["close"    ] = FILECFG_CMD_CLOSE;
    g_lutFileCfgCmd["start"    ] = FILECFG_CMD_START;
    g_lutFileCfgCmd["stop"     ] = FILECFG_CMD_STOP;
    // Create digout command LUT
    g_lutDigOutCmd["set_value"     ] = DIGOUT_CMD_SETVALUE;
    // Create digout command LUT
    g_lutMaskCmd["off"     ] = MASK_CMD_OFF;
    g_lutMaskCmd["on"      ] = MASK_CMD_ON;
    // Create CCF command LUT
    g_lutCCFCmd["save"     ] = CCF_CMD_SAVE;
    g_lutCCFCmd["send"     ] = CCF_CMD_SEND;
    g_lutCCFCmd["convert"  ] = CCF_CMD_CONVERT;
    // Create Config command LUT
    g_lutConfigInputsCmd["userflags"     ] = CONFIG_CMD_USERFLAGS;
    g_lutConfigInputsCmd["smpfilter"     ] = CONFIG_CMD_SMPFILTER;
    g_lutConfigInputsCmd["smpgroup"      ] = CONFIG_CMD_SMPGROUP;
    g_lutConfigInputsCmd["spkfilter"     ] = CONFIG_CMD_SPKFILTER;
    g_lutConfigInputsCmd["spkgroup"      ] = CONFIG_CMD_SPKGROUP;
    g_lutConfigInputsCmd["spkthrlevel"   ] = CONFIG_CMD_SPKTHRLEVEL;
    g_lutConfigInputsCmd["amplrejpos"    ] = CONFIG_CMD_AMPLREJPOS;
    g_lutConfigInputsCmd["amplrejneg"    ] = CONFIG_CMD_AMPLREJNEG;
    g_lutConfigInputsCmd["refelecchan"   ] = CONFIG_CMD_REFELECCHAN;

    g_lutConfigOutputsCmd["userflags"     ] = CONFIG_CMD_USERFLAGS;
    g_lutConfigOutputsCmd["smpfilter"     ] = CONFIG_CMD_SMPFILTER;
    g_lutConfigOutputsCmd["smpgroup"      ] = CONFIG_CMD_SMPGROUP;
    g_lutConfigOutputsCmd["spkfilter"     ] = CONFIG_CMD_SPKFILTER;
    g_lutConfigOutputsCmd["spkgroup"      ] = CONFIG_CMD_SPKGROUP;
    g_lutConfigOutputsCmd["spkthrlevel"   ] = CONFIG_CMD_SPKTHRLEVEL;
    g_lutConfigOutputsCmd["amplrejpos"    ] = CONFIG_CMD_AMPLREJPOS;
    g_lutConfigOutputsCmd["amplrejneg"    ] = CONFIG_CMD_AMPLREJNEG;
    g_lutConfigOutputsCmd["refelecchan"   ] = CONFIG_CMD_REFELECCHAN;
    g_lutConfigOutputsCmd["scale"         ] = CONFIG_CMD_SACLE;
    // Create Connection parameter LUT
    g_lutConnectionParam["inst-addr"           ] = CONNECTION_PARAM_INST_ADDR;
    g_lutConnectionParam["inst-port"           ] = CONNECTION_PARAM_INST_PORT;
    g_lutConnectionParam["client-addr"         ] = CONNECTION_PARAM_CLIENT_ADDR;
    g_lutConnectionParam["client-port"         ] = CONNECTION_PARAM_CLIENT_PORT;
    g_lutConnectionParam["receive-buffer-size" ] = CONNECTION_PARAM_RECBUFSIZE;
    return 0;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Return library version
static PyObject * cbpy_Version(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nInstance = 0;
    cbSdkVersion ver;
    static char kw[][32] = {"instance"};
    static char * kwlist[] = {kw[0], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|i", kwlist, &nInstance))
        return NULL;
    cbSdkResult sdkres = cbSdkGetVersion(nInstance, &ver);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
        res = Py_BuildValue(
                "{s:I,s:I,s:I,s:I"
                ",s:I,s:I"
                ",s:I,s:I,s:I,s:I}",
                "major", ver.major, "minor", ver.minor, "release", ver.release, "beta", ver.beta,
                "protocol_major", ver.majorp, "protocol_minor", ver.minorp,
                "nsp_major", ver.nspmajor, "nsp_minor", ver.nspminor, "nsp_release", ver.nsprelease, "nsp_beta", ver.nspbeta);
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Register a Python callback function
static PyObject * cbpy_Register(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    if (!PyEval_ThreadsInitialized())
    {
        PyEval_InitThreads();
        PyEval_ReleaseLock();
    }
    cbSdkVersion ver;
    PyObject *pCallback = NULL;
    char * pSzType = NULL;
    int nInstance = 0;
    static char kw[][32] = {"type", "callback", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "sO|i:Register", kwlist, &pSzType, &pCallback, &nInstance))
        return NULL;
    if (!PyCallable_Check(pCallback))
        return PyErr_Format(PyExc_TypeError, "callback parameter should be callable");
    Py_XINCREF(pCallback);
    //Py_XDECREF(my_callback);
    //my_callback = pCallback;

    Py_INCREF(Py_None);
    res = Py_None;
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Unregister a Python callback function
static PyObject * cbpy_Unregister(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    Py_INCREF(Py_None);
    res = Py_None;
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: The only registered callback with SDK
static void sdk_callback(const cbSdkPktType type, const void* pEventData, void* pCallbackData)
{
    g_gilState = PyGILState_Ensure();
    int idx = type;
    if (type == cbSdkPkt_PACKETLOST)
    {
        // Only the first registered callback receives packet lost events
        for (int i = 0; i < cbSdkPkt_COUNT; ++i)
        {
            if (g_callback[i] != NULL)
                idx = i;
        }
    }
    if (g_callback[idx] != NULL)
        PyObject_CallFunctionObjArgs(g_callback[idx]);
    PyGILState_Release(g_gilState);
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Open library
static PyObject * cbpy_Open(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    char * pSzConnection = NULL;
    int nInstance = 0;
    PyObject * pConParam = NULL;

    static char kw[][32] = {"connection", "parameter", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|sO!i", kwlist,
                                     &pSzConnection, &PyDict_Type, &pConParam, &nInstance))
        return NULL;

    cbSdkConnectionType conType = CBSDKCONNECTION_DEFAULT;
    if (pSzConnection == NULL || _strcmpi(pSzConnection, "default") == 0)
        conType = CBSDKCONNECTION_DEFAULT;
    else if (_strcmpi(pSzConnection, "master") == 0)
        conType = CBSDKCONNECTION_UDP;
    else if (_strcmpi(pSzConnection, "slave") == 0)
        conType = CBSDKCONNECTION_CENTRAL;
    else
        return PyErr_Format(PyExc_ValueError, "Invalid connection (%s); should be: default, master, slave", pSzConnection);

    // Construct default connection parameter
    cbSdkConnection con;
    if (pConParam)
    {
        PyObject *pKey, *pValue;
        Py_ssize_t pos = 0;
        // all optional parameters
        while (PyDict_Next(pConParam, &pos, &pKey, &pValue))
        {
            if (PyUnicode_Check(pKey))
                return PyErr_Format(PyExc_TypeError, "Invalid connection parameter key; unicode not supported yet");
            const char * pszKey = PyString_AsString(pKey);
            if (pszKey == NULL)
                return PyErr_Format(PyExc_TypeError, "Invalid connection parameter key; should be string");
            LUT_CONNECTION_PARAM::iterator it = g_lutConnectionParam.find(pszKey);
            if (it == g_lutConnectionParam.end())
                return PyErr_Format(PyExc_ValueError, "Invalid connection parameter key (%s)", pszKey);
            CONNECTION_PARAM cmd = it->second;
            switch (cmd)
            {
            case CONNECTION_PARAM_INST_ADDR:
                if (PyUnicode_Check(pValue))
                    return PyErr_Format(PyExc_TypeError, "Invalid instrument IP address; unicode not supported yet");
                con.szInIP = PyString_AsString(pValue);
                if (con.szInIP == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid instrument IP address; should be string");
                break;
            case CONNECTION_PARAM_INST_PORT:
                if (PyInt_Check(pValue))
                    con.nInPort = PyInt_AsLong(pValue);
                else
                    return PyErr_Format(PyExc_TypeError, "Invalid instrument port number; should be integer");
                break;
            case CONNECTION_PARAM_CLIENT_ADDR:
                if (PyUnicode_Check(pValue))
                    return PyErr_Format(PyExc_TypeError, "Invalid client IP address; unicode not supported yet");
                con.szOutIP = PyString_AsString(pValue);
                if (con.szOutIP == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid client IP address; should be string");
                break;
            case CONNECTION_PARAM_CLIENT_PORT:
                if (PyInt_Check(pValue))
                    con.nOutPort = PyInt_AsLong(pValue);
                else
                    return PyErr_Format(PyExc_TypeError, "Invalid client port number; should be integer");
                break;
            case CONNECTION_PARAM_RECBUFSIZE:
                if (PyInt_Check(pValue))
                    con.nRecBufSize = PyInt_AsLong(pValue);
                else
                    return PyErr_Format(PyExc_TypeError, "Invalid receive buffer size number; should be integer");
                break;
            }
        }
    }
    cbSdkResult sdkres = cbSdkOpen(nInstance, conType, con);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    cbSdkInstrumentType instType;
    sdkres = cbSdkGetType(nInstance, &conType, &instType);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres, "error cbSdkGetType");
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    char strConnection[CBSDKCONNECTION_COUNT + 1][8] = {"Default", "Slave", "Master", "Closed", "Unknown"};
    char strInstrument[CBSDKINSTRUMENT_COUNT + 1][13] = {"NSP", "nPlay", "Local NSP", "Remote nPlay", "Unknown"};
    if (conType < 0 || conType > CBSDKCONNECTION_CLOSED)
        conType = CBSDKCONNECTION_COUNT;
    if (instType < 0 || instType > CBSDKINSTRUMENT_COUNT)
        instType = CBSDKINSTRUMENT_COUNT;
    res = Py_BuildValue(
            "{s:s,s:s}",
            "connection", strConnection[conType], "instrument", strInstrument[instType]);
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Close library
static PyObject * cbpy_Close(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nInstance = 0;
    static char kw[][32] = {"instance"};
    static char * kwlist[] = {kw[0], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|i", kwlist, &nInstance))
        return NULL;
    cbSdkResult sdkres = cbSdkClose(nInstance);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Get instrument time
static PyObject * cbpy_Time(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres = CBSDKRESULT_SUCCESS;
    char * pSzUnit = NULL;
    int nInstance = 0;
    PyObject * res = NULL;
    static char kw[][32] = {"unit", "instance"};
    static char * kwlist[] = {kw[0], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|si", kwlist, &pSzUnit, &nInstance))
        return NULL;
    int factor = 1;
    if (pSzUnit == NULL || _strcmpi(pSzUnit, "samples") == 0)
    {
        factor = 1;
    }
    else if (_strcmpi(pSzUnit, "seconds") == 0 || _strcmpi(pSzUnit, "s") == 0)
    {
        UINT32 sysfreq;
        sdkres = cbSdkGetSysConfig(nInstance, NULL, NULL, &sysfreq);
        if (sdkres != CBSDKRESULT_SUCCESS)
            cbPySetErrorFromSdkError(sdkres, "error cbSdkGetSysConfig");
        if (sdkres < CBSDKRESULT_SUCCESS)
            return res;
        factor = sysfreq;
    }
    else if (_strcmpi(pSzUnit, "milliseconds") == 0 || _strcmpi(pSzUnit, "ms") == 0)
    {
        UINT32 sysfreq;
        sdkres = cbSdkGetSysConfig(nInstance, NULL, NULL, &sysfreq);
        if (sdkres != CBSDKRESULT_SUCCESS)
            cbPySetErrorFromSdkError(sdkres, "error cbSdkGetSysConfig");
        if (sdkres < CBSDKRESULT_SUCCESS)
            return res;
        factor = sysfreq / 1000;
    } else {
        return PyErr_Format(PyExc_ValueError, "Invalid unit (%s); should be samples, seconds(s), milliseconds(ms)", pSzUnit);
    }
    UINT32 cbtime;
    sdkres = cbSdkGetTime(nInstance, &cbtime);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        if (factor == 1)
            res = Py_BuildValue("I", cbtime);
        else
            res = Py_BuildValue("f", float(cbtime) / factor);
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Get or set channel label
//           Optionally get channel validity and unit validity
static PyObject * cbpy_ChanLabel(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nInstance = 0;
    UINT16 nChannel = 0;
    PyObject * pNewLabel = NULL;
    PyObject * pOutputsParam = NULL; // List of strings to specify the optional outputs
    static char kw[][32] = {"channel", "new_label", "outputs", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "i|OOi", kwlist, &nChannel, &pNewLabel, &pOutputsParam, &nInstance))
        return NULL;
    if (nChannel == 0 || nChannel > cbMAXCHANS)
        return PyErr_Format(PyExc_ValueError, "Channel number out of range");

    // If neither new label nor outputs is specified, old label is returned
    // If new labels are specified, old labels are not returned by default unless listed in outputs
    UINT32 nOutputs = (pNewLabel == NULL && pOutputsParam == NULL) ? CHANLABEL_OUTPUTS_LABEL : CHANLABEL_OUTPUTS_NONE;
    if (pOutputsParam != NULL)
    {
        if (PyUnicode_Check(pOutputsParam))
            return PyErr_Format(PyExc_TypeError, "Invalid outputs; unicode not supported yet");
        if (PyString_Check(pOutputsParam))
        {
            const char * pszLabel = PyString_AsString(pOutputsParam);
            if (pszLabel == NULL)
                return NULL;
            LUT_CHANLABEL_OUTPUTS::iterator it = g_lutChanLabelOutputs.find(pszLabel);
            if (it == g_lutChanLabelOutputs.end())
                return PyErr_Format(PyExc_ValueError, "Invalid outputs item (%s); requested output not recognized", pszLabel);
            nOutputs |= it->second;
        }
        else if (PyList_Check(pOutputsParam))
        {
            // enumerate all the requested outputs
            for (UINT32 i = 0; i < (UINT32)PyList_GET_SIZE(pOutputsParam); ++i)
            {
                PyObject * pParam = PyList_GET_ITEM(pOutputsParam, i);
                if (PyUnicode_Check(pParam))
                    return PyErr_Format(PyExc_TypeError, "Invalid outputs; unicode not supported yet");
                const char * pszLabel = PyString_AsString(pParam);
                if (pszLabel == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid outputs item; should be string");
                LUT_CHANLABEL_OUTPUTS::iterator it = g_lutChanLabelOutputs.find(pszLabel);
                if (it == g_lutChanLabelOutputs.end())
                    return PyErr_Format(PyExc_ValueError, "Invalid outputs item (%s); requested output not recognized", pszLabel);
                nOutputs |= it->second;
            }
        } else {
            return PyErr_Format(PyExc_TypeError, "Invalid outputs; should be string or list of strings");
        }
    } // if (pOutputsParam != NULL

    // Read old channel labels before changing them
    char   old_label[32] = {'\0'};
    if (nOutputs & CHANLABEL_OUTPUTS_LABEL)
    {
        // Get channel label specified
        cbSdkResult sdkres = cbSdkGetChannelLabel(nInstance, nChannel, NULL, old_label, NULL, NULL);
        if (sdkres != CBSDKRESULT_SUCCESS)
            cbPySetErrorFromSdkError(sdkres);
        if (sdkres < CBSDKRESULT_SUCCESS)
            return NULL;
    }
    // New labels to assign
    if (pNewLabel != NULL)
    {
        if (PyUnicode_Check(pNewLabel))
            return PyErr_Format(PyExc_TypeError, "Invalid label; unicode not supported yet");
        const char * pszLabel = PyString_AsString(pNewLabel);
        if (pszLabel == NULL)
            return PyErr_Format(PyExc_TypeError, "Invalid label; should be string");
        // Set new channel label
        cbSdkResult sdkres = cbSdkSetChannelLabel(nInstance, nChannel, pszLabel, 0, NULL);
        if (sdkres != CBSDKRESULT_SUCCESS)
            cbPySetErrorFromSdkError(sdkres);
        if (sdkres < CBSDKRESULT_SUCCESS)
            return NULL;
    } // end if (pNewLabel != NULL
    if (nOutputs == CHANLABEL_OUTPUTS_NONE)
    {
        Py_INCREF(Py_None);
        res = Py_None;
        return res;
    }

    res = PyDict_New();
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output dictionary");
    if (nOutputs & CHANLABEL_OUTPUTS_LABEL)
    {
        PyObject * pVal = PyString_FromString(old_label);
        if (pVal == NULL || PyDict_SetItemString(res, "label", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning label");
        }
    }
    UINT32 bValid[cbMAXUNITS + 1] = {0};
    if (nOutputs & (CHANLABEL_OUTPUTS_ENABLED | CHANLABEL_OUTPUTS_UNIT_VALID))
    {
        cbSdkResult sdkres = cbSdkGetChannelLabel(nInstance, nChannel, bValid);
        if (sdkres != CBSDKRESULT_SUCCESS)
            cbPySetErrorFromSdkError(sdkres);
        if (sdkres < CBSDKRESULT_SUCCESS)
            return NULL;
    }
    if (nOutputs & CHANLABEL_OUTPUTS_ENABLED)
    {
        PyObject * pVal = PyBool_FromLong(bValid[0]);
        if (pVal == NULL || PyDict_SetItemString(res, "enabled", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning enabled");
        }
    }
    if (nOutputs & CHANLABEL_OUTPUTS_UNIT_VALID)
    {
        // Only input channel have valid units
        if (nChannel <= cbNUM_ANALOG_CHANS)
        {
            int dims[2] = {cbMAXUNITS, 1};
            PyArrayObject * pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
            if (pArr == NULL || PyDict_SetItemString(res, "valid_unit", (PyObject * )pArr) != 0)
            {
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Error in returning valid_unit array");
            }
            UINT32 * pValid = (UINT32 *)PyArray_DATA(pArr);
            for (int nUnit = 0; nUnit < cbMAXUNITS; ++nUnit)
            {
                pValid[nUnit] = bValid[nUnit + 1];
            }
        }
    }

    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Configure trial settings
static PyObject * cbpy_TrialConfig(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    UINT16 uBegChan   = 0;
    UINT32 uBegMask   = 0;
    UINT32 uBegVal    = 0;
    UINT16 uEndChan   = 0;
    UINT32 uEndMask   = 0;
    UINT32 uEndVal    = 0;
    bool   bDouble    = false;
    bool   bAbsolute  = false;
    UINT32 uWaveforms = 0;
    UINT32 uConts     = cbSdk_CONTINUOUS_DATA_SAMPLES;
    UINT32 uEvents    = cbSdk_EVENT_DATA_SAMPLES;
    UINT32 uComments  = 0;
    UINT32 uTrackings = 0;
    UINT32 bWithinTrial = 0;

    int nInstance = 0;
    PyObject * pbActive = NULL;
    PyObject * pBufferParam = NULL;
    PyObject * pRangeParam = NULL;

    static char kw[][32] = {"reset", "buffer_parameter", "range_parameter", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!|O!O!i", kwlist,
                                     &PyBool_Type, &pbActive, &PyDict_Type, &pBufferParam, &PyDict_Type, &pRangeParam, &nInstance))
        return NULL;

    // Boolean check
    int bActive = PyObject_IsTrue(pbActive);
    if (bActive == -1)
        return PyErr_Format(PyExc_TypeError, "Invalid reset parameter; should be boolean");

    cbSdkResult sdkres = cbSdkGetTrialConfig(nInstance, &bWithinTrial, &uBegChan, &uBegMask, &uBegVal, &uEndChan, &uEndMask, &uEndVal,
            &bDouble, &uWaveforms, &uConts, &uEvents, &uComments, &uTrackings, &bAbsolute);

    // If any buffer parameter specified
    if (pBufferParam)
    {
        // all optional parameters
        PyObject * pParam = PyDict_GetItemString(pBufferParam, "double");
        if (pParam != NULL)
        {
            int nDouble = PyObject_IsTrue(pParam);
            if (nDouble == -1)
                return PyErr_Format(PyExc_TypeError, "Invalid double parameter; should be boolean");
            bDouble = nDouble;
        }
        pParam = PyDict_GetItemString(pBufferParam, "absolute");
        if (pParam != NULL)
        {
            int nAbsolute = PyObject_IsTrue(pParam);
            if (nAbsolute == -1)
                return PyErr_Format(PyExc_TypeError, "Invalid absolute parameter; should be boolean");
            bAbsolute = nAbsolute;
        }
        pParam = PyDict_GetItemString(pBufferParam, "continuous_length");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid continuous buffer length; should be integer");
            uConts = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pBufferParam, "event_length");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid event buffer length; should be integer");
            uEvents = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pBufferParam, "comment_length");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid comment buffer length; should be integer");
            uComments = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pBufferParam, "tracking_length");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid tracking buffer length; should be integer");
            uTrackings = PyInt_AsLong(pParam);
        }
    }
    // If any range parameter specified
    if (pRangeParam)
    {
        // all optional parameters
        PyObject * pParam = PyDict_GetItemString(pRangeParam, "begin_channel");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid begin_channel; should be integer");
            uBegChan = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pRangeParam, "begin_mask");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid begin_mask; should be integer");
            uBegMask = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pRangeParam, "begin_value");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid begin_value; should be integer");
            uBegVal = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pRangeParam, "end_channel");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid end_channel; should be integer");
            uEndChan = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pRangeParam, "end_mask");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid end_mask; should be integer");
            uEndMask = PyInt_AsLong(pParam);
        }
        pParam = PyDict_GetItemString(pRangeParam, "end_value");
        if (pParam != NULL)
        {
            if (!PyInt_Check(pParam))
                return PyErr_Format(PyExc_TypeError, "Invalid end_value; should be integer");
            uEndVal = PyInt_AsLong(pParam);
        }
    }

    sdkres = cbSdkSetTrialConfig(nInstance,
            bActive, uBegChan, uBegMask, uBegVal, uEndChan, uEndMask, uEndVal,
            bDouble, uWaveforms, uConts, uEvents, uComments, uTrackings, bAbsolute);

    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
        res = PyBool_FromLong(bActive);
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Trial continuous data
static PyObject * cbpy_TrialCont(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    bool   bDouble = false;
    bool bFlushBuffer = false;
    PyObject * pbActive = NULL;
    int nInstance = 0;

    static char kw[][32] = {"reset", "instance"};
    static char * kwlist[] = {kw[0], kw[1], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|O!i", kwlist,
                                     &PyBool_Type, &pbActive, &nInstance))
        return NULL;

    if (pbActive != NULL)
    {
        // Boolean check
        int bActive = PyObject_IsTrue(pbActive);
        if (bActive == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid reset parameter; should be boolean");
        bFlushBuffer = (bActive != 0);
    }

    cbSdkResult sdkres = cbSdkGetTrialConfig(nInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bDouble);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    // Initialize continuous trial
    cbSdkTrialCont trialcont;
    sdkres = cbSdkInitTrialData(nInstance, NULL, &trialcont, NULL, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    res = PyList_New(trialcont.count);
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output list");
    if (trialcont.count == 0)
        return res;
    for (UINT32 channel = 0; channel < trialcont.count; channel++)
    {
        trialcont.samples[channel] = NULL;
        UINT32 num_samples = trialcont.num_samples[channel];

        PyObject * pTuple = PyTuple_New(2);
        if (pTuple == NULL)
        {
            Py_DECREF(pTuple);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple");
        }
        PyObject * pVal = PyLong_FromLong(trialcont.chan[channel]);
        if (pVal == NULL)
        {
            Py_DECREF(pTuple);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple channel");
        }
        // First tuple element is the channel number
        PyTuple_SET_ITEM(pTuple, 0, pVal);

        int dims[2] = {num_samples, 1};
        PyArrayObject * pArr = (PyArrayObject *)PyArray_FromDims(1, dims, bDouble ? NPY_FLOAT64 : NPY_INT16);
        if (pArr == NULL)
        {
            Py_DECREF(pTuple);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple array");
        }
        trialcont.samples[channel] = PyArray_DATA(pArr);
        // Second tuple element is the array of data
        PyTuple_SET_ITEM(pTuple, 1, (PyObject *)pArr);

        // Add tuple to the list
        PyList_SET_ITEM(res, channel, pTuple);
    }
    // Now get buffered data
    sdkres = cbSdkGetTrialData(nInstance, bFlushBuffer, NULL, &trialcont, NULL, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
    {
        Py_XDECREF(res);
        res = NULL;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Trial spike and event data
static PyObject * cbpy_TrialEvent(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    bool   bDouble = false;
    bool bFlushBuffer = false;
    PyObject * pbActive = NULL;
    int nInstance = 0;

    static char kw[][32] = {"reset", "instance"};
    static char * kwlist[] = {kw[0], kw[1], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|O!i", kwlist,
                                     &PyBool_Type, &pbActive, &nInstance))
        return NULL;

    if (pbActive != NULL)
    {
        // Boolean check
        int bActive = PyObject_IsTrue(pbActive);
        if (bActive == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid reset parameter; should be boolean");
        bFlushBuffer = (bActive != 0);
    }

    cbSdkResult sdkres = cbSdkGetTrialConfig(nInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &bDouble);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres, "Cannot determine trial property");
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    // Initialize event trial
    cbSdkTrialEvent trialevent;
    sdkres = cbSdkInitTrialData(nInstance, &trialevent, NULL, NULL, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres, "Cannot initialize trial");
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;
    res = PyList_New(trialevent.count);
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output list");
    if (trialevent.count == 0)
        return res;
    for (UINT32 channel = 0; channel < trialevent.count; channel++)
    {
        UINT16 ch = trialevent.chan[channel]; // Actual channel number
        PyObject * pTuple = NULL;
        // For digital channels return a pair of channel and its value,
        //   for other channels, return channel and timestamps of each unit
        if (ch == MAX_CHANS_DIGITAL_IN || ch == MAX_CHANS_SERIAL)
            pTuple = PyTuple_New(2);
        else
            pTuple = PyTuple_New(cbMAXUNITS + 1);
        if (pTuple == NULL)
        {
            Py_DECREF(pTuple);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple");
        }
        PyObject * pVal = PyLong_FromLong(ch);
        if (pVal == NULL)
        {
            Py_DECREF(pTuple);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple channel");
        }
        // First tuple element is the channel number
        PyTuple_SET_ITEM(pTuple, 0, pVal);
        PyArrayObject * pArr = NULL;
        // Fill values for non-empty digital or serial channels
        if (ch == MAX_CHANS_DIGITAL_IN || ch == MAX_CHANS_SERIAL)
        {
            UINT32 num_samples = trialevent.num_samples[channel][0];
            int dims[2] = {num_samples, 1};
            pArr = (PyArrayObject *)PyArray_FromDims(1, dims, bDouble ? NPY_FLOAT64 : NPY_UINT16);
            if (pArr == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output tuple array");
            }
            // this tuple element is the array of data
            PyTuple_SET_ITEM(pTuple, 1, (PyObject *)pArr);
            trialevent.waveforms[channel] = NULL;
            if (num_samples)
                trialevent.waveforms[channel] = PyArray_DATA(pArr);
        } else {
            // Fill timestamps for non-empty channels
            for(UINT u = 0; u <= cbMAXUNITS; u++)
            {
                trialevent.timestamps[channel][u] = NULL;
                UINT32 num_samples = trialevent.num_samples[channel][u];
                int dims[2] = {num_samples, 1};
                pArr = (PyArrayObject *)PyArray_FromDims(1, dims, bDouble ? NPY_FLOAT64 : NPY_UINT32);
                if (pArr == NULL)
                {
                    Py_DECREF(pTuple);
                    Py_DECREF(res);
                    return PyErr_Format(PyExc_MemoryError, "Could not create output tuple array");
                }
                // this tuple element is the array of data
                PyTuple_SET_ITEM(pTuple, 1 + u, (PyObject *)pArr);
                if (num_samples)
                    trialevent.timestamps[channel][u] = PyArray_DATA(pArr);
            }
        }

        // Add tuple to the list
        PyList_SET_ITEM(res, channel, pTuple);
    }
    // Now get buffered data
    sdkres = cbSdkGetTrialData(nInstance, bFlushBuffer, &trialevent, NULL, NULL, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
    {
        Py_XDECREF(res);
        res = NULL;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Trial comments.
static PyObject * cbpy_TrialComment(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nTupleCount = 0; // output tuple size
    bool bFlushBuffer = false;
    PyObject * pbActive = NULL, * pbCharsets = NULL, * pbTimestamps = NULL, * pbData = NULL, * pbComments = NULL;
    bool bCharsets = true, bTimestamps = true, bRgbas = true, bComments = true;
    int nInstance = 0;

    static char kw[][32] = {"reset", "charsets", "timestamps", "data", "comments", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], kw[4], kw[5], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|O!O!O!O!O!i", kwlist,
                                     &PyBool_Type, &pbActive,
                                     &PyBool_Type, &pbCharsets, &PyBool_Type, &pbTimestamps,
                                     &PyBool_Type, &pbData, &PyBool_Type, &pbComments, &nInstance))
        return NULL;

    if (pbActive != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid reset parameter; should be boolean");
        bFlushBuffer = (nVal != 0);
    }
    if (pbCharsets != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid charsets parameter; should be boolean");
        bCharsets = (nVal != 0);
        if (bCharsets)
            nTupleCount++;
    }
    if (pbTimestamps != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid timestamps parameter; should be boolean");
        bTimestamps = (nVal != 0);
        if (bTimestamps)
            nTupleCount++;
    }
    if (pbData != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid data parameter; should be boolean");
        bRgbas = (nVal != 0);
        if (bRgbas)
            nTupleCount++;
    }
    if (pbComments != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid comments parameter; should be boolean");
        bComments = (nVal != 0);
        if (bComments)
            nTupleCount++;
    }

    // Initialize event trial
    cbSdkTrialComment trialcomment;
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, NULL, NULL, &trialcomment, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres, "Cannot initialize trial");
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;

    UINT32 num_samples = trialcomment.num_samples;
    int dims[2] = {num_samples, 1};

    res = PyTuple_New(nTupleCount);
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output tuple");

    int nTupleIdx = 0;
    PyArrayObject * pArr = NULL;

    trialcomment.charsets = NULL;
    if (bCharsets)
    {
        pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT8);
        if (pArr == NULL)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create charset array");
        }
        if (num_samples)
            trialcomment.charsets = (UINT8 *)PyArray_DATA(pArr);
        // Add charsets
        PyTuple_SET_ITEM(res, nTupleIdx++, (PyObject *)pArr);
    }

    trialcomment.timestamps = NULL;
    if (bTimestamps)
    {
        pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
        if (pArr == NULL)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output timestamp array");
        }
        if (num_samples)
            trialcomment.timestamps = (UINT8 *)PyArray_DATA(pArr);
        // Add charsets
        PyTuple_SET_ITEM(res, nTupleIdx++, (PyObject *)pArr);
    }

    trialcomment.rgbas = NULL;
    if (bRgbas)
    {
        pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
        if (pArr == NULL)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output rgba array");
        }
        if (num_samples)
            trialcomment.rgbas = (UINT32 *)PyArray_DATA(pArr);
        // Add charsets
        PyTuple_SET_ITEM(res, nTupleIdx++, (PyObject *)pArr);
    }

    trialcomment.comments = NULL;
    if (bComments)
    {
        trialcomment.comments = (UINT8 * *) PyMem_Malloc(num_samples * sizeof(UINT8 *));
        if (trialcomment.comments == NULL)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create output comments");
        }
        PyObject * pList = PyList_New(num_samples);
        if (pList == NULL)
        {
            PyMem_Free(trialcomment.comments);
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Could not create comments list");
        }
        for (int i = 0; i < num_samples; ++i)
        {
            trialcomment.comments[i] = NULL;
            PyObject * pVal = PyString_FromStringAndSize(NULL, cbMAX_COMMENT + 1);
            if (pVal == NULL)
            {
                PyMem_Free(trialcomment.comments);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output comment");
            }
            if (num_samples)
                trialcomment.comments[i] = (UINT8 *)PyString_AsString(pVal);
            // Add comment to the list
            PyList_SET_ITEM(pList, i, pVal);
        }
        // Add comments list
        PyTuple_SET_ITEM(res, nTupleIdx++, pList);
        PyMem_Free(trialcomment.comments);
    }

    // Now get buffered data
    sdkres = cbSdkGetTrialData(nInstance, bFlushBuffer, NULL, NULL, &trialcomment, NULL);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
    {
        Py_XDECREF(res);
        res = NULL;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Trial tracking points
static PyObject * cbpy_TrialTracking(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nTupleCount = 0; // output tuple size
    bool bFlushBuffer = false;
    PyObject * pbActive = NULL, * pbTimestamps = NULL, * pbSynchTimestamps = NULL, * pbSynchFrameNumbers = NULL, * pbCoordinates = NULL;
    bool bTimestamps = true, bSynchTimestamps = true, bSynchFrameNumbers = true, bCoordinates = true;
    int nInstance = 0;

    static char kw[][32] = {"reset", "timestamps", "synch_timestamps", "synch_frame_numbers", "coordinates", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], kw[4], kw[5], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|O!O!O!O!O!i", kwlist,
                                     &PyBool_Type, &pbActive,
                                     &PyBool_Type, &pbTimestamps, &PyBool_Type, &pbSynchTimestamps,
                                     &PyBool_Type, &pbSynchFrameNumbers, &PyBool_Type, &pbCoordinates, &nInstance))
        return NULL;

    if (pbActive != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid reset parameter; should be boolean");
        bFlushBuffer = (nVal != 0);
    }
    if (pbTimestamps != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid timestamps parameter; should be boolean");
        bTimestamps = (nVal != 0);
        if (bTimestamps)
            nTupleCount++;
    }
    if (pbSynchTimestamps != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid synch_timestamps parameter; should be boolean");
        bSynchTimestamps = (nVal != 0);
        if (bSynchTimestamps)
            nTupleCount++;
    }
    if (pbSynchFrameNumbers != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid synch_frame_numbers parameter; should be boolean");
        bSynchFrameNumbers = (nVal != 0);
        if (bSynchFrameNumbers)
            nTupleCount++;
    }
    if (pbCoordinates != NULL)
    {
        // Boolean check
        int nVal = PyObject_IsTrue(pbActive);
        if (nVal == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid coordinates parameter; should be boolean");
        bCoordinates = (nVal != 0);
        if (bCoordinates)
            nTupleCount++;
    }

    // Initialize event trial
    cbSdkTrialTracking trialtracking;
    cbSdkResult sdkres = cbSdkInitTrialData(nInstance, NULL, NULL, NULL, &trialtracking);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres, "Cannot initialize trial");
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;

    res = PyList_New(trialtracking.count);
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output list");
    if (trialtracking.count == 0)
        return res;

    PyObject * pCoordsList[cbMAXTRACKOBJ] = {NULL};
    for (UINT32 id = 0; id < trialtracking.count; id++)
    {
        UINT16 trackable_id = trialtracking.ids[id]; // Actual tracking id
        UINT16 trackable_type = trialtracking.types[id]; // Actual tracking id
        PyObject * pTuple = PyTuple_New(nTupleCount + 2);
        if (pTuple == NULL)
            return PyErr_Format(PyExc_MemoryError, "Could not create output tuple");
        UINT32 num_samples = trialtracking.num_samples[id];
        int dims[2] = {num_samples, 1};
        int nTupleIdx = 0;

        {
            PyObject * pVal = PyLong_FromLong(trackable_id);
            if (pVal == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output trackable id");
            }
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, pVal);
        }

        {
            PyObject * pVal = PyLong_FromLong(trackable_type);
            if (pVal == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output trackable type");
            }
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, pVal);
        }

        trialtracking.timestamps[id] = NULL;
        if (bTimestamps)
        {
            PyArrayObject * pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
            if (pArr == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output timestamps array");
            }
            if (num_samples)
                trialtracking.timestamps[id] = (UINT32 *)PyArray_DATA(pArr);
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, (PyObject *)pArr);
        }

        trialtracking.synch_timestamps[id] = NULL;
        if (bSynchTimestamps)
        {
            PyArrayObject * pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
            if (pArr == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output synch_timestamps array");
            }
            if (num_samples)
                trialtracking.synch_timestamps[id] = (UINT32 *)PyArray_DATA(pArr);
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, (PyObject *)pArr);
        }

        trialtracking.synch_frame_numbers[id] = NULL;
        if (bSynchFrameNumbers)
        {
            PyArrayObject * pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
            if (pArr == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output synch_frame_numbers array");
            }
            if (num_samples)
                trialtracking.synch_frame_numbers[id] = (UINT32 *)PyArray_DATA(pArr);
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, (PyObject *)pArr);
        }

        trialtracking.point_counts[id] = NULL;
        if (bCoordinates)
        {
            trialtracking.point_counts[id] = (UINT16 *) PyMem_Malloc(num_samples * sizeof(UINT16));
            if (trialtracking.point_counts[id] == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output point_counts memory");
            }

            bool bWordData = false; // if data is of word-length
            int dim_count = 2; // number of dimensionf for each point
            switch(trialtracking.types[id])
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
                trialtracking.coords[id] = (void * *) PyMem_Malloc(num_samples * sizeof(UINT32 *));
            else
                trialtracking.coords[id] = (void * *) PyMem_Malloc(num_samples * sizeof(UINT16 *));

            if (trialtracking.coords[id] == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output coords memory");
            }

            // Coordinates is a list of possibly varying length
            pCoordsList[id] = PyList_New(num_samples);
            if (pCoordsList[id] == NULL)
            {
                Py_DECREF(pTuple);
                Py_DECREF(res);
                return PyErr_Format(PyExc_MemoryError, "Could not create output coords list");
            }
            // We allocate for the maximum number of points, later we reduce dimension
            for (int j = 0; j < num_samples; ++j)
            {
                PyArrayObject * pArr = NULL;
                int dims[2] = {trialtracking.max_point_counts[id], 1};
                if (bWordData)
                {
                    pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT32);
                    if (pArr == NULL)
                    {
                        Py_DECREF(pTuple);
                        Py_DECREF(res);
                        PyMem_Free(trialtracking.point_counts[id]);
                        PyMem_Free(trialtracking.coords[id]);
                        return PyErr_Format(PyExc_MemoryError, "Could not create output coords array");
                    }
                    trialtracking.coords[id][j] = (UINT32 *)PyArray_DATA(pArr);
                } else {
                    pArr = (PyArrayObject *)PyArray_FromDims(1, dims, NPY_UINT16);
                    if (pArr == NULL)
                    {
                        Py_DECREF(pTuple);
                        Py_DECREF(res);
                        PyMem_Free(trialtracking.point_counts[id]);
                        PyMem_Free(trialtracking.coords[id]);
                        return PyErr_Format(PyExc_MemoryError, "Could not create output coords array");
                    }
                    trialtracking.coords[id][j] = (UINT16 *)PyArray_DATA(pArr);
                }
                PyList_SET_ITEM(pCoordsList[id], j, (PyObject *)pArr);
            } // end for (int j =
            PyTuple_SET_ITEM(pTuple, nTupleIdx++, (PyObject *)pCoordsList[id]);
        } // end if (bCoordinates
        // Now add tuple to the list
        PyList_SET_ITEM(res, id, pTuple);
    } // end for (UINT32 id = 0

    // Now get buffered data
    sdkres = cbSdkGetTrialData(nInstance, bFlushBuffer, NULL, NULL, NULL, &trialtracking);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
    {
        Py_XDECREF(res);
        res = NULL;
    }

    for (UINT32 id = 0; id < trialtracking.count; id++)
    {
        UINT32 num_samples = trialtracking.num_samples[id];
        // Go through the list and reduce dimensions if needed
        for (int j = 0; j < num_samples; ++j)
        {
            UINT16 count = trialtracking.point_counts[id][j];
            if (count < trialtracking.max_point_counts[id])
            {
                PyArrayObject * pArr = (PyArrayObject *)PyList_GET_ITEM(pCoordsList[id], j);
                Py_ssize_t new_size[2] = {count, 1};
                PyArray_Dims dims;
                dims.len = 1;
                dims.ptr = &new_size[0];
                PyObject * ref  = PyArray_Resize(pArr, &dims, 1, NPY_CORDER);
                Py_XDECREF(ref);
            } //end if (count <
        } // end for (int j =
        // Free dynamic memory
        PyMem_Free(trialtracking.point_counts[id]);
        PyMem_Free(trialtracking.coords[id]);
    } // end for (UINT32 id = 0

    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Configure remote file recording
static PyObject * cbpy_FileConfig(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres;
    PyObject * res = NULL;
    int nTupleCount = 0; // output tuple size
    char * pSzCmd = NULL;
    char * pSzFileName = NULL;
    char * pSzComment = NULL;
    int nInstance = 0;

    static char kw[][32] = {"command", "filename", "comment", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|ssi", kwlist,
                                     &pSzCmd, &pSzFileName, &pSzComment, &nInstance))
        return NULL;

    LUT_FILECFG_CMD::iterator it = g_lutFileCfgCmd.find(pSzCmd);
    if (it == g_lutFileCfgCmd.end())
        return PyErr_Format(PyExc_ValueError, "Invalid command (%s); should be open, close, start, stop", pSzCmd);

    UINT32 nStart = FALSE;
    UINT32 options = cbFILECFG_OPT_NONE;
    FILECFG_CMD cmd = it->second;
    switch (cmd)
    {
    case FILECFG_CMD_INFO:
        if (pSzFileName != NULL || pSzComment != NULL)
        {
            return PyErr_Format(PyExc_ValueError, "Filename and comment must not be specified for 'info' command");
        }
        cbPySetErrorFromSdkError(CBSDKRESULT_NOTIMPLEMENTED, "info switch");
        {
#if 0
            char filename[256] = {'\0'};
            char username[256] = {'\0'};
            bool bRecording = false;
            sdkres = cbSdkGetFileConfig(nInstance, filename, username, &bRecording);
            if (sdkres != CBSDKRESULT_SUCCESS)
                cbPySetErrorFromSdkError(sdkres);
            if (sdkres >= CBSDKRESULT_SUCCESS)
            {
                res = PyDict_New();
                if (res == NULL)
                    return PyErr_Format(PyExc_MemoryError, "Could not create output dictionary");
                PyObject * pVal = PyBool_FromLong(bRecording ? 1 : 0);
                if (pVal == NULL || PyDict_SetItemString(res, "Recording", pVal) != 0)
                {
                    Py_DECREF(res);
                    return PyErr_Format(PyExc_ValueError, "Error in returning recording state");
                }
                pVal = PyString_FromString(filename);
                if (pVal == NULL || PyDict_SetItemString(res, "FileName", pVal) != 0)
                {
                    Py_DECREF(res);
                    return PyErr_Format(PyExc_ValueError, "Error in returning the file name");
                }
                pVal = PyString_FromString(username);
                if (pVal == NULL || PyDict_SetItemString(res, "UserName", pVal) != 0)
                {
                    Py_DECREF(res);
                    return PyErr_Format(PyExc_ValueError, "Cannot retrieve username");
                }
            }
#endif
            return res;
        }
        break;
    case FILECFG_CMD_OPEN:
        if (pSzFileName != NULL || pSzComment != NULL)
            return PyErr_Format(PyExc_ValueError, "Filename and comment must not be specified for 'open' command");
        options = cbFILECFG_OPT_OPEN;
        break;
    case FILECFG_CMD_CLOSE:
        options = cbFILECFG_OPT_CLOSE;
        break;
    case FILECFG_CMD_START:
        nStart = TRUE;
        if (pSzFileName == NULL || pSzComment == NULL)
            return PyErr_Format(PyExc_ValueError, "Filename and comment must be specified for 'start' command");
        break;
    case FILECFG_CMD_STOP:
        nStart = FALSE;
        if (pSzFileName == NULL || pSzComment == NULL)
            return PyErr_Format(PyExc_ValueError, "Filename and comment must be specified for 'stop' command");
        break;
    }
    sdkres = cbSdkSetFileConfig(nInstance,
            pSzFileName == NULL ? "" : pSzFileName,
            pSzComment == NULL ? "" : pSzComment, nStart, options);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Digital output command
static PyObject * cbpy_DigitalOut(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres;
    PyObject * res = NULL;
    PyObject * pCmdValue = NULL; // Command value (its type depends on command)
    UINT16 nChannel = 0;
    char * pSzCmd = NULL;
    int nInstance = 0;
    UINT16 nValue = 0;

    static char kw[][32] = {"channel", "command", "value", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "is|Oi", kwlist,
                                     &nChannel, &pSzCmd, &pCmdValue, &nInstance))
        return NULL;

    LUT_DIGOUT_CMD::iterator it = g_lutDigOutCmd.find(pSzCmd);
    if (it == g_lutDigOutCmd.end())
        return PyErr_Format(PyExc_ValueError, "Invalid command (%s); should be set_value", pSzCmd);

    DIGOUT_CMD cmd = it->second;
    switch (cmd)
    {
    case DIGOUT_CMD_SETVALUE:
        if (pCmdValue == NULL || !PyString_Check(pCmdValue))
        {
            return PyErr_Format(PyExc_ValueError, "Value parameter ('high' or 'low') must be specified for 'set_value' command");
        }
        {
            char * pSzValue = PyString_AsString(pCmdValue);
            if (_strcmpi(pSzValue, "high") == 0)
                nValue = 1;
            else if (_strcmpi(pSzValue, "low") == 0)
                nValue = 0;
            else
                return PyErr_Format(PyExc_ValueError, "Value parameter ('high' or 'low') must be specified for 'set_value' command");
        }
        break;
    }
    sdkres = cbSdkSetDigitalOutput(nInstance, nChannel, nValue);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Analog output command.
static PyObject * cbpy_AnalogOut(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    // TODO implement for 6.04
    cbSdkResult sdkres = CBSDKRESULT_NOTIMPLEMENTED;
    cbPySetErrorFromSdkError(sdkres);
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Mask channels for trials
static PyObject * cbpy_Mask(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres;
    PyObject * res = NULL;
    UINT16 nChannel = 0;
    char * pSzCmd = NULL;
    int nInstance = 0;
    UINT32 nActive = 1;

    static char kw[][32] = {"channel", "command", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "is|i", kwlist,
                                     &nChannel, &pSzCmd, &nInstance))
        return NULL;

    LUT_MASK_CMD::iterator it = g_lutMaskCmd.find(pSzCmd);
    if (it == g_lutMaskCmd.end())
        return PyErr_Format(PyExc_ValueError, "Invalid command (%s); should be off, on", pSzCmd);

    MASK_CMD cmd = it->second;
    switch (cmd)
    {
    case MASK_CMD_OFF:
        nActive = 0;
        break;
    case MASK_CMD_ON:
        nActive = 1;
        break;
    }
    sdkres = cbSdkSetChannelMask(nInstance, nChannel, nActive);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Comment or custom event
static PyObject * cbpy_Comment(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres;
    PyObject * res = NULL;
    UINT16 nChannel = 0;
    char * pSzComment = NULL;
    int nInstance = 0;
    UINT32 rgba = 0;
    UINT8 charset = 0;

    static char kw[][32] = {"comment", "rgba", "charset", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|Iii", kwlist,
                                     &pSzComment, &rgba, &charset, &nInstance))
        return NULL;

    sdkres = cbSdkSetComment(nInstance, rgba, charset, pSzComment);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Configure a channel
static PyObject * cbpy_Config(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    int nInstance = 0;
    UINT16 nChannel = 0;
    PyObject * pNewConfig = NULL;
    PyObject * pOutputsParam = NULL; // List of strings to specify the optional outputs
    static char kw[][32] = {"channel", "config", "outputs", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "i|O!Oi", kwlist, &nChannel, &PyDict_Type, &pNewConfig, &pOutputsParam, &nInstance))
        return NULL;

    // If neither new config nor outputs is specified, all old config is returned
    // If new labels are specified, old labels are not returned by default unless listed in outputs
    UINT32 nOutputs = (pNewConfig == NULL && pOutputsParam == NULL) ? CONFIG_CMD_ALL : CONFIG_CMD_NONE;
    if (pOutputsParam != NULL)
    {
        if (PyUnicode_Check(pOutputsParam))
            return PyErr_Format(PyExc_TypeError, "Invalid outputs; unicode not supported yet");
        if (PyString_Check(pOutputsParam))
        {
            const char * pszLabel = PyString_AsString(pOutputsParam);
            if (pszLabel == NULL)
                return NULL;
            LUT_CONFIG_OUTPUTS_CMD::iterator it = g_lutConfigOutputsCmd.find(pszLabel);
            if (it == g_lutConfigOutputsCmd.end())
                return PyErr_Format(PyExc_ValueError, "Invalid outputs item (%s); requested output not recognized", pszLabel);
            nOutputs |= it->second;
        }
        else if (PyList_Check(pOutputsParam))
        {
            // enumerate all the requested outputs
            for (UINT32 i = 0; i < (UINT32)PyList_GET_SIZE(pOutputsParam); ++i)
            {
                PyObject * pParam = PyList_GET_ITEM(pOutputsParam, i);
                if (PyUnicode_Check(pParam))
                    return PyErr_Format(PyExc_TypeError, "Invalid outputs; unicode not supported yet");
                const char * pszLabel = PyString_AsString(pParam);
                if (pszLabel == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid outputs item; should be string");
                LUT_CONFIG_OUTPUTS_CMD::iterator it = g_lutConfigOutputsCmd.find(pszLabel);
                if (it == g_lutConfigOutputsCmd.end())
                    return PyErr_Format(PyExc_ValueError, "Invalid outputs item (%s); requested output not recognized", pszLabel);
                nOutputs |= it->second;
            }
        } else {
            return PyErr_Format(PyExc_TypeError, "Invalid outputs; should be string or list of strings");
        }
    } // if (pOutputsParam != NULL

    // Keep old channel config before changing it
    cbPKT_CHANINFO chaninfo_old, chaninfo;
    cbSdkResult sdkres = cbSdkGetChannelConfig(nInstance, nChannel, &chaninfo);
    chaninfo_old = chaninfo;
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return NULL;

    bool bHasNewParams = false;
    PyObject *pKey, *pValue;
    Py_ssize_t pos = 0;
    while (PyDict_Next(pNewConfig, &pos, &pKey, &pValue))
    {
        if (PyUnicode_Check(pKey))
            return PyErr_Format(PyExc_TypeError, "Invalid config key; unicode not supported yet");
        const char * pszKey = PyString_AsString(pKey);
        if (pszKey == NULL)
            return PyErr_Format(PyExc_TypeError, "Invalid config key; should be string");
        LUT_CONFIG_INPUTS_CMD::iterator it = g_lutConfigInputsCmd.find(pszKey);
        if (it == g_lutConfigInputsCmd.end())
            return PyErr_Format(PyExc_ValueError, "Invalid config input (%s)", pszKey);
        bHasNewParams = true;
        CONFIG_CMD cmd = it->second;
        switch (cmd)
        {
        case CONFIG_CMD_USERFLAGS:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid userflags number");
            chaninfo.userflags = PyInt_AsLong(pValue);
            break;
        case CONFIG_CMD_SMPFILTER:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid smpfilter number");
            chaninfo.smpfilter = PyInt_AsLong(pValue);
            break;
        case CONFIG_CMD_SMPGROUP:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid smpgroup number");
            chaninfo.smpgroup = PyInt_AsLong(pValue);
            break;
        case CONFIG_CMD_SPKFILTER:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid spkfilter number");
            chaninfo.spkfilter = PyInt_AsLong(pValue);
            break;
        case CONFIG_CMD_SPKGROUP:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid spkgroup number");
            chaninfo.spkgroup = PyInt_AsLong(pValue);
            break;
        case CONFIG_CMD_SPKTHRLEVEL:
            if (PyUnicode_Check(pKey))
                return PyErr_Format(PyExc_TypeError, "Invalid spkthrlevel value; unicode not supported yet");
            if (PyInt_Check(pValue))
            {
                chaninfo.spkthrlevel = PyInt_AsLong(pValue);
            } else {
                const char * pSzValue = PyString_AsString(pValue);
                if (pSzValue == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid spkthrlevel value; should be string or integer");
                INT32 nValue = 0;
                sdkres = cbSdkAnalogToDigital(nInstance, nChannel, pSzValue, &nValue);
                if (sdkres != CBSDKRESULT_SUCCESS)
                    cbPySetErrorFromSdkError(sdkres, "cbSdkAnalogToDigital()");
                if (sdkres < CBSDKRESULT_SUCCESS)
                    return NULL;
                chaninfo.spkthrlevel = nValue;
            }
            break;
        case CONFIG_CMD_AMPLREJPOS:
            if (PyUnicode_Check(pKey))
                return PyErr_Format(PyExc_TypeError, "Invalid amplrejpos value; unicode not supported yet");
            if (PyInt_Check(pValue))
            {
                chaninfo.amplrejpos = PyInt_AsLong(pValue);
            } else {
                const char * pSzValue = PyString_AsString(pValue);
                if (pSzValue == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid amplrejpos value; should be string or integer");
                INT32 nValue = 0;
                sdkres = cbSdkAnalogToDigital(nInstance, nChannel, pSzValue, &nValue);
                if (sdkres != CBSDKRESULT_SUCCESS)
                    cbPySetErrorFromSdkError(sdkres, "cbSdkAnalogToDigital()");
                if (sdkres < CBSDKRESULT_SUCCESS)
                    return NULL;
                chaninfo.amplrejpos = nValue;
            }
            break;
        case CONFIG_CMD_AMPLREJNEG:
            if (PyUnicode_Check(pKey))
                return PyErr_Format(PyExc_TypeError, "Invalid amplrejneg value; unicode not supported yet");
            if (PyInt_Check(pValue))
            {
                chaninfo.amplrejneg = PyInt_AsLong(pValue);
            } else {
                const char * pSzValue = PyString_AsString(pValue);
                if (pSzValue == NULL)
                    return PyErr_Format(PyExc_TypeError, "Invalid amplrejneg value; should be string or integer");
                INT32 nValue = 0;
                sdkres = cbSdkAnalogToDigital(nInstance, nChannel, pSzValue, &nValue);
                if (sdkres != CBSDKRESULT_SUCCESS)
                    cbPySetErrorFromSdkError(sdkres, "cbSdkAnalogToDigital()");
                if (sdkres < CBSDKRESULT_SUCCESS)
                    return NULL;
                chaninfo.amplrejneg = nValue;
            }
            break;
        case CONFIG_CMD_REFELECCHAN:
            if (!PyInt_Check(pValue))
                return PyErr_Format(PyExc_ValueError, "Invalid refelecchan number");
            chaninfo.refelecchan = PyInt_AsLong(pValue);
            break;
        default:
            // Ignore the rest
            break;
        } //end switch (cmd
    } //end while (PyDict_Next

    sdkres = cbSdkSetChannelConfig(nInstance, nChannel, &chaninfo);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return res;

    // If no outputs, we are done here
    if (nOutputs == CONFIG_CMD_NONE)
    {
        Py_INCREF(Py_None);
        res = Py_None;
        return res;
    }

    res = PyDict_New();
    if (res == NULL)
        return PyErr_Format(PyExc_MemoryError, "Could not create output dictionary");
    if (nOutputs & CONFIG_CMD_USERFLAGS)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.userflags);
        if (pVal == NULL || PyDict_SetItemString(res, "userflags", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning userflags");
        }
    }
    if (nOutputs & CONFIG_CMD_SMPFILTER)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.smpfilter);
        if (pVal == NULL || PyDict_SetItemString(res, "smpfilter", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning smpfilter");
        }
    }
    if (nOutputs & CONFIG_CMD_SMPGROUP)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.smpgroup);
        if (pVal == NULL || PyDict_SetItemString(res, "smpgroup", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning smpgroup");
        }
    }
    if (nOutputs & CONFIG_CMD_SPKFILTER)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.spkfilter);
        if (pVal == NULL || PyDict_SetItemString(res, "spkfilter", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning spkfilter");
        }
    }
    if (nOutputs & CONFIG_CMD_SPKGROUP)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.spkgroup);
        if (pVal == NULL || PyDict_SetItemString(res, "spkgroup", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning spkgroup");
        }
    }
    if (nOutputs & CONFIG_CMD_SPKTHRLEVEL)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.spkthrlevel);
        if (pVal == NULL || PyDict_SetItemString(res, "spkthrlevel", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning spkthrlevel");
        }
    }
    if (nOutputs & CONFIG_CMD_AMPLREJPOS)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.amplrejpos);
        if (pVal == NULL || PyDict_SetItemString(res, "amplrejpos", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning amplrejpos");
        }
    }
    if (nOutputs & CONFIG_CMD_AMPLREJNEG)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.amplrejneg);
        if (pVal == NULL || PyDict_SetItemString(res, "amplrejneg", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning amplrejneg");
        }
    }
    if (nOutputs & CONFIG_CMD_REFELECCHAN)
    {
        PyObject * pVal = PyInt_FromLong(chaninfo_old.refelecchan);
        if (pVal == NULL || PyDict_SetItemString(res, "refelecchan", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_ValueError, "Error in returning refelecchan");
        }
    }
    if (nOutputs & CONFIG_CMD_SACLE)
    {
        PyObject * pVal = NULL;

        pVal = PyString_FromString(chaninfo_old.physcalin.anaunit);
        if (pVal == NULL || PyDict_SetItemString(res, "analog_unit", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Error in returning anaunit");
        }
        pVal = PyInt_FromLong(chaninfo_old.physcalin.anamax);
        if (pVal == NULL || PyDict_SetItemString(res, "max_analog", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Error in returning anamax");
        }
        pVal = PyInt_FromLong(chaninfo_old.physcalin.digmax);
        if (pVal == NULL || PyDict_SetItemString(res, "max_digital", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Error in returning digmax");
        }
        pVal = PyInt_FromLong(chaninfo_old.physcalin.anamin);
        if (pVal == NULL || PyDict_SetItemString(res, "min_analog", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Error in returning anamin");
        }
        pVal = PyInt_FromLong(chaninfo_old.physcalin.digmin);
        if (pVal == NULL || PyDict_SetItemString(res, "min_digital", pVal) != 0)
        {
            Py_DECREF(res);
            return PyErr_Format(PyExc_MemoryError, "Error in returning digmin");
        }
    }

    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Load or convert CCF
static PyObject * cbpy_CCF(PyObject *self, PyObject *args, PyObject *keywds)
{
    cbSdkResult sdkres = CBSDKRESULT_SUCCESS;
    PyObject * res = NULL;
    char * pSzCmd = NULL;
    char * pSzSrc = NULL;
    char * pSzDst = NULL;
    PyObject * pbThreaded = NULL;
    bool bThreaded = false;
    int nInstance = 0;

    static char kw[][32] = {"command", "source", "destination", "threaded", "instance"};
    static char * kwlist[] = {kw[0], kw[1], kw[2], kw[3], kw[4], NULL};
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|ssO!i", kwlist,
                                     &pSzCmd, &pSzSrc, &pSzDst, &PyBool_Type, &pbThreaded, &nInstance))
        return NULL;

    if (pbThreaded != NULL)
    {
        // Boolean check
        int nThreaded = PyObject_IsTrue(pbThreaded);
        if (nThreaded == -1)
            return PyErr_Format(PyExc_TypeError, "Invalid threaded parameter; should be boolean");
        bThreaded = (nThreaded != 0);
    }

    LUT_CCF_CMD::iterator it = g_lutCCFCmd.find(pSzCmd);
    if (it == g_lutCCFCmd.end())
        return PyErr_Format(PyExc_ValueError, "Invalid command (%s); should be save, send, convert", pSzCmd);

    // Read cannot be threaded if the result is needed in this function
    cbSdkCCF ccf;
    CCF_CMD cmd = it->second;
    switch (cmd)
    {
    case CCF_CMD_SAVE:
        if (pSzDst == NULL)
            return PyErr_Format(PyExc_ValueError, "destination must be specified for 'save' command");
        if (pSzSrc != NULL)
            return PyErr_Format(PyExc_ValueError, "source must not be specified for 'save' command (use 'convert' for old CCF files)");
        sdkres = cbSdkReadCCF(nInstance, &ccf, NULL, true, false, false);
        break;
    case CCF_CMD_SEND:
        if (pSzDst != NULL)
            return PyErr_Format(PyExc_ValueError, "destination must not be specified for 'send' command (it is sent to NSP)");
        if (pSzSrc == NULL)
            return PyErr_Format(PyExc_ValueError, "source must be specified for 'send' command");
        // Send can be done in one operation
        sdkres = cbSdkReadCCF(nInstance, &ccf, pSzSrc, true, true, bThreaded);
        break;
    case CCF_CMD_CONVERT:
        if (pSzSrc == NULL || pSzDst == NULL)
            return PyErr_Format(PyExc_ValueError, "source and destination must be specified for 'convert' command");
        sdkres = cbSdkReadCCF(nInstance, &ccf, pSzSrc, true, false, false);
        break;
    }
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres < CBSDKRESULT_SUCCESS)
        return res;

    if (cmd == CCF_CMD_SAVE || cmd == CCF_CMD_CONVERT)
        sdkres = cbSdkWriteCCF(nInstance, &ccf, pSzDst, bThreaded);

    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Instrument system runtime command
static PyObject * cbpy_System(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject * res = NULL;
    static char kw[][32] = {"command", "instance"};
    static char * kwlist[] = {kw[0], NULL};
    char * pSzCmd = NULL;
    int nInstance = 0;
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s|i", kwlist, &pSzCmd, &nInstance))
        return NULL;
    LUT_SYSTEM::iterator it = g_lutSystem.find(pSzCmd);
    if (it == g_lutSystem.end())
        return PyErr_Format(PyExc_ValueError, "Invalid command (%s); should be reset, shutdown, standby", pSzCmd);
    cbSdkSystemType cmd = it->second;
    cbSdkResult sdkres = cbSdkSystem(nInstance, cmd);
    if (sdkres != CBSDKRESULT_SUCCESS)
        cbPySetErrorFromSdkError(sdkres);
    if (sdkres >= CBSDKRESULT_SUCCESS)
    {
        Py_INCREF(Py_None);
        res = Py_None;
    }
    return res;
}

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Generate the right exception or warning based on SDK error
// Inputs:
//   sdkres - cbsdk result
//   szErr  - optional error message for unknown errors
void cbPySetErrorFromSdkError(cbSdkResult sdkres, const char * szErr)
{
    int stacklevel = 1;
    switch (sdkres)
    {
    case CBSDKRESULT_WARNCONVERT:
        PyErr_WarnEx(PyExc_UserWarning, "File conversion is needed", stacklevel);
        break;
    case CBSDKRESULT_WARNCLOSED:
        PyErr_WarnEx(PyExc_UserWarning, "Library is already closed", stacklevel);
        break;
    case CBSDKRESULT_WARNOPEN:
        PyErr_WarnEx(PyExc_UserWarning, "Library is already opened", stacklevel);
        break;
    case CBSDKRESULT_SUCCESS:
        break;
    case CBSDKRESULT_NOTIMPLEMENTED:
        PyErr_SetString(g_cbpyError, "Not implemented");
        break;
    case CBSDKRESULT_INVALIDPARAM:
        PyErr_SetString(g_cbpyError, "Invalid parameter");
        break;
    case CBSDKRESULT_CLOSED:
        PyErr_SetString(g_cbpyError, "Interface is closed cannot do this operation");
        break;
    case CBSDKRESULT_OPEN:
        PyErr_SetString(g_cbpyError, "Interface is open cannot do this operation");
        break;
    case CBSDKRESULT_NULLPTR:
        PyErr_SetString(g_cbpyError, "Null pointer");
        break;
    case CBSDKRESULT_ERROPENCENTRAL:
        PyErr_SetString(g_cbpyError, "Unable to open Central interface");
        break;
    case CBSDKRESULT_ERROPENUDP:
        PyErr_SetString(g_cbpyError, "Unable to open UDP interface (might happen if default)");
        break;
    case CBSDKRESULT_ERROPENUDPPORT:
        PyErr_SetString(g_cbpyError, "Unable to open UDP port");
        break;
    case CBSDKRESULT_ERRMEMORYTRIAL:
        PyErr_SetString(g_cbpyError, "Unable to allocate RAM for trial cache data");
        break;
    case CBSDKRESULT_ERROPENUDPTHREAD:
        PyErr_SetString(g_cbpyError, "Unable to open UDP timer thread");
        break;
    case CBSDKRESULT_ERROPENCENTRALTHREAD:
        PyErr_SetString(g_cbpyError, "Unable to open Central communication thread");
        break;
    case CBSDKRESULT_INVALIDCHANNEL:
        PyErr_SetString(g_cbpyError, "Invalid channel number");
        break;
    case CBSDKRESULT_INVALIDCOMMENT:
        PyErr_SetString(g_cbpyError, "Comment too long or invalid");
        break;
    case CBSDKRESULT_INVALIDFILENAME:
        PyErr_SetString(g_cbpyError, "Filename too long or invalid");
        break;
    case CBSDKRESULT_INVALIDCALLBACKTYPE:
        PyErr_SetString(g_cbpyError, "Invalid callback type");
        break;
    case CBSDKRESULT_CALLBACKREGFAILED:
        PyErr_SetString(g_cbpyError, "Callback register/unregister failed");
        break;
    case CBSDKRESULT_ERRCONFIG:
        PyErr_SetString(g_cbpyError, "Trying to run an unconfigured method");
        break;
    case CBSDKRESULT_INVALIDTRACKABLE:
        PyErr_SetString(g_cbpyError, "Invalid trackable id, or trackable not present");
        break;
    case CBSDKRESULT_INVALIDVIDEOSRC:
        PyErr_SetString(g_cbpyError, "Invalid video source id, or video source not present");
        break;
    case CBSDKRESULT_ERROPENFILE:
        PyErr_SetString(g_cbpyError, "Cannot open file");
        break;
    case CBSDKRESULT_ERRFORMATFILE:
        PyErr_SetString(g_cbpyError, "Wrong file format");
        break;
    case CBSDKRESULT_OPTERRUDP:
        PyErr_SetString(g_cbpyError, "Socket option error (possibly permission problem)");
        break;
    case CBSDKRESULT_MEMERRUDP:
    	PyErr_SetString(g_cbpyError, ERR_UDP_MESSAGE);
        break;
    case CBSDKRESULT_INVALIDINST:
        PyErr_SetString(g_cbpyError, "Invalid range or instrument address");
        break;
    case CBSDKRESULT_UNKNOWN:
        if (szErr != NULL)
            PyErr_SetString(g_cbpyError, szErr);
        else
            PyErr_SetString(g_cbpyError, "Unknown SDK error!");
        break;
    case CBSDKRESULT_ERRMEMORY:
#ifdef __APPLE__
        PyErr_SetString(g_cbpyError, "Memory allocation error trying to establish master connection\n"
                "Consider 'sysctl -w kern.sysv.shmmax=16777216'  and 'sysctl -w kern.sysv.shmall=4194304'");

#else
        PyErr_SetString(g_cbpyError, "Memory allocation error trying to establish master connection");
#endif
        break;
    case CBSDKRESULT_ERRINIT:
        PyErr_SetString(g_cbpyError, "Initialization error");
        break;
    case CBSDKRESULT_TIMEOUT:
        PyErr_SetString(g_cbpyError, "Connection timeout error");
        break;
    case CBSDKRESULT_BUSY:
        PyErr_SetString(g_cbpyError, "Resource is busy");
        break;
    case CBSDKRESULT_ERROFFLINE:
        PyErr_SetString(g_cbpyError, "Instrument is offline");
        break;
    default:
        PyErr_SetString(g_cbpyError, "Unhandled SDK error!");
        break;
    }
}

// All function names start with Capital letter and follow camel notation
static PyMethodDef g_cbpyMethods[] =
{
    {"Version",  (PyCFunction)cbpy_Version, METH_VARARGS | METH_KEYWORDS, "Library and instrument version."},
    {"Register",  (PyCFunction)cbpy_Register, METH_VARARGS | METH_KEYWORDS, "Register a callback function."},
    {"Unregister",  (PyCFunction)cbpy_Unregister, METH_VARARGS | METH_KEYWORDS, "Unregister a callback function."},
    {"Open",  (PyCFunction)cbpy_Open, METH_VARARGS | METH_KEYWORDS, "Open library."},
    {"Close",  (PyCFunction)cbpy_Close, METH_VARARGS | METH_KEYWORDS, "Close library."},
    {"Time",  (PyCFunction)cbpy_Time, METH_VARARGS | METH_KEYWORDS, "Instrument time."},
    {"ChanLabel",  (PyCFunction)cbpy_ChanLabel, METH_VARARGS | METH_KEYWORDS, "Get or set channel label."},
    {"TrialConfig",  (PyCFunction)cbpy_TrialConfig, METH_VARARGS | METH_KEYWORDS, "Configure trial settings."},
    {"TrialCont",  (PyCFunction)cbpy_TrialCont, METH_VARARGS | METH_KEYWORDS, "Trial continuous data"},
    {"TrialEvent",  (PyCFunction)cbpy_TrialEvent, METH_VARARGS | METH_KEYWORDS, "Trial spike and event data"},
    {"TrialComment",  (PyCFunction)cbpy_TrialComment, METH_VARARGS | METH_KEYWORDS, "Trial comments."},
    {"TrialTracking",  (PyCFunction)cbpy_TrialTracking, METH_VARARGS | METH_KEYWORDS, "Trial tracking data."},
    {"FileConfig",  (PyCFunction)cbpy_FileConfig, METH_VARARGS | METH_KEYWORDS, "Configure remote file recording."},
    {"DigitalOut",  (PyCFunction)cbpy_DigitalOut, METH_VARARGS | METH_KEYWORDS, "Digital output command."},
    {"AnalogOut",  (PyCFunction)cbpy_AnalogOut, METH_VARARGS | METH_KEYWORDS, "Analog output command."},
    {"Mask",  (PyCFunction)cbpy_Mask, METH_VARARGS | METH_KEYWORDS, "Mask channels for trials."},
    {"Comment",  (PyCFunction)cbpy_Comment, METH_VARARGS | METH_KEYWORDS, "Comment or custom event."},
    {"Config",  (PyCFunction)cbpy_Config, METH_VARARGS | METH_KEYWORDS, "Configure a channel."},
    {"CCF",  (PyCFunction)cbpy_CCF, METH_VARARGS | METH_KEYWORDS, "Load or convert Cerebus Config File (CCF)."},
    {"System",  (PyCFunction)cbpy_System, METH_VARARGS | METH_KEYWORDS, "Instrument system runtime command."},
    {NULL, NULL, 0, NULL} // This has to be the last
};

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "cbpy",              /* m_name */
        "cerebus python",    /* m_doc */
        -1,                  /* m_size */
        g_cbpyMethods,       /* m_methods */
        NULL,                /* m_reload */
        NULL,                /* m_traverse */
        NULL,                /* m_clear */
        NULL,                /* m_free */
    };
#endif

#if PY_MAJOR_VERSION >= 3
    #define MOD_INIT(name) PyObject * PyInit_##name(void)
    #define MOD_ERROR_VAL NULL
    #define MOD_SUCCESS_VAL(val) val
#else
    #define MOD_INIT(name) void init##name(void)
    #define MOD_ERROR_VAL
    #define MOD_SUCCESS_VAL(val)
#endif

// Author & Date: Ehsan Azar       6 May 2012
// Purpose: Module initialization
extern "C" CBSDKAPI MOD_INIT(cbpy)
{
    PyObject * m;

#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule3("cbpy", g_cbpyMethods, "cerebus python");
#endif

    if (m == NULL)
        return MOD_ERROR_VAL;

    // Setup numpy
    import_array();

    // Add cbpy exception
    char szcbpyerr[] = "cbpy.error";
    char szcbpyerrdoc[] = "cerebus python exception";
    g_cbpyError = PyErr_NewExceptionWithDoc(szcbpyerr, szcbpyerrdoc, NULL, NULL);
    Py_INCREF(g_cbpyError);
    PyModule_AddObject(m, "error", g_cbpyError);

    // Create all LUTs
    if (CreateLUTs())
    {
        PyErr_SetString(g_cbpyError, "Module; could not setup lookup tables");
        return MOD_ERROR_VAL;
    }

    //TODO: add this for callbacks
    // Add cbpy specific types to the module
    //if (cbPyAddTypes(m))
    //{
    //    PyErr_SetString(g_cbpyError, "Module; could not add types");
    //    return MOD_ERROR_VAL;
    //}
    return MOD_SUCCESS_VAL(m);
}
