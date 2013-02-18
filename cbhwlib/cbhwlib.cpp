// =STS=> cbhwlib.cpp[2730].aa14   open     SMID:15 
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Cerebus Interface Library
//
// Copyright (C) 2001-2003 Bionic Technologies, Inc.
// (c) Copyright 2003-2008 Cyberkinetics, Inc
// (c) Copyright 2008-2013 Blackrock Microsystems
//
// Developed by Shane Guillory and Angela Wang
//
// $Workfile: cbhwlib.cpp $
// $Archive: /Cerebus/Human/WindowsApps/cbhwlib/cbhwlib.cpp $
// $Revision: 24 $
// $Date: 4/26/05 2:56p $
// $Author: Kkorver $
//
// $NoKeywords: $
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
// cbhwlib library is currently based on non unicode only
#undef UNICODE
#undef _UNICODE
#ifndef  QT_APP
    #include "DataVector.h" // TODO: This file is now being used outside of "Single",
#else
#ifndef WIN32
    // For non-windows Qt applications
    #include <QSharedMemory>
    #include <semaphore.h>
    #include <sys/file.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
#endif
#ifndef _MSC_VER
    #define Sleep(x) usleep((x) * 1000)
    #define strncpy_s( dst, dstSize, src, count ) strncpy( dst, src, count < dstSize ? count : dstSize )
#endif                                // so it should probably be moved...
#include "debugmacs.h"
#include "cbhwlib.h"



// forward reference
cbRESULT CreateSharedObjects(UINT32 nInstance);



///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Internal Library variables
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// buffer handles
HANDLE      cb_xmt_global_buffer_hnd[cbMAXOPEN] = {NULL};       // Transmit queues to send out of this PC
cbXMTBUFF*  cb_xmt_global_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR GLOBAL_XMT_NAME = "XmtGlobal";
HANDLE      cb_xmt_local_buffer_hnd[cbMAXOPEN] = {NULL};        // Transmit queues only for local (this PC) use
cbXMTBUFF*  cb_xmt_local_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR LOCAL_XMT_NAME = "XmtLocal";

LPCSTR REC_BUF_NAME = "cbRECbuffer";
HANDLE      cb_rec_buffer_hnd[cbMAXOPEN] = {NULL};
cbRECBUFF*  cb_rec_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR CFG_BUF_NAME = "cbCFGbuffer";
HANDLE      cb_cfg_buffer_hnd[cbMAXOPEN] = {NULL};
cbCFGBUFF*  cb_cfg_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR STATUS_BUF_NAME = "cbSTATUSbuffer";
HANDLE      cb_pc_status_buffer_hnd[cbMAXOPEN] = {NULL};
cbPcStatus* cb_pc_status_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR SPK_BUF_NAME = "cbSPKbuffer";
HANDLE      cb_spk_buffer_hnd[cbMAXOPEN] = {NULL};
cbSPKBUFF*  cb_spk_buffer_ptr[cbMAXOPEN] = {NULL};
LPCSTR SIG_EVT_NAME = "cbSIGNALevent";
HANDLE      cb_sig_event_hnd[cbMAXOPEN] = {NULL};

//
UINT32      cb_library_index[cbMAXOPEN] = {0};
UINT32      cb_library_initialized[cbMAXOPEN] = {FALSE};
UINT32      cb_recbuff_tailwrap[cbMAXOPEN]  = {0};
UINT32      cb_recbuff_tailindex[cbMAXOPEN] = {0};
UINT32      cb_recbuff_processed[cbMAXOPEN] = {0};
UINT32      cb_recbuff_lasttime[cbMAXOPEN] = {0};

// Handle to system lock associated with shared resources
HANDLE      cb_sys_lock_hnd[cbMAXOPEN] = {NULL};


// Local functions to make life easier
inline cbOPTIONTABLE & GetOptionTable(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];
    return cb_cfg_buffer_ptr[nIdx]->optiontable;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Library Initialization Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////



// Returns the major/minor revision of the current library in the upper/lower UINT16 fields.
UINT32 cbVersion()
{
    return MAKELONG(cbVERSION_MINOR, cbVERSION_MAJOR);
}

// Author & Date:       Kirk Korver         17 Jun 2003
// Purpose: Release and clear the memory assocated with this handle/pointer
// Inputs:
//  hMem - the handle to the memory to free up
//  ppMem - the pointer to this same memory
void DestroySharedObject(HANDLE & hMem, void ** ppMem)
{
#ifdef WIN32
    if (*ppMem != NULL)
        UnmapViewOfFile(*ppMem);
    if (hMem != NULL)
        CloseHandle(hMem);
#else
    if (*ppMem != NULL)
    {
        QSharedMemory * pHnd = static_cast<QSharedMemory *>(hMem);
        if (pHnd)
            pHnd->detach();
    }
#endif
    hMem = 0;
    *ppMem = 0;
}

// Author & Date:       Almut Branner         28 Mar 2006
// Purpose: Release and clear the shared memory objects
// Inputs:
//   nInstance - nsp number to close library for
void DestroySharedObjects(BOOL bStandAlone, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    if (bStandAlone)
    {
        // clear out the version string
        if (cb_cfg_buffer_ptr[nIdx]) 
            cb_cfg_buffer_ptr[nIdx]->version = 0;
    }

    // close out the signal events
#ifdef WIN32
    if (cb_sig_event_hnd[nIdx]) 
        CloseHandle(cb_sig_event_hnd[nIdx]);
#else
    if (cb_sig_event_hnd[nIdx])
    {
        sem_close((sem_t *)cb_sig_event_hnd[nIdx]);
        if (bStandAlone)
        {
            char buf[64] = {0};
            if (nInstance == 0)
                _snprintf(buf, sizeof(buf), "%s", SIG_EVT_NAME);
            else
                _snprintf(buf, sizeof(buf), "%s%d", SIG_EVT_NAME, nInstance);
            sem_unlink(buf);
        }
    }
#endif

    // release the shared pc status memory space
    DestroySharedObject(cb_pc_status_buffer_hnd[nIdx], (void **)&cb_pc_status_buffer_ptr[nIdx]);

    // release the shared configuration memory space
    DestroySharedObject(cb_spk_buffer_hnd[nIdx], (void **)&cb_spk_buffer_ptr[nIdx]);

    // release the shared configuration memory space
    DestroySharedObject(cb_cfg_buffer_hnd[nIdx], (void **)&cb_cfg_buffer_ptr[nIdx]);

    // release the shared global transmit memory space
    DestroySharedObject(cb_xmt_global_buffer_hnd[nIdx], (void **)&cb_xmt_global_buffer_ptr[nIdx]);

    // release the shared local transmit memory space
    DestroySharedObject(cb_xmt_local_buffer_hnd[nIdx], (void **)&cb_xmt_local_buffer_ptr[nIdx]);

    // release the shared receive memory space
    DestroySharedObject(cb_rec_buffer_hnd[nIdx], (void **)&cb_rec_buffer_ptr[nIdx]);
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Open shared memory section
// Inputs:
//   szName    - buffer name
//   bReadOnly - if should open memory for read-only operation
HANDLE OpenSharedBuffer(LPCSTR szName, bool bReadOnly)
{
    HANDLE hnd = NULL;
#ifdef WIN32
    // Keep windows version unchanged
    hnd = OpenFileMapping(bReadOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, szName);
#else
    QString strKey(szName);
    QSharedMemory * pHnd = new QSharedMemory(strKey);
    if (pHnd)
    {
        if (pHnd->attach(bReadOnly ? QSharedMemory::ReadOnly : QSharedMemory::ReadWrite))
            hnd = pHnd;
        else
            delete pHnd;
    }
#endif
    return hnd;
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Open shared memory section
// Inputs:
//   szName - buffer name
//   size   - shared segment size to create
HANDLE CreateSharedBuffer(LPCSTR szName, UINT32 size)
{
    HANDLE hnd = NULL;
#ifdef WIN32
    // Keep windows version unchanged
    hnd = CreateFileMapping((HANDLE)INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, szName);
#else
    QString strKey(szName);
    QSharedMemory * pHnd = new QSharedMemory(strKey);
    if (pHnd)
    {
        if (pHnd->create(size))
            hnd = pHnd;
        // Reattach: This might happen as a result of previous crash
        else if (pHnd->attach(QSharedMemory::ReadWrite))
            hnd = pHnd;
        else
            delete pHnd;
    }
#endif
    return hnd;
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Get access to shared memory section data
// Inputs:
//   hnd       - shared memory handle
//   bReadOnly - if should open memory for read-only operation
void * GetSharedBuffer(HANDLE hnd, bool bReadOnly)
{
    void * ret = NULL;
    if (hnd == NULL)
        return ret;
#ifdef WIN32
    // Keep windows version unchanged
    ret = MapViewOfFile(hnd, bReadOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, 0, 0);
#else
    QSharedMemory * pHnd = static_cast<QSharedMemory *>(hnd);
    if (pHnd)
        ret = pHnd->data();
#endif
    return ret;
}

// Purpose: Open library as stand-alone or under given application with thread
//    identifier for multiple threads each with its own IP
// Inputs:
//   bStandAlone - if should open library as stand-alone
//   nInstance     - integer index identifier of library instance (0-based up to cbMAXOPEN-1)
cbRESULT cbOpen(BOOL bStandAlone, UINT32 nInstance)
{
    char buf[64] = {0};
    cbRESULT cbRet;

    if (nInstance >= cbMAXOPEN)
        return cbRESULT_INSTINVALID;

    // Find an empty stub
    UINT32 nIdx = cbMAXOPEN;
    for (int i = 0; i < cbMAXOPEN; ++i)
    {
        if (!cb_library_initialized[i])
        {
            nIdx = i;
            break;
        }
    }
    if (nIdx >= cbMAXOPEN)
        return cbRESULT_LIBINITERROR;

    char szLockName[64] = {0};
    if (nInstance == 0)
        _snprintf(szLockName, sizeof(szLockName), "cbCentralAppMutex");
    else
        _snprintf(szLockName, sizeof(szLockName), "cbCentralAppMutex%d", nInstance);

    // If it is stand alone application
    if (bStandAlone)
    {
        // Aquire lock
        cbRet = cbAquireSystemLock(szLockName, cb_sys_lock_hnd[nInstance]);
        if (cbRet)
            return cbRet;
        cb_library_index[nInstance] = nIdx;
        // Create the shared memory and synchronization objects
        cbRet = CreateSharedObjects(nInstance);
        // Library initialized if the objects are created
        if (cbRet == cbRESULT_OK)
        {
            cb_library_initialized[nIdx] = TRUE;      // We are in the library, so it is initialized
        }
        return cbRet;
    } else {
        // Check if mutex is locked
        cbRet = cbCheckApp(szLockName);
        if (cbRet == cbRESULT_NOCENTRALAPP)
            return cbRet;
    }

    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", REC_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", REC_BUF_NAME, nInstance);
    // Create the shared neuromatic receive buffer, if unsuccessful, return FALSE
    cb_rec_buffer_hnd[nIdx] = OpenSharedBuffer(buf, true);
    cb_rec_buffer_ptr[nIdx] = (cbRECBUFF*)GetSharedBuffer(cb_rec_buffer_hnd[nIdx], true);
    if (cb_rec_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }


    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", GLOBAL_XMT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", GLOBAL_XMT_NAME, nInstance);            
    // Create the shared global transmit buffer; if unsuccessful, release rec buffer and return FALSE
    cb_xmt_global_buffer_hnd[nIdx] = OpenSharedBuffer(buf, false);;
    cb_xmt_global_buffer_ptr[nIdx] = (cbXMTBUFF*)GetSharedBuffer(cb_xmt_global_buffer_hnd[nIdx], false);
    if (cb_xmt_global_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }

    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", LOCAL_XMT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", LOCAL_XMT_NAME, nInstance);
    // Create the shared local transmit buffer; if unsuccessful, release rec buffer and return FALSE
    cb_xmt_local_buffer_hnd[nIdx] = OpenSharedBuffer(buf, false);;
    cb_xmt_local_buffer_ptr[nIdx] = (cbXMTBUFF*)GetSharedBuffer(cb_xmt_local_buffer_hnd[nIdx], false);
    if (cb_xmt_local_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }

    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", CFG_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", CFG_BUF_NAME, nInstance);
    // Create the shared neuromatic configuration buffer; if unsuccessful, release rec buffer and return FALSE
    cb_cfg_buffer_hnd[nIdx] = OpenSharedBuffer(buf, true);;
    cb_cfg_buffer_ptr[nIdx] = (cbCFGBUFF*)GetSharedBuffer(cb_cfg_buffer_hnd[nIdx], true);
    if (cb_cfg_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }

    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", STATUS_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", STATUS_BUF_NAME, nInstance);            
    // Create the shared pc status buffer; if unsuccessful, release rec buffer and return FALSE
    cb_pc_status_buffer_hnd[nIdx] = OpenSharedBuffer(buf, false);;
    cb_pc_status_buffer_ptr[nIdx] = (cbPcStatus*)GetSharedBuffer(cb_pc_status_buffer_hnd[nIdx], false);
    if (cb_pc_status_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }

    // Create the shared spike buffer
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SPK_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SPK_BUF_NAME, nInstance);            
    cb_spk_buffer_hnd[nIdx] = OpenSharedBuffer(buf, false);;
    cb_spk_buffer_ptr[nIdx] = (cbSPKBUFF*)GetSharedBuffer(cb_spk_buffer_hnd[nIdx], false);
    if (cb_spk_buffer_ptr[nIdx] == NULL) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }

    // open the data availability signals
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SIG_EVT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SIG_EVT_NAME, nInstance);            
#ifdef WIN32
    cb_sig_event_hnd[nIdx] = OpenEvent(SYNCHRONIZE, TRUE, buf);
    if (cb_sig_event_hnd[nIdx] == NULL)  {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }
#else
    sem_t *sem = sem_open(buf, 0);
    if (sem == SEM_FAILED) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }
    cb_sig_event_hnd[nIdx] = sem;
#endif

    cb_library_index[nInstance] = nIdx;
    cb_library_initialized[nIdx] = TRUE;

    // Initialize read indices to the current head position
    cbMakePacketReadingBeginNow(nInstance);

    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar   29 April 2012
// Purpose: Find if a Cerebus app instance is running
// Inputs:
//   lpName - name of the Cerebus application mutex
// Outputs:
//  Returns cbRESULT_NOCENTRALAPP if application is not running, otherwise cbRESULT_OK
cbRESULT cbCheckApp(const char * lpName)
{
    cbRESULT cbRet = cbRESULT_OK;
    if (lpName == NULL)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    // Test for availability of central application by attempting to open/close Central App Mutex
    HANDLE hCentralMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, lpName);
    CloseHandle(hCentralMutex);
    if (hCentralMutex == NULL)
        cbRet = cbRESULT_NOCENTRALAPP;
#else
    {
        char szLockName[64] = {0};
        char * szTmpDir = getenv("TMPDIR");
        _snprintf(szLockName, sizeof(szLockName), "%s/%s.lock", szTmpDir == NULL ? "/tmp" : szTmpDir, lpName);
        FILE * pflock = fopen(szLockName, "w+");
        if (pflock == NULL)
            cbRet = cbRESULT_OK; // Assume root has the lock
        else if (flock(fileno(pflock), LOCK_EX | LOCK_NB) == 0)
            cbRet = cbRESULT_NOCENTRALAPP;
        if (pflock != NULL)
        {
            fclose(pflock);
            if (cbRet == cbRESULT_NOCENTRALAPP)
                remove(szLockName);
        }
    }
#endif
    return cbRet;
}

// Author & Date:   Ehsan Azar   29 Oct 2012
// Purpose: Aquire a system mutex for Cerebus application
// Inputs:
//   lpName - name of the Cerebus application mutex
// Outputs:
//   hLock  - the handle to newly created system lock
//  Returns the error code (cbRESULT_OK if successful)
cbRESULT cbAquireSystemLock(const char * lpName, HANDLE & hLock)
{
    if (lpName == NULL)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    // Try creating the system mutex
    HANDLE hMutex = CreateMutex(NULL, TRUE, lpName);
    if (hMutex == 0 || GetLastError() == ERROR_ACCESS_DENIED || GetLastError() == ERROR_ALREADY_EXISTS)
        return cbRESULT_SYSLOCK;
    hLock = hMutex;
#else
    // There are other methods such as sharedmemory but they break if application crash
    //  only a file lock seems resilient to crash, also with tmp mounted as tmpfs this is as fast as it could be
    char szLockName[64] = {0};
    char * szTmpDir = getenv("TMPDIR");
    _snprintf(szLockName, sizeof(szLockName), "%s/%s.lock", szTmpDir == NULL ? "/tmp" : szTmpDir, lpName);
    FILE * pflock = fopen(szLockName, "w+");
    if (pflock == NULL)
        return cbRESULT_SYSLOCK;
    if (flock(fileno(pflock), LOCK_EX | LOCK_NB) != 0)
    {
        return cbRESULT_SYSLOCK;
    }
    fprintf(pflock, "%u", (UINT32)getppid());
    hLock = (void *)pflock;
#endif
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar   29 Oct 2012
// Purpose: Release system mutex
// Inputs:
//   lpName - name of the Cerebus application mutex
//   hLock - System mutex handle
// Outputs:
//  Returns the error code (cbRESULT_OK if successful)
cbRESULT cbReleaseSystemLock(const char * lpName, HANDLE & hLock)
{
    if (lpName == NULL)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    if (CloseHandle(hLock) == 0)
        return cbRESULT_SYSLOCK;
#else
    if (hLock)
    {
        FILE * pflock = (FILE *)hLock;
        fclose(pflock); // Close mutex
        remove(lpName); // Cleanup
    }
#endif
    hLock = NULL;
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar   15 March 2010
// Purpose: get instrument information.
//            If hardware is offline, detect local instrument if any
// Outputs:
//  instInfo - combination of cbINSTINFO_*
//  cbRESULT_OK if successful
cbRESULT cbGetInstInfo(UINT32 *instInfo, UINT32 nInstance)
{
    *instInfo = 0;
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    int type = cbINSTINFO_NPLAY;
    cbPROCINFO isInfo;
    if (cbGetProcInfo(cbNSP1, &isInfo, nInstance) == cbRESULT_OK)
    {
        if (strstr(isInfo.ident, "Cereplex") != NULL)
            type = cbINSTINFO_CEREPLEX;
        else if (strstr(isInfo.ident, "Emulator") != NULL)
            type = cbINSTINFO_EMULATOR;
    }

    if (cbCheckApp("cbNPlayMutex") == cbRESULT_OK)
    {
        *instInfo |= cbINSTINFO_LOCAL; // Local
        *instInfo |= type;
    }

    UINT32 flags = 0;
    // If online get more info (this will detect remote instruments)
    if (cbGetNplay(0, 0, &flags, 0, 0, 0, 0, nInstance) == cbRESULT_OK)
    {
        if (flags != cbNPLAY_FLAG_NONE) // If nPlay is running
            *instInfo |= type;
    }

    // Sysinfo is the last config packet
    if (cb_cfg_buffer_ptr[nIdx]->sysinfo.chid == 0)
        return cbRESULT_HARDWAREOFFLINE;

    // If this is reached instrument is ready and known
    *instInfo |= cbINSTINFO_READY;

    return cbRESULT_OK;
}

// Author & Date:       Ehsan Azar     30 Aug 2012
// Purpose: get NSP latency based on its version
//           this is latency between tail and irq output
// Outputs:
//  nLatency - the latency (in number of samples)
//  cbRESULT_OK if successful
cbRESULT cbGetLatency(UINT32 *nLatency, UINT32 nInstance)
{
    *nLatency = 0;
    cbPROCINFO isInfo;
    memset(&isInfo, 0, sizeof(cbPROCINFO));
    cbRESULT res = cbGetProcInfo(cbNSP1, &isInfo, nInstance);
    if (res)
        return res;

    union _NSP_VERSION
    {
        UINT32 dwComplete;
        struct
        {
            BYTE byMajor;       // The major version - must match Central
            BYTE byMinor;       // The minor version - must match Central
            BYTE byRelease;     // the current release - interim releases
            BYTE byBeta;        // Beta number will only work with same beta of Central
        };
    } ver;

    ver.dwComplete = isInfo.idcode;
    // Linearize the version for easy compare
    UINT32 nVersion = (UINT32)ver.byBeta + ((UINT32)ver.byRelease << 8) + ((UINT32)ver.byMinor << 16) + ((UINT32)ver.byMajor << 24);

    UINT32 spikelen;
    res = cbGetSpikeLength(&spikelen, NULL, NULL, nInstance);
    if (res)
        return res;

    // Version 6.03.02.00 is the first to use (2 * spikelen + 16) total latency (between irq-in and irq-out)
    if (nVersion < 0x06030200)
    {
        // This is the difference between 600 and (2 * spikelen + 6)
        *nLatency = 600 - (2 * spikelen + 6);
    }
    else 
    {
        // This is the difference between (2 * spikelen + 16) and (2 * spikelen + 6)
        *nLatency = 10;
    }
    return cbRESULT_OK;
}

cbRESULT cbMakePacketReadingBeginNow(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];
    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Initialize read indices to the current head position
    cb_recbuff_tailwrap[nIdx]  = cb_rec_buffer_ptr[nIdx]->headwrap;
    cb_recbuff_tailindex[nIdx] = cb_rec_buffer_ptr[nIdx]->headindex;
    cb_recbuff_processed[nIdx] = cb_rec_buffer_ptr[nIdx]->received;
    cb_recbuff_lasttime[nIdx]  = cb_rec_buffer_ptr[nIdx]->lasttime;

    return cbRESULT_OK;
}

cbRESULT cbClose(BOOL bStandAlone, UINT32 nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return cbRESULT_INSTINVALID;
    UINT32 nIdx = cb_library_index[nInstance];

    // clear lib init flag variable
    cb_library_initialized[nIdx] = FALSE;

    // destroy the shared neuromatic memory and synchronization objects
    DestroySharedObjects(bStandAlone, nInstance);
    // If it is stand alone application
    if (bStandAlone)
    {
        char buf[64] = {0};
        if (nInstance == 0)
            _snprintf(buf, sizeof(buf), "cbCentralAppMutex");
        else
            _snprintf(buf, sizeof(buf), "cbCentralAppMutex%d", nInstance);
        if (cb_sys_lock_hnd[nInstance])
            cbReleaseSystemLock(buf, cb_sys_lock_hnd[nInstance]);
        return cbRESULT_OK;
    }

    return cbRESULT_OK;
}


// Purpose: Processor Inquiry Function
//
cbRESULT cbGetProcInfo(UINT32 proc, cbPROCINFO *procinfo, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;
    if (!cb_cfg_buffer_ptr[nIdx]) return cbRESULT_NOLIBRARY;
    if (cb_cfg_buffer_ptr[nIdx]->procinfo[proc - 1].chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (procinfo) memcpy(procinfo, &(cb_cfg_buffer_ptr[nIdx]->procinfo[proc-1].idcode), sizeof(cbPROCINFO));
    return cbRESULT_OK;
}


// Purpose: Bank Inquiry Function
//
cbRESULT cbGetBankInfo(UINT32 proc, UINT32 bank, cbBANKINFO *bankinfo, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS) || ((bank - 1) >= cbMAXBANKS)) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->bankinfo[proc - 1][bank - 1].chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (bankinfo) memcpy(bankinfo, &(cb_cfg_buffer_ptr[nIdx]->bankinfo[proc - 1][bank - 1].idcode), sizeof(cbBANKINFO));
    return cbRESULT_OK;
}


