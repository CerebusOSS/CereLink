// =STS=> cbhwlib.cpp[2730].aa14   open     SMID:15 
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Cerebus Interface Library
//
// Copyright (C) 2001-2003 Bionic Technologies, Inc.
// (c) Copyright 2003-2008 Cyberkinetics, Inc
// (c) Copyright 2008-2021 Blackrock Microsystems
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
// cbhwlib library is currently based on non Unicode only
#undef UNICODE
#undef _UNICODE
#ifndef WIN32
    // For non-Windows systems, include POSIX headers
    #include <sys/mman.h>
    #include <semaphore.h>
    #include <sys/file.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <cerrno>
#endif
#include "DataVector.h"
#ifndef _MSC_VER
#include <unistd.h>
#endif
#ifndef WIN32
    #define Sleep(x) usleep((x) * 1000)
    #define strncpy_s( dst, dstSize, src, count ) strncpy( dst, src, count < dstSize ? count : dstSize )
#endif // WIN32
#include "debugmacs.h"
#include "../include/cerelink/cbhwlib.h"
#include "cbHwlibHi.h"


// forward reference
cbRESULT CreateSharedObjects(uint32_t nInstance);


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Internal Library variables
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// buffer handles
cbSharedMemHandle  cb_xmt_global_buffer_hnd[cbMAXOPEN] = {nullptr};       // Transmit queues to send out of this PC
cbXMTBUFF*  cb_xmt_global_buffer_ptr[cbMAXOPEN] = {nullptr};
auto GLOBAL_XMT_NAME = "XmtGlobal";
cbSharedMemHandle  cb_xmt_local_buffer_hnd[cbMAXOPEN] = {nullptr};        // Transmit queues only for local (this PC) use
cbXMTBUFF*  cb_xmt_local_buffer_ptr[cbMAXOPEN] = {nullptr};
auto LOCAL_XMT_NAME = "XmtLocal";

auto REC_BUF_NAME = "cbRECbuffer";
cbSharedMemHandle  cb_rec_buffer_hnd[cbMAXOPEN] = {nullptr};
cbRECBUFF*  cb_rec_buffer_ptr[cbMAXOPEN] = {nullptr};
auto CFG_BUF_NAME = "cbCFGbuffer";
cbSharedMemHandle  cb_cfg_buffer_hnd[cbMAXOPEN] = {nullptr};
cbCFGBUFF*  cb_cfg_buffer_ptr[cbMAXOPEN] = {nullptr};
auto STATUS_BUF_NAME = "cbSTATUSbuffer";
cbSharedMemHandle  cb_pc_status_buffer_hnd[cbMAXOPEN] = {nullptr};
cbPcStatus* cb_pc_status_buffer_ptr[cbMAXOPEN] = {nullptr};
auto SPK_BUF_NAME = "cbSPKbuffer";
cbSharedMemHandle  cb_spk_buffer_hnd[cbMAXOPEN] = {nullptr};
cbSPKBUFF*  cb_spk_buffer_ptr[cbMAXOPEN] = {nullptr};
auto SIG_EVT_NAME = "cbSIGNALevent";
HANDLE      cb_sig_event_hnd[cbMAXOPEN] = {nullptr};

//
uint32_t      cb_library_index[cbMAXOPEN] = {0};
uint32_t      cb_library_initialized[cbMAXOPEN] = {false};
uint32_t      cb_recbuff_tailwrap[cbMAXOPEN]  = {0};
uint32_t      cb_recbuff_tailindex[cbMAXOPEN] = {0};
uint32_t      cb_recbuff_processed[cbMAXOPEN] = {0};
PROCTIME      cb_recbuff_lasttime[cbMAXOPEN] = {0};

// Handle to system lock associated with shared resources
HANDLE      cb_sys_lock_hnd[cbMAXOPEN] = {nullptr};


// Local functions to make life easier
inline cbOPTIONTABLE & GetOptionTable(const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];
    return cb_cfg_buffer_ptr[nIdx]->optiontable;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Library Initialization Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////



// Returns the major/minor revision of the current library in the upper/lower uint16_t fields.
uint32_t cbVersion()
{
    return MAKELONG(cbVERSION_MINOR, cbVERSION_MAJOR);
}

// Author & Date:       Kirk Korver         17 Jun 2003
// Purpose: Release and clear the memory assocated with this handle/pointer
// Inputs:
//  hMem - the handle to the memory to free up
//  ppMem - the pointer to this same memory
void DestroySharedObject(cbSharedMemHandle & shmem, void ** ppMem)
{
#ifdef WIN32
    if (*ppMem != nullptr)
        UnmapViewOfFile(*ppMem);
    if (shmem.hnd != nullptr)
        CloseHandle(shmem.hnd);
#else
    if (*ppMem != nullptr)
    {
        // Get the number of current attachments
        if (munmap(shmem.hnd, shmem.size) == -1)
        {
            printf("munmap() failed with errno %d\n", errno);
        }
        close(shmem.fd);
        shm_unlink(shmem.name);
    }
#endif
    shmem.hnd = nullptr;
    *ppMem = nullptr;
}

