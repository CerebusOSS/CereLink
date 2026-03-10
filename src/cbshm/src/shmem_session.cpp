///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   shmem_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Shared memory session implementation
///
/// Implements cross-platform shared memory management with dual layout support:
/// - CENTRAL layout: Central-compatible (4 instruments, 848 channels, ~1.2 GB)
/// - NATIVE layout: Per-device (1 instrument, 284 channels, ~265 MB)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#ifndef _WIN32
    #include <sys/mman.h>
    #include <sys/time.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <semaphore.h>
    #include <time.h>
    #include <errno.h>
#endif

#include <cbshm/shmem_session.h>
#include <cbshm/central_types.h>
#include <cbshm/native_types.h>
#include <cbproto/packet_translator.h>
#include <cstring>

namespace cbshm {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Platform-specific implementation details (Pimpl idiom)
///
struct ShmemSession::Impl {
    Mode mode;
    ShmemLayout layout;
    std::string cfg_name;            // Config buffer name (e.g., "cbCFGbuffer")
    std::string rec_name;            // Receive buffer name (e.g., "cbRECbuffer")
    std::string xmt_name;            // Transmit buffer name (e.g., "XmtGlobal")
    std::string xmt_local_name;      // Local transmit buffer name (e.g., "XmtLocal")
    std::string status_name;         // PC status buffer name (e.g., "cbSTATUSbuffer")
    std::string spk_name;            // Spike cache buffer name (e.g., "cbSPKbuffer")
    std::string signal_event_name;   // Signal event name (e.g., "cbSIGNALevent")
    bool is_open;

    // Platform-specific handles (separate segments for each buffer)
#ifdef _WIN32
    HANDLE cfg_file_mapping;
    HANDLE rec_file_mapping;
    HANDLE xmt_file_mapping;
    HANDLE xmt_local_file_mapping;
    HANDLE status_file_mapping;
    HANDLE spk_file_mapping;
    HANDLE signal_event;            // Named Event for data availability signaling
#else
    int cfg_shm_fd;
    int rec_shm_fd;
    int xmt_shm_fd;
    int xmt_local_shm_fd;
    int status_shm_fd;
    int spk_shm_fd;
    sem_t* signal_event;            // Named semaphore for data availability signaling
#endif

    // Pointers to shared memory buffers (void* to support dual layout)
    void* cfg_buffer_raw;
    void* rec_buffer_raw;
    void* xmt_buffer_raw;
    void* xmt_local_buffer_raw;
    void* status_buffer_raw;
    void* spike_buffer_raw;

    // Buffer sizes (set during open(), used for munmap and bounds checks)
    size_t cfg_buffer_size;
    size_t rec_buffer_size;
    size_t xmt_buffer_size;
    size_t xmt_local_buffer_size;
    size_t status_buffer_size;
    size_t spike_buffer_size;

    // Runtime receive buffer length (replaces hardcoded CENTRAL_cbRECBUFFLEN)
    uint32_t rec_buffer_len;

    // Receive buffer read tracking (for CLIENT mode reading)
    uint32_t rec_tailindex;      // Our read position in receive buffer
    uint32_t rec_tailwrap;       // Our wrap counter

    // Instrument filter for CENTRAL_COMPAT mode (-1 = no filter)
    int32_t instrument_filter;

    // Detected protocol version for CENTRAL_COMPAT mode
    cbproto_protocol_version_t compat_protocol;

    // Typed accessors for config buffer
    CentralConfigBuffer* centralCfg() { return static_cast<CentralConfigBuffer*>(cfg_buffer_raw); }
    const CentralConfigBuffer* centralCfg() const { return static_cast<const CentralConfigBuffer*>(cfg_buffer_raw); }
    NativeConfigBuffer* nativeCfg() { return static_cast<NativeConfigBuffer*>(cfg_buffer_raw); }
    const NativeConfigBuffer* nativeCfg() const { return static_cast<const NativeConfigBuffer*>(cfg_buffer_raw); }
    CentralLegacyCFGBUFF* legacyCfg() { return static_cast<CentralLegacyCFGBUFF*>(cfg_buffer_raw); }
    const CentralLegacyCFGBUFF* legacyCfg() const { return static_cast<const CentralLegacyCFGBUFF*>(cfg_buffer_raw); }

    // Generic receive buffer header access (header fields are at identical offsets in both layouts)
    uint32_t& recReceived() {
        return *static_cast<uint32_t*>(rec_buffer_raw);
    }
    PROCTIME& recLasttime() {
        // lasttime is at offset sizeof(uint32_t) in both CentralReceiveBuffer and NativeReceiveBuffer
        return *reinterpret_cast<PROCTIME*>(static_cast<char*>(rec_buffer_raw) + sizeof(uint32_t));
    }
    uint32_t& recHeadwrap() {
        return *reinterpret_cast<uint32_t*>(static_cast<char*>(rec_buffer_raw) + sizeof(uint32_t) + sizeof(PROCTIME));
    }
    uint32_t& recHeadindex() {
        return *reinterpret_cast<uint32_t*>(static_cast<char*>(rec_buffer_raw) + sizeof(uint32_t) + sizeof(PROCTIME) + sizeof(uint32_t));
    }
    uint32_t* recBuffer() {
        return reinterpret_cast<uint32_t*>(static_cast<char*>(rec_buffer_raw) + sizeof(uint32_t) + sizeof(PROCTIME) + sizeof(uint32_t) + sizeof(uint32_t));
    }

    // Transmit buffer accessors (header fields are identical between Central and Native)
    struct XmtHeader {
        uint32_t transmitted;
        uint32_t headindex;
        uint32_t tailindex;
        uint32_t last_valid_index;
        uint32_t bufferlen;
    };
    XmtHeader* xmtGlobal() { return static_cast<XmtHeader*>(xmt_buffer_raw); }
    uint32_t* xmtGlobalBuffer() { return reinterpret_cast<uint32_t*>(static_cast<char*>(xmt_buffer_raw) + sizeof(XmtHeader)); }
    XmtHeader* xmtLocal() { return static_cast<XmtHeader*>(xmt_local_buffer_raw); }
    uint32_t* xmtLocalBuffer() { return reinterpret_cast<uint32_t*>(static_cast<char*>(xmt_local_buffer_raw) + sizeof(XmtHeader)); }

    Impl()
        : mode(Mode::STANDALONE)
        , layout(ShmemLayout::CENTRAL)
        , is_open(false)
#ifdef _WIN32
        , cfg_file_mapping(nullptr)
        , rec_file_mapping(nullptr)
        , xmt_file_mapping(nullptr)
        , xmt_local_file_mapping(nullptr)
        , status_file_mapping(nullptr)
        , spk_file_mapping(nullptr)
        , signal_event(nullptr)
#else
        , cfg_shm_fd(-1)
        , rec_shm_fd(-1)
        , xmt_shm_fd(-1)
        , xmt_local_shm_fd(-1)
        , status_shm_fd(-1)
        , spk_shm_fd(-1)
        , signal_event(SEM_FAILED)
#endif
        , cfg_buffer_raw(nullptr)
        , rec_buffer_raw(nullptr)
        , xmt_buffer_raw(nullptr)
        , xmt_local_buffer_raw(nullptr)
        , status_buffer_raw(nullptr)
        , spike_buffer_raw(nullptr)
        , cfg_buffer_size(0)
        , rec_buffer_size(0)
        , xmt_buffer_size(0)
        , xmt_local_buffer_size(0)
        , status_buffer_size(0)
        , spike_buffer_size(0)
        , rec_buffer_len(0)
        , rec_tailindex(0)
        , rec_tailwrap(0)
        , instrument_filter(-1)
        , compat_protocol(CBPROTO_PROTOCOL_CURRENT)
    {}

    ~Impl() {
        close();
    }

    /// @brief Compute buffer sizes based on layout
    void computeBufferSizes() {
        if (layout == ShmemLayout::NATIVE) {
            cfg_buffer_size = sizeof(NativeConfigBuffer);
            rec_buffer_size = sizeof(NativeReceiveBuffer);
            xmt_buffer_size = sizeof(NativeTransmitBuffer);
            xmt_local_buffer_size = sizeof(NativeTransmitBufferLocal);
            status_buffer_size = sizeof(NativePCStatus);
            spike_buffer_size = sizeof(NativeSpikeBuffer);
            rec_buffer_len = NATIVE_cbRECBUFFLEN;
        } else if (layout == ShmemLayout::CENTRAL_COMPAT) {
            cfg_buffer_size = sizeof(CentralLegacyCFGBUFF);
            // All other buffers use Central sizes (receive, xmt, spike, status are compatible)
            rec_buffer_size = sizeof(CentralReceiveBuffer);
            xmt_buffer_size = sizeof(CentralTransmitBuffer);
            xmt_local_buffer_size = sizeof(CentralTransmitBufferLocal);
            status_buffer_size = sizeof(CentralPCStatus);
            spike_buffer_size = sizeof(CentralSpikeBuffer);
            rec_buffer_len = CENTRAL_cbRECBUFFLEN;
        } else {
            cfg_buffer_size = sizeof(CentralConfigBuffer);
            rec_buffer_size = sizeof(CentralReceiveBuffer);
            xmt_buffer_size = sizeof(CentralTransmitBuffer);
            xmt_local_buffer_size = sizeof(CentralTransmitBufferLocal);
            status_buffer_size = sizeof(CentralPCStatus);
            spike_buffer_size = sizeof(CentralSpikeBuffer);
            rec_buffer_len = CENTRAL_cbRECBUFFLEN;
        }
    }