// Purpose:
// Retreives the total number of channels in the system
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
cbRESULT cbGetChanCount(UINT32 *count, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Sweep through the processor information banks and sum up the number of channels
    *count = 0;
    for(int p = 0; p < cbMAXPROCS; p++)
        if (cb_cfg_buffer_ptr[nIdx]->procinfo[p].chid) *count += cb_cfg_buffer_ptr[nIdx]->procinfo[p].chancount;

    return cbRESULT_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Systemwide Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////


cbRESULT cbSendPacket(void * pPacket, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_GENERIC * pPkt = static_cast<cbPKT_GENERIC*>(pPacket);

    // The logic here is quite complicated. Data is filled in from other processes
    // in a 2 pass mode. First they fill all except they skip the first 4 bytes.
    // The final step in the process is to convert the 1st dword from "0" to some other number.
    // This step is done in a thread-safe manner
    // Consequently, all packets can not have "0" as the first DWORD. At the time of writing,
    // We were looking at the "time" value of a packet.

    pPkt->time = cb_rec_buffer_ptr[nIdx]->lasttime;

    // Time cannot be equal to 0
    if (pPkt->time == 0)
        pPkt->time = 1;

    ASSERT( *(static_cast<DWORD const *>(pPacket)) != 0);


    UINT32 quadletcount = pPkt->dlen + cbPKT_HEADER_32SIZE;
    UINT32 orig_headindex;

    // give 3 attempts to claim a spot in the circular xmt buffer for the packet
    int insertionattempts = 0;
    static const int NUM_INSERTION_ATTEMPTS = 3;
    while (TRUE)
    {
        bool bTryToFill = false;        // Should I try to stuff data into the queue, or is it filled
                                        // TRUE = go ahead a try; FALSE = wait a bit

        // Copy the current buffer indices
        // NOTE: may want to use a 64-bit transfer to atomically get indices.  Otherwise, grab
        // tail index first to get "worst case" scenario;
        UINT32 orig_tailindex = cb_xmt_global_buffer_ptr[nIdx]->tailindex;
        orig_headindex = cb_xmt_global_buffer_ptr[nIdx]->headindex;

        // Compute the new head index at after the target packet position.
        // If the new head index is within (cbCER_UDP_SIZE_MAX / 4) quadlets of the buffer end, wrap it around.
        // Also, make sure that we are not wrapping around the tail pointer
        UINT32 mod_headindex = orig_headindex + quadletcount;
        UINT32 nLastOKPosition = cb_xmt_global_buffer_ptr[nIdx]->last_valid_index;
        if (mod_headindex > nLastOKPosition)
        {
            // time to wrap around in the circular buffer....
            mod_headindex = 0;

            // Is there room?
            if (mod_headindex < orig_tailindex)
                bTryToFill = true;
        }
        else if (orig_tailindex > orig_headindex)   // wrapped recently, but not yet caught up?
        {
            // Is there room?
            if (mod_headindex < orig_tailindex)  // Do I have space to continue?
                bTryToFill = true;
        }

        else    // no wrapping at all...and plenty of room
        {
            bTryToFill = true;
        }


        if (bTryToFill)
        {
            // attempt to atomically update the head pointer and verify that the head pointer
            // was not changed since orig_head_index was loaded and calculated upon
            LONG * pDest = (LONG *) &cb_xmt_global_buffer_ptr[nIdx]->headindex;
            LONG  dwExch = (LONG) mod_headindex;
            LONG  dwComp = (LONG) orig_headindex;
#ifdef WIN32
            if (InterlockedCompareExchange(pDest, dwExch, dwComp) == dwComp)
                break;
#else
            if (__sync_bool_compare_and_swap(pDest, dwComp, dwExch))
                break;
#endif
                // NORMAL EXIT
        }


        // check to see if we should give up or not
        if ((++insertionattempts) >= NUM_INSERTION_ATTEMPTS)
        {
            return cbRESULT_MEMORYUNAVAIL;
        }

        // Sleep for a bit to let buffers clear and try again
        Sleep(10);
    }

    // Copy all but the first 4 bytes of the packet to the target packet location.
    // The Central App will not transmit the packet while the first UINT32 is zero.
    memcpy(&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex+1]), ((UINT32*)pPacket)+1, ((quadletcount-1)<<2) );

    // atomically copy the first 4 bytes of the packet
#ifdef WIN32
    InterlockedExchange((LPLONG)&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#else
    __sync_lock_test_and_set((LPLONG)&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#endif
    return cbRESULT_OK;
}


// Author & Date:   Kirk Korver     17 Jun 2003
// Purpose: send a packet only to ourselves (think IPC)
cbRESULT cbSendLoopbackPacket(void * pPacket, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_GENERIC * pPkt = static_cast<cbPKT_GENERIC*>(pPacket);

    // The logic here is quite complicated. Data is filled in from other processes
    // in a 2 pass mode. First they fill all except they skip the first 4 bytes.
    // The final step in the process is to convert the 1st dword from "0" to some other number.
    // This step is done in a thread-safe manner
    // Consequently, all packets can not have "0" as the first DWORD. At the time of writing,
    // We were looking at the "time" value of a packet.

    pPkt->time = cb_rec_buffer_ptr[nIdx]->lasttime;

    // Time cannot be equal to 0
    if (pPkt->time == 0)
        pPkt->time = 1;

    ASSERT( *(static_cast<DWORD const *>(pPacket)) != 0);

    UINT32 quadletcount = pPkt->dlen + cbPKT_HEADER_32SIZE;
    UINT32 orig_headindex;

    // give 3 attempts to claim a spot in the circular xmt buffer for the packet
    int insertionattempts = 0;
    static const int NUM_INSERTION_ATTEMPTS = 3;
    for (;;)
    {
        bool bTryToFill = false;        // Should I try to stuff data into the queue, or is it filled
                                        // TRUE = go ahead a try; FALSE = wait a bit

        // Copy the current buffer indices
        // NOTE: may want to use a 64-bit transfer to atomically get indices.  Otherwise, grab
        // tail index first to get "worst case" scenario;
        UINT32 orig_tailindex = cb_xmt_local_buffer_ptr[nIdx]->tailindex;
        orig_headindex = cb_xmt_local_buffer_ptr[nIdx]->headindex;

        // Compute the new head index at after the target packet position.
        // If the new head index is within (cbCER_UDP_SIZE_MAX / 4) quadlets of the buffer end, wrap it around.
        // Also, make sure that we are not wrapping around the tail pointer, if we are, next if will get it
        UINT32 mod_headindex = orig_headindex + quadletcount;
        UINT32 nLastOKPosition = cb_xmt_local_buffer_ptr[nIdx]->last_valid_index;
        if (mod_headindex > nLastOKPosition)
        {
            // time to wrap around in the circular buffer....
            mod_headindex = 0;

            // Is there room?
            if (mod_headindex < orig_tailindex)
                bTryToFill = true;
        }
        else if (orig_tailindex > orig_headindex)   // wrapped recently, but not yet caught up?
        {
            // Is there room?
            if (mod_headindex < orig_tailindex)  // Do I have space to continue?
                bTryToFill = true;
        }

        else    // no wrapping at all...and plenty of room
        {
            bTryToFill = true;
        }


        if (bTryToFill)
        {
            // attempt to atomically update the head pointer and verify that the head pointer
            // was not changed since orig_head_index was loaded and calculated upon
            LONG * pDest = (LONG*) &cb_xmt_local_buffer_ptr[nIdx]->headindex;
            LONG dwExch = (LONG) mod_headindex;
            LONG dwComp = (LONG) orig_headindex;
#ifdef WIN32
            if (InterlockedCompareExchange( pDest, dwExch, dwComp ) == dwComp)
                break;
#else
            if (__sync_bool_compare_and_swap(pDest, dwComp, dwExch))
                break;
#endif
                // NORMAL EXIT
        }

        // check to see if we should give up or not
        if ((++insertionattempts) >= NUM_INSERTION_ATTEMPTS)
            return cbRESULT_MEMORYUNAVAIL;

        // Sleep for a bit to let buffers clear and try again
        Sleep(10);
    }

    // Copy all but the first 4 bytes of the packet to the target packet location.
    // The Central App will not transmit the packet while the first UINT32 is zero.
    memcpy(&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex+1]), ((UINT32*)pPacket)+1, ((quadletcount-1)<<2) );

    // atomically copy the first 4 bytes of the packet
#ifdef WIN32
    InterlockedExchange((LPLONG)&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#else
    __sync_lock_test_and_set((LPLONG)&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#endif

    return cbRESULT_OK;
}



cbRESULT cbGetSystemRunLevel(UINT32 *runlevel, UINT32 *runflags, UINT32 *resetque, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached packet available and initialized
    if ((cb_cfg_buffer_ptr[nIdx]->sysinfo.chid == 0)||(cb_cfg_buffer_ptr[nIdx]->sysinfo.runlevel == 0))
    {
        if (runlevel) *runlevel=0;
        if (resetque) *resetque=0;
        if (runflags) *runflags=0;
        return cbRESULT_HARDWAREOFFLINE;
    }

    // otherwise, return the data
    if (runlevel) *runlevel = cb_cfg_buffer_ptr[nIdx]->sysinfo.runlevel;
    if (resetque) *resetque = cb_cfg_buffer_ptr[nIdx]->sysinfo.resetque;
    if (runflags) *runflags = cb_cfg_buffer_ptr[nIdx]->sysinfo.runflags;
    return cbRESULT_OK;
}


cbRESULT cbSetSystemRunLevel(UINT32 runlevel, UINT32 runflags, UINT32 resetque, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_SYSINFO sysinfo;
    sysinfo.time     = cb_rec_buffer_ptr[nIdx]->lasttime;
    sysinfo.chid     = 0x8000;
    sysinfo.type     = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.dlen     = cbPKTDLEN_SYSINFO;
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = runflags;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&sysinfo, nInstance);
}


cbRESULT cbGetSystemClockFreq(UINT32 *freq, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // hardwire the rate to 30ksps
    *freq = 30000;

    return cbRESULT_OK;
}


cbRESULT cbGetSystemClockTime(UINT32 *time, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *time = cb_rec_buffer_ptr[nIdx]->lasttime;

    return cbRESULT_OK;
}

cbRESULT cbSetComment(UINT8 charset, UINT8 flags, UINT32 data, const char * comment, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_COMMENT pktComment;
    memset(&pktComment, 0, sizeof(pktComment));
    pktComment.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktComment.chid           = 0x8000;
    pktComment.type           = cbPKTTYPE_COMMENTSET;
    pktComment.info.charset   = charset;
    pktComment.info.flags     = flags;
    pktComment.data           = data;
    UINT32 nLen = 0;
    if (comment)
        strncpy(pktComment.comment, comment, cbMAX_COMMENT);
    pktComment.comment[cbMAX_COMMENT - 1] = 0;
    nLen = (UINT32)strlen(pktComment.comment);
    pktComment.dlen           = (UINT32)cbPKTDLEN_COMMENTSHORT + (nLen + 3) / 4;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&pktComment, nInstance);
}

cbRESULT cbGetLncParameters(UINT32 *nLncFreq, UINT32 *nLncRefChan, UINT32 *nLncGMode, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isLnc.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (nLncFreq)     *nLncFreq     = cb_cfg_buffer_ptr[nIdx]->isLnc.lncFreq;
    if (nLncRefChan)  *nLncRefChan  = cb_cfg_buffer_ptr[nIdx]->isLnc.lncRefChan;
    if (nLncGMode)    *nLncGMode    = cb_cfg_buffer_ptr[nIdx]->isLnc.lncGlobalMode;
    return cbRESULT_OK;
}


cbRESULT cbSetLncParameters(UINT32 nLncFreq, UINT32 nLncRefChan, UINT32 nLncGMode, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_LNC pktLnc;
    pktLnc.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktLnc.chid           = 0x8000;
    pktLnc.type           = cbPKTTYPE_LNCSET;
    pktLnc.dlen           = cbPKTDLEN_LNC;
    pktLnc.lncFreq        = nLncFreq;
    pktLnc.lncRefChan     = nLncRefChan;
    pktLnc.lncGlobalMode  = nLncGMode;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&pktLnc, nInstance);
}