// Author & Date:       Almut Branner         28 Mar 2006
// Purpose: Release and clear the shared memory objects
// Inputs:
//   nInstance - nsp number to close library for
void DestroySharedObjects(const bool bStandAlone, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
        sem_close(static_cast<sem_t *>(cb_sig_event_hnd[nIdx]));
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
    DestroySharedObject(cb_pc_status_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_pc_status_buffer_ptr[nIdx]));

    // release the shared spike buffer memory space
    DestroySharedObject(cb_spk_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_spk_buffer_ptr[nIdx]));

    // release the shared configuration memory space
    DestroySharedObject(cb_cfg_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_cfg_buffer_ptr[nIdx]));

    // release the shared global transmit memory space
    DestroySharedObject(cb_xmt_global_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_xmt_global_buffer_ptr[nIdx]));

    // release the shared local transmit memory space
    DestroySharedObject(cb_xmt_local_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_xmt_local_buffer_ptr[nIdx]));

    // release the shared receive memory space
    DestroySharedObject(cb_rec_buffer_hnd[nIdx], reinterpret_cast<void **>(&cb_rec_buffer_ptr[nIdx]));
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Open shared memory section
// Inputs:
//   shmem    - cbSharedMemHandle object with .name filled in.
//              This method will update .hnd (and .size on POSIX)
//   bReadOnly - if should open memory for read-only operation
void OpenSharedBuffer(cbSharedMemHandle& shmem, bool bReadOnly)
{
#ifdef WIN32
    // Keep windows version unchanged
    shmem.hnd = OpenFileMappingA(bReadOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, shmem.name);
#else
    const int oflag = (bReadOnly ? O_RDONLY : O_RDWR);
    const mode_t omode = (bReadOnly ? 0444 : 0660);
    shmem.fd = shm_open(shmem.name, oflag, omode);
    if (shmem.fd == -1)
        /* Handle error */;
    struct stat shm_stat{};
    if (fstat(shmem.fd, &shm_stat) == -1)
        /* Handle error */;
    const int prot = (bReadOnly ? PROT_READ : PROT_READ | PROT_WRITE);
    shmem.size = shm_stat.st_size;
    shmem.hnd = mmap(nullptr, shm_stat.st_size, prot, MAP_SHARED, shmem.fd, 0);
    if (shmem.hnd == MAP_FAILED || !shmem.hnd)
        /* Handle error */;
#endif
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Open shared memory section
// Inputs:
//   shmem - a cbSharedMemHandle object with correct szName and minimum size. Passed by ref and will be updated.
void CreateSharedBuffer(cbSharedMemHandle& shmem)
{
#ifdef WIN32
    // Keep windows version unchanged
    shmem.hnd = CreateFileMappingA((HANDLE)INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, shmem.size, shmem.name);
#else
    // round up to the next pagesize.
    const int pagesize = getpagesize();
    shmem.size = shmem.size - (shmem.size % pagesize) + pagesize;
    // Pre-emptively attempt to unlink in case it already exists.
    int errsv = 0;
    if (shm_unlink(shmem.name)) {
        errsv = errno;
        if (errsv != ENOENT)
            printf("shm_unlink() failed with errno %d\n", errsv);
    }
    // Create the shared memory object
    shmem.fd = shm_open(shmem.name, O_CREAT | O_RDWR, 0660);
    if (shmem.fd == -1){
        errsv = errno;
        printf("shm_open() failed with errno %d\n", errsv);
    }
    // Set the size of the shared memory object.
    if (ftruncate(shmem.fd, shmem.size) == -1) {
        errsv = errno;
        printf("ftruncate(fd, %d) failed with errno %d\n", shmem.size, errsv);
        close(shmem.fd);
        shm_unlink(shmem.name);
        return;
    }
    // Get a pointer to the shmem object.
    void *rptr = mmap(nullptr, shmem.size,
                      PROT_READ | PROT_WRITE, MAP_SHARED, shmem.fd, 0);
    if (rptr == MAP_FAILED) {
        errsv = errno;
        printf("mmap(nullptr, %d, ...) failed with errno %d\n", shmem.size, errsv);
        close(shmem.fd);
        shm_unlink(shmem.name);
    }
    else
        shmem.hnd = rptr;
#endif
}

// Author & Date:   Ehsan Azar     29 April 2012
// Purpose: Get access to shared memory section data
// Inputs:
//   hnd       - shared memory handle
//   bReadOnly - if should open memory for read-only operation
void * GetSharedBuffer(HANDLE hnd, bool bReadOnly)
{
    void * ret = nullptr;
    if (hnd == nullptr)
        return ret;
#ifdef WIN32
    // Keep windows version unchanged
    ret = MapViewOfFile(hnd, bReadOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, 0, 0);
#else
    ret = hnd;
#endif
    return ret;
}

// Purpose: Open library as stand-alone or under given application with thread
//    identifier for multiple threads each with its own IP
// Inputs:
//   bStandAlone - if should open library as stand-alone
//   nInstance     - integer index identifier of library instance (0-based up to cbMAXOPEN-1)
cbRESULT cbOpen(const bool bStandAlone, const uint32_t nInstance)
{
    char buf[64] = {0};
    cbRESULT cbRet;

    if (nInstance >= cbMAXOPEN)
        return cbRESULT_INSTINVALID;

    // Find an empty stub
    uint32_t nIdx = cbMAXOPEN;
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
        _snprintf(szLockName, sizeof(szLockName), "cbSharedDataMutex");
    else
        _snprintf(szLockName, sizeof(szLockName), "cbSharedDataMutex%d", nInstance);

    // If it is stand-alone application
    if (bStandAlone)
    {
        // Acquire lock
        cbRet = cbAcquireSystemLock(szLockName, cb_sys_lock_hnd[nInstance]);
        if (cbRet)
            return cbRet;
        cb_library_index[nInstance] = nIdx;
        // Create the shared memory and synchronization objects
        cbRet = CreateSharedObjects(nInstance);
        // Library initialized if the objects are created
        if (cbRet == cbRESULT_OK)
        {
            cb_library_initialized[nIdx] = true;      // We are in the library, so it is initialized
        } else {
            cbReleaseSystemLock(szLockName, cb_sys_lock_hnd[nInstance]);
            cbClose(bStandAlone, nInstance);
        }
        return cbRet;
    } else {
        // Check if mutex is locked
        cbRet = cbCheckApp(szLockName);
        if (cbRet == cbRESULT_NOCENTRALAPP)
            return cbRet;
    }

    // Create the shared neuromatic receive buffer, if unsuccessful, return FALSE
    if (nInstance == 0)
        _snprintf(cb_rec_buffer_hnd[nIdx].name, sizeof(cb_rec_buffer_hnd[nIdx].name), "%s", REC_BUF_NAME);
    else
        _snprintf(cb_rec_buffer_hnd[nIdx].name, sizeof(cb_rec_buffer_hnd[nIdx].name), "%s%d", REC_BUF_NAME, nInstance);
    OpenSharedBuffer(cb_rec_buffer_hnd[nIdx], true);
    cb_rec_buffer_ptr[nIdx] = static_cast<cbRECBUFF *>(GetSharedBuffer(cb_rec_buffer_hnd[nIdx].hnd, true));
    if (cb_rec_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    if (nInstance == 0)
        _snprintf(cb_xmt_global_buffer_hnd[nIdx].name, sizeof(cb_xmt_global_buffer_hnd[nIdx].name), "%s", GLOBAL_XMT_NAME);
    else
        _snprintf(cb_xmt_global_buffer_hnd[nIdx].name, sizeof(cb_xmt_global_buffer_hnd[nIdx].name), "%s%d", GLOBAL_XMT_NAME, nInstance);
    // Create the shared global transmit buffer; if unsuccessful, release rec buffer and return FALSE
    OpenSharedBuffer(cb_xmt_global_buffer_hnd[nIdx], false);
    cb_xmt_global_buffer_ptr[nIdx] = static_cast<cbXMTBUFF *>(GetSharedBuffer(cb_xmt_global_buffer_hnd[nIdx].hnd, false));
    if (cb_xmt_global_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    if (nInstance == 0)
        _snprintf(cb_xmt_local_buffer_hnd[nIdx].name, sizeof(cb_xmt_local_buffer_hnd[nIdx].name), "%s", LOCAL_XMT_NAME);
    else
        _snprintf(cb_xmt_local_buffer_hnd[nIdx].name, sizeof(cb_xmt_local_buffer_hnd[nIdx].name), "%s%d", LOCAL_XMT_NAME, nInstance);
    // Create the shared local transmit buffer; if unsuccessful, release rec buffer and return FALSE
    OpenSharedBuffer(cb_xmt_local_buffer_hnd[nIdx], false);
    cb_xmt_local_buffer_ptr[nIdx] = static_cast<cbXMTBUFF *>(GetSharedBuffer(cb_xmt_local_buffer_hnd[nIdx].hnd, false));
    if (cb_xmt_local_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    if (nInstance == 0)
        _snprintf(cb_cfg_buffer_hnd[nIdx].name, sizeof(cb_cfg_buffer_hnd[nIdx].name), "%s", CFG_BUF_NAME);
    else
        _snprintf(cb_cfg_buffer_hnd[nIdx].name, sizeof(cb_cfg_buffer_hnd[nIdx].name), "%s%d", CFG_BUF_NAME, nInstance);
    // Create the shared neuromatic configuration buffer; if unsuccessful, release rec buffer and return FALSE
    OpenSharedBuffer(cb_cfg_buffer_hnd[nIdx], true);
    cb_cfg_buffer_ptr[nIdx] = static_cast<cbCFGBUFF *>(GetSharedBuffer(cb_cfg_buffer_hnd[nIdx].hnd, true));
    if (cb_cfg_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    // Check version of hardware protocol if available
    if (cb_cfg_buffer_ptr[nIdx]->procinfo[0].cbpkt_header.chid != 0) {
        if (cb_cfg_buffer_ptr[nIdx]->procinfo[0].version > cbVersion()) {
            cbClose(false, nInstance);
            return cbRESULT_LIBOUTDATED;
        }
        if (cb_cfg_buffer_ptr[nIdx]->procinfo[0].version < cbVersion()) {
            cbClose(false, nInstance);
            return cbRESULT_INSTOUTDATED;
        }
    }

    if (nInstance == 0)
        _snprintf(cb_pc_status_buffer_hnd[nIdx].name, sizeof(cb_pc_status_buffer_hnd[nIdx].name), "%s", STATUS_BUF_NAME);
    else
        _snprintf(cb_pc_status_buffer_hnd[nIdx].name, sizeof(cb_pc_status_buffer_hnd[nIdx].name), "%s%d", STATUS_BUF_NAME, nInstance);
    // Create the shared pc status buffer; if unsuccessful, release rec buffer and return FALSE
    OpenSharedBuffer(cb_pc_status_buffer_hnd[nIdx], false);
    cb_pc_status_buffer_ptr[nIdx] = static_cast<cbPcStatus *>(GetSharedBuffer(cb_pc_status_buffer_hnd[nIdx].hnd, false));
    if (cb_pc_status_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    // Create the shared spike buffer
    if (nInstance == 0)
        _snprintf(cb_spk_buffer_hnd[nIdx].name, sizeof(cb_spk_buffer_hnd[nIdx].name), "%s", SPK_BUF_NAME);
    else
        _snprintf(cb_spk_buffer_hnd[nIdx].name, sizeof(cb_spk_buffer_hnd[nIdx].name), "%s%d", SPK_BUF_NAME, nInstance);
    OpenSharedBuffer(cb_spk_buffer_hnd[nIdx], false);
    cb_spk_buffer_ptr[nIdx] = static_cast<cbSPKBUFF *>(GetSharedBuffer(cb_spk_buffer_hnd[nIdx].hnd, false));
    if (cb_spk_buffer_ptr[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }

    // open the data availability signals
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SIG_EVT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SIG_EVT_NAME, nInstance);
#ifdef WIN32
    cb_sig_event_hnd[nIdx] = OpenEventA(SYNCHRONIZE, TRUE, buf);
    if (cb_sig_event_hnd[nIdx] == nullptr)
    {
        cbClose(false, nInstance);
        return cbRESULT_LIBINITERROR;
    }
#else
    sem_t *sem = sem_open(buf, 0);
    if (sem == SEM_FAILED) {  cbClose(false, nInstance);  return cbRESULT_LIBINITERROR; }
    cb_sig_event_hnd[nIdx] = sem;
#endif

    cb_library_index[nInstance] = nIdx;
    cb_library_initialized[nIdx] = true;

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
    if (lpName == nullptr)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    // Test for availability of central application by attempting to open/close Central App Mutex
    HANDLE hCentralMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, lpName);
    CloseHandle(hCentralMutex);
    if (hCentralMutex == nullptr)
        cbRet = cbRESULT_NOCENTRALAPP;
#else
    {
        char szLockName[256] = {0};
        char * szTmpDir = getenv("TMPDIR");
        _snprintf(szLockName, sizeof(szLockName), "%s/%s.lock", szTmpDir == nullptr ? "/tmp" : szTmpDir, lpName);
        FILE * pflock = fopen(szLockName, "w+");
        if (pflock == nullptr)
            cbRet = cbRESULT_OK; // Assume root has the lock
        else if (flock(fileno(pflock), LOCK_EX | LOCK_NB) == 0)
            cbRet = cbRESULT_NOCENTRALAPP;
        if (pflock != nullptr)
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
cbRESULT cbAcquireSystemLock(const char * lpName, HANDLE & hLock)
{
    if (lpName == nullptr)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    // Try creating the system mutex
    HANDLE hMutex = CreateMutexA(nullptr, TRUE, lpName);
    if (hMutex == 0 || GetLastError() == ERROR_ACCESS_DENIED || GetLastError() == ERROR_ALREADY_EXISTS)
        return cbRESULT_SYSLOCK;
    hLock = hMutex;
#else
    // There are other methods such as sharedmemory but they break if application crash
    //  only a file lock seems resilient to crash, also with tmp mounted as tmpfs this is as fast as it could be
    char szLockName[256] = {0};
    char * szTmpDir = getenv("TMPDIR");
    _snprintf(szLockName, sizeof(szLockName), "%s/%s.lock", szTmpDir == nullptr ? "/tmp" : szTmpDir, lpName);
    FILE * pflock = fopen(szLockName, "w+");
    if (pflock == nullptr)
        return cbRESULT_SYSLOCK;
    if (flock(fileno(pflock), LOCK_EX | LOCK_NB) != 0)
    {
        return cbRESULT_SYSLOCK;
    }
    fprintf(pflock, "%u", static_cast<uint32_t>(getpid()));
    hLock = static_cast<void *>(pflock);
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
    if (lpName == nullptr)
        return cbRESULT_SYSLOCK;
#ifdef WIN32
    if (CloseHandle(hLock) == 0)
        return cbRESULT_SYSLOCK;
#else
    if (hLock)
    {
        char szLockName[256] = {0};
        const char * szTmpDir = getenv("TMPDIR");
        _snprintf(szLockName, sizeof(szLockName), "%s/%s.lock", szTmpDir == nullptr ? "/tmp" : szTmpDir, lpName);
        const auto pflock = static_cast<FILE *>(hLock);
        fclose(pflock); // Close mutex
        remove(szLockName); // Cleanup
    }
#endif
    hLock = nullptr;
    return cbRESULT_OK;
}

// Author & Date:   Ehsan Azar   15 March 2010
// Purpose: get instrument information.
//            If hardware is offline, detect local instrument if any
// Outputs:
//  instInfo - combination of cbINSTINFO_*
//  cbRESULT_OK if successful
cbRESULT cbGetInstInfo(uint32_t *instInfo, const uint32_t nInstance)
{
    *instInfo = 0;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    int type = cbINSTINFO_NPLAY;
    cbPROCINFO isInfo;
    if (cbGetProcInfo(cbNSP1, &isInfo, nInstance) == cbRESULT_OK)
    {
        if (strstr(isInfo.ident, "Cereplex") != nullptr)
            type = cbINSTINFO_CEREPLEX;
        else if (strstr(isInfo.ident, "Emulator") != nullptr)
            type = cbINSTINFO_EMULATOR;
        else if (strstr(isInfo.ident, "wNSP") != nullptr)
            type = cbINSTINFO_WNSP;
        if (strstr(isInfo.ident, "NSP1 ") != nullptr)
            type |= cbINSTINFO_NSP1;
    }

    if (cbCheckApp("cbNPlayMutex") == cbRESULT_OK)
    {
        *instInfo |= cbINSTINFO_LOCAL; // Local
        *instInfo |= type;
    }

    uint32_t flags = 0;
    // If online get more info (this will detect remote instruments)
    if (cbGetNplay(nullptr, nullptr, &flags, nullptr, nullptr, nullptr, nullptr, nInstance) == cbRESULT_OK)
    {
        if (flags != cbNPLAY_FLAG_NONE) // If nPlay is running
            *instInfo |= type;
    }

    // Sysinfo is the last config packet
    if (cb_cfg_buffer_ptr[nIdx]->sysinfo.cbpkt_header.chid == 0)
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
cbRESULT cbGetLatency(uint32_t *nLatency, const uint32_t nInstance)
{
    *nLatency = 0;

    uint32_t spikelen;
    if (const cbRESULT res = cbGetSpikeLength(&spikelen, nullptr, nullptr, nInstance))
        return res;
    if (nLatency) *nLatency = (2 * spikelen + 16);
    return cbRESULT_OK;
}

cbRESULT cbMakePacketReadingBeginNow(const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];
    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Initialize read indices to the current head position
    cb_recbuff_tailwrap[nIdx]  = cb_rec_buffer_ptr[nIdx]->headwrap;
    cb_recbuff_tailindex[nIdx] = cb_rec_buffer_ptr[nIdx]->headindex;
    cb_recbuff_processed[nIdx] = cb_rec_buffer_ptr[nIdx]->received;
    cb_recbuff_lasttime[nIdx]  = cb_rec_buffer_ptr[nIdx]->lasttime;

    return cbRESULT_OK;
}

cbRESULT cbClose(const bool bStandAlone, const uint32_t nInstance)
{
    if (nInstance >= cbMAXOPEN)
        return cbRESULT_INSTINVALID;
    const uint32_t nIdx = cb_library_index[nInstance];

    // clear lib init flag variable
    cb_library_initialized[nIdx] = false;

    // destroy the shared neuromatic memory and synchronization objects
    DestroySharedObjects(bStandAlone, nInstance);
    // If it is stand-alone application
    if (bStandAlone)
    {
        char buf[256] = {0};
        if (nInstance == 0)
            _snprintf(buf, sizeof(buf), "cbSharedDataMutex");
        else
            _snprintf(buf, sizeof(buf), "cbSharedDataMutex%d", nInstance);
        if (cb_sys_lock_hnd[nInstance])
            cbReleaseSystemLock(buf, cb_sys_lock_hnd[nInstance]);
        return cbRESULT_OK;
    }

    return cbRESULT_OK;
}


// Purpose: Processor Inquiry Function
//
cbRESULT cbGetProcInfo(uint32_t proc, cbPROCINFO *procinfo, const uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;
    if (!cb_cfg_buffer_ptr[nIdx]) return cbRESULT_NOLIBRARY;
    if (cb_cfg_buffer_ptr[nIdx]->procinfo[proc - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (procinfo) memcpy(procinfo, &(cb_cfg_buffer_ptr[nIdx]->procinfo[proc-1].idcode), sizeof(cbPROCINFO));
    return cbRESULT_OK;
}


// Purpose: Bank Inquiry Function
//
cbRESULT cbGetBankInfo(uint32_t proc, uint32_t bank, cbBANKINFO *bankinfo, const uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS) || ((bank - 1) >= cb_cfg_buffer_ptr[nIdx]->procinfo[0].bankcount)) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->bankinfo[proc - 1][bank - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (bankinfo) memcpy(bankinfo, &(cb_cfg_buffer_ptr[nIdx]->bankinfo[proc - 1][bank - 1].idcode), sizeof(cbBANKINFO));
    return cbRESULT_OK;
}


uint32_t GetInstrumentLocalChan(uint32_t nChan, const uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];
    return cb_cfg_buffer_ptr[nIdx]->chaninfo[nChan - 1].chan;
}


// Purpose:
// Retreives the total number of channels in the system
// Returns: cbRESULT_OK if data successfully retreived.
//          cbRESULT_NOLIBRARY if the library was not properly initialized.
cbRESULT cbGetChanCount(uint32_t *count, const uint32_t nInstance)
{
    uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Sweep through the processor information banks and sum up the number of channels
    *count = 0;
    for(const auto & p : cb_cfg_buffer_ptr[nIdx]->procinfo)
        if (p.cbpkt_header.chid) *count += p.chancount;

    return cbRESULT_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Systemwide Inquiry and Configuration Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////
cbRESULT cbSendPacketToInstrument(void * pPacket, const uint32_t nInstance, uint32_t nInstrument)
{
#ifndef CBPROTO_311
    auto * pPkt = static_cast<cbPKT_GENERIC*>(pPacket);
    pPkt->cbpkt_header.instrument = nInstrument;
#endif
    return cbSendPacket(pPacket, nInstance);
}

cbRESULT cbSendPacket(void * pPacket, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    auto * pPkt = static_cast<cbPKT_GENERIC*>(pPacket);

    // The logic here is quite complicated. Data is filled in from other processes
    // in a 2 pass mode. First they fill all except they skip the first sizeof(PROCTIME) bytes.
    // The final step in the process is to convert the 1st PROCTIME from "0" to some other number.
    // This step is done in a thread-safe manner
    // Consequently, all packets can not have "0" as the first PROCTIME. At the time of writing,
    // We were looking at the "time" value of a packet.

    pPkt->cbpkt_header.time = cb_rec_buffer_ptr[nIdx]->lasttime;

    // Time cannot be equal to 0
    if (pPkt->cbpkt_header.time == 0)
        pPkt->cbpkt_header.time = 1;

    ASSERT( *(static_cast<PROCTIME const *>(pPacket)) != 0);


    uint32_t quadletcount = pPkt->cbpkt_header.dlen + cbPKT_HEADER_32SIZE;
    uint32_t orig_headindex;

    // give 3 attempts to claim a spot in the circular xmt buffer for the packet
    int insertionattempts = 0;
    static const int NUM_INSERTION_ATTEMPTS = 3;
    while (true)
    {
        bool bTryToFill = false;        // Should I try to stuff data into the queue, or is it filled
                                        // TRUE = go ahead a try; FALSE = wait a bit

        // Copy the current buffer indices
        // NOTE: may want to use a 64-bit transfer to atomically get indices.  Otherwise, grab
        // tail index first to get "worst case" scenario;
        uint32_t orig_tailindex = cb_xmt_global_buffer_ptr[nIdx]->tailindex;
        orig_headindex = cb_xmt_global_buffer_ptr[nIdx]->headindex;

        // Compute the new head index at after the target packet position.
        // If the new head index is within (cbCER_UDP_SIZE_MAX / 4) quadlets of the buffer end, wrap it around.
        // Also, make sure that we are not wrapping around the tail pointer
        uint32_t mod_headindex = orig_headindex + quadletcount;
        uint32_t nLastOKPosition = cb_xmt_global_buffer_ptr[nIdx]->last_valid_index;
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
            auto * pDest = reinterpret_cast<int32_t *>(&cb_xmt_global_buffer_ptr[nIdx]->headindex);
            auto  dwExch = static_cast<int32_t>(mod_headindex);
            auto  dwComp = static_cast<int32_t>(orig_headindex);
#ifdef WIN32
            if ((int32_t)InterlockedCompareExchange((volatile unsigned long *)pDest, (unsigned long)dwExch, (unsigned long)dwComp) == dwComp)
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
    // The Central App will not transmit the packet while the first uint32_t is zero.
    memcpy(&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex+1]), static_cast<uint32_t *>(pPacket)+1, ((quadletcount-1)<<2) );

    // atomically copy the first 4 bytes of the packet
#ifdef WIN32
    InterlockedExchange((LPLONG)&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#else
    // buffer is uint32_t[] which guarantees 4-byte alignment, but compiler can't prove it at compile-time
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Watomic-alignment"
    __atomic_exchange_n(reinterpret_cast<int32_t *>(&(cb_xmt_global_buffer_ptr[nIdx]->buffer[orig_headindex])),*static_cast<int32_t *>(pPacket), __ATOMIC_SEQ_CST);
    #pragma clang diagnostic pop
#endif
    return cbRESULT_OK;
}


// Author & Date:   Kirk Korver     17 Jun 2003
// Purpose: send a packet only to ourselves (think IPC)
cbRESULT cbSendLoopbackPacket(void * pPacket, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    auto * pPkt = static_cast<cbPKT_GENERIC*>(pPacket);

    // The logic here is quite complicated. Data is filled in from other processes
    // in a 2 pass mode. First they fill all except they skip the first 4 bytes.
    // The final step in the process is to convert the 1st dword from "0" to some other number.
    // This step is done in a thread-safe manner
    // Consequently, all packets can not have "0" as the first DWORD. At the time of writing,
    // We were looking at the "time" value of a packet.

    pPkt->cbpkt_header.time = cb_rec_buffer_ptr[nIdx]->lasttime;

    // Time cannot be equal to 0
    if (pPkt->cbpkt_header.time == 0)
        pPkt->cbpkt_header.time = 1;

    ASSERT( *(static_cast<unsigned int const *>(pPacket)) != 0);

    const uint32_t quadletcount = pPkt->cbpkt_header.dlen + cbPKT_HEADER_32SIZE;
    uint32_t orig_headindex;

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
        const uint32_t orig_tailindex = cb_xmt_local_buffer_ptr[nIdx]->tailindex;
        orig_headindex = cb_xmt_local_buffer_ptr[nIdx]->headindex;

        // Compute the new head index at after the target packet position.
        // If the new head index is within (cbCER_UDP_SIZE_MAX / 4) quadlets of the buffer end, wrap it around.
        // Also, make sure that we are not wrapping around the tail pointer, if we are, next if will get it
        uint32_t mod_headindex = orig_headindex + quadletcount;
        uint32_t nLastOKPosition = cb_xmt_local_buffer_ptr[nIdx]->last_valid_index;
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
            auto * pDest = reinterpret_cast<int32_t *>(&cb_xmt_local_buffer_ptr[nIdx]->headindex);
            auto dwExch = static_cast<int32_t>(mod_headindex);
            auto dwComp = static_cast<int32_t>(orig_headindex);
#ifdef WIN32
            if ((int32_t)InterlockedCompareExchange((volatile unsigned long *)pDest, (unsigned long)dwExch, (unsigned long)dwComp) == dwComp)
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
    // The Central App will not transmit the packet while the first uint32_t is zero.
    memcpy(&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex+1]), static_cast<uint32_t *>(pPacket)+1, ((quadletcount-1)<<2) );

    // atomically copy the first 4 bytes of the packet
#ifdef WIN32
    InterlockedExchange((LPLONG)&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex]),*((LONG*)pPacket));
#else
    // buffer is uint32_t[] which guarantees 4-byte alignment, but compiler can't prove it at compile-time
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Watomic-alignment"
    __atomic_exchange_n(reinterpret_cast<int32_t *>(&(cb_xmt_local_buffer_ptr[nIdx]->buffer[orig_headindex])),*static_cast<int32_t *>(pPacket), __ATOMIC_SEQ_CST);
    #pragma clang diagnostic pop
#endif

    return cbRESULT_OK;
}



cbRESULT cbGetSystemRunLevel(uint32_t *runlevel, uint32_t *runflags, uint32_t *resetque, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached packet available and initialized
    if ((cb_cfg_buffer_ptr[nIdx]->sysinfo.cbpkt_header.chid == 0)||(cb_cfg_buffer_ptr[nIdx]->sysinfo.runlevel == 0))
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


cbRESULT cbSetSystemRunLevel(const uint32_t runlevel, const uint32_t runflags, const uint32_t resetque, const uint8_t nInstrument, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_SYSINFO sysinfo;
    sysinfo.cbpkt_header.time     = cb_rec_buffer_ptr[nIdx]->lasttime;
    sysinfo.cbpkt_header.chid     = 0x8000;
    sysinfo.cbpkt_header.type     = cbPKTTYPE_SYSSETRUNLEV;
    sysinfo.cbpkt_header.dlen     = cbPKTDLEN_SYSINFO;
    sysinfo.runlevel = runlevel;
    sysinfo.resetque = resetque;
    sysinfo.runflags = runflags;

    // Enter the packet into the XMT buffer queue
    return cbSendPacketToInstrument(&sysinfo, nInstance, nInstrument);
}


cbRESULT cbGetSystemClockFreq(uint32_t *freq, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // hardwire the rate to 30ksps
    *freq = 30000;

    return cbRESULT_OK;
}


cbRESULT cbGetSystemClockTime(PROCTIME *time, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *time = cb_rec_buffer_ptr[nIdx]->lasttime;

    return cbRESULT_OK;
}

cbRESULT cbSetComment(const uint8_t charset, const uint32_t rgba, const PROCTIME time, const char* comment, const uint32_t nInstance)
{
    cbRESULT bRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_COMMENT pktComment = {};
    pktComment.cbpkt_header.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktComment.cbpkt_header.chid           = 0x8000;
    pktComment.cbpkt_header.type           = cbPKTTYPE_COMMENTSET;
    pktComment.info.charset   = charset;
#ifndef CBPROTO_311
    pktComment.rgba = rgba;
    pktComment.timeStarted = time;
// else pktComment.info.flags, pktComment.data
#endif
    uint32_t nLen = 0;
    if (comment)
        strncpy(pktComment.comment, comment, cbMAX_COMMENT);
    pktComment.comment[cbMAX_COMMENT - 1] = 0;
    nLen = static_cast<uint32_t>(strlen(pktComment.comment)) + 1;      // include terminating null
    pktComment.cbpkt_header.dlen           = static_cast<uint32_t>(cbPKTDLEN_COMMENTSHORT) + (nLen + 3) / 4;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == bRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            bRes = cbSendPacketToInstrument(&pktComment, nInstance, nProc - 1);
    }
    return bRes;
}

cbRESULT cbGetLncParameters(const uint32_t nProc, uint32_t* nLncFreq, uint32_t* nLncRefChan, uint32_t* nLncGMode, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isLnc[nProc - 1].cbpkt_header.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (nLncFreq)     *nLncFreq = cb_cfg_buffer_ptr[nIdx]->isLnc[nProc - 1].lncFreq;
    if (nLncRefChan)  *nLncRefChan = cb_cfg_buffer_ptr[nIdx]->isLnc[nProc - 1].lncRefChan;
    if (nLncGMode)    *nLncGMode = cb_cfg_buffer_ptr[nIdx]->isLnc[nProc - 1].lncGlobalMode;
    return cbRESULT_OK;
}


cbRESULT cbSetLncParameters(const uint32_t nProc, const uint32_t nLncFreq, const uint32_t nLncRefChan, const uint32_t nLncGMode, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_LNC pktLnc;
    pktLnc.cbpkt_header.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktLnc.cbpkt_header.chid           = 0x8000;
    pktLnc.cbpkt_header.type           = cbPKTTYPE_LNCSET;
    pktLnc.cbpkt_header.dlen           = cbPKTDLEN_LNC;
    pktLnc.lncFreq        = nLncFreq;
    pktLnc.lncRefChan     = nLncRefChan;
    pktLnc.lncGlobalMode  = nLncGMode;

    // Enter the packet into the XMT buffer queue
    return cbSendPacketToInstrument(&pktLnc, nInstance, nProc - 1);
}

cbRESULT cbGetNplay(char *fname, float *speed, uint32_t *flags, PROCTIME *ftime, PROCTIME *stime, PROCTIME *etime, PROCTIME * filever, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isNPlay.cbpkt_header.chid == 0) return cbRESULT_HARDWAREOFFLINE;

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

cbRESULT cbSetNplay(const char *fname, const float speed, const uint32_t mode, const PROCTIME val, const PROCTIME stime, const PROCTIME etime, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->isNPlay.cbpkt_header.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // Create the packet data structure and fill it in
    cbPKT_NPLAY pktNPlay;
    pktNPlay.cbpkt_header.time           = cb_rec_buffer_ptr[nIdx]->lasttime;
    pktNPlay.cbpkt_header.chid           = 0x8000;
    pktNPlay.cbpkt_header.type           = cbPKTTYPE_NPLAYSET;
    pktNPlay.cbpkt_header.dlen           = cbPKTDLEN_NPLAY;
    pktNPlay.speed          = speed;
    pktNPlay.mode           = mode;
    pktNPlay.flags          = cbNPLAY_FLAG_NONE; // No flags here
    pktNPlay.val            = val;
    pktNPlay.stime          = stime;
    pktNPlay.etime          = etime;
    pktNPlay.fname[0]       = 0;
    if (fname) strncpy(pktNPlay.fname, fname, cbNPLAY_FNAME_LEN);

    // Enter the packet into the XMT buffer queue
    return cbSendPacketToInstrument(&pktNPlay, nInstance, cbNSP1 - 1);
}

// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   id - video source ID (1 to cbMAXVIDEOSOURCE)
// Outputs:
//   name - name of the video source
//   fps  - the frame rate of the video source
cbRESULT cbGetVideoSource(char *name, float *fps, const uint32_t id, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbGetTrackObj(char *name, uint16_t *type, uint16_t *pointCount, const uint32_t id, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbSetVideoSource(const char *name, const float fps, const uint32_t id, const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_NM pktNm = {};
    pktNm.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pktNm.cbpkt_header.type = cbPKTTYPE_NMSET;
    pktNm.cbpkt_header.dlen = cbPKTDLEN_NM;
    pktNm.mode = cbNM_MODE_SETVIDEOSOURCE;
    pktNm.flags = id + 1;
    pktNm.value = static_cast<uint32_t>(fps * 1000); // frame rate times 1000
    strncpy(pktNm.name, name, cbLEN_STR_LABEL);

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&pktNm, nInstance, nProc - 1);
    }
    return cbRes;
}

// Author & Date: Ehsan Azar       25 May 2010
// Inputs:
//   id - trackable object ID (start from 0)
//   name - name of the video source
//   type - type of the trackable object (start from 0)
//   pointCount - the maximum number of points for this trackable object
cbRESULT cbSetTrackObj(const char *name, const uint16_t type, const uint16_t pointCount, const uint32_t id, const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_NM pktNm = {};
    pktNm.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pktNm.cbpkt_header.type = cbPKTTYPE_NMSET;
    pktNm.cbpkt_header.dlen = cbPKTDLEN_NM;
    pktNm.mode = cbNM_MODE_SETTRACKABLE;
    pktNm.flags = id + 1;
    pktNm.value = type | (pointCount << 16);
    strncpy(pktNm.name, name, cbLEN_STR_LABEL);

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&pktNm, nInstance, nProc - 1);
    }
    return cbRes;
}

cbRESULT cbGetSpikeLength(uint32_t *length, uint32_t *pretrig, uint32_t * pSysfreq, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Check for cached sysinfo packet initialized
    if (cb_cfg_buffer_ptr[nIdx]->sysinfo.cbpkt_header.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (length)      *length      = cb_cfg_buffer_ptr[nIdx]->sysinfo.spikelen;
    if (pretrig)     *pretrig     = cb_cfg_buffer_ptr[nIdx]->sysinfo.spikepre;
    if (pSysfreq)    *pSysfreq    = cb_cfg_buffer_ptr[nIdx]->sysinfo.sysfreq;
    return cbRESULT_OK;
}


cbRESULT cbSetSpikeLength(const uint32_t length, const uint32_t pretrig, const uint32_t nInstance)
{
    cbRESULT bRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Create the packet data structure and fill it in
    cbPKT_SYSINFO sysinfo;
    sysinfo.cbpkt_header.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    sysinfo.cbpkt_header.chid        = 0x8000;
    sysinfo.cbpkt_header.type        = cbPKTTYPE_SYSSETSPKLEN;
    sysinfo.cbpkt_header.dlen        = cbPKTDLEN_SYSINFO;
    sysinfo.spikelen    = length;
    sysinfo.spikepre    = pretrig;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == bRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            bRes = cbSendPacketToInstrument(&sysinfo, nInstance, nProc - 1);  // Enter the packet into the XMT buffer queue
    }
    return bRes;
}

// Author & Date: Ehsan Azar       16 Jan 2012
// Inputs:
//   channel  - 1-based channel number
//   mode     - waveform type
//   repeats  - number of repeats
//   trig     - trigget type
//   trigChan - Channel for trigger
//   wave     - waveform data
cbRESULT cbGetAoutWaveform(uint32_t channel, uint8_t  const trigNum, uint16_t  * mode, uint32_t  * repeats, uint16_t  * trig,
                           uint16_t  * trigChan, uint16_t  * trigValue, cbWaveformData * wave, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];
    uint32_t wavenum = 0;

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].aoutcaps & cbAOUT_WAVEFORM)) return cbRESULT_INVALIDFUNCTION;
    if (trigNum >= cbMAX_AOUT_TRIGGER) return cbRESULT_INVALIDFUNCTION;
    //hls channels not in contiguous order anymore  if (channel <= cb_pc_status_buffer_ptr[0]->GetNumAnalogChans()) return cbRESULT_INVALIDCHANNEL;
    if (cbRESULT_OK != cbGetAoutWaveformNumber(channel, &wavenum)) return cbRESULT_INVALIDCHANNEL;
    channel = wavenum;
    //hls channel -= (cb_pc_status_buffer_ptr[0]->GetNumAnalogChans() + 1); // make it 0-based
    if (channel >= AOUT_NUM_GAIN_CHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].cbpkt_header.type == 0) return cbRESULT_INVALIDCHANNEL;

    // otherwise, return the data
    if (mode)     *mode = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].mode;
    if (repeats)  *repeats = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].repeats;
    if (trig)     *trig = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trig;
    if (trigChan) *trigChan = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trigChan;
    if (trigValue) *trigValue = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].trigValue;
    if (wave)     *wave = cb_cfg_buffer_ptr[nIdx]->isWaveform[channel][trigNum].wave;
    return cbRESULT_OK;
}