    void close() {
        if (!is_open) return;

        // Unmap shared memory
#ifdef _WIN32
        if (cfg_buffer_raw) UnmapViewOfFile(cfg_buffer_raw);
        if (rec_buffer_raw) UnmapViewOfFile(rec_buffer_raw);
        if (xmt_buffer_raw) UnmapViewOfFile(xmt_buffer_raw);
        if (xmt_local_buffer_raw) UnmapViewOfFile(xmt_local_buffer_raw);
        if (status_buffer_raw) UnmapViewOfFile(status_buffer_raw);
        if (spike_buffer_raw) UnmapViewOfFile(spike_buffer_raw);
        if (cfg_file_mapping) CloseHandle(cfg_file_mapping);
        if (rec_file_mapping) CloseHandle(rec_file_mapping);
        if (xmt_file_mapping) CloseHandle(xmt_file_mapping);
        if (xmt_local_file_mapping) CloseHandle(xmt_local_file_mapping);
        if (status_file_mapping) CloseHandle(status_file_mapping);
        if (spk_file_mapping) CloseHandle(spk_file_mapping);
        if (signal_event) CloseHandle(signal_event);
        cfg_file_mapping = nullptr;
        rec_file_mapping = nullptr;
        xmt_file_mapping = nullptr;
        xmt_local_file_mapping = nullptr;
        status_file_mapping = nullptr;
        spk_file_mapping = nullptr;
        signal_event = nullptr;
#else
        // POSIX requires shared memory names to start with "/"
        std::string posix_cfg_name = (cfg_name[0] == '/') ? cfg_name : ("/" + cfg_name);
        std::string posix_rec_name = (rec_name[0] == '/') ? rec_name : ("/" + rec_name);
        std::string posix_xmt_name = (xmt_name[0] == '/') ? xmt_name : ("/" + xmt_name);
        std::string posix_xmt_local_name = (xmt_local_name[0] == '/') ? xmt_local_name : ("/" + xmt_local_name);
        std::string posix_status_name = (status_name[0] == '/') ? status_name : ("/" + status_name);
        std::string posix_spk_name = (spk_name[0] == '/') ? spk_name : ("/" + spk_name);
        std::string posix_signal_name = (signal_event_name[0] == '/') ? signal_event_name : ("/" + signal_event_name);

        if (cfg_buffer_raw) munmap(cfg_buffer_raw, cfg_buffer_size);
        if (rec_buffer_raw) munmap(rec_buffer_raw, rec_buffer_size);
        if (xmt_buffer_raw) munmap(xmt_buffer_raw, xmt_buffer_size);
        if (xmt_local_buffer_raw) munmap(xmt_local_buffer_raw, xmt_local_buffer_size);
        if (status_buffer_raw) munmap(status_buffer_raw, status_buffer_size);
        if (spike_buffer_raw) munmap(spike_buffer_raw, spike_buffer_size);

        if (cfg_shm_fd >= 0) {
            ::close(cfg_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_cfg_name.c_str());
        }
        if (rec_shm_fd >= 0) {
            ::close(rec_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_rec_name.c_str());
        }
        if (xmt_shm_fd >= 0) {
            ::close(xmt_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_xmt_name.c_str());
        }
        if (xmt_local_shm_fd >= 0) {
            ::close(xmt_local_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_xmt_local_name.c_str());
        }
        if (status_shm_fd >= 0) {
            ::close(status_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_status_name.c_str());
        }
        if (spk_shm_fd >= 0) {
            ::close(spk_shm_fd);
            if (mode == Mode::STANDALONE) shm_unlink(posix_spk_name.c_str());
        }
        if (signal_event != SEM_FAILED) {
            sem_close(signal_event);
            if (mode == Mode::STANDALONE) sem_unlink(posix_signal_name.c_str());
        }

        cfg_shm_fd = -1;
        rec_shm_fd = -1;
        xmt_shm_fd = -1;
        xmt_local_shm_fd = -1;
        status_shm_fd = -1;
        spk_shm_fd = -1;
        signal_event = SEM_FAILED;
#endif

        cfg_buffer_raw = nullptr;
        rec_buffer_raw = nullptr;
        xmt_buffer_raw = nullptr;
        xmt_local_buffer_raw = nullptr;
        status_buffer_raw = nullptr;
        spike_buffer_raw = nullptr;
        is_open = false;
    }

    /// @brief Write a packet to the receive buffer ring
    Result<void> writeToReceiveBuffer(const cbPKT_GENERIC& pkt) {
        if (!rec_buffer_raw) {
            return Result<void>::error("Receive buffer not initialized");
        }

        uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

        if (pkt_size_words > rec_buffer_len) {
            return Result<void>::error("Packet too large for receive buffer");
        }

        uint32_t head = recHeadindex();

        if (head + pkt_size_words > rec_buffer_len) {
            head = 0;
            recHeadwrap()++;
        }

        uint32_t* buf = recBuffer();
        const uint32_t* pkt_data = reinterpret_cast<const uint32_t*>(&pkt);
        for (uint32_t i = 0; i < pkt_size_words; ++i) {
            buf[head + i] = pkt_data[i];
        }

        recHeadindex() = head + pkt_size_words;
        recReceived()++;
        recLasttime() = pkt.cbpkt_header.time;

        return Result<void>::ok();
    }

#ifndef _WIN32
    /// @brief POSIX helper: open/create one shared memory segment and mmap it
    /// @return Result<void*> with mapped pointer on success
    Result<void*> openPosixSegment(const std::string& name, size_t size, int& fd_out,
                                    int flags, mode_t perms, int prot) {
        std::string posix_name = (name[0] == '/') ? name : ("/" + name);

        if (mode == Mode::STANDALONE) {
            shm_unlink(posix_name.c_str());  // Clean up any previous
        }

        fd_out = shm_open(posix_name.c_str(), flags, perms);
        if (fd_out < 0) {
            return Result<void*>::error("Failed to open shared memory '" + name + "': " + strerror(errno));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(fd_out, size) < 0) {
                std::string err = "Failed to set size for '" + name + "': " + strerror(errno);
                ::close(fd_out);
                fd_out = -1;
                return Result<void*>::error(err);
            }
        }

        void* ptr = mmap(nullptr, size, prot, MAP_SHARED, fd_out, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd_out);
            fd_out = -1;
            return Result<void*>::error("Failed to map shared memory '" + name + "'");
        }

        return Result<void*>::ok(ptr);
    }
#endif

    Result<void> open() {
        if (is_open) {
            return Result<void>::error("Session already open");
        }

        // Compute buffer sizes based on layout
        computeBufferSizes();

#ifdef _WIN32
        // Windows implementation
        // Helper lambda to create/map a segment
        // writable: when true, segment is opened read-write even in CLIENT mode
        //           (needed for transmit buffers so clients can enqueue packets)
        auto createSegment = [&](const std::string& name, size_t size, HANDLE& mapping, void*& buffer, bool writable = false) -> Result<void> {
            DWORD size_high = static_cast<DWORD>((static_cast<uint64_t>(size)) >> 32);
            DWORD size_low = static_cast<DWORD>(size & 0xFFFFFFFF);

            bool need_write = (mode == Mode::STANDALONE) || writable;
            DWORD seg_access = need_write ? PAGE_READWRITE : PAGE_READONLY;
            DWORD seg_map = need_write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

            if (mode == Mode::STANDALONE) {
                mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, seg_access, size_high, size_low, name.c_str());
            } else {
                mapping = OpenFileMappingA(seg_map, FALSE, name.c_str());
            }
            if (!mapping) {
                DWORD err = GetLastError();
                return Result<void>::error("Failed to create/open file mapping '" + name +
                    "' (size=" + std::to_string(size) + ", err=" + std::to_string(err) + ")");
            }
            buffer = MapViewOfFile(mapping, seg_map, 0, 0, (mode == Mode::STANDALONE) ? size : 0);
            if (!buffer) {
                DWORD err = GetLastError();
                CloseHandle(mapping);
                mapping = nullptr;
                return Result<void>::error("Failed to map view of file '" + name +
                    "' (size=" + std::to_string(size) + ", err=" + std::to_string(err) + ")");
            }
            return Result<void>::ok();
        };

        auto r = createSegment(cfg_name, cfg_buffer_size, cfg_file_mapping, cfg_buffer_raw);
        if (r.isError()) { close(); return r; }

        r = createSegment(rec_name, rec_buffer_size, rec_file_mapping, rec_buffer_raw);
        if (r.isError()) { close(); return r; }

        // Transmit buffers need write access in CLIENT mode too (clients enqueue packets)
        r = createSegment(xmt_name, xmt_buffer_size, xmt_file_mapping, xmt_buffer_raw, true);
        if (r.isError()) { close(); return r; }

        r = createSegment(xmt_local_name, xmt_local_buffer_size, xmt_local_file_mapping, xmt_local_buffer_raw, true);
        if (r.isError()) { close(); return r; }

        r = createSegment(status_name, status_buffer_size, status_file_mapping, status_buffer_raw);
        if (r.isError()) { close(); return r; }

        r = createSegment(spk_name, spike_buffer_size, spk_file_mapping, spike_buffer_raw);
        if (r.isError()) { close(); return r; }

        // Create/open signal event
        if (mode == Mode::STANDALONE) {
            signal_event = CreateEventA(nullptr, TRUE, FALSE, signal_event_name.c_str());
        } else {
            signal_event = OpenEventA(SYNCHRONIZE, FALSE, signal_event_name.c_str());
        }
        if (!signal_event) {
            close();
            return Result<void>::error("Failed to create/open signal event");
        }