cbRESULT cbGetNplay(char *fname, float *speed, UINT32 *flags, UINT32 *ftime, UINT32 *stime, UINT32 *etime, UINT32 * filever, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isNPlay.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (fname)     strncpy(fname, cb_cfg_buffer_ptr[nIdx]->isNPlay.fname, cbNPLAY_FNAME_LEN);
    if (speed)     *speed   = cb_cfg_buffer_ptr[nIdx]->isNPlay.speed;
    if (flags)     *flags   = cb_cfg_buffer_ptr[nIdx]->isNPlay.flags;
    if (ftime)     *ftime   = cb_cfg_buffer_ptr[nIdx]->isNPlay.ftime;
    if (stime)     *stime   = cb_cfg_buffer_ptr[nIdx]->isNPlay.stime;
    if (etime)     *etime   = cb_cfg_buffer_ptr[nIdx]->isNPlay.etime;
    if (filever)   *filever = cb_cfg_buffer_ptr[nIdx]->isNPlay.val;
    return cbRESULT_OK;
}

cbRESULT cbSetNplay(const char *fname, float speed, UINT32 mode, UINT32 val, UINT32 stime, UINT32 etime, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isNPlay.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // Create the packet data structure and fill it in
    cbPKT_NPLAY pktNPlay;
    pktNPlay.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktNPlay.chid           = 0x8000;
    pktNPlay.type           = cbPKTTYPE_NPLAYSET;
    pktNPlay.dlen           = cbPKTDLEN_NPLAY;
    pktNPlay.speed          = speed;
    pktNPlay.mode           = mode;
    pktNPlay.flags          = cbNPLAY_FLAG_NONE; // No flags here
    pktNPlay.val            = val;
    pktNPlay.stime          = stime;
    pktNPlay.etime          = etime;
    pktNPlay.fname[0]       = 0;
    if (fname) strncpy(pktNPlay.fname, fname, cbNPLAY_FNAME_LEN);

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&pktNPlay, nInstance);
}

// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   id - video source ID (1 to cbMAXVIDEOSOURCE)
// Outputs:
//   name - name of the video source
//   fps  - the frame rate of the video source
cbRESULT cbGetVideoSource(char *name, float *fps, UINT32 id, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    if ((id - 1) >= cbMAXVIDEOSOURCE) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->isVideoSource[id - 1].fps == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (name)     strncpy_s(name, cbLEN_STR_LABEL, cb_cfg_buffer_ptr[nIdx]->isVideoSource[id - 1].name, cbLEN_STR_LABEL-1);
    if (fps)     *fps  = cb_cfg_buffer_ptr[nIdx]->isVideoSource[id - 1].fps;

    return cbRESULT_OK;
}

// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   id - trackable object ID (1 to cbMAXTRACKOBJ)
// Outputs:
//   name - name of the video source
//   type - type of the trackable object (start from 0)
//   pointCount - the maximum number of points for this trackable
cbRESULT cbGetTrackObj(char *name, UINT16 *type, UINT16 *pointCount, UINT32 id, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    if ((id - 1) >= cbMAXTRACKOBJ) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->isTrackObj[id - 1].type == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (name)     strncpy_s(name, cbLEN_STR_LABEL, cb_cfg_buffer_ptr[nIdx]->isTrackObj[id - 1].name, cbLEN_STR_LABEL - 1);
    if (type)    *type  = cb_cfg_buffer_ptr[nIdx]->isTrackObj[id-1].type;
    if (pointCount) *pointCount = cb_cfg_buffer_ptr[nIdx]->isTrackObj[id-1].pointCount;

    return cbRESULT_OK;
}


// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   vs   - video source ID (start from 0)
//   name - name of the video source
//   fps  - the frame rate of the video source
cbRESULT cbSetVideoSource(const char *name, float fps, UINT32 vs, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_NM pktNm;
    memset(&pktNm, 0, sizeof(pktNm));
    pktNm.chid = cbPKTCHAN_CONFIGURATION;
    pktNm.type = cbPKTTYPE_NMSET;
    pktNm.dlen = cbPKTDLEN_NM;
    pktNm.mode = cbNM_MODE_SETVIDEOSOURCE;
    pktNm.flags = vs + 1;
    pktNm.value = UINT32(fps * 1000); // frame rate times 1000
    strncpy(pktNm.name, name, cbLEN_STR_LABEL);

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&pktNm, nInstance);
}

// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   id - trackable object ID (start from 0)
//   name - name of the video source
//   type - type of the trackable object (start from 0)
//   pointCount - the maximum number of points for this trackable object
cbRESULT cbSetTrackObj(const char *name, UINT16 type, UINT16 pointCount, UINT32 id, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_NM pktNm;
    memset(&pktNm, 0, sizeof(pktNm));
    pktNm.chid = cbPKTCHAN_CONFIGURATION;
    pktNm.type = cbPKTTYPE_NMSET;
    pktNm.dlen = cbPKTDLEN_NM;
    pktNm.mode = cbNM_MODE_SETTRACKABLE;
    pktNm.flags = id + 1;
    pktNm.value = type | (pointCount << 16);
    strncpy(pktNm.name, name, cbLEN_STR_LABEL);

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&pktNm, nInstance);
}

cbRESULT cbGetSpikeLength(UINT32 *length, UINT32 *pretrig, UINT32 * pSysfreq, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->sysinfo.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (length)      *length      = cb_cfg_buffer_ptr[nIdx]->sysinfo.spikelen;
    if (pretrig)     *pretrig     = cb_cfg_buffer_ptr[nIdx]->sysinfo.spikepre;
    if (pSysfreq)    *pSysfreq    = cb_cfg_buffer_ptr[nIdx]->sysinfo.sysfreq;
    return cbRESULT_OK;
}


cbRESULT cbSetSpikeLength(UINT32 length, UINT32 pretrig, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_SYSINFO sysinfo;
    sysinfo.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    sysinfo.chid        = 0x8000;
    sysinfo.type        = cbPKTTYPE_SYSSETSPKLEN;
    sysinfo.dlen        = cbPKTDLEN_SYSINFO;
    sysinfo.spikelen    = length;
    sysinfo.spikepre    = pretrig;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&sysinfo, nInstance);
}

// Author & Date: Ehsan Azar       16 Jan 2012
// Inputs:
//   channel  - 1-based channel number
//   mode     - waveform type
//   repeats  - number of repeats
//   trig     - trigget type
//   trigChan - Channel for trigger
//   wave     - waveform data
cbRESULT cbGetAoutWaveform(UINT32 channel, UINT8  trigNum, UINT16  * mode, UINT32  * repeats, UINT16  * trig,
                           UINT16  * trigChan, UINT16  * trigValue, cbWaveformData * wave, UINT32 nInstance)
{
#if 0
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].aoutcaps & cbAOUT_WAVEFORM)) return cbRESULT_INVALIDFUNCTION;
    if (trigNum >= cbMAX_AOUT_TRIGGER) return cbRESULT_INVALIDFUNCTION;
    if (channel <= cbFIRST_ANAOUT_CHAN) return cbRESULT_INVALIDCHANNEL;
    channel -= (cbFIRST_ANAOUT_CHAN + 1); // make it 0-based
    if (channel >= AOUT_NUM_GAIN_CHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].type == 0) return cbRESULT_INVALIDCHANNEL;

    // otherwise, return the data
    if (mode)     *mode = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].mode;
    if (repeats)  *repeats = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].repeats;
    if (trig)     *trig = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trig;
    if (trigChan) *trigChan = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trigChan;
    if (trigValue) *trigValue = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trigValue;
    if (wave)     *wave = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].wave;
#endif
    return cbRESULT_OK;
}

cbRESULT cbGetFilterDesc(UINT32 proc, UINT32 filt, cbFILTDESC *filtdesc, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;
    if ((filt-1) >= cbMAXFILTS) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->filtinfo[proc-1][filt-1].chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    memcpy(filtdesc,&(cb_cfg_buffer_ptr[nIdx]->filtinfo[proc-1][filt-1].label[0]),sizeof(cbFILTDESC));
    return cbRESULT_OK;
}

cbRESULT cbGetFileInfo(cbPKT_FILECFG * filecfg, UINT32 nInstance)
{
#if 0
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if (cb_cfg_buffer_ptr[nIdx]->fileinfo.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (filecfg) *filecfg = cb_cfg_buffer_ptr[nIdx]->fileinfo;
#endif
    return cbRESULT_OK;
}

cbRESULT cbGetSampleGroupInfo( UINT32 proc, UINT32 group, char *label, UINT32 *period, UINT32* length, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS)||((group - 1) >= cbMAXGROUPS)) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (label)  memcpy(label,cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].label, cbLEN_STR_LABEL);
    if (period) *period = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].period;
    if (length) *length = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].length;
    return cbRESULT_OK;
}


cbRESULT cbGetSampleGroupList( UINT32 proc, UINT32 group, UINT32 *length, UINT32 *list, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS)||((group - 1) >= cbMAXGROUPS))
        return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].chid == 0)
        return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (length)
        *length = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].length;

    if (list)
        memcpy(list,&(cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].list[0]),
                    cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].length * 4);

    return cbRESULT_OK;
}

cbRESULT cbGetChanLoc(UINT32 chan, UINT32 *proc, UINT32 *bank, char *banklabel, UINT32 *term, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that requested procinfo structure is not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    UINT32 nProcessor = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].proc;
    UINT32 nBank = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].bank;

    // Return the requested data from the rec buffer
    if (proc) *proc = nProcessor;       // 1 based
    if (bank) *bank = nBank;            // 1 based
    if (banklabel)
        memcpy(banklabel,
               cb_cfg_buffer_ptr[nIdx]->bankinfo[nProcessor-1][nBank-1].label,
               cbLEN_STR_LABEL);

    if (term)
        *term = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].term;

    return cbRESULT_OK;
}

cbRESULT cbGetChanCaps(UINT32 chan, UINT32 *chancaps, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (chancaps) *chancaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps;

    return cbRESULT_OK;
}

cbRESULT cbGetChanLabel( UINT32 chan, char *label, UINT32 *userflags, INT32 *position, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (label) memcpy(label,cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].label, cbLEN_STR_LABEL);
    if (userflags) *userflags = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].userflags;
    if (position) memcpy(position,&(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].position[0]),4*sizeof(INT32));

    return cbRESULT_OK;
}

cbRESULT cbSetChanLabel(UINT32 chan, const char *label, UINT32 userflags, INT32 *position, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETLABEL;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    memcpy(chaninfo.label, label, cbLEN_STR_LABEL);
    chaninfo.userflags = userflags;
    if (position) 
        memcpy(&chaninfo.position, position, 4 * sizeof(INT32));
    else 
        memcpy(&chaninfo.position, &(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].position[0]), 4 * sizeof(INT32));

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetChanUnitMapping(UINT32 chan, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (unitmapping) 
        memcpy(unitmapping, &cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].unitmapping[0], cbMAXUNITS * sizeof(cbMANUALUNITMAPPING));

    return cbRESULT_OK;
}


cbRESULT cbSetChanUnitMapping(UINT32 chan, cbMANUALUNITMAPPING *unitmapping, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // if null pointer, nothing to do so return cbRESULT_OK
    if (!unitmapping) return cbRESULT_OK;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETUNITOVERRIDES;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    if (unitmapping) 
        memcpy(&chaninfo.unitmapping, unitmapping, cbMAXUNITS * sizeof(cbMANUALUNITMAPPING));

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetChanNTrodeGroup(UINT32 chan, UINT32 *NTrodeGroup, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (NTrodeGroup) *NTrodeGroup = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkgroup;

    return cbRESULT_OK;
}


cbRESULT cbSetChanNTrodeGroup( UINT32 chan, const UINT32 NTrodeGroup, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETNTRODEGROUP;
    chaninfo.dlen      = cbPKTDLEN_CHANINFOSHORT;
    chaninfo.chan      = chan;
    chaninfo.spkgroup  = NTrodeGroup;
    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

cbRESULT cbGetChanAmplitudeReject(UINT32 chan, cbAMPLITUDEREJECT *AmplitudeReject, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (AmplitudeReject)
    {
        AmplitudeReject->bEnabled = cbAINPSPK_REJAMPL == (cbAINPSPK_REJAMPL & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts);
        AmplitudeReject->nAmplPos = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].amplrejpos;
        AmplitudeReject->nAmplNeg = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].amplrejneg;
    }

    return cbRESULT_OK;
}