cbRESULT cbGetAoutWaveformNumber(const uint32_t channel, uint32_t* wavenum, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];
    uint32_t nWaveNum = 0;

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[channel - 1].aoutcaps & cbAOUT_WAVEFORM)) return cbRESULT_INVALIDFUNCTION;

    for (uint32_t nChan = 0; nChan < cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans(); ++nChan)
    {
        if (IsChanAnalogOut(nChan) || IsChanAudioOut(nChan))
        {
            if (nChan == channel)
            {
                break;
            }
            ++nWaveNum;
        }
    }

    if (nWaveNum >= AOUT_NUM_GAIN_CHANS) return cbRESULT_INVALIDCHANNEL;

    // otherwise, return the data
    if (wavenum)    *wavenum = nWaveNum;
    return cbRESULT_OK;
}


cbRESULT cbGetFilterDesc(const uint32_t proc, const uint32_t filt, cbFILTDESC *filtdesc, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;
    if ((filt-1) >= cbMAXFILTS) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->filtinfo[proc-1][filt-1].cbpkt_header.chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    memcpy(filtdesc,&(cb_cfg_buffer_ptr[nIdx]->filtinfo[proc-1][filt-1].label[0]),sizeof(cbFILTDESC));
    return cbRESULT_OK;
}