#else
        // POSIX (macOS/Linux) implementation
        int flags = (mode == Mode::STANDALONE) ? (O_CREAT | O_RDWR) : O_RDONLY;
        mode_t perms = (mode == Mode::STANDALONE) ? 0644 : 0;
        int prot = (mode == Mode::STANDALONE) ? (PROT_READ | PROT_WRITE) : PROT_READ;

        // Transmit buffers need write access in CLIENT mode too (clients enqueue packets)
        int xmt_flags = (mode == Mode::STANDALONE) ? (O_CREAT | O_RDWR) : O_RDWR;
        int xmt_prot = PROT_READ | PROT_WRITE;

        auto r1 = openPosixSegment(cfg_name, cfg_buffer_size, cfg_shm_fd, flags, perms, prot);
        if (r1.isError()) { close(); return Result<void>::error(r1.error()); }
        cfg_buffer_raw = r1.value();

        auto r2 = openPosixSegment(rec_name, rec_buffer_size, rec_shm_fd, flags, perms, prot);
        if (r2.isError()) { close(); return Result<void>::error(r2.error()); }
        rec_buffer_raw = r2.value();

        auto r3 = openPosixSegment(xmt_name, xmt_buffer_size, xmt_shm_fd, xmt_flags, perms, xmt_prot);
        if (r3.isError()) { close(); return Result<void>::error(r3.error()); }
        xmt_buffer_raw = r3.value();

        auto r4 = openPosixSegment(xmt_local_name, xmt_local_buffer_size, xmt_local_shm_fd, xmt_flags, perms, xmt_prot);
        if (r4.isError()) { close(); return Result<void>::error(r4.error()); }
        xmt_local_buffer_raw = r4.value();

        auto r5 = openPosixSegment(status_name, status_buffer_size, status_shm_fd, flags, perms, prot);
        if (r5.isError()) { close(); return Result<void>::error(r5.error()); }
        status_buffer_raw = r5.value();

        auto r6 = openPosixSegment(spk_name, spike_buffer_size, spk_shm_fd, flags, perms, prot);
        if (r6.isError()) { close(); return Result<void>::error(r6.error()); }
        spike_buffer_raw = r6.value();

        // Create/open signal event (named semaphore)
        std::string posix_signal_name = (signal_event_name[0] == '/') ? signal_event_name : ("/" + signal_event_name);
        if (mode == Mode::STANDALONE) {
            sem_unlink(posix_signal_name.c_str());
            signal_event = sem_open(posix_signal_name.c_str(), O_CREAT | O_EXCL, 0666, 0);
            if (signal_event == SEM_FAILED) {
                signal_event = sem_open(posix_signal_name.c_str(), O_CREAT, 0666, 0);
            }
        } else {
            signal_event = sem_open(posix_signal_name.c_str(), 0);
        }

        if (signal_event == SEM_FAILED) {
            close();
            return Result<void>::error("Failed to create/open signal semaphore: " + std::string(strerror(errno)));
        }