cbRESULT cbSetChanAmplitudeReject(UINT32 chan, const cbAMPLITUDEREJECT AmplitudeReject, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETREJECTAMPLITUDE;
    chaninfo.dlen      = cbPKTDLEN_CHANINFOSHORT;
    chaninfo.chan      = chan;
    chaninfo.spkopts   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts & ~cbAINPSPK_REJAMPL;
    chaninfo.spkopts  |= AmplitudeReject.bEnabled ? cbAINPSPK_REJAMPL : 0;
    chaninfo.amplrejpos = AmplitudeReject.nAmplPos;
    chaninfo.amplrejneg = AmplitudeReject.nAmplNeg;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

cbRESULT cbGetChanAutoThreshold(UINT32 chan, UINT32 *bEnabled, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (bEnabled)
        *bEnabled = (cbAINPSPK_THRAUTO == (cbAINPSPK_THRAUTO & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts));

    return cbRESULT_OK;
}


cbRESULT cbSetChanAutoThreshold( UINT32 chan, const UINT32 bEnabled, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // if null pointer, nothing to do so return cbRESULT_OK
    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETAUTOTHRESHOLD;
    chaninfo.dlen      = cbPKTDLEN_CHANINFOSHORT;
    chaninfo.chan      = chan;
    chaninfo.spkopts   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts & ~cbAINPSPK_THRAUTO;
    chaninfo.spkopts  |= bEnabled ? cbAINPSPK_THRAUTO : 0;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetNTrodeInfo( const UINT32 ntrode, char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], 
                         UINT16 * nSite, UINT16 * chans, UINT16 * fs, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode-1].chid == 0) return cbRESULT_INVALIDNTRODE;

    if (label)  memcpy(label, cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].label, cbLEN_STR_LABEL);
    int n_size = sizeof(cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses);
    if (ellipses) memcpy(ellipses, &cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses[0][0], n_size);
    if (nSite) *nSite = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].nSite;
    if (chans) memcpy(chans, cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].nChan, cbMAXSITES * sizeof(UINT16));
    if (fs) *fs = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs;
    return cbRESULT_OK;
}

cbRESULT cbSetNTrodeInfo( const UINT32 ntrode, const char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], UINT16 fs, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXCHANS/2) return cbRESULT_INVALIDNTRODE;

    // Create the packet data structure and fill it in
    cbPKT_NTRODEINFO ntrodeinfo;
    ntrodeinfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    ntrodeinfo.chid      = 0x8000;
    ntrodeinfo.type      = cbPKTTYPE_SETNTRODEINFO;
    ntrodeinfo.dlen      = cbPKTDLEN_NTRODEINFO;
    ntrodeinfo.ntrode    = ntrode;
    ntrodeinfo.fs        = fs;
    if (label) memcpy(ntrodeinfo.label, label, sizeof(ntrodeinfo.label));
    int size_ell = sizeof(ntrodeinfo.ellipses);
    if (ellipses)
        memcpy(ntrodeinfo.ellipses, ellipses, size_ell);
    else
        memset(ntrodeinfo.ellipses, 0, size_ell);

    return cbSendPacket(&ntrodeinfo, nInstance);
}

// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbGetDinpCaps(UINT32 chan, UINT32 *dinpcaps, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (dinpcaps) *dinpcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].dinpcaps;

    return cbRESULT_OK;
}

// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbGetDinpOptions(UINT32 chan, UINT32 *options, UINT32 *eopchar, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DINP)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].dinpopts;
    if (eopchar) *eopchar = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].eopchar;

    return cbRESULT_OK;
}


// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbSetDinpOptions(UINT32 chan, UINT32 options, UINT32 eopchar, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DINP)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETDINP;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.dinpopts  = options;     // digital input options (composed of nmDINP_* flags)
    chaninfo.eopchar   = eopchar;     // digital input capablities (given by nmDINP_* flags)

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbGetDoutCaps(UINT32 chan, UINT32 *doutcaps, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (doutcaps) *doutcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutcaps;

    return cbRESULT_OK;
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbGetDoutOptions(UINT32 chan, UINT32 *options, UINT32 *monchan, UINT32 *value, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutopts;
    if (monchan) *monchan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource;
    if (value)   *value   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].outvalue;

    return cbRESULT_OK;
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbSetDoutOptions(UINT32 chan, UINT32 options, UINT32 monchan, UINT32 value, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETDOUT;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.doutopts  = options;
    chaninfo.monsource = monchan;
    chaninfo.outvalue  = value;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpCaps( UINT32 chan, UINT32 *ainpcaps, cbSCALING *physcalin, cbFILTDESC *phyfiltin, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (ainpcaps)  *ainpcaps  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps;
    if (physcalin) *physcalin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].physcalin;
    if (phyfiltin) *phyfiltin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].phyfiltin;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpOpts(UINT32 chan, UINT32 *ainpopts, UINT32 *LNCrate, UINT32 *refElecChan, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_LNC)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (ainpopts) *ainpopts = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpopts;
    if (LNCrate) *LNCrate = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].lncrate;
    if (refElecChan) *refElecChan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].refelecchan;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpOpts(UINT32 chan, const UINT32 ainpopts,  UINT32 LNCrate, const UINT32 refElecChan, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_LNC)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time		 = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid		 = 0x8000;
    chaninfo.type		 = cbPKTTYPE_CHANSETAINP;
    chaninfo.dlen		 = cbPKTDLEN_CHANINFOSHORT;
    chaninfo.chan		 = chan;
    chaninfo.ainpopts    = ainpopts;
    chaninfo.lncrate	 = LNCrate;
    chaninfo.refelecchan = refElecChan;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpScaling(UINT32 chan, cbSCALING *scalin, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (scalin)  *scalin  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalin;
    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpScaling(UINT32 chan, cbSCALING *scalin, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETSCALE;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.scalin    = *scalin;
    chaninfo.scalout   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalout;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpDisplay(UINT32 chan, INT32 *smpdispmin, INT32 *smpdispmax, INT32 *spkdispmax, INT32 *lncdispmax, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the cfg buffer
    if (smpdispmin) *smpdispmin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmin;
    if (smpdispmax) *smpdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmax;
    if (spkdispmax) *spkdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkdispmax;
    if (lncdispmax) *lncdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].lncdispmax;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpDisplay( UINT32 chan, INT32 smpdispmin, INT32 smpdispmax, INT32 spkdispmax, INT32 lncdispmax, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETDISP;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    if (smpdispmin) chaninfo.smpdispmin = smpdispmin;
        else chaninfo.smpdispmin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmin;
    if (smpdispmax) chaninfo.smpdispmax = smpdispmax;
        else chaninfo.smpdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmax;
    if (spkdispmax) chaninfo.spkdispmax = spkdispmax;
        else chaninfo.spkdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkdispmax;
    if (lncdispmax) chaninfo.lncdispmax = lncdispmax;
        else chaninfo.lncdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].lncdispmax;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpPreview(UINT32 chan, UINT32 prevopts, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    ASSERT(prevopts == cbAINPPREV_LNC ||
           prevopts == cbAINPPREV_STREAM ||
           prevopts == cbAINPPREV_ALL );

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    //  chan == 0 means a request for ALL possible channels,
    //  the NSP will find the good channels, so we don't have
    //  to worry about the testing
    if (chan != 0)
    {
        if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
        if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
        if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP)) return cbRESULT_INVALIDFUNCTION;
    }

    if ( !
         (prevopts == cbAINPPREV_LNC ||
          prevopts == cbAINPPREV_STREAM ||
          prevopts == cbAINPPREV_ALL ))
    {
        return cbRESULT_INVALIDFUNCTION;
    }

    // Create the packet data structure and fill it in
    cbPKT_GENERIC packet;
    packet.time     = cb_rec_buffer_ptr[nIdx]->lasttime;
    packet.chid     = 0x8000 + chan;
    packet.type     = prevopts;
    packet.dlen     = 0;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&packet, nInstance);
}

// Purpose: AINP Sampling Stream Functions
cbRESULT cbGetAinpSampling(UINT32 chan, UINT32 *filter, UINT32 *group, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SMPSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer for non-null pointers
    if (group)  *group = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpgroup;
    if (filter) *filter = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpfilter;

    return cbRESULT_OK;
}