cbRESULT cbGetFileInfo(cbPKT_FILECFG * filecfg, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if (cb_cfg_buffer_ptr[nIdx]->fileinfo.cbpkt_header.chid == 0) return cbRESULT_HARDWAREOFFLINE;

    // otherwise, return the data
    if (filecfg) *filecfg = cb_cfg_buffer_ptr[nIdx]->fileinfo;
    return cbRESULT_OK;
}

cbRESULT cbGetSampleGroupInfo(const uint32_t proc, const uint32_t group, char *label, uint32_t *period, uint32_t* length, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS)||((group - 1) >= cbMAXGROUPS)) return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (label)  memcpy(label,cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].label, cbLEN_STR_LABEL);
    if (period) *period = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].period;
    if (length) *length = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].length;
    return cbRESULT_OK;
}


cbRESULT cbGetSampleGroupList(const uint32_t proc, const uint32_t group, uint32_t *length, uint16_t *list, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if (((proc - 1) >= cbMAXPROCS)||((group - 1) >= cbMAXGROUPS))
        return cbRESULT_INVALIDADDRESS;
    if (cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].cbpkt_header.chid == 0)
        return cbRESULT_INVALIDADDRESS;

    // otherwise, return the data
    if (length)
        *length = cb_cfg_buffer_ptr[nIdx]->groupinfo[proc - 1][group - 1].length;

    if (list)
        memcpy(list, &(cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].list[0]),
                    cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].length * sizeof(cb_cfg_buffer_ptr[nIdx]->groupinfo[proc-1][group-1].list[0]));

    return cbRESULT_OK;
}

cbRESULT cbGetChanLoc(const uint32_t chan, uint32_t *proc, uint32_t *bank, char *banklabel, uint32_t *term, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that requested procinfo structure is not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    const uint32_t nProcessor = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].proc;
    const uint32_t nBank = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].bank;

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

cbRESULT cbGetChanCaps(const uint32_t chan, uint32_t *chancaps, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (chancaps) *chancaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps;

    return cbRESULT_OK;
}

cbRESULT cbGetChanLabel(const uint32_t chan, char *label, uint32_t *userflags, int32_t *position,const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (label) memcpy(label,cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].label, cbLEN_STR_LABEL);
    if (userflags) *userflags = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].userflags;
    if (position) memcpy(position,&(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].position[0]),4*sizeof(int32_t));

    return cbRESULT_OK;
}

cbRESULT cbSetChanLabel(const uint32_t chan, const char *label, const uint32_t userflags, const int32_t *position, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETLABEL;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan                   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    memcpy(chaninfo.label, label, cbLEN_STR_LABEL);
    chaninfo.userflags = userflags;
    if (position)
        memcpy(&chaninfo.position, position, 4 * sizeof(int32_t));
    else
        memcpy(&chaninfo.position, &(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].position[0]), 4 * sizeof(int32_t));

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetChanUnitMapping(const uint32_t chan, cbMANUALUNITMAPPING *unitmapping, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (unitmapping)
        memcpy(unitmapping, &cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].unitmapping[0], cbMAXUNITS * sizeof(cbMANUALUNITMAPPING));

    return cbRESULT_OK;
}


cbRESULT cbSetChanUnitMapping(const uint32_t chan, const cbMANUALUNITMAPPING *unitmapping, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // if null pointer, nothing to do so return cbRESULT_OK
    if (!unitmapping) return cbRESULT_OK;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETUNITOVERRIDES;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan                    = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    if (unitmapping)
        memcpy(&chaninfo.unitmapping, unitmapping, cbMAXUNITS * sizeof(cbMANUALUNITMAPPING));

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetChanNTrodeGroup(const uint32_t chan, uint32_t *NTrodeGroup, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (NTrodeGroup) *NTrodeGroup = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkgroup;

    return cbRESULT_OK;
}


cbRESULT cbSetChanNTrodeGroup(const uint32_t chan, const uint32_t NTrodeGroup, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETNTRODEGROUP;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFOSHORT;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan                    = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    if (0 == NTrodeGroup)
        chaninfo.spkgroup = 0;
    else
        chaninfo.spkgroup = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[NTrodeGroup - 1].ntrode;  //NTrodeGroup;
    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

cbRESULT cbGetChanAmplitudeReject(const uint32_t chan, cbAMPLITUDEREJECT *AmplitudeReject, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (AmplitudeReject)
    {
        AmplitudeReject->bEnabled = cbAINPSPK_REJAMPL == (cbAINPSPK_REJAMPL & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts);
        AmplitudeReject->nAmplPos = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].amplrejpos;
        AmplitudeReject->nAmplNeg = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].amplrejneg;
    }

    return cbRESULT_OK;
}