#endif

        // Initialize buffers in standalone mode
        if (mode == Mode::STANDALONE) {
            initBuffers();
        }

        is_open = true;

        // In CLIENT mode, sync our read position to the current head so we only
        // read NEW packets, not stale data that was already in the ring buffer.
        if (mode == Mode::CLIENT) {
            rec_tailindex = recHeadindex();
            rec_tailwrap = recHeadwrap();
        }

        // Detect protocol version for CENTRAL_COMPAT mode
        detectCompatProtocol();

        return Result<void>::ok();
    }

    /// @brief Initialize buffers for STANDALONE mode
    void initBuffers() {
        if (layout == ShmemLayout::NATIVE) {
            initNativeBuffers();
        } else if (layout == ShmemLayout::CENTRAL_COMPAT) {
            initLegacyBuffers();
        } else {
            initCentralBuffers();
        }
    }

    void initCentralBuffers() {
        auto* cfg = centralCfg();
        std::memset(cfg, 0, cfg_buffer_size);
        cfg->version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR;
        for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
            cfg->instrument_status[i] = static_cast<uint32_t>(InstrumentStatus::INACTIVE);
        }

        // Initialize receive buffer
        std::memset(rec_buffer_raw, 0, rec_buffer_size);

        // Initialize transmit buffers
        auto* xmt = static_cast<CentralTransmitBuffer*>(xmt_buffer_raw);
        std::memset(xmt, 0, xmt_buffer_size);
        xmt->last_valid_index = CENTRAL_cbXMT_GLOBAL_BUFFLEN - 1;
        xmt->bufferlen = CENTRAL_cbXMT_GLOBAL_BUFFLEN;

        auto* xmt_local = static_cast<CentralTransmitBufferLocal*>(xmt_local_buffer_raw);
        std::memset(xmt_local, 0, xmt_local_buffer_size);
        xmt_local->last_valid_index = CENTRAL_cbXMT_LOCAL_BUFFLEN - 1;
        xmt_local->bufferlen = CENTRAL_cbXMT_LOCAL_BUFFLEN;

        // Initialize status buffer
        auto* status = static_cast<CentralPCStatus*>(status_buffer_raw);
        std::memset(status, 0, status_buffer_size);
        status->m_nNumFEChans = CENTRAL_cbNUM_FE_CHANS;
        status->m_nNumAnainChans = CENTRAL_cbNUM_ANAIN_CHANS;
        status->m_nNumAnalogChans = CENTRAL_cbNUM_ANALOG_CHANS;
        status->m_nNumAoutChans = CENTRAL_cbNUM_ANAOUT_CHANS;
        status->m_nNumAudioChans = CENTRAL_cbNUM_AUDOUT_CHANS;
        status->m_nNumAnalogoutChans = CENTRAL_cbNUM_ANALOGOUT_CHANS;
        status->m_nNumDiginChans = CENTRAL_cbNUM_DIGIN_CHANS;
        status->m_nNumSerialChans = CENTRAL_cbNUM_SERIAL_CHANS;
        status->m_nNumDigoutChans = CENTRAL_cbNUM_DIGOUT_CHANS;
        status->m_nNumTotalChans = CENTRAL_cbMAXCHANS;
        for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
            status->m_nNspStatus[i] = NSPStatus::NSP_INIT;
        }

        // Initialize spike cache buffer
        auto* spike = static_cast<CentralSpikeBuffer*>(spike_buffer_raw);
        std::memset(spike, 0, spike_buffer_size);
        spike->chidmax = CENTRAL_cbNUM_ANALOG_CHANS;
        spike->linesize = sizeof(CentralSpikeCache);
        for (uint32_t ch = 0; ch < CENTRAL_cbPKT_SPKCACHELINECNT; ++ch) {
            spike->cache[ch].chid = ch;
            spike->cache[ch].pktcnt = CENTRAL_cbPKT_SPKCACHEPKTCNT;
            spike->cache[ch].pktsize = sizeof(cbPKT_SPK);
        }
    }

    void initLegacyBuffers() {
        auto* cfg = legacyCfg();
        std::memset(cfg, 0, cfg_buffer_size);
        cfg->version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR;

        // Set procinfo version so detectCompatProtocol() identifies current format.
        // In STANDALONE mode, CereLink owns the memory and writes current-format packets.
        // MAKELONG(minor, major) = (major << 16) | minor
        for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
            cfg->procinfo[i].version = (cbVERSION_MAJOR << 16) | cbVERSION_MINOR;
        }

        // Initialize receive buffer
        std::memset(rec_buffer_raw, 0, rec_buffer_size);

        // Initialize transmit buffers (same struct as Central)
        auto* xmt = static_cast<CentralTransmitBuffer*>(xmt_buffer_raw);
        std::memset(xmt, 0, xmt_buffer_size);
        xmt->last_valid_index = CENTRAL_cbXMT_GLOBAL_BUFFLEN - 1;
        xmt->bufferlen = CENTRAL_cbXMT_GLOBAL_BUFFLEN;

        auto* xmt_local = static_cast<CentralTransmitBufferLocal*>(xmt_local_buffer_raw);
        std::memset(xmt_local, 0, xmt_local_buffer_size);
        xmt_local->last_valid_index = CENTRAL_cbXMT_LOCAL_BUFFLEN - 1;
        xmt_local->bufferlen = CENTRAL_cbXMT_LOCAL_BUFFLEN;

        // Initialize status buffer (same struct as Central)
        auto* status = static_cast<CentralPCStatus*>(status_buffer_raw);
        std::memset(status, 0, status_buffer_size);
        status->m_nNumFEChans = CENTRAL_cbNUM_FE_CHANS;
        status->m_nNumAnainChans = CENTRAL_cbNUM_ANAIN_CHANS;
        status->m_nNumAnalogChans = CENTRAL_cbNUM_ANALOG_CHANS;
        status->m_nNumAoutChans = CENTRAL_cbNUM_ANAOUT_CHANS;
        status->m_nNumAudioChans = CENTRAL_cbNUM_AUDOUT_CHANS;
        status->m_nNumAnalogoutChans = CENTRAL_cbNUM_ANALOGOUT_CHANS;
        status->m_nNumDiginChans = CENTRAL_cbNUM_DIGIN_CHANS;
        status->m_nNumSerialChans = CENTRAL_cbNUM_SERIAL_CHANS;
        status->m_nNumDigoutChans = CENTRAL_cbNUM_DIGOUT_CHANS;
        status->m_nNumTotalChans = CENTRAL_cbMAXCHANS;
        for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
            status->m_nNspStatus[i] = NSPStatus::NSP_INIT;
        }

        // Initialize spike cache buffer (same struct as Central)
        auto* spike = static_cast<CentralSpikeBuffer*>(spike_buffer_raw);
        std::memset(spike, 0, spike_buffer_size);
        spike->chidmax = CENTRAL_cbNUM_ANALOG_CHANS;
        spike->linesize = sizeof(CentralSpikeCache);
        for (uint32_t ch = 0; ch < CENTRAL_cbPKT_SPKCACHELINECNT; ++ch) {
            spike->cache[ch].chid = ch;
            spike->cache[ch].pktcnt = CENTRAL_cbPKT_SPKCACHEPKTCNT;
            spike->cache[ch].pktsize = sizeof(cbPKT_SPK);
        }
    }

    void initNativeBuffers() {
        auto* cfg = nativeCfg();
        std::memset(cfg, 0, cfg_buffer_size);
        cfg->version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR;
        cfg->instrument_status = static_cast<uint32_t>(InstrumentStatus::INACTIVE);

        // Initialize receive buffer
        std::memset(rec_buffer_raw, 0, rec_buffer_size);

        // Initialize transmit buffers
        auto* xmt = static_cast<NativeTransmitBuffer*>(xmt_buffer_raw);
        std::memset(xmt, 0, xmt_buffer_size);
        xmt->last_valid_index = NATIVE_cbXMT_GLOBAL_BUFFLEN - 1;
        xmt->bufferlen = NATIVE_cbXMT_GLOBAL_BUFFLEN;

        auto* xmt_local = static_cast<NativeTransmitBufferLocal*>(xmt_local_buffer_raw);
        std::memset(xmt_local, 0, xmt_local_buffer_size);
        xmt_local->last_valid_index = NATIVE_cbXMT_LOCAL_BUFFLEN - 1;
        xmt_local->bufferlen = NATIVE_cbXMT_LOCAL_BUFFLEN;

        // Initialize status buffer
        auto* status = static_cast<NativePCStatus*>(status_buffer_raw);
        std::memset(status, 0, status_buffer_size);
        status->m_nNumFEChans = NATIVE_NUM_FE_CHANS;
        status->m_nNumAnainChans = cbNUM_ANAIN_CHANS;
        status->m_nNumAnalogChans = NATIVE_NUM_ANALOG_CHANS;
        status->m_nNumAoutChans = cbNUM_ANAOUT_CHANS;
        status->m_nNumAudioChans = cbNUM_AUDOUT_CHANS;
        status->m_nNumAnalogoutChans = cbNUM_ANALOGOUT_CHANS;
        status->m_nNumDiginChans = cbNUM_DIGIN_CHANS;
        status->m_nNumSerialChans = cbNUM_SERIAL_CHANS;
        status->m_nNumDigoutChans = cbNUM_DIGOUT_CHANS;
        status->m_nNumTotalChans = NATIVE_MAXCHANS;
        status->m_nNspStatus = NSPStatus::NSP_INIT;

        // Initialize spike cache buffer
        auto* spike = static_cast<NativeSpikeBuffer*>(spike_buffer_raw);
        std::memset(spike, 0, spike_buffer_size);
        spike->chidmax = NATIVE_NUM_ANALOG_CHANS;
        spike->linesize = sizeof(NativeSpikeCache);
        for (uint32_t ch = 0; ch < NATIVE_cbPKT_SPKCACHELINECNT; ++ch) {
            spike->cache[ch].chid = ch;
            spike->cache[ch].pktcnt = NATIVE_cbPKT_SPKCACHEPKTCNT;
            spike->cache[ch].pktsize = sizeof(cbPKT_SPK);
        }
    }

    /// @brief Detect protocol version from config buffer (CENTRAL_COMPAT only)
    void detectCompatProtocol() {
        if (layout != ShmemLayout::CENTRAL_COMPAT) {
            compat_protocol = CBPROTO_PROTOCOL_CURRENT;
            return;
        }

        auto* cfg = legacyCfg();
        if (!cfg) {
            compat_protocol = CBPROTO_PROTOCOL_CURRENT;
            return;
        }

        // procinfo[0].version = MAKELONG(minor, major) = (major << 16) | minor
        uint32_t ver = cfg->procinfo[0].version;
        uint16_t major = (ver >> 16) & 0xFFFF;
        uint16_t minor = ver & 0xFFFF;

        if (major < 4) {
            compat_protocol = CBPROTO_PROTOCOL_311;
        } else if (major == 4 && minor == 0) {
            compat_protocol = CBPROTO_PROTOCOL_400;
        } else if (major == 4 && minor == 1) {
            compat_protocol = CBPROTO_PROTOCOL_410;
        } else {
            compat_protocol = CBPROTO_PROTOCOL_CURRENT;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// ShmemSession public API implementation

ShmemSession::ShmemSession()
    : m_impl(std::make_unique<Impl>())
{}

ShmemSession::~ShmemSession() = default;

ShmemSession::ShmemSession(ShmemSession&& other) noexcept = default;
ShmemSession& ShmemSession::operator=(ShmemSession&& other) noexcept = default;

Result<ShmemSession> ShmemSession::create(const std::string& cfg_name, const std::string& rec_name,
                                           const std::string& xmt_name, const std::string& xmt_local_name,
                                           const std::string& status_name, const std::string& spk_name,
                                           const std::string& signal_event_name, Mode mode,
                                           ShmemLayout layout) {
    ShmemSession session;
    session.m_impl->cfg_name = cfg_name;
    session.m_impl->rec_name = rec_name;
    session.m_impl->xmt_name = xmt_name;
    session.m_impl->xmt_local_name = xmt_local_name;
    session.m_impl->status_name = status_name;
    session.m_impl->spk_name = spk_name;
    session.m_impl->signal_event_name = signal_event_name;
    session.m_impl->mode = mode;
    session.m_impl->layout = layout;

    auto result = session.m_impl->open();
    if (result.isError()) {
        return Result<ShmemSession>::error(result.error());
    }

    return Result<ShmemSession>::ok(std::move(session));
}

bool ShmemSession::isOpen() const {
    return m_impl && m_impl->is_open;
}

Mode ShmemSession::getMode() const {
    return m_impl->mode;
}

ShmemLayout ShmemSession::getLayout() const {
    return m_impl->layout;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument Status Management

Result<bool> ShmemSession::isInstrumentActive(cbproto::InstrumentId id) const {
    if (!isOpen()) {
        return Result<bool>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<bool>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<bool>::error("Native mode: single instrument only (index 0)");
        }
        bool active = (m_impl->nativeCfg()->instrument_status == static_cast<uint32_t>(InstrumentStatus::ACTIVE));
        return Result<bool>::ok(active);
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        // CentralLegacyCFGBUFF has no instrument_status field;
        // if the shared memory exists, instruments are as Central configured them
        return Result<bool>::ok(true);
    } else {
        bool active = (m_impl->centralCfg()->instrument_status[idx] == static_cast<uint32_t>(InstrumentStatus::ACTIVE));
        return Result<bool>::ok(active);
    }
}

Result<void> ShmemSession::setInstrumentActive(cbproto::InstrumentId id, bool active) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    uint32_t val = active ? static_cast<uint32_t>(InstrumentStatus::ACTIVE) : static_cast<uint32_t>(InstrumentStatus::INACTIVE);

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<void>::error("Native mode: single instrument only (index 0)");
        }
        m_impl->nativeCfg()->instrument_status = val;
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        return Result<void>::error("CENTRAL_COMPAT mode: instrument status is read-only (no instrument_status field in Central's layout)");
    } else {
        m_impl->centralCfg()->instrument_status[idx] = val;
    }

    return Result<void>::ok();
}

Result<cbproto::InstrumentId> ShmemSession::getFirstActiveInstrument() const {
    if (!isOpen()) {
        return Result<cbproto::InstrumentId>::error("Session not open");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (m_impl->nativeCfg()->instrument_status == static_cast<uint32_t>(InstrumentStatus::ACTIVE)) {
            return Result<cbproto::InstrumentId>::ok(cbproto::InstrumentId::fromIndex(0));
        }
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        // No instrument_status in legacy layout; return first instrument (always "active")
        return Result<cbproto::InstrumentId>::ok(cbproto::InstrumentId::fromIndex(0));
    } else {
        for (uint8_t i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
            if (m_impl->centralCfg()->instrument_status[i] == static_cast<uint32_t>(InstrumentStatus::ACTIVE)) {
                return Result<cbproto::InstrumentId>::ok(cbproto::InstrumentId::fromIndex(i));
            }
        }
    }

    return Result<cbproto::InstrumentId>::error("No active instruments");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Read Operations

Result<cbPKT_PROCINFO> ShmemSession::getProcInfo(cbproto::InstrumentId id) const {
    if (!isOpen()) {
        return Result<cbPKT_PROCINFO>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<cbPKT_PROCINFO>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<cbPKT_PROCINFO>::error("Native mode: single instrument only");
        }
        return Result<cbPKT_PROCINFO>::ok(m_impl->nativeCfg()->procinfo);
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<cbPKT_PROCINFO>::error("instrument index out of range");
        return Result<cbPKT_PROCINFO>::ok(m_impl->legacyCfg()->procinfo[idx]);
    } else {
        return Result<cbPKT_PROCINFO>::ok(m_impl->centralCfg()->procinfo[idx]);
    }
}