// Purpose: AINP Sampling Stream Functions
cbRESULT cbSetAinpSampling(UINT32 chan, UINT32 filter, UINT32 group, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SMPSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETSMP;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.smpfilter = filter;
    chaninfo.smpgroup  = group;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeCaps(UINT32 chan, UINT32 *spkcaps, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (spkcaps) *spkcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeOptions(UINT32 chan, UINT32 *options, UINT32 *filter, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts;
    if (filter) *filter = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkfilter;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeOptions(UINT32 chan, UINT32 spkopts, UINT32 spkfilter, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETSPK;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.spkopts   = spkopts;
    chaninfo.spkfilter = spkfilter;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeThreshold(UINT32 chan, INT32 *spkthrlevel, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_EXTRACT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (spkthrlevel) *spkthrlevel = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkthrlevel;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeThreshold(UINT32 chan, INT32 spkthrlevel, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS)
        return cbRESULT_INVALIDCHANNEL;

    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0)
        return cbRESULT_INVALIDCHANNEL;

    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_EXTRACT))
        return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid        = 0x8000;
    chaninfo.type        = cbPKTTYPE_CHANSETSPKTHR;
    chaninfo.dlen        = cbPKTDLEN_CHANINFO;
    chaninfo.chan        = chan;
    chaninfo.spkthrlevel = spkthrlevel;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeHoops(UINT32 chan, cbHOOP *hoops, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_HOOPSORT)) return cbRESULT_INVALIDFUNCTION;

    memcpy(hoops, &(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkhoops[0][0]),
        sizeof(cbHOOP)*cbMAXUNITS*cbMAXHOOPS );

    // Return the requested data from the cfg buffer
    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeHoops(UINT32 chan, cbHOOP *hoops, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_HOOPSORT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid        = 0x8000;
    chaninfo.type        = cbPKTTYPE_CHANSETSPKHPS;
    chaninfo.dlen        = cbPKTDLEN_CHANINFO;
    chaninfo.chan        = chan;

    memcpy(&(chaninfo.spkhoops[0][0]), hoops, sizeof(cbHOOP)*cbMAXUNITS*cbMAXHOOPS );

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

#define  nmAOUTCAP_AUDIO        0x00000001  // Channel is physically optimized for audio output
#define  nmAOUTCAP_STATIC       0x00000002  // Output a static value
#define  nmAOUTCAP_MONITOR      0x00000004  // Monitor an analog signal line
#define  nmAOUTCAP_MONITORGAIN  0x00000008  // Channel can set gain to the channel
#define  nmAOUTCAP_STIMULATE    0x00000010  // Stimulation waveform functions are available.

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbGetAoutCaps( UINT32 chan, UINT32 *aoutcaps, cbSCALING *physcalout, cbFILTDESC *phyfiltout, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (aoutcaps)   *aoutcaps  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].aoutcaps;
    if (physcalout) *physcalout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].physcalout;
    if (phyfiltout) *phyfiltout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].phyfiltout;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbGetAoutScaling(UINT32 chan, cbSCALING *scalout, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (scalout) *scalout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalout;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbSetAoutScaling(UINT32 chan, cbSCALING *scalout, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid      = 0x8000;
    chaninfo.type      = cbPKTTYPE_CHANSETSCALE;
    chaninfo.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = chan;
    chaninfo.scalin    = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalin;
    chaninfo.scalout   = *scalout;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbGetAoutOptions(UINT32 chan, UINT32 *options, UINT32 *monchan, UINT32 *value, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].aoutopts;
    if (monchan) *monchan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource;
    if (value)   *value   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].outvalue;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbSetAoutOptions(UINT32 chan, UINT32 options, UINT32 monchan, UINT32 value, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.time       = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.chid       = 0x8000;
    chaninfo.type       = cbPKTTYPE_CHANSETAOUT;
    chaninfo.dlen       = cbPKTDLEN_CHANINFO;
    chaninfo.chan       = chan;
    chaninfo.aoutopts   = options;
    chaninfo.monsource  = monchan;
    chaninfo.outvalue   = value;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Author & Date:   Kirk Korver     26 Apr 2005
// Purpose: Request that the ENTIRE sorting model be updated
cbRESULT cbGetSortingModel(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Send out the request for the sorting rules
    cbPKT_SS_MODELALLSET isPkt;
    isPkt.time = cb_rec_buffer_ptr[nIdx]->lasttime;
    isPkt.chid = 0x8000;
    isPkt.type = cbPKTTYPE_SS_MODELALLSET;
    isPkt.dlen = 0;

    cbRESULT ret = cbSendPacket(&isPkt, nInstance);
    // FIXME: relying on sleep is racy, refactor the code
    Sleep(250);           // give the "model" packets a chance to show up

    return ret;
}


// Author & Date:   Hyrum L. Sessions   22 Apr 2009
// Purpose: Request that the ENTIRE sorting model be updated
cbRESULT cbGetFeatureSpaceDomain(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Send out the request for the sorting rules
    cbPKT_FS_BASIS isPkt;
    isPkt.time = cb_rec_buffer_ptr[nIdx]->lasttime;
    isPkt.chid = 0x8000;
    isPkt.type = cbPKTTYPE_FS_BASISSET;
    isPkt.dlen = cbPKTDLEN_FS_BASISSHORT;

    isPkt.chan = 0;
    isPkt.mode = cbBASIS_CHANGE;
    isPkt.fs = cbAUTOALG_PCA;

    cbRESULT ret = cbSendPacket(&isPkt, nInstance);
    // FIXME: relying on sleep is racy, refactor the code
    Sleep(250);           // give the packets a chance to show up

    return ret;
}

void cbPKT_SS_NOISE_BOUNDARY::GetAxisLengths(float afAxisLen[3]) const
{
    // TODO: must be implemented for non MSC
#ifndef QT_APP
    afAxisLen[0] = sqrt(afS[0][0]*afS[0][0] + afS[0][1]*afS[0][1] + afS[0][2]*afS[0][2]);
    afAxisLen[1] = sqrt(afS[1][0]*afS[1][0] + afS[1][1]*afS[1][1] + afS[1][2]*afS[1][2]);
    afAxisLen[2] = sqrt(afS[2][0]*afS[2][0] + afS[2][1]*afS[2][1] + afS[2][2]*afS[2][2]);
#endif
}

void cbPKT_SS_NOISE_BOUNDARY::GetRotationAngles(float afTheta[3]) const
{
    // TODO: must be implemented for non MSC
#ifndef QT_APP
    Vector3f major(afS[0]);
    Vector3f minor_1(afS[1]);
    Vector3f minor_2(afS[2]);

    ::GetRotationAngles(major, minor_1, minor_2, afTheta);
#endif
}

// Author & Date:   Jason Scott     23 Jan 2009
// Purpose: Get the noise boundary parameters
// Inputs:
//  chanIdx  - channel number (1-based)
// Outputs:
//  centroid - the center of an ellipsoid
//  major - major axis of the ellipsoid
//  minor_1 - first minor axis of the ellipsoid
//  minor_2 - second minor axis of the ellipsoid
//  cbRESULT_OK if life is good
cbRESULT cbSSGetNoiseBoundary(UINT32 chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cbNUM_ANALOG_CHANS) return cbRESULT_INVALIDCHANNEL;

    cbPKT_SS_NOISE_BOUNDARY const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktNoiseBoundary[chanIdx - 1];
    if (afCentroid)
    {
        afCentroid[0] = rPkt.afc[0];
        afCentroid[1] = rPkt.afc[1];
        afCentroid[2] = rPkt.afc[2];
    }

    if (afMajor)
    {
        afMajor[0] = rPkt.afS[0][0];
        afMajor[1] = rPkt.afS[0][1];
        afMajor[2] = rPkt.afS[0][2];
    }

    if (afMinor_1)
    {
        afMinor_1[0] = rPkt.afS[1][0];
        afMinor_1[1] = rPkt.afS[1][1];
        afMinor_1[2] = rPkt.afS[1][2];
    }

    if (afMinor_2)
    {
        afMinor_2[0] = rPkt.afS[2][0];
        afMinor_2[1] = rPkt.afS[2][1];
        afMinor_2[2] = rPkt.afS[2][2];
    }

    return cbRESULT_OK;
}

// Author & Date:   Jason Scott     23 Jan 2009
// Purpose: Set the noise boundary parameters
// Inputs:
//  chanIdx  - channel number (1-based)
//  centroid - the center of an ellipsoid
//  major - major axis of the ellipsoid
//  minor_1 - first minor axis of the ellipsoid
//  minor_2 - second minor axis of the ellipsoid
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetNoiseBoundary(UINT32 chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cbNUM_ANALOG_CHANS) return cbRESULT_INVALIDCHANNEL;

    cbPKT_SS_NOISE_BOUNDARY icPkt;
    icPkt.set(chanIdx, afCentroid[0], afCentroid[1], afCentroid[2],
        afMajor[0], afMajor[1], afMajor[2],
        afMinor_1[0], afMinor_1[1], afMinor_1[2],
        afMinor_2[0], afMinor_2[1], afMinor_2[2]);

    return cbSendPacket(&icPkt, nInstance);
}

// Author & Date:   Jason Scott     August 7 2009
// Purpose: Get the noise boundary center, axis lengths, and rotation angles
// Inputs:
//  chanIdx  - channel number (1-based)
//  centroid - the center of an ellipsoid
//  axisLen - lengths of the major, first minor, and second minor axes (in that order)
//  theta - angles of rotation around the x-axis, y-axis, and z-axis (in that order)
//          Rotations should be performed in that order (yes, it matters)
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSGetNoiseBoundaryByTheta(UINT32 chanIdx, float afCentroid[3], float afAxisLen[3], float afTheta[3], UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cbNUM_ANALOG_CHANS) return cbRESULT_INVALIDCHANNEL;

    // get noise boundary info
    cbPKT_SS_NOISE_BOUNDARY const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktNoiseBoundary[chanIdx - 1];

    // move over the centroid info
    if (afCentroid)
    {
        afCentroid[0] = rPkt.afc[0];
        afCentroid[1] = rPkt.afc[1];
        afCentroid[2] = rPkt.afc[2];
    }

    // calculate the lengths
    if(afAxisLen)
    {
        rPkt.GetAxisLengths(afAxisLen);
    }

    // calculate the rotation angels
    if(afTheta)
    {
        rPkt.GetRotationAngles(afTheta);
    }

    return cbRESULT_OK;
}

// Author & Date:   Jason Scott     August 7 2009
// Purpose: Set the noise boundary via center, axis lengths, and rotation angles
// Inputs:
//  chanIdx - channel number (1-based)
//  centroid - the center of an ellipsoid
//  axisLen - lengths of the major, first minor, and second minor axes (in that order)
//  theta - angles of rotation around the x-axis, y-axis, and z-axis (in that order)
//          Rotations will be performed in that order (yes, it matters)
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetNoiseBoundaryByTheta(UINT32 chanIdx, const float afCentroid[3], const float afAxisLen[3], const float afTheta[3], UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cbNUM_ANALOG_CHANS) return cbRESULT_INVALIDCHANNEL;

    // TODO: must be implemented for non MSC
#ifndef QT_APP
    // initialize the axes on the coordinate axes
    Vector3f major(afAxisLen[0], 0, 0);
    Vector3f minor_1(0, afAxisLen[1], 0);
    Vector3f minor_2(0, 0, afAxisLen[2]);

    // rotate the axes
    ApplyRotationAngles(major, afTheta[0], afTheta[1], afTheta[2]);
    ApplyRotationAngles(minor_1, afTheta[0], afTheta[1], afTheta[2]);
    ApplyRotationAngles(minor_2, afTheta[0], afTheta[1], afTheta[2]);

    // Create the packet
    cbPKT_SS_NOISE_BOUNDARY icPkt;
    icPkt.set(chanIdx,
        afCentroid[0], afCentroid[1], afCentroid[2],
        major[0], major[1], major[2],
        minor_1[0], minor_1[1], minor_1[2],
        minor_2[0], minor_2[1], minor_2[2]);
    // Send it
    return cbSendPacket(&icPkt, nInstance);
#else
    return 0;
#endif

}

// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: Getting spike sorting statistics (NULL = don't want that value)
// Outputs:
//  cbRESULT_OK if life is good
//
//  pfFreezeMinutes - how many minutes until the number of units is "frozen"
//  pnUpdateSpikes - update rate in spike counts
//  pfUpdateMinutes - update rate in minutes
//  pfMinClusterSpreadFactor - larger number = more apt to combine 2 clusters into 1
//  pfMaxSubclusterSpreadFactor - larger number = less apt to split because of 2 clusers
//  fMinClusterHistCorrMajMeasure - larger number = more apt to split 1 cluster into 2
//  fMaxClusterPairHistCorrMajMeasure - larger number = less apt to combine 2 clusters into 1
//  fClusterHistMajValleyPercentage - larger number = less apt to split nearby clusters
//  fClusterHistMajPeakPercentage - larger number = less apt to split separated clusters
cbRESULT cbSSGetStatistics(UINT32 * pnUpdateSpikes, UINT32 * pnAutoalg, UINT32 * pnMode,
                           float * pfMinClusterPairSpreadFactor,
                           float * pfMaxSubclusterSpreadFactor,
                           float * pfMinClusterHistCorrMajMeasure,
                           float * pfMaxClusterPairHistCorrMajMeasure,
                           float * pfClusterHistValleyPercentage,
                           float * pfClusterHistClosePeakPercentage,
                           float * pfClusterHistMinPeakPercentage,
                           UINT32 * pnWaveBasisSize,
                           UINT32 * pnWaveSampleSize, 
                           UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATISTICS const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatistics;

    if (pnUpdateSpikes) *pnUpdateSpikes = rPkt.nUpdateSpikes;

    if (pnAutoalg) *pnAutoalg = rPkt.nAutoalg;
    if (pnMode) *pnMode = rPkt.nMode;

    if (pfMinClusterPairSpreadFactor) *pfMinClusterPairSpreadFactor = rPkt.fMinClusterPairSpreadFactor;
    if (pfMaxSubclusterSpreadFactor) *pfMaxSubclusterSpreadFactor = rPkt.fMaxSubclusterSpreadFactor;

    if (pfMinClusterHistCorrMajMeasure) *pfMinClusterHistCorrMajMeasure = rPkt.fMinClusterHistCorrMajMeasure;
    if (pfMaxClusterPairHistCorrMajMeasure) *pfMaxClusterPairHistCorrMajMeasure = rPkt.fMaxClusterPairHistCorrMajMeasure;

    if (pfClusterHistValleyPercentage) *pfClusterHistValleyPercentage = rPkt.fClusterHistValleyPercentage;
    if (pfClusterHistClosePeakPercentage) *pfClusterHistClosePeakPercentage = rPkt.fClusterHistClosePeakPercentage;
    if (pfClusterHistMinPeakPercentage) *pfClusterHistMinPeakPercentage = rPkt.fClusterHistMinPeakPercentage;

    if (pnWaveBasisSize) *pnWaveBasisSize = rPkt.nWaveBasisSize;
    if (pnWaveSampleSize) *pnWaveSampleSize = rPkt.nWaveSampleSize;

    return cbRESULT_OK;
}


// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: Setting spike sorting statistics
// Inputs:
//  fFreezeMinutes - time (in minutes) at which to freeze the updating
//  nUpdateSpikes - the update rate in spike counts
//  fUpdateMinutes - the update rate in minutes
//  fMinClusterSpreadFactor - larger number = more apt to combine 2 clusters into 1
//  fMaxSubclusterSpreadFactor - larger numbers  = less apt to split because of 2 clusers
//  fMinClusterHistCorrMajMeasure - larger number = more apt to split 1 cluster into 2
//  fMaxClusterPairHistCorrMajMeasure - larger number = less apt to combine 2 clusters into 1
//  fClusterHistMajValleyPercentage - larger number = less apt to split nearby clusters
//  fClusterHistMajPeakPercentage - larger number = less apt to split separated clusters
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetStatistics(UINT32 nUpdateSpikes, UINT32 nAutoalg, UINT32 nMode,
                           float fMinClusterPairSpreadFactor,
                           float fMaxSubclusterSpreadFactor,
                           float fMinClusterHistCorrMajMeasure,
                           float fMaxClusterMajHistCorrMajMeasure,
                           float fClusterHistValleyPercentage,
                           float fClusterHistClosePeakPercentage,
                           float fClusterHistMinPeakPercentage,
                           UINT32 nWaveBasisSize,
                           UINT32 nWaveSampleSize, 
                           UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATISTICS icPkt;
    icPkt.set(nUpdateSpikes, nAutoalg, nMode, fMinClusterPairSpreadFactor, fMaxSubclusterSpreadFactor,
              fMinClusterHistCorrMajMeasure, fMaxClusterMajHistCorrMajMeasure,
              fClusterHistValleyPercentage, fClusterHistClosePeakPercentage,
              fClusterHistMinPeakPercentage, nWaveBasisSize, nWaveSampleSize);

    return cbSendPacket(&icPkt, nInstance);
}

// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: set the artifact rejection parameters
// Outputs:
//  pnMaxChans - the maximum number of channels that can fire within 48 samples
//  pnRefractorySamples - num of samples (30 kHz) are "refractory" and thus ignored for detection
//  cbRESULT_OK if life is good
cbRESULT cbSSGetArtifactReject(UINT32 * pnMaxChans, UINT32 * pnRefractorySamples, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_ARTIF_REJECT const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktArtifReject;
    *pnMaxChans          = rPkt.nMaxSimulChans;
    *pnRefractorySamples = rPkt.nRefractoryCount;

    return cbRESULT_OK;
}

// Author & Date:   Kirk Korver     21 Jun 2005
// Inputs:
//  nMaxChans - the maximum number of channels that can fire within 48 samples
//  nRefractorySamples - num of samples (30 kHz) are "refractory" and thus ignored for detection
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetArtifactReject(UINT32 nMaxChans, UINT32 nRefractorySamples, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_ARTIF_REJECT isPkt;
    isPkt.set(nMaxChans, nRefractorySamples);
    return cbSendPacket(&isPkt, nInstance);
}


// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: get the spike detection parameters
// Outputs:
//  pfThreshold - the base threshold value
//  pfScaling - the threshold scaling factor
//  cbRESULT_OK if life is good
cbRESULT cbSSGetDetect(float * pfThreshold, float * pfScaling, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_DETECT const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktDetect;
    if (pfThreshold) *pfThreshold = rPkt.fThreshold;
    if (pfScaling)   *pfScaling   = rPkt.fMultiplier;

    return cbRESULT_OK;
}

// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: set the spike detection parameters
// Inputs:
//  pfThreshold - the base threshold value
//  pfScaling - the threshold scaling factor
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetDetect(float fThreshold, float fScaling, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_DETECT isPkt;
    isPkt.set(fThreshold, fScaling);
    return cbSendPacket(&isPkt, nInstance);
}


// Author & Date:   Hyrum L. Sessions   18 Nov 2005
// Purpose: get the spike sorting status
// Outputs:
//  pnMode - 0=number of units is still adapting  1=number of units is frozen
//  pfElapsedMinutes - this only makes sense if nMode=0 - minutes from start adapting
//  cbRESULT_OK if life is good
cbRESULT cbSSGetStatus(cbAdaptControl * pcntlUnitStats, cbAdaptControl * pcntlNumUnits, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATUS const & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktStatus;
    if (pcntlUnitStats)     *pcntlUnitStats     = rPkt.cntlUnitStats;
    if (pcntlNumUnits)      *pcntlNumUnits      = rPkt.cntlNumUnits;

    return cbRESULT_OK;
}