cbRESULT cbSetChanAmplitudeReject(const uint32_t chan, const cbAMPLITUDEREJECT AmplitudeReject, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETREJECTAMPLITUDE;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFOSHORT;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument =  cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan      = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.spkopts   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts & ~cbAINPSPK_REJAMPL;
    chaninfo.spkopts  |= AmplitudeReject.bEnabled ? cbAINPSPK_REJAMPL : 0;
    chaninfo.amplrejpos = AmplitudeReject.nAmplPos;
    chaninfo.amplrejneg = AmplitudeReject.nAmplNeg;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Author & Date:   Ehsan Azar     22 Jan 2013
// Purpose: Get full channel config
// Inputs:
//   chan - channel number (1-based)
// Outputs:
//   chaninfo   - shared segment size to create
//   Returns the error code
cbRESULT cbGetChanInfo(const uint32_t chan, cbPKT_CHANINFO *pChanInfo, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (pChanInfo) memcpy(pChanInfo, &(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1]), sizeof(cbPKT_CHANINFO));

    return cbRESULT_OK;
}

cbRESULT cbGetChanAutoThreshold(const uint32_t chan, uint32_t *bEnabled, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (bEnabled)
        *bEnabled = (cbAINPSPK_THRAUTO == (cbAINPSPK_THRAUTO & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts));

    return cbRESULT_OK;
}


cbRESULT cbSetChanAutoThreshold(const uint32_t chan, const uint32_t bEnabled, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // if null pointer, nothing to do so return cbRESULT_OK
    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETAUTOTHRESHOLD;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFOSHORT;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan      = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.spkopts   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts & ~cbAINPSPK_THRAUTO;
    chaninfo.spkopts  |= bEnabled ? cbAINPSPK_THRAUTO : 0;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}


cbRESULT cbGetNTrodeInfo(const uint32_t ntrode, char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS],
                         uint16_t * nSite, uint16_t * chans, uint16_t * fs, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXNTRODES) return cbRESULT_INVALIDNTRODE;
    if (cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode-1].cbpkt_header.chid == 0) return cbRESULT_INVALIDNTRODE;

    if (label)  memcpy(label, cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].label, cbLEN_STR_LABEL);
    int n_size = sizeof(cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses);
    if (ellipses) memcpy(ellipses, &cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses[0][0], n_size);
    if (nSite) *nSite = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].nSite;
    if (chans) memcpy(chans, cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].nChan, cbMAXSITES * sizeof(uint16_t));
    if (fs) *fs = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs;
    return cbRESULT_OK;
}

cbRESULT cbSetNTrodeInfo( const uint32_t ntrode, const char *label, cbMANUALUNITMAPPING ellipses[][cbMAXUNITS], const uint16_t fs, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXCHANS/2) return cbRESULT_INVALIDNTRODE;

    // Create the packet data structure and fill it in
    cbPKT_NTRODEINFO ntrodeinfo;
    ntrodeinfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    ntrodeinfo.cbpkt_header.chid      = 0x8000;
    ntrodeinfo.cbpkt_header.type      = cbPKTTYPE_SETNTRODEINFO;
    ntrodeinfo.cbpkt_header.dlen      = cbPKTDLEN_NTRODEINFO;
    ntrodeinfo.ntrode = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ntrode;
    ntrodeinfo.fs        = fs;
    if (label) memcpy(ntrodeinfo.label, label, sizeof(ntrodeinfo.label));
    int size_ell = sizeof(ntrodeinfo.ellipses);
    if (ellipses)
        memcpy(ntrodeinfo.ellipses, ellipses, size_ell);
    else
        memset(ntrodeinfo.ellipses, 0, size_ell);

    return cbSendPacketToInstrument(&ntrodeinfo, nInstance, cbGetNTrodeInstrument(ntrode) - 1);
}


/// @author Hyrum L. Sessions
/// @date   May the Forth be with you 2020
/// @brief  Set the N-Trode label without affecting other data
cbRESULT cbSetNTrodeLabel(const uint32_t ntrode, const char* label, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the NTrode number is valid and initialized
    if ((ntrode - 1) >= cbMAXCHANS / 2) return cbRESULT_INVALIDNTRODE;

    // Create the packet data structure and fill it in
    cbPKT_NTRODEINFO ntrodeinfo;
    ntrodeinfo.cbpkt_header.time = cb_rec_buffer_ptr[nIdx]->lasttime;
    ntrodeinfo.cbpkt_header.chid = 0x8000;
    ntrodeinfo.cbpkt_header.type = cbPKTTYPE_SETNTRODEINFO;
    ntrodeinfo.cbpkt_header.dlen = cbPKTDLEN_NTRODEINFO;
    ntrodeinfo.ntrode = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ntrode;
    ntrodeinfo.fs = cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].fs;
    if (label) memcpy(ntrodeinfo.label, label, sizeof(ntrodeinfo.label));
    int size_ell = sizeof(ntrodeinfo.ellipses);
    memcpy(ntrodeinfo.ellipses, cb_cfg_buffer_ptr[nIdx]->isNTrodeInfo[ntrode - 1].ellipses, size_ell);

    return cbSendPacketToInstrument(&ntrodeinfo, nInstance, cbGetNTrodeInstrument(ntrode) - 1);
}


// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbGetDinpCaps(const uint32_t chan, uint32_t *dinpcaps, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (dinpcaps) *dinpcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].dinpcaps;

    return cbRESULT_OK;
}

// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbGetDinpOptions(const uint32_t chan, uint32_t *options, uint32_t *eopchar, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DINP)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].dinpopts;
    if (eopchar) *eopchar = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].eopchar;

    return cbRESULT_OK;
}


// Purpose: Digital Input Inquiry and Configuration Functions
//
cbRESULT cbSetDinpOptions(const uint32_t chan, const uint32_t options, const uint32_t eopchar, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DINP)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETDINP;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.dinpopts  = options;     // digital input options (composed of nmDINP_* flags)
    chaninfo.eopchar   = eopchar;     // digital input capablities (given by nmDINP_* flags)

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbGetDoutCaps(const uint32_t chan, uint32_t *doutcaps, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (doutcaps) *doutcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutcaps;

    return cbRESULT_OK;
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbGetDoutOptions(const uint32_t chan, uint32_t *options, uint32_t *monchan, int32_t *doutval,
                          uint8_t *triggertype, uint16_t *trigchan, uint16_t *trigval, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options)		*options		= cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutopts;
    if (monchan)
    {
        if ((cbDOUT_FREQUENCY & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutopts) ||
            (cbDOUT_TRIGGERED & cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].doutopts))
        {
#ifdef CBPROTO_311
            *monchan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource;
        }
        else {
            *monchan = cbGetExpandedChannelNumber(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource&0xfff,
                                                  (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource >> 16)&0xfff);
        }
#else
            *monchan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].moninst | (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monchan << 16);
            }
        else {
            *monchan = cbGetExpandedChannelNumber(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].moninst + 1,
                                                  cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monchan);
        }
#endif
    }
    if (doutval)			*doutval			= cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].outvalue;
    if (triggertype)    *triggertype	= cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].trigtype;
    if (trigchan)		*trigchan		= cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].trigchan;
    if (trigval)        *trigval		= cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].trigval;

    return cbRESULT_OK;
}

// Purpose: Digital Output Inquiry and Configuration Functions
//
cbRESULT cbSetDoutOptions(const uint32_t chan, const uint32_t options, uint32_t monchan, const int32_t doutval,
                          const uint8_t triggertype, const uint16_t trigchan, const uint16_t trigval, const uint32_t nInstance)
{
    cbRESULT nResult = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];
    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_DOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETDOUT;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.doutopts  = options;
    if ((cbDOUT_FREQUENCY & options) || (cbDOUT_TRIGGERED & options))
    {
#ifdef CBPROTO_311
        chaninfo.monsource = monchan;
#else
        chaninfo.moninst = monchan & 0xFFFF;
        chaninfo.monchan = (monchan >> 16) & 0xFFFF;
#endif
    }
    else
    {
        if (0 != monchan)
        {
#ifdef CBPROTO_311
            chaninfo.monsource = cbGetInstrumentLocalChannelNumber(monchan);
#else
            chaninfo.moninst = cbGetChanInstrument(monchan) - 1;
            chaninfo.monchan = cbGetInstrumentLocalChannelNumber(monchan);
#endif
        }
    }
    chaninfo.outvalue  = doutval;
    chaninfo.trigtype  = triggertype;
    chaninfo.trigchan  = trigchan;
    chaninfo.trigval   = trigval;

    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == nResult) && (NSP_FOUND == cbGetNspStatus(nProc)))
            nResult = cbSendPacketToInstrument(&chaninfo, nInstance, nProc - 1);
    }
    return nResult;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpCaps(const uint32_t chan, uint32_t *ainpcaps, cbSCALING *physcalin, cbFILTDESC *phyfiltin, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (ainpcaps)  *ainpcaps  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps;
    if (physcalin) *physcalin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].physcalin;
    if (phyfiltin) *phyfiltin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].phyfiltin;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpOpts(const uint32_t chan, uint32_t *ainpopts, uint32_t *LNCrate, uint32_t *refElecChan, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_LNC)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (ainpopts) *ainpopts = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpopts;
    if (LNCrate) *LNCrate = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].lncrate;
    if (refElecChan) *refElecChan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].refelecchan;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpOpts(const uint32_t chan, const uint32_t ainpopts, const uint32_t LNCrate, const uint32_t refElecChan, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_LNC)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time		 = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid		 = 0x8000;
    chaninfo.cbpkt_header.type		 = cbPKTTYPE_CHANSETAINP;
    chaninfo.cbpkt_header.dlen		 = cbPKTDLEN_CHANINFOSHORT;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan        = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.ainpopts    = ainpopts;
    chaninfo.lncrate	 = LNCrate;
    chaninfo.refelecchan = refElecChan;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpScaling(const uint32_t chan, cbSCALING *scaling, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (scaling)  *scaling  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalin;
    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpScaling(const uint32_t chan, const cbSCALING *scaling, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETSCALE;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
    chaninfo.chan      = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.scalin    = *scaling;
    chaninfo.scalout   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalout;

    // Enter the packet into the XMT buffer queue
    return cbSendPacketToInstrument(&chaninfo, nInstance, cbGetChanInstrument(chan) - 1);
}

// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbGetAinpDisplay(uint32_t chan, int32_t *smpdispmin, int32_t *smpdispmax, int32_t *spkdispmax, int32_t *lncdispmax, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if ((cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AINP) != cbCHAN_AINP) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the cfg buffer
    if (smpdispmin) *smpdispmin = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmin;
    if (smpdispmax) *smpdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpdispmax;
    if (spkdispmax) *spkdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkdispmax;
    if (lncdispmax) *lncdispmax = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].lncdispmax;

    return cbRESULT_OK;
}


// Purpose: Analog Input Inquiry and Configuration Functions
//
cbRESULT cbSetAinpDisplay(
    const uint32_t chan,
    const int32_t smpdispmin, const int32_t smpdispmax, const int32_t spkdispmax, const int32_t lncdispmax,
    const uint32_t nInstance
)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETDISP;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
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
cbRESULT cbSetAinpPreview(const uint32_t chan, const int32_t prevopts, const uint32_t nInstance)
{
    cbRESULT res = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

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
        if ((chan - 1) >= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()) return cbRESULT_INVALIDCHANNEL;
        if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
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
    packet.cbpkt_header.time     = cb_rec_buffer_ptr[nIdx]->lasttime;
    packet.cbpkt_header.type     = prevopts;
    packet.cbpkt_header.dlen     = 0;
    if (0 == chan)
    {
        packet.cbpkt_header.chid = 0x8000;
        // Send it to all NSPs
        for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
        {
            if ((cbRESULT_OK == res) && (NSP_FOUND == cbGetNspStatus(nProc)))
                res = cbSendPacketToInstrument(&packet, nInstance, nProc - 1);
        }
    }
    else
    {
        packet.cbpkt_header.chid = 0x8000 + cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
        // Enter the packet into the XMT buffer queue
        res = cbSendPacketToInstrument(&packet, nInstance, cbGetChanInstrument(chan) - 1);
    }

    return res;
}

// Purpose: AINP Sampling Stream Functions
cbRESULT cbGetAinpSampling(uint32_t chan, uint32_t *filter, uint32_t *group, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SMPSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer for non-null pointers
    if (group)  *group = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpgroup;
    if (filter) *filter = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].smpfilter;

    return cbRESULT_OK;
}