Result<cbPKT_BANKINFO> ShmemSession::getBankInfo(cbproto::InstrumentId id, uint32_t bank) const {
    if (!isOpen()) {
        return Result<cbPKT_BANKINFO>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<cbPKT_BANKINFO>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    uint32_t max_banks = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXBANKS : CENTRAL_cbMAXBANKS;

    if (bank == 0 || bank > max_banks) {
        return Result<cbPKT_BANKINFO>::error("Bank number out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<cbPKT_BANKINFO>::error("Native mode: single instrument only");
        }
        return Result<cbPKT_BANKINFO>::ok(m_impl->nativeCfg()->bankinfo[bank - 1]);
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<cbPKT_BANKINFO>::error("instrument index out of range");
        return Result<cbPKT_BANKINFO>::ok(m_impl->legacyCfg()->bankinfo[idx][bank - 1]);
    } else {
        return Result<cbPKT_BANKINFO>::ok(m_impl->centralCfg()->bankinfo[idx][bank - 1]);
    }
}

Result<cbPKT_FILTINFO> ShmemSession::getFilterInfo(cbproto::InstrumentId id, uint32_t filter) const {
    if (!isOpen()) {
        return Result<cbPKT_FILTINFO>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<cbPKT_FILTINFO>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    uint32_t max_filts = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXFILTS : CENTRAL_cbMAXFILTS;

    if (filter == 0 || filter > max_filts) {
        return Result<cbPKT_FILTINFO>::error("Filter number out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<cbPKT_FILTINFO>::error("Native mode: single instrument only");
        }
        return Result<cbPKT_FILTINFO>::ok(m_impl->nativeCfg()->filtinfo[filter - 1]);
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<cbPKT_FILTINFO>::error("instrument index out of range");
        return Result<cbPKT_FILTINFO>::ok(m_impl->legacyCfg()->filtinfo[idx][filter - 1]);
    } else {
        return Result<cbPKT_FILTINFO>::ok(m_impl->centralCfg()->filtinfo[idx][filter - 1]);
    }
}

Result<cbPKT_CHANINFO> ShmemSession::getChanInfo(uint32_t channel) const {
    if (!isOpen()) {
        return Result<cbPKT_CHANINFO>::error("Session not open");
    }

    uint32_t max_chans = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXCHANS : CENTRAL_cbMAXCHANS;

    if (channel >= max_chans) {
        return Result<cbPKT_CHANINFO>::error("Channel index out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        return Result<cbPKT_CHANINFO>::ok(m_impl->nativeCfg()->chaninfo[channel]);
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        return Result<cbPKT_CHANINFO>::ok(m_impl->legacyCfg()->chaninfo[channel]);
    } else {
        return Result<cbPKT_CHANINFO>::ok(m_impl->centralCfg()->chaninfo[channel]);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Write Operations

Result<void> ShmemSession::setProcInfo(cbproto::InstrumentId id, const cbPKT_PROCINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<void>::error("Native mode: single instrument only");
        }
        m_impl->nativeCfg()->procinfo = info;
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<void>::error("instrument index out of range");
        m_impl->legacyCfg()->procinfo[idx] = info;
    } else {
        m_impl->centralCfg()->procinfo[idx] = info;
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::setBankInfo(cbproto::InstrumentId id, uint32_t bank, const cbPKT_BANKINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    uint32_t max_banks = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXBANKS : CENTRAL_cbMAXBANKS;

    if (bank == 0 || bank > max_banks) {
        return Result<void>::error("Bank number out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<void>::error("Native mode: single instrument only");
        }
        m_impl->nativeCfg()->bankinfo[bank - 1] = info;
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<void>::error("instrument index out of range");
        m_impl->legacyCfg()->bankinfo[idx][bank - 1] = info;
    } else {
        m_impl->centralCfg()->bankinfo[idx][bank - 1] = info;
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::setFilterInfo(cbproto::InstrumentId id, uint32_t filter, const cbPKT_FILTINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }
    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    uint32_t max_filts = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXFILTS : CENTRAL_cbMAXFILTS;

    if (filter == 0 || filter > max_filts) {
        return Result<void>::error("Filter number out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (idx != 0) {
            return Result<void>::error("Native mode: single instrument only");
        }
        m_impl->nativeCfg()->filtinfo[filter - 1] = info;
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        if (idx >= CENTRAL_cbMAXPROCS) return Result<void>::error("instrument index out of range");
        m_impl->legacyCfg()->filtinfo[idx][filter - 1] = info;
    } else {
        m_impl->centralCfg()->filtinfo[idx][filter - 1] = info;
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::setChanInfo(uint32_t channel, const cbPKT_CHANINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    uint32_t max_chans = (m_impl->layout == ShmemLayout::NATIVE) ? NATIVE_MAXCHANS : CENTRAL_cbMAXCHANS;

    if (channel >= max_chans) {
        return Result<void>::error("Channel index out of range");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        m_impl->nativeCfg()->chaninfo[channel] = info;
    } else if (m_impl->layout == ShmemLayout::CENTRAL_COMPAT) {
        m_impl->legacyCfg()->chaninfo[channel] = info;
    } else {
        m_impl->centralCfg()->chaninfo[channel] = info;
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration Buffer Direct Access

cbConfigBuffer* ShmemSession::getConfigBuffer() {
    if (!isOpen() || m_impl->layout != ShmemLayout::CENTRAL) {
        return nullptr;
    }
    return m_impl->centralCfg();
}

const cbConfigBuffer* ShmemSession::getConfigBuffer() const {
    if (!isOpen() || m_impl->layout != ShmemLayout::CENTRAL) {
        return nullptr;
    }
    return m_impl->centralCfg();
}

NativeConfigBuffer* ShmemSession::getNativeConfigBuffer() {
    if (!isOpen() || m_impl->layout != ShmemLayout::NATIVE) {
        return nullptr;
    }
    return m_impl->nativeCfg();
}

const NativeConfigBuffer* ShmemSession::getNativeConfigBuffer() const {
    if (!isOpen() || m_impl->layout != ShmemLayout::NATIVE) {
        return nullptr;
    }
    return m_impl->nativeCfg();
}

CentralLegacyCFGBUFF* ShmemSession::getLegacyConfigBuffer() {
    if (!isOpen() || m_impl->layout != ShmemLayout::CENTRAL_COMPAT) {
        return nullptr;
    }
    return m_impl->legacyCfg();
}

const CentralLegacyCFGBUFF* ShmemSession::getLegacyConfigBuffer() const {
    if (!isOpen() || m_impl->layout != ShmemLayout::CENTRAL_COMPAT) {
        return nullptr;
    }
    return m_impl->legacyCfg();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Routing (THE KEY FIX!)

Result<void> ShmemSession::storePacket(const cbPKT_GENERIC& pkt) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    // CRITICAL: Write ALL packets to receive buffer ring (Central's architecture)
    auto rec_result = m_impl->writeToReceiveBuffer(pkt);
    if (rec_result.isError()) {
        // Log error but don't fail - config updates may still work
    }

    // NOTE: Config parsing (PROCINFO, BANKINFO, etc.) is NOT done here.
    // Config parsing belongs in the device session (DeviceSession::updateConfigFromBuffer),
    // which owns the device_config struct. shmem is a transport layer only.
    // Use setProcInfo()/setBankInfo()/etc. directly to update the config buffer.

    return Result<void>::ok();
}

Result<void> ShmemSession::storePackets(const cbPKT_GENERIC* pkts, size_t count) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    for (size_t i = 0; i < count; ++i) {
        auto result = storePacket(pkts[i]);
        if (result.isError()) {
            return result;
        }
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Transmit Queue Operations
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> ShmemSession::enqueuePacket(const cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->xmt_buffer_raw) {
        return Result<void>::error("Transmit buffer not initialized");
    }

    // In CENTRAL_COMPAT mode with an older protocol, translate to the legacy format
    const bool needs_translation = (m_impl->layout == ShmemLayout::CENTRAL_COMPAT &&
                                     m_impl->compat_protocol != CBPROTO_PROTOCOL_CURRENT);

    const uint8_t* write_data;
    uint32_t write_size_bytes;

    uint8_t translated_buf[cbPKT_MAX_SIZE];

    if (needs_translation) {
        if (m_impl->compat_protocol == CBPROTO_PROTOCOL_311) {
            // Translate header: current (16 bytes) → 3.11 (8 bytes)
            auto& dest_hdr = *reinterpret_cast<cbproto::cbPKT_HEADER_311*>(translated_buf);
            dest_hdr.time = static_cast<uint32_t>(pkt.cbpkt_header.time * 30000ULL / 1000000000ULL);
            dest_hdr.chid = pkt.cbpkt_header.chid;
            dest_hdr.type = static_cast<uint8_t>(pkt.cbpkt_header.type);
            dest_hdr.dlen = static_cast<uint8_t>(pkt.cbpkt_header.dlen);
            // Translate payload
            size_t dest_dlen = cbproto::PacketTranslator::translatePayload_current_to_311(pkt, translated_buf);
            dest_hdr.dlen = static_cast<uint8_t>(dest_dlen);
            write_size_bytes = cbproto::HEADER_SIZE_311 + dest_dlen * 4;
        } else if (m_impl->compat_protocol == CBPROTO_PROTOCOL_400) {
            // Translate header: current (16 bytes) → 4.0 (16 bytes, different field layout)
            auto& dest_hdr = *reinterpret_cast<cbproto::cbPKT_HEADER_400*>(translated_buf);
            dest_hdr.time = pkt.cbpkt_header.time;
            dest_hdr.chid = pkt.cbpkt_header.chid;
            dest_hdr.type = static_cast<uint8_t>(pkt.cbpkt_header.type);
            dest_hdr.dlen = pkt.cbpkt_header.dlen;
            dest_hdr.instrument = pkt.cbpkt_header.instrument;
            dest_hdr.reserved = 0;
            // Translate payload
            size_t dest_dlen = cbproto::PacketTranslator::translatePayload_current_to_400(pkt, translated_buf);
            dest_hdr.dlen = static_cast<uint16_t>(dest_dlen);
            write_size_bytes = cbproto::HEADER_SIZE_400 + dest_dlen * 4;
        } else {
            // 4.1 (CBPROTO_PROTOCOL_410): header identical, only some payloads differ
            std::memcpy(translated_buf, &pkt, cbPKT_HEADER_SIZE + pkt.cbpkt_header.dlen * 4);
            size_t dest_dlen = cbproto::PacketTranslator::translatePayload_current_to_410(pkt, translated_buf);
            auto& dest_hdr = *reinterpret_cast<cbPKT_HEADER*>(translated_buf);
            dest_hdr.dlen = static_cast<uint16_t>(dest_dlen);
            write_size_bytes = cbPKT_HEADER_SIZE + dest_dlen * 4;
        }
        write_data = translated_buf;
    } else {
        write_data = reinterpret_cast<const uint8_t*>(&pkt);
        write_size_bytes = (cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen) * sizeof(uint32_t);
    }

    // Round up to dword-aligned size for ring buffer
    uint32_t pkt_size_words = (write_size_bytes + 3) / 4;

    auto* xmt = m_impl->xmtGlobal();
    uint32_t* buf = m_impl->xmtGlobalBuffer();

    uint32_t head = xmt->headindex;
    uint32_t tail = xmt->tailindex;
    uint32_t last_valid = xmt->last_valid_index;

    // Linear buffer with wrap-to-zero (matches Central's cbSendPacket):
    // Packets are always written CONTIGUOUSLY. If the next packet doesn't fit
    // before last_valid_index, headindex jumps to 0 (not per-word modulo).
    uint32_t new_head = head + pkt_size_words;

    if (new_head > last_valid) {
        // Wrap to start of buffer
        new_head = pkt_size_words;
        head = 0;
        // Check room between 0 and tail
        if (new_head >= tail) {
            return Result<void>::error("Transmit buffer full (wrap)");
        }
    } else if (tail > head) {
        // Tail is ahead, check room
        if (new_head >= tail) {
            return Result<void>::error("Transmit buffer full");
        }
    }
    // else: tail <= head, no wrap issues, plenty of room

    // Two-pass write protocol (matches Central's cbSendPacket):
    // Central's consumer skips entries where the first uint32_t (time field) is 0.
    // 1. Write everything EXCEPT the first uint32_t (time=0 means "not ready")
    // 2. Atomically write the first uint32_t (time field) to signal "packet ready"
    //
    // The time field (first uint32_t) MUST be non-zero. If the caller didn't set it,
    // stamp it from the receive buffer (like old cbSendPacket did).
    const uint32_t* pkt_words = reinterpret_cast<const uint32_t*>(write_data);
    uint32_t time_word = pkt_words[0];
    if (time_word == 0) {
        PROCTIME t = m_impl->recLasttime();
        time_word = (t != 0) ? static_cast<uint32_t>(t) : 1;
    }

    // Pass 1: write payload contiguously, skip first word (leave time=0)
    std::memcpy(&buf[head + 1], &pkt_words[1], (pkt_size_words - 1) * sizeof(uint32_t));

    // Advance head index
    xmt->headindex = new_head;

    // Pass 2: atomically write the time field to mark packet as ready
#ifdef _WIN32
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&buf[head]),
                        static_cast<LONG>(time_word));
#else
    __atomic_store_n(&buf[head], time_word, __ATOMIC_SEQ_CST);
#endif
    return Result<void>::ok();
}

Result<bool> ShmemSession::dequeuePacket(cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }
    if (!m_impl->xmt_buffer_raw) {
        return Result<bool>::error("Transmit buffer not initialized");
    }

    auto* xmt = m_impl->xmtGlobal();
    uint32_t* buf = m_impl->xmtGlobalBuffer();

    uint32_t head = xmt->headindex;
    uint32_t tail = xmt->tailindex;

    if (head == tail) {
        return Result<bool>::ok(false);  // Queue is empty
    }

    // Linear contiguous read (packets never straddle buffer boundary)
    // If tail > head, it means the writer wrapped — skip to 0
    if (tail > head) {
        tail = 0;
    }

    // Wait for the time field to become non-zero (two-pass write protocol)
    if (buf[tail] == 0) {
        return Result<bool>::ok(false);  // Packet not yet ready
    }

    uint32_t* pkt_data = reinterpret_cast<uint32_t*>(&pkt);

    // Read header contiguously
    pkt_data[0] = buf[tail];
    pkt_data[1] = buf[tail + 1];
    pkt_data[2] = buf[tail + 2];
    pkt_data[3] = buf[tail + 3];

    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    // Read remaining payload contiguously
    std::memcpy(&pkt_data[4], &buf[tail + 4], (pkt_size_words - 4) * sizeof(uint32_t));

    // Clear the time field to 0 so it's clean for next use
    buf[tail] = 0;

    xmt->tailindex = tail + pkt_size_words;
    xmt->transmitted++;

    return Result<bool>::ok(true);
}

bool ShmemSession::hasTransmitPackets() const {
    if (!m_impl || !m_impl->is_open || !m_impl->xmt_buffer_raw) {
        return false;
    }
    auto* xmt = m_impl->xmtGlobal();
    return xmt->headindex != xmt->tailindex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Local Transmit Queue Operations (IPC-only packets)
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> ShmemSession::enqueueLocalPacket(const cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->xmt_local_buffer_raw) {
        return Result<void>::error("Local transmit buffer not initialized");
    }

    auto* xmt_local = m_impl->xmtLocal();
    uint32_t* buf = m_impl->xmtLocalBuffer();

    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    uint32_t head = xmt_local->headindex;
    uint32_t tail = xmt_local->tailindex;
    uint32_t last_valid = xmt_local->last_valid_index;

    // Linear buffer with wrap-to-zero (matches Central's cbSendLoopbackPacket)
    uint32_t new_head = head + pkt_size_words;

    if (new_head > last_valid) {
        new_head = pkt_size_words;
        head = 0;
        if (new_head >= tail) {
            return Result<void>::error("Local transmit buffer full (wrap)");
        }
    } else if (tail > head) {
        if (new_head >= tail) {
            return Result<void>::error("Local transmit buffer full");
        }
    }

    // Two-pass write: payload first, then atomically write time field
    const uint32_t* pkt_data = reinterpret_cast<const uint32_t*>(&pkt);
    uint32_t time_word = pkt_data[0];
    if (time_word == 0) {
        PROCTIME t = m_impl->recLasttime();
        time_word = (t != 0) ? static_cast<uint32_t>(t) : 1;
    }

    std::memcpy(&buf[head + 1], &pkt_data[1], (pkt_size_words - 1) * sizeof(uint32_t));
    xmt_local->headindex = new_head;

#ifdef _WIN32
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&buf[head]),
                        static_cast<LONG>(time_word));
#else
    __atomic_store_n(&buf[head], time_word, __ATOMIC_SEQ_CST);
#endif
    return Result<void>::ok();
}

Result<bool> ShmemSession::dequeueLocalPacket(cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }
    if (!m_impl->xmt_local_buffer_raw) {
        return Result<bool>::error("Local transmit buffer not initialized");
    }

    auto* xmt_local = m_impl->xmtLocal();
    uint32_t* buf = m_impl->xmtLocalBuffer();

    uint32_t head = xmt_local->headindex;
    uint32_t tail = xmt_local->tailindex;

    if (head == tail) {
        return Result<bool>::ok(false);  // Queue is empty
    }

    uint32_t buflen = xmt_local->bufferlen;

    uint32_t* pkt_data = reinterpret_cast<uint32_t*>(&pkt);

    if (tail + 4 <= buflen) {
        pkt_data[0] = buf[tail];
        pkt_data[1] = buf[tail + 1];
        pkt_data[2] = buf[tail + 2];
        pkt_data[3] = buf[tail + 3];
    } else {
        pkt_data[0] = buf[tail];
        pkt_data[1] = buf[(tail + 1) % buflen];
        pkt_data[2] = buf[(tail + 2) % buflen];
        pkt_data[3] = buf[(tail + 3) % buflen];
    }

    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    tail = (tail + 4) % buflen;
    for (uint32_t i = 4; i < pkt_size_words; ++i) {
        pkt_data[i] = buf[tail];
        tail = (tail + 1) % buflen;
    }

    xmt_local->tailindex = tail;
    xmt_local->transmitted++;

    return Result<bool>::ok(true);
}

bool ShmemSession::hasLocalTransmitPackets() const {
    if (!m_impl || !m_impl->is_open || !m_impl->xmt_local_buffer_raw) {
        return false;
    }
    auto* xmt_local = m_impl->xmtLocal();
    return xmt_local->headindex != xmt_local->tailindex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PC Status Buffer Access

Result<uint32_t> ShmemSession::getNumTotalChans() const {
    if (!m_impl || !m_impl->is_open) {
        return Result<uint32_t>::error("Session is not open");
    }
    if (!m_impl->status_buffer_raw) {
        return Result<uint32_t>::error("Status buffer not initialized");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        return Result<uint32_t>::ok(static_cast<NativePCStatus*>(m_impl->status_buffer_raw)->m_nNumTotalChans);
    } else {
        // CENTRAL and CENTRAL_COMPAT share the same CentralPCStatus struct
        return Result<uint32_t>::ok(static_cast<CentralPCStatus*>(m_impl->status_buffer_raw)->m_nNumTotalChans);
    }
}

Result<NSPStatus> ShmemSession::getNspStatus(cbproto::InstrumentId id) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<NSPStatus>::error("Session is not open");
    }
    if (!m_impl->status_buffer_raw) {
        return Result<NSPStatus>::error("Status buffer not initialized");
    }

    uint32_t index = id.toIndex();

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (index != 0) {
            return Result<NSPStatus>::error("Native mode: single instrument only");
        }
        return Result<NSPStatus>::ok(static_cast<NativePCStatus*>(m_impl->status_buffer_raw)->m_nNspStatus);
    } else {
        if (index >= CENTRAL_cbMAXPROCS) {
            return Result<NSPStatus>::error("Invalid instrument ID");
        }
        return Result<NSPStatus>::ok(static_cast<CentralPCStatus*>(m_impl->status_buffer_raw)->m_nNspStatus[index]);
    }
}

Result<void> ShmemSession::setNspStatus(cbproto::InstrumentId id, NSPStatus status) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->status_buffer_raw) {
        return Result<void>::error("Status buffer not initialized");
    }

    uint32_t index = id.toIndex();

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (index != 0) {
            return Result<void>::error("Native mode: single instrument only");
        }
        static_cast<NativePCStatus*>(m_impl->status_buffer_raw)->m_nNspStatus = status;
    } else {
        if (index >= CENTRAL_cbMAXPROCS) {
            return Result<void>::error("Invalid instrument ID");
        }
        static_cast<CentralPCStatus*>(m_impl->status_buffer_raw)->m_nNspStatus[index] = status;
    }

    return Result<void>::ok();
}