// Author & Date:   Dan Sebald     3 Dec 2005
// Purpose: Setting spike sorting control status
// Inputs:
//  cntlUnitStats - control/timer information for unit statistics adaptation
//  cntlNumUnits - control/timer information for number of units
// Outputs:
//  cbRESULT_OK if life is good
cbRESULT cbSSSetStatus(cbAdaptControl cntlUnitStats, cbAdaptControl cntlNumUnits, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATUS icPkt;
    icPkt.set(cntlUnitStats, cntlNumUnits);

    return cbSendPacket(&icPkt, nInstance);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Data checking and processing functions
//
// To get data from the shared memory buffers used in the Central App, the user can:
// 1) periodically poll for new data using a multimedia or windows timer
// 2) create a thread that uses a Win32 Event synchronization object to que the data polling
//
///////////////////////////////////////////////////////////////////////////////////////////////////


cbRESULT cbCheckforData(cbLevelOfConcern & nLevelOfConcern, UINT32 *pktstogo /* = NULL */, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for data loss by checking that
    //    [(head wraparound != Tail wraparound)AND((head index + (max_datagram_size/4)q) >= read index)]
    // OR [head wraparound is more than twice ahead of the read pointer set]
    if (((cb_rec_buffer_ptr[nIdx]->headwrap != cb_recbuff_tailwrap[nIdx]) &&
        (
          (cb_rec_buffer_ptr[nIdx]->headindex + (cbCER_UDP_SIZE_MAX / 4)) >= cb_recbuff_tailindex[nIdx])) ||
          (cb_rec_buffer_ptr[nIdx]->headwrap > (cb_recbuff_tailwrap[nIdx] + 1))
        )
    {
        cbMakePacketReadingBeginNow(nInstance);
        nLevelOfConcern = LOC_CRITICAL;
        return cbRESULT_DATALOST;
    }

    if (pktstogo)
        *pktstogo = cb_rec_buffer_ptr[nIdx]->received - cb_recbuff_processed[nIdx];

    // Level of concern is based on fourths
    int nDiff = cb_rec_buffer_ptr[nIdx]->headindex - cb_recbuff_tailindex[nIdx];
    if (nDiff < 0)
        nDiff += cbRECBUFFLEN;

    nLevelOfConcern = static_cast<cbLevelOfConcern>( (nDiff * LOC_COUNT) / cbRECBUFFLEN );

    return cbRESULT_OK;
}

#if defined __APPLE__
// Author & Date:   Ehsan Azar     16 Feb 2013
// Purpose: OSX compatibility wrapper
// Inputs:
//   sem    - buffer name
//   ms     - milliseconds to try semaphore
int sem_timedwait(sem_t * sem, int ms)
{
	int err = 1;
	while (ms > 0)
	{
		if (sem_trywait(sem) == 0)
		{
			err = 0;
			break;
		}
		usleep(1000);
		ms--;
	}
	return err;
}
#endif

// Purpose: Wait for master application (usually Central) to fill buffers
cbRESULT cbWaitforData(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

#ifdef WIN32
    if (WaitForSingleObject(cb_sig_event_hnd[nIdx], 250) == WAIT_OBJECT_0)
        return cbRESULT_OK;
#elif defined __APPLE__
    if (sem_timedwait((sem_t *)cb_sig_event_hnd[nIdx], 250) == 0)
        return cbRESULT_OK;
#else
    timespec ts;
    long ns = 250000000;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec = (ts.tv_nsec + ns) % NSEC_PER_SEC;
    ts.tv_sec += (ts.tv_nsec + ns) / NSEC_PER_SEC;
    if (sem_timedwait((sem_t *)cb_sig_event_hnd[nIdx], &ts) == 0)
        return cbRESULT_OK;
#endif
    else if (!(cb_cfg_buffer_ptr[nIdx]->version))
    {
        TRACE("cbWaitforData says no Central App\n");
        return cbRESULT_NOCENTRALAPP;
    }
    else
        return cbRESULT_NONEWDATA;
}


cbPKT_GENERIC * cbGetNextPacketPtr(UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // if there is new data return the next data packet and increment the pointer
    if (cb_recbuff_processed[nIdx] < cb_rec_buffer_ptr[nIdx]->received)
    {
        // get the pointer to the current packet
        cbPKT_GENERIC *packetptr = (cbPKT_GENERIC*)&(cb_rec_buffer_ptr[nIdx]->buffer[cb_recbuff_tailindex[nIdx]]);

        // increament the read index
        cb_recbuff_tailindex[nIdx] += (cbPKT_HEADER_32SIZE + packetptr->dlen);

        // check for read buffer wraparound, if so increment relevant variables
        if (cb_recbuff_tailindex[nIdx] > (cbRECBUFFLEN - (cbCER_UDP_SIZE_MAX / 4)))
        {
            cb_recbuff_tailindex[nIdx] = 0;
            cb_recbuff_tailwrap[nIdx]++;
        }

        // increment the processed count
        cb_recbuff_processed[nIdx]++;

        // update the timestamp index
        cb_recbuff_lasttime[nIdx] = packetptr->time;

        // return the packet
        return packetptr;
    }
    else
        return NULL;
}


// Purpose: options sharing
//
cbRESULT cbGetColorTable(cbCOLORTABLE **colortable, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *colortable = &(cb_cfg_buffer_ptr[nIdx]->colortable);
    return cbRESULT_OK;
}

// Purpose: options sharing spike cache
//
cbRESULT cbGetSpkCache(UINT32 chid, cbSPKCACHE **cache, UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *cache = (cbSPKCACHE*)( ((BYTE*)&(cb_spk_buffer_ptr[nIdx]->cache)) + ((chid - 1)*(cb_spk_buffer_ptr[nIdx]->linesize)) );
    return cbRESULT_OK;
}

// Author & Date:   Kirk Korver     29 May 2003
// Purpose: Get the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
float cbGetRMSAutoThresholdDistance(UINT32 nInstance)
{
    return GetOptionTable(nInstance).fRMSAutoThresholdDistance;
}

// Author & Date:   Kirk Korver     29 May 2003
// Purpose: Set the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
void cbSetRMSAutoThresholdDistance(float fRMSAutoThresholdDistance, UINT32 nInstance)
{
    GetOptionTable(nInstance).fRMSAutoThresholdDistance = fRMSAutoThresholdDistance;
}


// Tell me about the current adaptive filter settings
cbRESULT cbGetAdaptFilter(UINT32  proc,             // which NSP processor?
                          UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          UINT32  * pnRefChan1,     // The first reference channel (1 based).
                          UINT32  * pnRefChan2,     // The second reference channel (1 based).
                          UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;

    // Allow the parameters to be NULL
    if (pnMode)         *pnMode         = cb_cfg_buffer_ptr[nIdx]->adaptinfo.nMode;
    if (pdLearningRate) *pdLearningRate = cb_cfg_buffer_ptr[nIdx]->adaptinfo.dLearningRate;
    if (pnRefChan1)     *pnRefChan1     = cb_cfg_buffer_ptr[nIdx]->adaptinfo.nRefChan1;
    if (pnRefChan2)     *pnRefChan2     = cb_cfg_buffer_ptr[nIdx]->adaptinfo.nRefChan2;

    return cbRESULT_OK;
}


// Update the adaptive filter settings
cbRESULT cbSetAdaptFilter(UINT32  proc,             // which NSP processor?
                          UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          UINT32  * pnRefChan1,     // The first reference channel (1 based).
                          UINT32  * pnRefChan2,     // The second reference channel (1 based).
                          UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;


    // Get the old values
    UINT32  nMode;         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    float   dLearningRate; // speed at which adaptation happens. Very small. e.g. 5e-12
    UINT32  nRefChan1;     // The first reference channel (1 based).
    UINT32  nRefChan2;     // The second reference channel (1 based).

    cbRESULT ret = cbGetAdaptFilter(proc, &nMode, &dLearningRate, &nRefChan1, &nRefChan2, nInstance);
    ASSERT(ret == cbRESULT_OK);
    if (ret != cbRESULT_OK)
        return ret;

    // Handle the cases where there are "NULL's" passed in
    if (pnMode)         nMode =         *pnMode;
    if (pdLearningRate) dLearningRate = *pdLearningRate;
    if (pnRefChan1)     nRefChan1 =     *pnRefChan1;
    if (pnRefChan2)     nRefChan2 =     *pnRefChan2;

    PktAdaptFiltInfo icPkt(nMode, dLearningRate, nRefChan1, nRefChan2);
    return cbSendPacket(&icPkt, nInstance);
}

// Tell me about the current RefElecive filter settings
cbRESULT cbGetRefElecFilter(UINT32  proc,             // which NSP processor?
                            UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            UINT32  * pnRefChan,      // The reference channel (1 based).
                            UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;

    // Allow the parameters to be NULL
    if (pnMode)         *pnMode         = cb_cfg_buffer_ptr[nIdx]->refelecinfo.nMode;
    if (pnRefChan)      *pnRefChan      = cb_cfg_buffer_ptr[nIdx]->refelecinfo.nRefChan;

    return cbRESULT_OK;
}


// Update the reference electrode filter settings
cbRESULT cbSetRefElecFilter(UINT32  proc,             // which NSP processor?
                            UINT32  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            UINT32  * pnRefChan,      // The reference channel (1 based).
                            UINT32 nInstance)
{
    UINT32 nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;


    // Get the old values
    UINT32  nMode;         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    UINT32  nRefChan;      // The reference channel (1 based).

    cbRESULT ret = cbGetRefElecFilter(proc, &nMode, &nRefChan, nInstance);
    ASSERT(ret == cbRESULT_OK);
    if (ret != cbRESULT_OK)
        return ret;

    // Handle the cases where there are "NULL's" passed in
    if (pnMode)         nMode =         *pnMode;
    if (pnRefChan)      nRefChan =      *pnRefChan;

    cbPKT_REFELECFILTINFO icPkt;
    icPkt.set(nMode, nRefChan);
    return cbSendPacket(&icPkt, nInstance);
}

// Author & Date:   Ehsan Azar     6 Nov 2012
// Purpose: Get the channel selection status
// Inputs:
//   szName    - buffer name
//   bReadOnly - if should open memory for read-only operation
cbRESULT cbGetChannelSelection(cbPKT_UNIT_SELECTION * pPktUnitSel, UINT32 nInstance)
{
#if 0
    UINT32 nIdx = cb_library_index[nInstance];
    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    if (cb_pc_status_buffer_ptr[nIdx]->isSelection.chid == 0)
        return cbRESULT_HARDWAREOFFLINE;

    if (pPktUnitSel) *pPktUnitSel = cb_pc_status_buffer_ptr[nIdx]->isSelection;
#endif
    return cbRESULT_OK;
}

// Author & Date:       Almut Branner         28 Mar 2006
// Purpose: Create the shared memory objects
// Inputs:
//   nInstance - nsp number to open library for
cbRESULT CreateSharedObjects(UINT32 nInstance)
{
    char buf[64] = {0};
    UINT32 nIdx = cb_library_index[nInstance];

    // Create the shared neuromatic receive buffer, if unsuccessful, return the associated error code
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", REC_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", REC_BUF_NAME, nInstance);            
    cb_rec_buffer_hnd[nIdx] = CreateSharedBuffer(buf, sizeof(cbRECBUFF));
    cb_rec_buffer_ptr[nIdx] = (cbRECBUFF*)GetSharedBuffer(cb_rec_buffer_hnd[nIdx], false);

    if (cb_rec_buffer_ptr[nIdx] == NULL)
        return cbRESULT_BUFRECALLOCERR;

    memset(cb_rec_buffer_ptr[nIdx], 0, sizeof(cbRECBUFF));

    // Create the shared transmit buffer; if unsuccessful, release rec buffer and associated error code
    {
        // declare the length of the buffer in UINT32 units
        const UINT32 cbXMT_GLOBAL_BUFFLEN = (cbCER_UDP_SIZE_MAX / 4) * 500 + 2;    // room for 500 packets
        const UINT32 cbXMT_LOCAL_BUFFLEN  = (cbCER_UDP_SIZE_MAX / 4) * 200 + 2;    // room for 200 packets

        // determine the XMT buffer structure size (header + data field)
        const UINT32 cbXMT_GLOBAL_BUFFSTRUCTSIZE = sizeof(cbXMTBUFF) + (sizeof(UINT32)*cbXMT_GLOBAL_BUFFLEN);
        const UINT32 cbXMT_LOCAL_BUFFSTRUCTSIZE  = sizeof(cbXMTBUFF) + (sizeof(UINT32)*cbXMT_LOCAL_BUFFLEN);

        // create the global transmit buffer space
        if (nInstance == 0)
            _snprintf(buf, sizeof(buf), "%s", GLOBAL_XMT_NAME);
        else
            _snprintf(buf, sizeof(buf), "%s%d", GLOBAL_XMT_NAME, nInstance);            
        cb_xmt_global_buffer_hnd[nIdx] = CreateSharedBuffer(buf, cbXMT_GLOBAL_BUFFSTRUCTSIZE);
        // map the global memory into local ram space and get pointer
        cb_xmt_global_buffer_ptr[nIdx] = (cbXMTBUFF*)GetSharedBuffer(cb_xmt_global_buffer_hnd[nIdx], false);

        // clean up if error occurs
        if (cb_xmt_global_buffer_ptr[nIdx] == NULL)
            return cbRESULT_BUFGXMTALLOCERR;

        // create the local transmit buffer space
        if (nInstance == 0)
            _snprintf(buf, sizeof(buf), "%s", LOCAL_XMT_NAME);
        else
            _snprintf(buf, sizeof(buf), "%s%d", LOCAL_XMT_NAME, nInstance);            
        cb_xmt_local_buffer_hnd[nIdx] = CreateSharedBuffer(buf, cbXMT_LOCAL_BUFFSTRUCTSIZE);
        // map the global memory into local ram space and get pointer
        cb_xmt_local_buffer_ptr[nIdx] = (cbXMTBUFF*)GetSharedBuffer(cb_xmt_local_buffer_hnd[nIdx], false);

        // clean up if error occurs
        if (cb_xmt_local_buffer_ptr[nIdx] == NULL)
            return cbRESULT_BUFLXMTALLOCERR;

        // initialize the buffers...they MUST all be initialized to 0 for later logic to work!!
        memset(cb_xmt_global_buffer_ptr[nIdx], 0, cbXMT_GLOBAL_BUFFSTRUCTSIZE);
        cb_xmt_global_buffer_ptr[nIdx]->bufferlen = cbXMT_GLOBAL_BUFFLEN;
        cb_xmt_global_buffer_ptr[nIdx]->last_valid_index =
            cbXMT_GLOBAL_BUFFLEN - (cbCER_UDP_SIZE_MAX / 4) - 1; // assuming largest packet   array is 0 based


        memset(cb_xmt_local_buffer_ptr[nIdx], 0, cbXMT_LOCAL_BUFFSTRUCTSIZE);
        cb_xmt_local_buffer_ptr[nIdx]->bufferlen = cbXMT_LOCAL_BUFFLEN;
        cb_xmt_local_buffer_ptr[nIdx]->last_valid_index =
            cbXMT_LOCAL_BUFFLEN - (cbCER_UDP_SIZE_MAX / 4) - 1; // assuming largest packet   array is 0 based

    }

    // Create the shared configuration buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", CFG_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", CFG_BUF_NAME, nInstance);
    cb_cfg_buffer_hnd[nIdx] = CreateSharedBuffer(buf, sizeof(cbCFGBUFF));
    cb_cfg_buffer_ptr[nIdx] = (cbCFGBUFF*)GetSharedBuffer(cb_cfg_buffer_hnd[nIdx], false);
    if (cb_cfg_buffer_ptr[nIdx] == NULL)
        return cbRESULT_BUFCFGALLOCERR;

    memset(cb_cfg_buffer_ptr[nIdx], 0, sizeof(cbCFGBUFF));

    // Create the shared pc status buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", STATUS_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", STATUS_BUF_NAME, nInstance);            
    cb_pc_status_buffer_hnd[nIdx] = CreateSharedBuffer(buf, sizeof(cbPcStatus));
    cb_pc_status_buffer_ptr[nIdx] = (cbPcStatus*)GetSharedBuffer(cb_pc_status_buffer_hnd[nIdx], false);
    if (cb_pc_status_buffer_ptr[nIdx] == NULL)
        return cbRESULT_BUFPCSTATALLOCERR;

    memset(cb_pc_status_buffer_ptr[nIdx], 0, sizeof(cbPcStatus));

    // Create the shared spike cache buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SPK_BUF_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SPK_BUF_NAME, nInstance);
    cb_spk_buffer_hnd[nIdx] = CreateSharedBuffer(buf, sizeof(cbSPKBUFF));
    cb_spk_buffer_ptr[nIdx] = (cbSPKBUFF*)GetSharedBuffer(cb_spk_buffer_hnd[nIdx], false);
    if (cb_spk_buffer_ptr[nIdx] == NULL)
        return cbRESULT_BUFSPKALLOCERR;

    memset(cb_spk_buffer_ptr[nIdx], 0, sizeof(cbSPKBUFF));
    cb_spk_buffer_ptr[nIdx]->chidmax  = cbPKT_SPKCACHELINECNT;
    cb_spk_buffer_ptr[nIdx]->linesize = sizeof(cbSPKCACHE);
    cb_spk_buffer_ptr[nIdx]->spkcount = cbPKT_SPKCACHEPKTCNT;
    for (int l=0; l<cbPKT_SPKCACHELINECNT; l++)
    {
        cb_spk_buffer_ptr[nIdx]->cache[l].chid    = l+1;
        cb_spk_buffer_ptr[nIdx]->cache[l].pktcnt  = cbPKT_SPKCACHEPKTCNT;
        cb_spk_buffer_ptr[nIdx]->cache[l].pktsize = sizeof(cbPKT_SPK);
    }

    // initialize the configuration fields
    cb_cfg_buffer_ptr[nIdx]->version = 96;
    cb_cfg_buffer_ptr[nIdx]->colortable.dispback     = RGB(  0,  0,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispgridmaj  = RGB( 80, 80, 80);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispgridmin  = RGB( 48, 48, 48);
    cb_cfg_buffer_ptr[nIdx]->colortable.disptext     = RGB(192,192,192);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispwave     = RGB(160,160,160);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispwavewarn = RGB(160,160,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispwaveclip = RGB(192,  0,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispthresh   = RGB(192,  0,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispmultunit = RGB(255,255,255);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[0]  = RGB(192,192,192);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[1]  = RGB(255, 51,153);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[2]  = RGB(  0,255,255);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[3]  = RGB(255,255,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[4]  = RGB(153,  0,204);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispunit[5]  = RGB(  0,255,  0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispnoise    = RGB( 78, 49, 31);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispchansel[0]  = RGB(0,0,0);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispchansel[1]  = RGB(192,192,192);
    cb_cfg_buffer_ptr[nIdx]->colortable.dispchansel[2]  = RGB(255,255,0);
    cb_cfg_buffer_ptr[nIdx]->colortable.disptemp[0]  = RGB(0,255,0);

    // create the shared event for data availability signalling
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SIG_EVT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SIG_EVT_NAME, nInstance);            
#ifdef WIN32
    cb_sig_event_hnd[nIdx] = CreateEvent(NULL, TRUE, FALSE, buf);
    if (cb_sig_event_hnd[nIdx] == NULL)
        return cbRESULT_EVSIGERR;
#else
    sem_t * sem = sem_open(buf, O_CREAT | O_EXCL, 0666, 0);
    if (sem == SEM_FAILED)
    {
        // Reattach: This might happen as a result of previous crash
        sem_unlink(buf);
        sem = sem_open(buf, O_CREAT | O_EXCL, 0666, 0);
        if (sem == SEM_FAILED)
            return cbRESULT_EVSIGERR;
    }
    cb_sig_event_hnd[nIdx] = sem;
#endif

    // No erro happned
    return cbRESULT_OK;
}