// Purpose: AINP Sampling Stream Functions
cbRESULT cbSetAinpSampling(uint32_t chan, uint32_t filter, uint32_t group, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SMPSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETSMP;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.smpfilter = filter;
    chaninfo.smpgroup  = group;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeCaps(uint32_t chan, uint32_t *spkcaps, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (spkcaps) *spkcaps = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeOptions(uint32_t chan, uint32_t *options, uint32_t *filter, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkopts;
    if (filter) *filter = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkfilter;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeOptions(uint32_t chan, uint32_t spkopts, uint32_t spkfilter, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].ainpcaps & cbAINP_SPKSTREAM)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETSPK;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.spkopts   = spkopts;
    chaninfo.spkfilter = spkfilter;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeThreshold(uint32_t chan, int32_t *spkthrlevel, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_EXTRACT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the cfg buffer
    if (spkthrlevel) *spkthrlevel = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkthrlevel;

    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeThreshold(uint32_t chan, int32_t spkthrlevel, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx])
        return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS)
        return cbRESULT_INVALIDCHANNEL;

    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0)
        return cbRESULT_INVALIDCHANNEL;

    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_EXTRACT))
        return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid        = 0x8000;
    chaninfo.cbpkt_header.type        = cbPKTTYPE_CHANSETSPKTHR;
    chaninfo.cbpkt_header.dlen        = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.spkthrlevel = spkthrlevel;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbGetAinpSpikeHoops(uint32_t chan, cbHOOP *hoops, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_HOOPSORT)) return cbRESULT_INVALIDFUNCTION;

    memcpy(hoops, &(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkhoops[0][0]),
        sizeof(cbHOOP)*cbMAXUNITS*cbMAXHOOPS );

    // Return the requested data from the cfg buffer
    return cbRESULT_OK;
}

// Purpose: AINP Spike Stream Functions
cbRESULT cbSetAinpSpikeHoops(const uint32_t chan, const cbHOOP *hoops, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the channel address is valid and initialized
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].spkcaps & cbAINPSPK_HOOPSORT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time        = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid        = 0x8000;
    chaninfo.cbpkt_header.type        = cbPKTTYPE_CHANSETSPKHPS;
    chaninfo.cbpkt_header.dlen        = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;

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
cbRESULT cbGetAoutCaps( uint32_t chan, uint32_t *aoutcaps, cbSCALING *physcalout, cbFILTDESC *phyfiltout, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;

    // Return the requested data from the rec buffer
    if (aoutcaps)   *aoutcaps  = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].aoutcaps;
    if (physcalout) *physcalout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].physcalout;
    if (phyfiltout) *phyfiltout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].phyfiltout;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbGetAoutScaling(uint32_t chan, cbSCALING *scalout, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (scalout) *scalout = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalout;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbSetAoutScaling(const uint32_t chan, const cbSCALING *scaling, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time      = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid      = 0x8000;
    chaninfo.cbpkt_header.type      = cbPKTTYPE_CHANSETSCALE;
    chaninfo.cbpkt_header.dlen      = cbPKTDLEN_CHANINFO;
#ifndef CBPROTO_311
    chaninfo.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.instrument;
#endif
    chaninfo.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.scalin    = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].scalin;
    chaninfo.scalout   = *scaling;

    // Enter the packet into the XMT buffer queue
    return cbSendPacket(&chaninfo, nInstance);
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbGetAoutOptions(const uint32_t chan, uint32_t *options, uint32_t *monchan, int32_t *value, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Return the requested data from the rec buffer
    if (options) *options = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].aoutopts;
#ifdef CBPROTO_311
    if (monchan) *monchan = (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monsource >> 16) & 0xFFFF;
#else
    if (monchan) *monchan = cbGetExpandedChannelNumber(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].moninst + 1, cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].monchan);
#endif
    if (value)   *value   = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].outvalue;

    return cbRESULT_OK;
}

// Purpose: Analog Output Inquiry and Configuration Functions
//
cbRESULT cbSetAoutOptions(uint32_t chan, const uint32_t options, const uint32_t monchan, const int32_t value, const uint32_t nInstance)
{
    cbRESULT nResult = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the addresses are valid and that necessary structures are not empty
    if ((chan - 1) >= cbMAXCHANS) return cbRESULT_INVALIDCHANNEL;
    // If cb_cfg_buffer_ptr was built for 128-channel system, but passed in channel is for 256-channel firmware.
    // TODO: Again, maybe we need a m_ChanIdxInType array.
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT) && (chan > (cbNUM_FE_CHANS / 2)))
        chan -= (cbNUM_FE_CHANS / 2);
    if (cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].cbpkt_header.chid == 0) return cbRESULT_INVALIDCHANNEL;
    if (!(cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chancaps & cbCHAN_AOUT)) return cbRESULT_INVALIDFUNCTION;

    // Create the packet data structure and fill it in
    cbPKT_CHANINFO chaninfo;
    chaninfo.cbpkt_header.time       = cb_rec_buffer_ptr[nIdx]->lasttime;
    chaninfo.cbpkt_header.chid       = 0x8000;
    chaninfo.cbpkt_header.type       = cbPKTTYPE_CHANSETAOUT;
    chaninfo.cbpkt_header.dlen       = cbPKTDLEN_CHANINFO;
    chaninfo.chan       = cb_cfg_buffer_ptr[nIdx]->chaninfo[chan - 1].chan;
    chaninfo.aoutopts   = options;
#ifdef CBPROTO_311
    chaninfo.monsource = (0 == monchan) ? 0 : cbGetInstrumentLocalChannelNumber(monchan);
#else
    chaninfo.moninst = (0 == monchan) ? 0 : cbGetChanInstrument(monchan) - 1;
    chaninfo.monchan = (0 == monchan) ? 0 : cbGetInstrumentLocalChannelNumber(monchan);
#endif
    chaninfo.outvalue   = value;

    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == nResult) && (NSP_FOUND == cbGetNspStatus(nProc)))
            nResult = cbSendPacketToInstrument(&chaninfo, nInstance, nProc - 1);
    }
    return nResult;
}

// Author & Date:   Kirk Korver     26 Apr 2005
// Purpose: Request that the ENTIRE sorting model be updated
cbRESULT cbGetSortingModel(const uint32_t nInstance)
{
    cbRESULT ret = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Send out the request for the sorting rules
    cbPKT_SS_MODELALLSET isPkt;
    isPkt.cbpkt_header.time = cb_rec_buffer_ptr[nIdx]->lasttime;
    isPkt.cbpkt_header.chid = 0x8000;
    isPkt.cbpkt_header.type = cbPKTTYPE_SS_MODELALLSET;
    isPkt.cbpkt_header.dlen = 0;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == ret) && (NSP_FOUND == cbGetNspStatus(nProc)))
            ret = cbSendPacketToInstrument(&isPkt, nInstance, nProc - 1);
    }

    // FIXME: relying on sleep is racy, refactor the code
    Sleep(250);           // give the "model" packets a chance to show up

    return ret;
}


// Author & Date:   Hyrum L. Sessions   22 Apr 2009
// Purpose: Request that the ENTIRE sorting model be updated
cbRESULT cbGetFeatureSpaceDomain(const uint32_t nInstance)
{
    cbRESULT ret = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Send out the request for the sorting rules
    cbPKT_FS_BASIS isPkt;
    isPkt.cbpkt_header.time = cb_rec_buffer_ptr[nIdx]->lasttime;
    isPkt.cbpkt_header.chid = 0x8000;
    isPkt.cbpkt_header.type = cbPKTTYPE_FS_BASISSET;
    isPkt.cbpkt_header.dlen = cbPKTDLEN_FS_BASISSHORT;

    isPkt.chan = 0;
    isPkt.mode = cbBASIS_CHANGE;
    isPkt.fs = cbAUTOALG_PCA;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == ret) && (NSP_FOUND == cbGetNspStatus(nProc)))
            ret = cbSendPacketToInstrument(&isPkt, nInstance, nProc - 1);
    }

    // FIXME: relying on sleep is racy, refactor the code
    Sleep(250);           // give the packets a chance to show up

    return ret;
}

void GetAxisLengths(const cbPKT_SS_NOISE_BOUNDARY* pPkt, float afAxisLen[3])
{
    afAxisLen[0] = sqrt(pPkt->afS[0][0] * pPkt->afS[0][0] +
                        pPkt->afS[0][1] * pPkt->afS[0][1] +
                        pPkt->afS[0][2] * pPkt->afS[0][2]);
    afAxisLen[1] = sqrt(pPkt->afS[1][0] * pPkt->afS[1][0] +
                        pPkt->afS[1][1] * pPkt->afS[1][1] +
                        pPkt->afS[1][2] * pPkt->afS[1][2]);
    afAxisLen[2] = sqrt(pPkt->afS[2][0] * pPkt->afS[2][0] +
                        pPkt->afS[2][1] * pPkt->afS[2][1] +
                        pPkt->afS[2][2] * pPkt->afS[2][2]);
}

void GetRotationAngles(const cbPKT_SS_NOISE_BOUNDARY* pPkt, float afTheta[3])
{
    const Vector3f major(pPkt->afS[0]);
    const Vector3f minor_1(pPkt->afS[1]);
    const Vector3f minor_2(pPkt->afS[2]);

    ::GetRotationAngles(major, minor_1, minor_2, afTheta);
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
cbRESULT cbSSGetNoiseBoundary(const uint32_t chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()) return cbRESULT_INVALIDCHANNEL;
    if (!IsChanAnalogIn(chanIdx)) return cbRESULT_INVALIDCHANNEL;

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

// Author & Date:   Hyrum Sessions  17 January 2023
// Purpose: Initialize SS Noise Boundary packet
void InitPktSSNoiseBoundary(
    cbPKT_SS_NOISE_BOUNDARY* pPkt, const uint32_t chan,
    const float cen1, const float cen2, const float cen3,
    const float maj1, const float maj2, const float maj3,
    const float min11, const float min12, const float min13,
    const float min21, const float min22, const float min23
)
{
    pPkt->cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    pPkt->cbpkt_header.type = cbPKTTYPE_SS_NOISE_BOUNDARYSET;
    pPkt->cbpkt_header.dlen = cbPKTDLEN_SS_NOISE_BOUNDARY;
    pPkt->chan = chan;
    pPkt->afc[0] = cen1;
    pPkt->afc[1] = cen2;
    pPkt->afc[2] = cen3;
    pPkt->afS[0][0] = maj1;
    pPkt->afS[0][1] = maj2;
    pPkt->afS[0][2] = maj3;
    pPkt->afS[1][0] = min11;
    pPkt->afS[1][1] = min12;
    pPkt->afS[1][2] = min13;
    pPkt->afS[2][0] = min21;
    pPkt->afS[2][1] = min22;
    pPkt->afS[2][2] = min23;
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
cbRESULT cbSSSetNoiseBoundary(const uint32_t chanIdx, float afCentroid[3], float afMajor[3], float afMinor_1[3], float afMinor_2[3], const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()) return cbRESULT_INVALIDCHANNEL;
    if (!IsChanAnalogIn(chanIdx)) return cbRESULT_INVALIDCHANNEL;

    cbPKT_SS_NOISE_BOUNDARY icPkt;
    InitPktSSNoiseBoundary(&icPkt, chanIdx, afCentroid[0], afCentroid[1], afCentroid[2],
                           afMajor[0], afMajor[1], afMajor[2],
                           afMinor_1[0], afMinor_1[1], afMinor_1[2],
                           afMinor_2[0], afMinor_2[1], afMinor_2[2]);
    icPkt.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chanIdx - 1].chan;
#ifndef CBPROTO_311
    icPkt.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chanIdx - 1].cbpkt_header.instrument;