Result<bool> ShmemSession::isGeminiSystem() const {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }
    if (!m_impl->status_buffer_raw) {
        return Result<bool>::error("Status buffer not initialized");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        return Result<bool>::ok(static_cast<NativePCStatus*>(m_impl->status_buffer_raw)->m_nGeminiSystem != 0);
    } else {
        return Result<bool>::ok(static_cast<CentralPCStatus*>(m_impl->status_buffer_raw)->m_nGeminiSystem != 0);
    }
}

Result<void> ShmemSession::setGeminiSystem(bool is_gemini) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->status_buffer_raw) {
        return Result<void>::error("Status buffer not initialized");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        static_cast<NativePCStatus*>(m_impl->status_buffer_raw)->m_nGeminiSystem = is_gemini ? 1 : 0;
    } else {
        static_cast<CentralPCStatus*>(m_impl->status_buffer_raw)->m_nGeminiSystem = is_gemini ? 1 : 0;
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Cache Buffer Access

Result<void> ShmemSession::getSpikeCache(uint32_t channel, CentralSpikeCache& cache) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->spike_buffer_raw) {
        return Result<void>::error("Spike buffer not initialized");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (channel >= NATIVE_cbPKT_SPKCACHELINECNT) {
            return Result<void>::error("Invalid channel number");
        }
        // Copy from NativeSpikeCache to CentralSpikeCache (same field layout)
        auto* spike = static_cast<NativeSpikeBuffer*>(m_impl->spike_buffer_raw);
        auto& src = spike->cache[channel];
        cache.chid = src.chid;
        cache.pktcnt = src.pktcnt;
        cache.pktsize = src.pktsize;
        cache.head = src.head;
        cache.valid = src.valid;
        std::memcpy(cache.spkpkt, src.spkpkt, sizeof(cbPKT_SPK) * src.pktcnt);
    } else {
        if (channel >= CENTRAL_cbPKT_SPKCACHELINECNT) {
            return Result<void>::error("Invalid channel number");
        }
        auto* spike = static_cast<CentralSpikeBuffer*>(m_impl->spike_buffer_raw);
        cache = spike->cache[channel];
    }

    return Result<void>::ok();
}