#endif

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
cbRESULT cbSSGetNoiseBoundaryByTheta(const uint32_t chanIdx, float afCentroid[3], float afAxisLen[3], float afTheta[3], const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()) return cbRESULT_INVALIDCHANNEL;
    if (!IsChanAnalogIn(chanIdx)) return cbRESULT_INVALIDCHANNEL;

    // get noise boundary info
    const cbPKT_SS_NOISE_BOUNDARY & rPkt = cb_cfg_buffer_ptr[nIdx]->isSortingOptions.pktNoiseBoundary[chanIdx - 1];

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
        GetAxisLengths(&rPkt, afAxisLen);
    }

    // calculate the rotation angels
    if(afTheta)
    {
        GetRotationAngles(&rPkt, afTheta);
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
cbRESULT cbSSSetNoiseBoundaryByTheta(const uint32_t chanIdx, const float afCentroid[3], const float afAxisLen[3], const float afTheta[3], const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;
    if ((chanIdx - 1) >= cb_pc_status_buffer_ptr[0]->cbGetNumTotalChans()) return cbRESULT_INVALIDCHANNEL;
    if (!IsChanAnalogIn(chanIdx)) return cbRESULT_INVALIDCHANNEL;

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
    cbPKT_SS_NOISE_BOUNDARY icPkt = {};
    InitPktSSNoiseBoundary(&icPkt, chanIdx,
                           afCentroid[0], afCentroid[1], afCentroid[2],
                           major[0], major[1], major[2],
                           minor_1[0], minor_1[1], minor_1[2],
                           minor_2[0], minor_2[1], minor_2[2]);
    icPkt.chan = cb_cfg_buffer_ptr[nIdx]->chaninfo[chanIdx - 1].chan;
    icPkt.cbpkt_header.instrument = cb_cfg_buffer_ptr[nIdx]->chaninfo[chanIdx - 1].cbpkt_header.instrument;
    // Send it
    return cbSendPacket(&icPkt, nInstance);
#else
    return 0;
#endif

}

// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: Getting spike sorting statistics (nullptr = don't want that value)
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
cbRESULT cbSSGetStatistics(uint32_t * pnUpdateSpikes, uint32_t * pnAutoalg, uint32_t * pnMode,
                           float * pfMinClusterPairSpreadFactor,
                           float * pfMaxSubclusterSpreadFactor,
                           float * pfMinClusterHistCorrMajMeasure,
                           float * pfMaxClusterPairHistCorrMajMeasure,
                           float * pfClusterHistValleyPercentage,
                           float * pfClusterHistClosePeakPercentage,
                           float * pfClusterHistMinPeakPercentage,
                           uint32_t * pnWaveBasisSize,
                           uint32_t * pnWaveSampleSize,
                           const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbSSSetStatistics(uint32_t nUpdateSpikes, uint32_t nAutoalg, uint32_t nMode,
                           float fMinClusterPairSpreadFactor,
                           float fMaxSubclusterSpreadFactor,
                           float fMinClusterHistCorrMajMeasure,
                           float fMaxClusterMajHistCorrMajMeasure,
                           float fClusterHistValleyPercentage,
                           float fClusterHistClosePeakPercentage,
                           float fClusterHistMinPeakPercentage,
                           uint32_t nWaveBasisSize,
                           uint32_t nWaveSampleSize,
                           const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATISTICS icPkt;

    icPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    icPkt.cbpkt_header.type = cbPKTTYPE_SS_STATISTICSSET;
    icPkt.cbpkt_header.dlen = ((sizeof(cbPKT_SS_STATISTICS) / 4) - cbPKT_HEADER_32SIZE);

    icPkt.nUpdateSpikes = nUpdateSpikes;
    icPkt.nAutoalg = nAutoalg;
    icPkt.nMode = nMode;
    icPkt.fMinClusterPairSpreadFactor = fMinClusterPairSpreadFactor;
    icPkt.fMaxSubclusterSpreadFactor = fMaxSubclusterSpreadFactor;
    icPkt.fMinClusterHistCorrMajMeasure = fMinClusterHistCorrMajMeasure;
    icPkt.fMaxClusterPairHistCorrMajMeasure = fMaxClusterMajHistCorrMajMeasure;
    icPkt.fClusterHistValleyPercentage = fClusterHistValleyPercentage;
    icPkt.fClusterHistClosePeakPercentage = fClusterHistClosePeakPercentage;
    icPkt.fClusterHistMinPeakPercentage = fClusterHistMinPeakPercentage;
    icPkt.nWaveBasisSize = nWaveBasisSize;
    icPkt.nWaveSampleSize = nWaveSampleSize;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&icPkt, nInstance, nProc - 1);
    }
    return cbRes;
}

// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: set the artifact rejection parameters
// Outputs:
//  pnMaxChans - the maximum number of channels that can fire within 48 samples
//  pnRefractorySamples - num of samples (30 kHz) are "refractory" and thus ignored for detection
//  cbRESULT_OK if life is good
cbRESULT cbSSGetArtifactReject(uint32_t * pnMaxChans, uint32_t * pnRefractorySamples, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbSSSetArtifactReject(const uint32_t nMaxChans, const uint32_t nRefractorySamples, const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_ARTIF_REJECT isPkt = {};

    isPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    isPkt.cbpkt_header.type = cbPKTTYPE_SS_ARTIF_REJECTSET;
    isPkt.cbpkt_header.dlen = cbPKTDLEN_SS_ARTIF_REJECT;

    isPkt.nMaxSimulChans = nMaxChans;
    isPkt.nRefractoryCount = nRefractorySamples;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&isPkt, nInstance, nProc - 1);
    }
    return cbRes;
}


// Author & Date:   Kirk Korver     21 Jun 2005
// Purpose: get the spike detection parameters
// Outputs:
//  pfThreshold - the base threshold value
//  pfScaling - the threshold scaling factor
//  cbRESULT_OK if life is good
cbRESULT cbSSGetDetect(float * pfThreshold, float * pfScaling, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbSSSetDetect(const float fThreshold, const float fScaling, const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_DETECT isPkt = {};

    isPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    isPkt.cbpkt_header.type = cbPKTTYPE_SS_DETECTSET;
    isPkt.cbpkt_header.dlen = ((sizeof(cbPKT_SS_DETECT) / 4) - cbPKT_HEADER_32SIZE);

    isPkt.fThreshold = fThreshold;
    isPkt.fMultiplier = fScaling;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&isPkt, nInstance, nProc - 1);
    }
    return cbRes;
}