Result<bool> ShmemSession::getRecentSpike(uint32_t channel, cbPKT_SPK& spike) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }
    if (!m_impl->spike_buffer_raw) {
        return Result<bool>::error("Spike buffer not initialized");
    }

    if (m_impl->layout == ShmemLayout::NATIVE) {
        if (channel >= NATIVE_cbPKT_SPKCACHELINECNT) {
            return Result<bool>::error("Invalid channel number");
        }
        auto* buf = static_cast<NativeSpikeBuffer*>(m_impl->spike_buffer_raw);
        const auto& cache = buf->cache[channel];
        if (cache.valid == 0) {
            return Result<bool>::ok(false);
        }
        uint32_t recent_idx = (cache.head == 0) ? (cache.pktcnt - 1) : (cache.head - 1);
        spike = cache.spkpkt[recent_idx];
        return Result<bool>::ok(true);
    } else {
        if (channel >= CENTRAL_cbPKT_SPKCACHELINECNT) {
            return Result<bool>::error("Invalid channel number");
        }
        auto* buf = static_cast<CentralSpikeBuffer*>(m_impl->spike_buffer_raw);
        const auto& cache = buf->cache[channel];
        if (cache.valid == 0) {
            return Result<bool>::ok(false);
        }
        uint32_t recent_idx = (cache.head == 0) ? (cache.pktcnt - 1) : (cache.head - 1);
        spike = cache.spkpkt[recent_idx];
        return Result<bool>::ok(true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Synchronization methods

Result<bool> ShmemSession::waitForData(uint32_t timeout_ms) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }

#ifdef _WIN32
    if (!m_impl->signal_event) {
        return Result<bool>::error("Signal event not initialized");
    }

    DWORD result = WaitForSingleObject(m_impl->signal_event, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        return Result<bool>::ok(true);
    } else if (result == WAIT_TIMEOUT) {
        return Result<bool>::ok(false);
    } else {
        return Result<bool>::error("WaitForSingleObject failed");
    }

#else
    if (m_impl->signal_event == SEM_FAILED) {
        return Result<bool>::error("Signal event not initialized");
    }

    timespec ts;
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif

    long ns = timeout_ms * 1000000L;
    const long NANOSECONDS_PER_SEC = 1000000000L;
    ts.tv_nsec += ns;
    if (ts.tv_nsec >= NANOSECONDS_PER_SEC) {
        ts.tv_sec += ts.tv_nsec / NANOSECONDS_PER_SEC;
        ts.tv_nsec = ts.tv_nsec % NANOSECONDS_PER_SEC;
    }

#ifdef __APPLE__
    int retries = timeout_ms / 10;
    for (int i = 0; i < retries; ++i) {
        if (sem_trywait(m_impl->signal_event) == 0) {
            return Result<bool>::ok(true);
        }
        usleep(10000);
    }
    if (sem_trywait(m_impl->signal_event) == 0) {
        return Result<bool>::ok(true);
    }
    return Result<bool>::ok(false);
#else
    int result = sem_timedwait(m_impl->signal_event, &ts);
    if (result == 0) {
        return Result<bool>::ok(true);
    } else if (errno == ETIMEDOUT) {
        return Result<bool>::ok(false);
    } else {
        return Result<bool>::error("sem_timedwait failed: " + std::string(strerror(errno)));
    }
#endif
#endif
}

Result<void> ShmemSession::signalData() {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

#ifdef _WIN32
    if (!m_impl->signal_event) {
        return Result<void>::error("Signal event not initialized");
    }
    if (!SetEvent(m_impl->signal_event)) {
        return Result<void>::error("SetEvent failed");
    }
    return Result<void>::ok();
#else
    if (m_impl->signal_event == SEM_FAILED) {
        return Result<void>::error("Signal event not initialized");
    }
    if (sem_post(m_impl->signal_event) != 0) {
        return Result<void>::error("sem_post failed: " + std::string(strerror(errno)));
    }
    return Result<void>::ok();
#endif
}

Result<void> ShmemSession::resetSignal() {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

#ifdef _WIN32
    if (!m_impl->signal_event) {
        return Result<void>::error("Signal event not initialized");
    }
    if (!ResetEvent(m_impl->signal_event)) {
        return Result<void>::error("ResetEvent failed");
    }
    return Result<void>::ok();
#else
    if (m_impl->signal_event == SEM_FAILED) {
        return Result<void>::error("Signal event not initialized");
    }
    while (sem_trywait(m_impl->signal_event) == 0) {
        // Drain all pending signals
    }
    return Result<void>::ok();
#endif
}

PROCTIME ShmemSession::getLastTime() const {
    if (!m_impl || !m_impl->is_open || !m_impl->rec_buffer_raw) {
        return 0;
    }
    return m_impl->recLasttime();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument Filtering

void ShmemSession::setInstrumentFilter(int32_t instrument_index) {
    m_impl->instrument_filter = instrument_index;
}

int32_t ShmemSession::getInstrumentFilter() const {
    return m_impl->instrument_filter;
}

cbproto_protocol_version_t ShmemSession::getCompatProtocolVersion() const {
    return m_impl->compat_protocol;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Receive buffer reading methods

Result<void> ShmemSession::readReceiveBuffer(cbPKT_GENERIC* packets, size_t max_packets, size_t& packets_read) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->rec_buffer_raw) {
        return Result<void>::error("Receive buffer not initialized");
    }
    if (!packets || max_packets == 0) {
        return Result<void>::error("Invalid parameters");
    }

    packets_read = 0;
    uint32_t* buf = m_impl->recBuffer();
    uint32_t buflen = m_impl->rec_buffer_len;

    uint32_t head_index = m_impl->recHeadindex();
    uint32_t head_wrap = m_impl->recHeadwrap();

    if (m_impl->rec_tailwrap == head_wrap && m_impl->rec_tailindex == head_index) {
        return Result<void>::ok();
    }

    const bool needs_translation = (m_impl->layout == ShmemLayout::CENTRAL_COMPAT &&
                                     m_impl->compat_protocol != CBPROTO_PROTOCOL_CURRENT);

    while (packets_read < max_packets) {
        if (m_impl->rec_tailwrap == head_wrap && m_impl->rec_tailindex == head_index) {
            break;
        }

        if ((m_impl->rec_tailwrap + 1 == head_wrap && m_impl->rec_tailindex < head_index) ||
            (m_impl->rec_tailwrap + 1 < head_wrap)) {
            m_impl->rec_tailindex = head_index;
            m_impl->rec_tailwrap = head_wrap;
            return Result<void>::error("Receive buffer overrun - data lost");
        }

        // Parse the packet header to determine packet size based on protocol version.
        // Central writes raw device packets; the header format depends on the protocol.
        uint32_t raw_header_32size;
        uint32_t raw_dlen;

        if (needs_translation && m_impl->compat_protocol == CBPROTO_PROTOCOL_311) {
            raw_header_32size = cbproto::HEADER_SIZE_311 / sizeof(uint32_t);  // 2
            auto* hdr = reinterpret_cast<const cbproto::cbPKT_HEADER_311*>(&buf[m_impl->rec_tailindex]);
            raw_dlen = hdr->dlen;
        } else if (needs_translation && m_impl->compat_protocol == CBPROTO_PROTOCOL_400) {
            raw_header_32size = cbproto::HEADER_SIZE_400 / sizeof(uint32_t);  // 4
            auto* hdr = reinterpret_cast<const cbproto::cbPKT_HEADER_400*>(&buf[m_impl->rec_tailindex]);
            raw_dlen = hdr->dlen;
        } else {
            // 4.1+ / current / NATIVE / CENTRAL layouts: use current header format
            raw_header_32size = cbPKT_HEADER_32SIZE;  // 4
            auto* hdr = reinterpret_cast<const cbPKT_HEADER*>(&buf[m_impl->rec_tailindex]);
            raw_dlen = hdr->dlen;
        }

        uint32_t pkt_size_dwords = raw_header_32size + raw_dlen;

        if (pkt_size_dwords == 0 || pkt_size_dwords > (sizeof(cbPKT_GENERIC) / sizeof(uint32_t))) {
            m_impl->rec_tailindex++;
            if (m_impl->rec_tailindex >= buflen) {
                m_impl->rec_tailindex = 0;
                m_impl->rec_tailwrap++;
            }
            continue;
        }

        if (needs_translation) {
            // Copy raw bytes from ring buffer into a temp buffer for translation.
            // Packets never straddle the buffer boundary (Central wraps before that),
            // but we handle it defensively.
            uint8_t raw_buf[cbPKT_MAX_SIZE];
            uint32_t raw_bytes = pkt_size_dwords * sizeof(uint32_t);
            uint32_t end_index = m_impl->rec_tailindex + pkt_size_dwords;

            if (end_index <= buflen) {
                std::memcpy(raw_buf, &buf[m_impl->rec_tailindex], raw_bytes);
            } else {
                uint32_t first_part = (buflen - m_impl->rec_tailindex) * sizeof(uint32_t);
                uint32_t second_part = raw_bytes - first_part;
                std::memcpy(raw_buf, &buf[m_impl->rec_tailindex], first_part);
                std::memcpy(raw_buf + first_part, &buf[0], second_part);
            }

            // Translate header + payload into the output slot
            uint8_t* dest = reinterpret_cast<uint8_t*>(&packets[packets_read]);
            auto& dest_hdr = packets[packets_read].cbpkt_header;

            if (m_impl->compat_protocol == CBPROTO_PROTOCOL_311) {
                auto* src_hdr = reinterpret_cast<const cbproto::cbPKT_HEADER_311*>(raw_buf);
                // Translate header: 3.11 (8 bytes) → current (16 bytes)
                dest_hdr.time = static_cast<PROCTIME>(src_hdr->time) * 1000000000ULL / 30000ULL;
                dest_hdr.chid = src_hdr->chid;
                dest_hdr.type = src_hdr->type;
                dest_hdr.dlen = src_hdr->dlen;
                dest_hdr.instrument = 0;
                dest_hdr.reserved = 0;
                // Translate payload
                cbproto::PacketTranslator::translatePayload_311_to_current(raw_buf, dest);
            } else if (m_impl->compat_protocol == CBPROTO_PROTOCOL_400) {
                auto* src_hdr = reinterpret_cast<const cbproto::cbPKT_HEADER_400*>(raw_buf);
                // Translate header: 4.0 (16 bytes, different field layout) → current (16 bytes)
                dest_hdr.time = src_hdr->time;
                dest_hdr.chid = src_hdr->chid;
                dest_hdr.type = src_hdr->type;  // 8-bit type → 16-bit (zero-extended)
                dest_hdr.dlen = src_hdr->dlen;
                dest_hdr.instrument = src_hdr->instrument;
                dest_hdr.reserved = 0;
                // Translate payload
                cbproto::PacketTranslator::translatePayload_400_to_current(raw_buf, dest);
            } else {
                // 4.1 (CBPROTO_PROTOCOL_410): header is identical, only some payloads differ
                std::memcpy(dest, raw_buf, raw_bytes);
                cbproto::PacketTranslator::translatePayload_410_to_current(dest, dest);
            }
        } else {
            // No translation needed (4.2+, NATIVE, or CENTRAL layout).
            // Copy directly from ring buffer to output.
            uint32_t end_index = m_impl->rec_tailindex + pkt_size_dwords;

            if (end_index <= buflen) {
                std::memcpy(&packets[packets_read],
                           &buf[m_impl->rec_tailindex],
                           pkt_size_dwords * sizeof(uint32_t));
            } else {
                uint32_t first_part_size = buflen - m_impl->rec_tailindex;
                uint32_t second_part_size = pkt_size_dwords - first_part_size;

                std::memcpy(&packets[packets_read],
                           &buf[m_impl->rec_tailindex],
                           first_part_size * sizeof(uint32_t));

                std::memcpy(reinterpret_cast<uint32_t*>(&packets[packets_read]) + first_part_size,
                           &buf[0],
                           second_part_size * sizeof(uint32_t));
            }
        }

        // Advance tail past this packet (consumed from ring buffer regardless of filter)
        m_impl->rec_tailindex += pkt_size_dwords;
        if (m_impl->rec_tailindex >= buflen) {
            m_impl->rec_tailindex -= buflen;
            m_impl->rec_tailwrap++;
        }

        // Apply instrument filter: skip packets not matching our instrument
        if (m_impl->instrument_filter >= 0) {
            uint8_t pkt_instrument = packets[packets_read].cbpkt_header.instrument;
            if (pkt_instrument != static_cast<uint8_t>(m_impl->instrument_filter)) {
                continue;  // Skip this packet, don't increment packets_read
            }
        }

        packets_read++;
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::getReceiveBufferStats(uint32_t& received, uint32_t& available) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }
    if (!m_impl->rec_buffer_raw) {
        return Result<void>::error("Receive buffer not initialized");
    }

    received = m_impl->recReceived();
    uint32_t buflen = m_impl->rec_buffer_len;

    uint32_t head_index = m_impl->recHeadindex();
    uint32_t head_wrap = m_impl->recHeadwrap();

    if (m_impl->rec_tailwrap == head_wrap) {
        if (head_index >= m_impl->rec_tailindex) {
            available = head_index - m_impl->rec_tailindex;
        } else {
            available = 0;
        }
    } else if (m_impl->rec_tailwrap + 1 == head_wrap) {
        available = (buflen - m_impl->rec_tailindex) + head_index;
    } else {
        available = 0;
    }

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Clock Synchronization

void ShmemSession::setClockSync(int64_t offset_ns, int64_t uncertainty_ns) {
    if (!isOpen())
        return;

    if (m_impl->layout == ShmemLayout::NATIVE) {
        auto* cfg = m_impl->nativeCfg();
        cfg->clock_offset_ns = offset_ns;
        cfg->clock_uncertainty_ns = uncertainty_ns;
        cfg->clock_sync_valid = 1;
    }
    // CENTRAL and CENTRAL_COMPAT layouts don't have clock sync fields
}

std::optional<int64_t> ShmemSession::getClockOffsetNs() const {
    if (!isOpen())
        return std::nullopt;

    if (m_impl->layout == ShmemLayout::NATIVE) {
        const auto* cfg = m_impl->nativeCfg();
        if (cfg->clock_sync_valid)
            return cfg->clock_offset_ns;
    }
    return std::nullopt;
}

std::optional<int64_t> ShmemSession::getClockUncertaintyNs() const {
    if (!isOpen())
        return std::nullopt;

    if (m_impl->layout == ShmemLayout::NATIVE) {
        const auto* cfg = m_impl->nativeCfg();
        if (cfg->clock_sync_valid)
            return cfg->clock_uncertainty_ns;
    }
    return std::nullopt;
}

} // namespace cbshm