// Author & Date:   Hyrum L. Sessions   18 Nov 2005
// Purpose: get the spike sorting status
// Outputs:
//  pnMode - 0=number of units is still adapting  1=number of units is frozen
//  pfElapsedMinutes - this only makes sense if nMode=0 - minutes from start adapting
//  cbRESULT_OK if life is good
cbRESULT cbSSGetStatus(cbAdaptControl * pcntlUnitStats, cbAdaptControl * pcntlNumUnits, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
cbRESULT cbSSSetStatus(const cbAdaptControl cntlUnitStats, const cbAdaptControl cntlNumUnits, const uint32_t nInstance)
{
    cbRESULT cbRes = cbRESULT_OK;
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    cbPKT_SS_STATUS icPkt = {};

    icPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    icPkt.cbpkt_header.type = cbPKTTYPE_SS_STATUSSET;
    icPkt.cbpkt_header.dlen = cbPKTDLEN_SS_STATUS;

    icPkt.cntlUnitStats = cntlUnitStats;
    icPkt.cntlNumUnits = cntlNumUnits;

    // Send it to all NSPs
    for (int nProc = cbNSP1; nProc <= cbMAXPROCS; ++nProc)
    {
        if ((cbRESULT_OK == cbRes) && (NSP_FOUND == cbGetNspStatus(nProc)))
            cbRes = cbSendPacketToInstrument(&icPkt, nInstance, nProc - 1);
    }
    return cbRes;
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


cbRESULT cbCheckforData(cbLevelOfConcern & nLevelOfConcern, uint32_t *pktstogo /* = nullptr */, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

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
    uint32_t nDiff = cb_rec_buffer_ptr[nIdx]->headindex - cb_recbuff_tailindex[nIdx];
    if (nDiff < 0)
        nDiff += cbRECBUFFLEN;

    const uint32_t xxx = nDiff * LOC_COUNT;
    const int xx = cbRECBUFFLEN;
    nLevelOfConcern = static_cast<cbLevelOfConcern>(xxx / xx);

    // make sure to return a valid value
    if (nLevelOfConcern < LOC_LOW)
        nLevelOfConcern = LOC_LOW;
    if (nLevelOfConcern > LOC_CRITICAL)
        nLevelOfConcern = LOC_CRITICAL;

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
cbRESULT cbWaitforData(const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

#ifdef WIN32
    if (WaitForSingleObject(cb_sig_event_hnd[nIdx], 250) == WAIT_OBJECT_0)
        return cbRESULT_OK;
#elif defined __APPLE__
    if (sem_timedwait(static_cast<sem_t *>(cb_sig_event_hnd[nIdx]), 250) == 0)
        return cbRESULT_OK;
#else
    timespec ts;
    long ns = 250000000;
    clock_gettime(CLOCK_REALTIME, &ts);
#define NANOSECONDS_PER_SEC    1000000000L
    ts.tv_nsec = (ts.tv_nsec + ns) % NANOSECONDS_PER_SEC;
    ts.tv_sec += (ts.tv_nsec + ns) / NANOSECONDS_PER_SEC;
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


cbPKT_GENERIC * cbGetNextPacketPtr(const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // if there is new data return the next data packet and increment the pointer
    if (cb_recbuff_processed[nIdx] < cb_rec_buffer_ptr[nIdx]->received)
    {
        // get the pointer to the current packet
        auto *packetptr = reinterpret_cast<cbPKT_GENERIC *>(&(cb_rec_buffer_ptr[nIdx]->buffer[cb_recbuff_tailindex[nIdx]]));

        // increament the read index
        cb_recbuff_tailindex[nIdx] += (cbPKT_HEADER_32SIZE + packetptr->cbpkt_header.dlen);

        // check for read buffer wraparound, if so increment relevant variables
        if (cb_recbuff_tailindex[nIdx] > (cbRECBUFFLEN - (cbCER_UDP_SIZE_MAX / 4)))
        {
            cb_recbuff_tailindex[nIdx] = 0;
            cb_recbuff_tailwrap[nIdx]++;
        }

        // increment the processed count
        cb_recbuff_processed[nIdx]++;

        // update the timestamp index
        cb_recbuff_lasttime[nIdx] = packetptr->cbpkt_header.time;

        // return the packet
        return packetptr;
    }
    else
        return nullptr;
}


// Purpose: options sharing
//
cbRESULT cbGetColorTable(cbCOLORTABLE **colortable, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *colortable = &(cb_cfg_buffer_ptr[nIdx]->colortable);
    return cbRESULT_OK;
}

// Purpose: options sharing spike cache
//
cbRESULT cbGetSpkCache(const uint32_t chid, cbSPKCACHE **cache, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    *cache = reinterpret_cast<cbSPKCACHE *>(reinterpret_cast<uint8_t *>(&(cb_spk_buffer_ptr[nIdx]->cache)) + (
                                                (chid - 1) * (cb_spk_buffer_ptr[nIdx]->linesize)));
    return cbRESULT_OK;
}

// Author & Date:   Kirk Korver     29 May 2003
// Purpose: Get the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
float cbGetRMSAutoThresholdDistance(const uint32_t nInstance)
{
    return GetOptionTable(nInstance).fRMSAutoThresholdDistance;
}

// Author & Date:   Kirk Korver     29 May 2003
// Purpose: Set the multiplier to use for autothresholdine when using RMS to guess noise
// This will adjust fAutoThresholdDistance above, but use the API instead
void cbSetRMSAutoThresholdDistance(const float fRMSAutoThresholdDistance, const uint32_t nInstance)
{
    GetOptionTable(nInstance).fRMSAutoThresholdDistance = fRMSAutoThresholdDistance;
}


// Tell me about the current adaptive filter settings
cbRESULT cbGetAdaptFilter(const uint32_t  proc,             // which NSP processor?
                          uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          uint32_t  * pnRefChan1,     // The first reference channel (1 based).
                          uint32_t  * pnRefChan2,     // The second reference channel (1 based).
                          const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;

    // Allow the parameters to be nullptr
    if (pnMode)         *pnMode         = cb_cfg_buffer_ptr[nIdx]->adaptinfo[proc - 1].nMode;
    if (pdLearningRate) *pdLearningRate = cb_cfg_buffer_ptr[nIdx]->adaptinfo[proc - 1].dLearningRate;
    if (pnRefChan1)     *pnRefChan1     = cb_cfg_buffer_ptr[nIdx]->adaptinfo[proc - 1].nRefChan1;
    if (pnRefChan2)     *pnRefChan2     = cb_cfg_buffer_ptr[nIdx]->adaptinfo[proc - 1].nRefChan2;

    return cbRESULT_OK;
}


// Update the adaptive filter settings
cbRESULT cbSetAdaptFilter(const uint32_t  proc,             // which NSP processor?
                          const uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                          const float   * pdLearningRate, // speed at which adaptation happens. Very small. e.g. 5e-12
                          const uint32_t  * pnRefChan1,     // The first reference channel (1 based).
                          const uint32_t  * pnRefChan2,     // The second reference channel (1 based).
                          const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;


    // Get the old values
    uint32_t  nMode;         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    float   dLearningRate; // speed at which adaptation happens. Very small. e.g. 5e-12
    uint32_t  nRefChan1;     // The first reference channel (1 based).
    uint32_t  nRefChan2;     // The second reference channel (1 based).

    const cbRESULT ret = cbGetAdaptFilter(proc, &nMode, &dLearningRate, &nRefChan1, &nRefChan2, nInstance);
    ASSERT(ret == cbRESULT_OK);
    if (ret != cbRESULT_OK)
        return ret;

    // Handle the cases where there are "nullptr's" passed in
    if (pnMode)         nMode =         *pnMode;
    if (pdLearningRate) dLearningRate = *pdLearningRate;
    if (pnRefChan1)     nRefChan1 =     *pnRefChan1;
    if (pnRefChan2)     nRefChan2 =     *pnRefChan2;

    PktAdaptFiltInfo icPkt(nMode, dLearningRate, nRefChan1, nRefChan2);
    return cbSendPacketToInstrument(&icPkt, nInstance, proc - 1);
}

// Tell me about the current RefElecive filter settings
cbRESULT cbGetRefElecFilter(const uint32_t  proc,             // which NSP processor?
                            uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            uint32_t  * pnRefChan,      // The reference channel (1 based).
                            const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;

    // Allow the parameters to be nullptr
    if (pnMode)         *pnMode     = cb_cfg_buffer_ptr[nIdx]->refelecinfo[proc - 1].nMode;
    if (pnRefChan)      *pnRefChan  = cb_cfg_buffer_ptr[nIdx]->refelecinfo[proc - 1].nRefChan;

    return cbRESULT_OK;
}


// Update the reference electrode filter settings
cbRESULT cbSetRefElecFilter(const uint32_t  proc,             // which NSP processor?
                            const uint32_t  * pnMode,         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
                            const uint32_t  * pnRefChan,      // The reference channel (1 based).
                            const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    // Test that the proc address is valid and that requested procinfo structure is not empty
    if ((proc - 1) >= cbMAXPROCS) return cbRESULT_INVALIDADDRESS;


    // Get the old values
    uint32_t  nMode;         // 0=disabled, 1=filter continuous & spikes, 2=filter spikes
    uint32_t  nRefChan;      // The reference channel (1 based).

    const cbRESULT ret = cbGetRefElecFilter(proc, &nMode, &nRefChan, nInstance);
    ASSERT(ret == cbRESULT_OK);
    if (ret != cbRESULT_OK)
        return ret;

    // Handle the cases where there are "nullptr's" passed in
    if (pnMode)         nMode =         *pnMode;
    if (pnRefChan)      nRefChan =      *pnRefChan;

    cbPKT_REFELECFILTINFO icPkt = {};

    icPkt.cbpkt_header.chid = cbPKTCHAN_CONFIGURATION;
    icPkt.cbpkt_header.type = cbPKTTYPE_REFELECFILTSET;
    icPkt.cbpkt_header.dlen = cbPKTDLEN_REFELECFILTINFO;

    icPkt.nMode = nMode;
    icPkt.nRefChan = nRefChan;

    return cbSendPacketToInstrument(&icPkt, nInstance, proc - 1);
}

// Author & Date:   Ehsan Azar     6 Nov 2012
// Purpose: Get the channel selection status
// Inputs:
//   szName    - buffer name
//   bReadOnly - if should open memory for read-only operation
cbRESULT cbGetChannelSelection(cbPKT_UNIT_SELECTION* pPktUnitSel, const uint32_t nProc, const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Test for prior library initialization
    if (!cb_library_initialized[nIdx]) return cbRESULT_NOLIBRARY;

    if (cb_pc_status_buffer_ptr[nIdx]->isSelection[nProc].cbpkt_header.chid == 0)
        return cbRESULT_HARDWAREOFFLINE;

    if (pPktUnitSel) *pPktUnitSel = cb_pc_status_buffer_ptr[nIdx]->isSelection[nProc];

    return cbRESULT_OK;
}

// Author & Date:       Almut Branner         28 Mar 2006
// Purpose: Create the shared memory objects
// Inputs:
//   nInstance - nsp number to open library for
cbRESULT CreateSharedObjects(const uint32_t nInstance)
{
    const uint32_t nIdx = cb_library_index[nInstance];

    // Create the shared neuromatic receive buffer, if unsuccessful, return the associated error code
    if (nInstance == 0)
        _snprintf(cb_rec_buffer_hnd[nIdx].name, sizeof(cb_rec_buffer_hnd[nIdx].name), "%s", REC_BUF_NAME);
    else
        _snprintf(cb_rec_buffer_hnd[nIdx].name, sizeof(cb_rec_buffer_hnd[nIdx].name), "%s%d", REC_BUF_NAME, nInstance);
    cb_rec_buffer_hnd[nIdx].size = sizeof(cbRECBUFF);
    CreateSharedBuffer(cb_rec_buffer_hnd[nIdx]);
    cb_rec_buffer_ptr[nIdx] = static_cast<cbRECBUFF *>(GetSharedBuffer(cb_rec_buffer_hnd[nIdx].hnd, false));
    if (cb_rec_buffer_ptr[nIdx] == nullptr)
        return cbRESULT_BUFRECALLOCERR;
    memset(cb_rec_buffer_ptr[nIdx], 0, cb_rec_buffer_hnd[nIdx].size);

    // Create the shared transmit buffer; if unsuccessful, release rec buffer and associated error code
    {
        // create the global transmit buffer space
        if (nInstance == 0)
            _snprintf(cb_xmt_global_buffer_hnd[nIdx].name, sizeof(cb_xmt_global_buffer_hnd[nIdx].name), "%s", GLOBAL_XMT_NAME);
        else
            _snprintf(cb_xmt_global_buffer_hnd[nIdx].name, sizeof(cb_xmt_global_buffer_hnd[nIdx].name), "%s%d", GLOBAL_XMT_NAME, nInstance);
        cb_xmt_global_buffer_hnd[nIdx].size = sizeof(cbXMTBUFF) + (sizeof(uint32_t)*cbXMT_GLOBAL_BUFFLEN);
        CreateSharedBuffer(cb_xmt_global_buffer_hnd[nIdx]);
        // map the global memory into local ram space and get pointer
        cb_xmt_global_buffer_ptr[nIdx] = static_cast<cbXMTBUFF *>(GetSharedBuffer(cb_xmt_global_buffer_hnd[nIdx].hnd, false));
        // clean up if error occurs
        if (cb_xmt_global_buffer_ptr[nIdx] == nullptr)
            return cbRESULT_BUFGXMTALLOCERR;
        // initialize the buffers...they MUST all be initialized to 0 for later logic to work!!
        memset(cb_xmt_global_buffer_ptr[nIdx], 0, cb_xmt_global_buffer_hnd[nIdx].size);
        cb_xmt_global_buffer_ptr[nIdx]->bufferlen = cbXMT_GLOBAL_BUFFLEN;
        cb_xmt_global_buffer_ptr[nIdx]->last_valid_index =
                cbXMT_GLOBAL_BUFFLEN - (cbCER_UDP_SIZE_MAX / 4) - 1; // assuming largest packet   array is 0 based

        // create the local transmit buffer space
        if (nInstance == 0)
            _snprintf(cb_xmt_local_buffer_hnd[nIdx].name, sizeof(cb_xmt_local_buffer_hnd[nIdx].name), "%s", LOCAL_XMT_NAME);
        else
            _snprintf(cb_xmt_local_buffer_hnd[nIdx].name, sizeof(cb_xmt_local_buffer_hnd[nIdx].name), "%s%d", LOCAL_XMT_NAME, nInstance);
        cb_xmt_local_buffer_hnd[nIdx].size = sizeof(cbXMTBUFF) + (sizeof(uint32_t)*cbXMT_LOCAL_BUFFLEN);
        CreateSharedBuffer(cb_xmt_local_buffer_hnd[nIdx]);
        // map the global memory into local ram space and get pointer
        cb_xmt_local_buffer_ptr[nIdx] = static_cast<cbXMTBUFF *>(GetSharedBuffer(cb_xmt_local_buffer_hnd[nIdx].hnd, false));
        // clean up if error occurs
        if (cb_xmt_local_buffer_ptr[nIdx] == nullptr)
            return cbRESULT_BUFLXMTALLOCERR;
        // initialize the buffers...they MUST all be initialized to 0 for later logic to work!!
        memset(cb_xmt_local_buffer_ptr[nIdx], 0, cb_xmt_local_buffer_hnd[nIdx].size);
        cb_xmt_local_buffer_ptr[nIdx]->bufferlen = cbXMT_LOCAL_BUFFLEN;
        cb_xmt_local_buffer_ptr[nIdx]->last_valid_index =
            cbXMT_LOCAL_BUFFLEN - (cbCER_UDP_SIZE_MAX / 4) - 1; // assuming largest packet   array is 0 based
    }

    // Create the shared configuration buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(cb_cfg_buffer_hnd[nIdx].name, sizeof(cb_cfg_buffer_hnd[nIdx].name), "%s", CFG_BUF_NAME);
    else
        _snprintf(cb_cfg_buffer_hnd[nIdx].name, sizeof(cb_cfg_buffer_hnd[nIdx].name), "%s%d", CFG_BUF_NAME, nInstance);
    cb_cfg_buffer_hnd[nIdx].size = sizeof(cbCFGBUFF);
    CreateSharedBuffer(cb_cfg_buffer_hnd[nIdx]);
    cb_cfg_buffer_ptr[nIdx] = static_cast<cbCFGBUFF *>(GetSharedBuffer(cb_cfg_buffer_hnd[nIdx].hnd, false));
    if (cb_cfg_buffer_ptr[nIdx] == nullptr)
        return cbRESULT_BUFCFGALLOCERR;
    memset(cb_cfg_buffer_ptr[nIdx], 0, cb_cfg_buffer_hnd[nIdx].size);

    // Create the shared pc status buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(cb_pc_status_buffer_hnd[nIdx].name, sizeof(cb_pc_status_buffer_hnd[nIdx].name), "%s", STATUS_BUF_NAME);
    else
        _snprintf(cb_pc_status_buffer_hnd[nIdx].name, sizeof(cb_pc_status_buffer_hnd[nIdx].name), "%s%d", STATUS_BUF_NAME, nInstance);
    cb_pc_status_buffer_hnd[nIdx].size = sizeof(cbPcStatus);
    CreateSharedBuffer(cb_pc_status_buffer_hnd[nIdx]);
    cb_pc_status_buffer_ptr[nIdx] = static_cast<cbPcStatus *>(GetSharedBuffer(cb_pc_status_buffer_hnd[nIdx].hnd, false));
    if (cb_pc_status_buffer_ptr[nIdx] == nullptr)
        return cbRESULT_BUFPCSTATALLOCERR;
    memset(cb_pc_status_buffer_ptr[nIdx], 0, cb_pc_status_buffer_hnd[nIdx].size);

    // Create the shared spike cache buffer; if unsuccessful, release rec buffer and return FALSE
    if (nInstance == 0)
        _snprintf(cb_spk_buffer_hnd[nIdx].name, sizeof(cb_spk_buffer_hnd[nIdx].name), "%s", SPK_BUF_NAME);
    else
        _snprintf(cb_spk_buffer_hnd[nIdx].name, sizeof(cb_spk_buffer_hnd[nIdx].name), "%s%d", SPK_BUF_NAME, nInstance);
    cb_spk_buffer_hnd[nIdx].size = sizeof(cbSPKBUFF);
    CreateSharedBuffer(cb_spk_buffer_hnd[nIdx]);
    cb_spk_buffer_ptr[nIdx] = static_cast<cbSPKBUFF *>(GetSharedBuffer(cb_spk_buffer_hnd[nIdx].hnd, false));
    if (cb_spk_buffer_ptr[nIdx] == nullptr)
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
    char buf[64] = {0};
    if (nInstance == 0)
        _snprintf(buf, sizeof(buf), "%s", SIG_EVT_NAME);
    else
        _snprintf(buf, sizeof(buf), "%s%d", SIG_EVT_NAME, nInstance);
#ifdef WIN32
    cb_sig_event_hnd[nIdx] = CreateEventA(nullptr, TRUE, FALSE, buf);
    if (cb_sig_event_hnd[nIdx] == nullptr)
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

    // No error happened
    return cbRESULT_OK;
}


