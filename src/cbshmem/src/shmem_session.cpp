///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   shmem_session.cpp
/// @author CereLink Development Team
/// @date   2025-11-11
///
/// @brief  Shared memory session implementation
///
/// Implements cross-platform shared memory management with Central-compatible layout
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cbshmem/shmem_session.h>
#include <cbshmem/central_types.h>
#include <cstring>

// Platform-specific headers
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/time.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <semaphore.h>
    #include <time.h>
    #include <errno.h>
#endif

namespace cbshmem {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Platform-specific implementation details (Pimpl idiom)
///
struct ShmemSession::Impl {
    Mode mode;
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

    // Pointers to shared memory buffers
    CentralConfigBuffer* cfg_buffer;
    CentralReceiveBuffer* rec_buffer;
    CentralTransmitBuffer* xmt_buffer;
    CentralTransmitBufferLocal* xmt_local_buffer;
    CentralPCStatus* status_buffer;
    CentralSpikeBuffer* spike_buffer;

    // Receive buffer read tracking (for CLIENT mode reading)
    uint32_t rec_tailindex;      // Our read position in receive buffer
    uint32_t rec_tailwrap;       // Our wrap counter

    Impl()
        : mode(Mode::STANDALONE)
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
        , cfg_buffer(nullptr)
        , rec_buffer(nullptr)
        , xmt_buffer(nullptr)
        , xmt_local_buffer(nullptr)
        , status_buffer(nullptr)
        , spike_buffer(nullptr)
        , rec_tailindex(0)
        , rec_tailwrap(0)
    {}

    ~Impl() {
        close();
    }

    void close() {
        if (!is_open) return;

        // Unmap shared memory
#ifdef _WIN32
        if (cfg_buffer) UnmapViewOfFile(cfg_buffer);
        if (rec_buffer) UnmapViewOfFile(rec_buffer);
        if (xmt_buffer) UnmapViewOfFile(xmt_buffer);
        if (xmt_local_buffer) UnmapViewOfFile(xmt_local_buffer);
        if (status_buffer) UnmapViewOfFile(status_buffer);
        if (spike_buffer) UnmapViewOfFile(spike_buffer);
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

        if (cfg_buffer) {
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
        }
        if (rec_buffer) {
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
        }
        if (xmt_buffer) {
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
        }
        if (xmt_local_buffer) {
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
        }
        if (status_buffer) {
            munmap(status_buffer, sizeof(CentralPCStatus));
        }
        if (spike_buffer) {
            munmap(spike_buffer, sizeof(CentralSpikeBuffer));
        }

        if (cfg_shm_fd >= 0) {
            ::close(cfg_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_cfg_name.c_str());
            }
        }

        if (rec_shm_fd >= 0) {
            ::close(rec_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_rec_name.c_str());
            }
        }

        if (xmt_shm_fd >= 0) {
            ::close(xmt_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_xmt_name.c_str());
            }
        }

        if (xmt_local_shm_fd >= 0) {
            ::close(xmt_local_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_xmt_local_name.c_str());
            }
        }

        if (status_shm_fd >= 0) {
            ::close(status_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_status_name.c_str());
            }
        }

        if (spk_shm_fd >= 0) {
            ::close(spk_shm_fd);
            if (mode == Mode::STANDALONE) {
                shm_unlink(posix_spk_name.c_str());
            }
        }

        // Close signal event (named semaphore)
        if (signal_event != SEM_FAILED) {
            sem_close(signal_event);
            if (mode == Mode::STANDALONE) {
                sem_unlink(posix_signal_name.c_str());
            }
        }

        cfg_shm_fd = -1;
        rec_shm_fd = -1;
        xmt_shm_fd = -1;
        xmt_local_shm_fd = -1;
        status_shm_fd = -1;
        spk_shm_fd = -1;
        signal_event = SEM_FAILED;
#endif

        cfg_buffer = nullptr;
        rec_buffer = nullptr;
        xmt_buffer = nullptr;
        xmt_local_buffer = nullptr;
        status_buffer = nullptr;
        spike_buffer = nullptr;
        is_open = false;
    }

    /// @brief Write a packet to the receive buffer ring
    /// @param pkt Packet to write
    /// @return Result indicating success or failure
    Result<void> writeToReceiveBuffer(const cbPKT_GENERIC& pkt) {
        if (!rec_buffer) {
            return Result<void>::error("Receive buffer not initialized");
        }

        // Calculate packet size in uint32_t words
        // packet header is cbPKT_HEADER_32SIZE words (4 words with 64-bit PROCTIME)
        // dlen is payload in uint32_t words
        uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

        // Check if packet fits in buffer
        if (pkt_size_words > CENTRAL_cbRECBUFFLEN) {
            return Result<void>::error("Packet too large for receive buffer");
        }

        // Get current head index
        uint32_t head = rec_buffer->headindex;

        // Check if we need to wrap
        if (head + pkt_size_words > CENTRAL_cbRECBUFFLEN) {
            // Wrap to beginning
            head = 0;
            rec_buffer->headwrap++;
        }

        // Copy packet data to buffer (as uint32_t words)
        const uint32_t* pkt_data = reinterpret_cast<const uint32_t*>(&pkt);
        for (uint32_t i = 0; i < pkt_size_words; ++i) {
            rec_buffer->buffer[head + i] = pkt_data[i];
        }

        // Update head index
        rec_buffer->headindex = head + pkt_size_words;

        // Update packet count and timestamp
        rec_buffer->received++;
        rec_buffer->lasttime = pkt.cbpkt_header.time;

        return Result<void>::ok();
    }

    Result<void> open() {
        if (is_open) {
            return Result<void>::error("Session already open");
        }

#ifdef _WIN32
        // Windows implementation - create two separate shared memory segments
        DWORD access = (mode == Mode::STANDALONE) ? PAGE_READWRITE : PAGE_READONLY;
        DWORD map_access = (mode == Mode::STANDALONE) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        // Create config buffer segment
        cfg_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralConfigBuffer),
            cfg_name.c_str()
        );

        if (!cfg_file_mapping) {
            return Result<void>::error("Failed to create config file mapping");
        }

        cfg_buffer = static_cast<CentralConfigBuffer*>(
            MapViewOfFile(cfg_file_mapping, map_access, 0, 0, sizeof(CentralConfigBuffer))
        );

        if (!cfg_buffer) {
            CloseHandle(cfg_file_mapping);
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to map config view of file");
        }

        // Create receive buffer segment
        rec_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralReceiveBuffer),
            rec_name.c_str()
        );

        if (!rec_file_mapping) {
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(cfg_file_mapping);
            cfg_buffer = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create receive file mapping");
        }

        rec_buffer = static_cast<CentralReceiveBuffer*>(
            MapViewOfFile(rec_file_mapping, map_access, 0, 0, sizeof(CentralReceiveBuffer))
        );

        if (!rec_buffer) {
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(cfg_file_mapping);
            CloseHandle(rec_file_mapping);
            cfg_buffer = nullptr;
            cfg_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            return Result<void>::error("Failed to map receive view of file");
        }

        // Create transmit buffer segment (separate from config and receive)
        xmt_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralTransmitBuffer),
            xmt_name.c_str()
        );

        if (!xmt_file_mapping) {
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create transmit file mapping");
        }

        xmt_buffer = static_cast<CentralTransmitBuffer*>(
            MapViewOfFile(xmt_file_mapping, map_access, 0, 0, sizeof(CentralTransmitBuffer))
        );

        if (!xmt_buffer) {
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to map transmit view of file");
        }

        // Create local transmit buffer segment (separate from global transmit)
        xmt_local_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralTransmitBufferLocal),
            xmt_local_name.c_str()
        );

        if (!xmt_local_file_mapping) {
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create local transmit file mapping");
        }

        xmt_local_buffer = static_cast<CentralTransmitBufferLocal*>(
            MapViewOfFile(xmt_local_file_mapping, map_access, 0, 0, sizeof(CentralTransmitBufferLocal))
        );

        if (!xmt_local_buffer) {
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to map local transmit view of file");
        }

        // Create PC status buffer segment
        status_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralPCStatus),
            status_name.c_str()
        );

        if (!status_file_mapping) {
            UnmapViewOfFile(xmt_local_buffer);
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create status file mapping");
        }

        status_buffer = static_cast<CentralPCStatus*>(
            MapViewOfFile(status_file_mapping, map_access, 0, 0, sizeof(CentralPCStatus))
        );

        if (!status_buffer) {
            UnmapViewOfFile(xmt_local_buffer);
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(status_file_mapping);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            status_file_mapping = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to map status view of file");
        }

        // Create spike cache buffer (6th segment)
        spk_file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralSpikeBuffer),
            spk_name.c_str()
        );

        if (!spk_file_mapping) {
            UnmapViewOfFile(status_buffer);
            UnmapViewOfFile(xmt_local_buffer);
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(status_file_mapping);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            status_file_mapping = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create spike buffer file mapping");
        }

        spike_buffer = static_cast<CentralSpikeBuffer*>(
            MapViewOfFile(spk_file_mapping, map_access, 0, 0, sizeof(CentralSpikeBuffer))
        );

        if (!spike_buffer) {
            UnmapViewOfFile(status_buffer);
            UnmapViewOfFile(xmt_local_buffer);
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(spk_file_mapping);
            CloseHandle(status_file_mapping);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            spike_buffer = nullptr;
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            spk_file_mapping = nullptr;
            status_file_mapping = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to map spike buffer view of file");
        }

        // Create/open signal event (7th: synchronization primitive)
        if (mode == Mode::STANDALONE) {
            // STANDALONE mode: Create the event (manual-reset, initially non-signaled)
            signal_event = CreateEventA(nullptr, TRUE, FALSE, signal_event_name.c_str());
        } else {
            // CLIENT mode: Open existing event (SYNCHRONIZE access for waiting)
            signal_event = OpenEventA(SYNCHRONIZE, FALSE, signal_event_name.c_str());
        }

        if (!signal_event) {
            UnmapViewOfFile(spike_buffer);
            UnmapViewOfFile(status_buffer);
            UnmapViewOfFile(xmt_local_buffer);
            UnmapViewOfFile(xmt_buffer);
            UnmapViewOfFile(rec_buffer);
            UnmapViewOfFile(cfg_buffer);
            CloseHandle(spk_file_mapping);
            CloseHandle(status_file_mapping);
            CloseHandle(xmt_local_file_mapping);
            CloseHandle(xmt_file_mapping);
            CloseHandle(rec_file_mapping);
            CloseHandle(cfg_file_mapping);
            spike_buffer = nullptr;
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            spk_file_mapping = nullptr;
            status_file_mapping = nullptr;
            xmt_local_file_mapping = nullptr;
            xmt_file_mapping = nullptr;
            rec_file_mapping = nullptr;
            cfg_file_mapping = nullptr;
            return Result<void>::error("Failed to create/open signal event");
        }

#else
        // POSIX (macOS/Linux) implementation - create six separate shared memory segments
        // POSIX requires shared memory names to start with "/"
        std::string posix_cfg_name = (cfg_name[0] == '/') ? cfg_name : ("/" + cfg_name);
        std::string posix_rec_name = (rec_name[0] == '/') ? rec_name : ("/" + rec_name);
        std::string posix_xmt_name = (xmt_name[0] == '/') ? xmt_name : ("/" + xmt_name);
        std::string posix_xmt_local_name = (xmt_local_name[0] == '/') ? xmt_local_name : ("/" + xmt_local_name);
        std::string posix_status_name = (status_name[0] == '/') ? status_name : ("/" + status_name);
        std::string posix_spk_name = (spk_name[0] == '/') ? spk_name : ("/" + spk_name);
        std::string posix_signal_name = (signal_event_name[0] == '/') ? signal_event_name : ("/" + signal_event_name);

        int flags = (mode == Mode::STANDALONE) ? (O_CREAT | O_RDWR) : O_RDONLY;
        mode_t perms = (mode == Mode::STANDALONE) ? 0644 : 0;
        int prot = (mode == Mode::STANDALONE) ? (PROT_READ | PROT_WRITE) : PROT_READ;

        // In STANDALONE mode, unlink any existing shared memory first
        if (mode == Mode::STANDALONE) {
            shm_unlink(posix_cfg_name.c_str());  // Ignore errors if it doesn't exist
            shm_unlink(posix_rec_name.c_str());
            shm_unlink(posix_xmt_name.c_str());
            shm_unlink(posix_xmt_local_name.c_str());
            shm_unlink(posix_status_name.c_str());
            shm_unlink(posix_spk_name.c_str());
        }

        // Create/open config buffer segment
        cfg_shm_fd = shm_open(posix_cfg_name.c_str(), flags, perms);
        if (cfg_shm_fd < 0) {
            return Result<void>::error("Failed to open config shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(cfg_shm_fd, sizeof(CentralConfigBuffer)) < 0) {
                std::string err_msg = "Failed to set config shared memory size: " + std::string(strerror(errno));
                ::close(cfg_shm_fd);
                cfg_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        cfg_buffer = static_cast<CentralConfigBuffer*>(
            mmap(nullptr, sizeof(CentralConfigBuffer), prot, MAP_SHARED, cfg_shm_fd, 0)
        );

        if (cfg_buffer == MAP_FAILED) {
            ::close(cfg_shm_fd);
            cfg_shm_fd = -1;
            cfg_buffer = nullptr;
            return Result<void>::error("Failed to map config shared memory");
        }

        // Create/open receive buffer segment
        rec_shm_fd = shm_open(posix_rec_name.c_str(), flags, perms);
        if (rec_shm_fd < 0) {
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(cfg_shm_fd);
            cfg_buffer = nullptr;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to open receive shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(rec_shm_fd, sizeof(CentralReceiveBuffer)) < 0) {
                std::string err_msg = "Failed to set receive shared memory size: " + std::string(strerror(errno));
                munmap(cfg_buffer, sizeof(CentralConfigBuffer));
                ::close(cfg_shm_fd);
                ::close(rec_shm_fd);
                cfg_buffer = nullptr;
                cfg_shm_fd = -1;
                rec_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        rec_buffer = static_cast<CentralReceiveBuffer*>(
            mmap(nullptr, sizeof(CentralReceiveBuffer), prot, MAP_SHARED, rec_shm_fd, 0)
        );

        if (rec_buffer == MAP_FAILED) {
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(cfg_shm_fd);
            ::close(rec_shm_fd);
            cfg_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_shm_fd = -1;
            rec_shm_fd = -1;
            return Result<void>::error("Failed to map receive shared memory");
        }

        // Create/open transmit buffer segment
        xmt_shm_fd = shm_open(posix_xmt_name.c_str(), flags, perms);
        if (xmt_shm_fd < 0) {
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to open transmit shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(xmt_shm_fd, sizeof(CentralTransmitBuffer)) < 0) {
                std::string err_msg = "Failed to set transmit shared memory size: " + std::string(strerror(errno));
                munmap(rec_buffer, sizeof(CentralReceiveBuffer));
                munmap(cfg_buffer, sizeof(CentralConfigBuffer));
                ::close(rec_shm_fd);
                ::close(cfg_shm_fd);
                ::close(xmt_shm_fd);
                rec_buffer = nullptr;
                cfg_buffer = nullptr;
                rec_shm_fd = -1;
                cfg_shm_fd = -1;
                xmt_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        xmt_buffer = static_cast<CentralTransmitBuffer*>(
            mmap(nullptr, sizeof(CentralTransmitBuffer), prot, MAP_SHARED, xmt_shm_fd, 0)
        );

        if (xmt_buffer == MAP_FAILED) {
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            ::close(xmt_shm_fd);
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            xmt_shm_fd = -1;
            return Result<void>::error("Failed to map transmit shared memory");
        }

        // Create/open local transmit buffer segment
        xmt_local_shm_fd = shm_open(posix_xmt_local_name.c_str(), flags, perms);
        if (xmt_local_shm_fd < 0) {
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to open local transmit shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(xmt_local_shm_fd, sizeof(CentralTransmitBufferLocal)) < 0) {
                std::string err_msg = "Failed to set local transmit shared memory size: " + std::string(strerror(errno));
                munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
                munmap(rec_buffer, sizeof(CentralReceiveBuffer));
                munmap(cfg_buffer, sizeof(CentralConfigBuffer));
                ::close(xmt_local_shm_fd);
                ::close(xmt_shm_fd);
                ::close(rec_shm_fd);
                ::close(cfg_shm_fd);
                xmt_buffer = nullptr;
                rec_buffer = nullptr;
                cfg_buffer = nullptr;
                xmt_local_shm_fd = -1;
                xmt_shm_fd = -1;
                rec_shm_fd = -1;
                cfg_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        xmt_local_buffer = static_cast<CentralTransmitBufferLocal*>(
            mmap(nullptr, sizeof(CentralTransmitBufferLocal), prot, MAP_SHARED, xmt_local_shm_fd, 0)
        );

        if (xmt_local_buffer == MAP_FAILED) {
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to map local transmit shared memory");
        }

        // Create/open status buffer segment
        status_shm_fd = shm_open(posix_status_name.c_str(), flags, perms);
        if (status_shm_fd < 0) {
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to open status shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(status_shm_fd, sizeof(CentralPCStatus)) < 0) {
                std::string err_msg = "Failed to set status shared memory size: " + std::string(strerror(errno));
                munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
                munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
                munmap(rec_buffer, sizeof(CentralReceiveBuffer));
                munmap(cfg_buffer, sizeof(CentralConfigBuffer));
                ::close(status_shm_fd);
                ::close(xmt_local_shm_fd);
                ::close(xmt_shm_fd);
                ::close(rec_shm_fd);
                ::close(cfg_shm_fd);
                xmt_local_buffer = nullptr;
                xmt_buffer = nullptr;
                rec_buffer = nullptr;
                cfg_buffer = nullptr;
                status_shm_fd = -1;
                xmt_local_shm_fd = -1;
                xmt_shm_fd = -1;
                rec_shm_fd = -1;
                cfg_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        status_buffer = static_cast<CentralPCStatus*>(
            mmap(nullptr, sizeof(CentralPCStatus), prot, MAP_SHARED, status_shm_fd, 0)
        );

        if (status_buffer == MAP_FAILED) {
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(status_shm_fd);
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            status_shm_fd = -1;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to map status shared memory");
        }

        // Create/open spike cache buffer segment
        spk_shm_fd = shm_open(posix_spk_name.c_str(), flags, perms);
        if (spk_shm_fd < 0) {
            munmap(status_buffer, sizeof(CentralPCStatus));
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(status_shm_fd);
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            status_shm_fd = -1;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to open spike cache shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            if (ftruncate(spk_shm_fd, sizeof(CentralSpikeBuffer)) < 0) {
                std::string err_msg = "Failed to set spike cache shared memory size: " + std::string(strerror(errno));
                munmap(status_buffer, sizeof(CentralPCStatus));
                munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
                munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
                munmap(rec_buffer, sizeof(CentralReceiveBuffer));
                munmap(cfg_buffer, sizeof(CentralConfigBuffer));
                ::close(spk_shm_fd);
                ::close(status_shm_fd);
                ::close(xmt_local_shm_fd);
                ::close(xmt_shm_fd);
                ::close(rec_shm_fd);
                ::close(cfg_shm_fd);
                status_buffer = nullptr;
                xmt_local_buffer = nullptr;
                xmt_buffer = nullptr;
                rec_buffer = nullptr;
                cfg_buffer = nullptr;
                spk_shm_fd = -1;
                status_shm_fd = -1;
                xmt_local_shm_fd = -1;
                xmt_shm_fd = -1;
                rec_shm_fd = -1;
                cfg_shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        spike_buffer = static_cast<CentralSpikeBuffer*>(
            mmap(nullptr, sizeof(CentralSpikeBuffer), prot, MAP_SHARED, spk_shm_fd, 0)
        );

        if (spike_buffer == MAP_FAILED) {
            munmap(status_buffer, sizeof(CentralPCStatus));
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(spk_shm_fd);
            ::close(status_shm_fd);
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            spike_buffer = nullptr;
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            spk_shm_fd = -1;
            status_shm_fd = -1;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to map spike cache shared memory");
        }

        // Create/open signal event (7th: named semaphore for synchronization)
        if (mode == Mode::STANDALONE) {
            // STANDALONE mode: Create the semaphore (initial value 0 = blocked)
            // First try to unlink any existing semaphore
            sem_unlink(posix_signal_name.c_str());
            signal_event = sem_open(posix_signal_name.c_str(), O_CREAT | O_EXCL, 0666, 0);
            if (signal_event == SEM_FAILED) {
                // If O_EXCL failed, try without it (semaphore already exists from crashed session)
                signal_event = sem_open(posix_signal_name.c_str(), O_CREAT, 0666, 0);
            }
        } else {
            // CLIENT mode: Open existing semaphore
            signal_event = sem_open(posix_signal_name.c_str(), 0);
        }

        if (signal_event == SEM_FAILED) {
            munmap(spike_buffer, sizeof(CentralSpikeBuffer));
            munmap(status_buffer, sizeof(CentralPCStatus));
            munmap(xmt_local_buffer, sizeof(CentralTransmitBufferLocal));
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
            ::close(spk_shm_fd);
            ::close(status_shm_fd);
            ::close(xmt_local_shm_fd);
            ::close(xmt_shm_fd);
            ::close(rec_shm_fd);
            ::close(cfg_shm_fd);
            spike_buffer = nullptr;
            status_buffer = nullptr;
            xmt_local_buffer = nullptr;
            xmt_buffer = nullptr;
            rec_buffer = nullptr;
            cfg_buffer = nullptr;
            spk_shm_fd = -1;
            status_shm_fd = -1;
            xmt_local_shm_fd = -1;
            xmt_shm_fd = -1;
            rec_shm_fd = -1;
            cfg_shm_fd = -1;
            return Result<void>::error("Failed to create/open signal semaphore: " + std::string(strerror(errno)));
        }
#endif

        // Initialize buffers in standalone mode
        if (mode == Mode::STANDALONE) {
            // Initialize config buffer
            std::memset(cfg_buffer, 0, sizeof(CentralConfigBuffer));
            cfg_buffer->version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR;

            // Mark all instruments as inactive initially
            for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
                cfg_buffer->instrument_status[i] = static_cast<uint32_t>(InstrumentStatus::INACTIVE);
            }

            // Initialize receive buffer (in separate shared memory segment)
            std::memset(rec_buffer, 0, sizeof(CentralReceiveBuffer));
            rec_buffer->received = 0;
            rec_buffer->lasttime = 0;
            rec_buffer->headwrap = 0;
            rec_buffer->headindex = 0;

            // Initialize transmit buffer (in separate shared memory segment)
            std::memset(xmt_buffer, 0, sizeof(CentralTransmitBuffer));
            xmt_buffer->transmitted = 0;
            xmt_buffer->headindex = 0;
            xmt_buffer->tailindex = 0;
            xmt_buffer->last_valid_index = CENTRAL_cbXMT_GLOBAL_BUFFLEN - 1;
            xmt_buffer->bufferlen = CENTRAL_cbXMT_GLOBAL_BUFFLEN;

            // Initialize local transmit buffer (in separate shared memory segment)
            std::memset(xmt_local_buffer, 0, sizeof(CentralTransmitBufferLocal));
            xmt_local_buffer->transmitted = 0;
            xmt_local_buffer->headindex = 0;
            xmt_local_buffer->tailindex = 0;
            xmt_local_buffer->last_valid_index = CENTRAL_cbXMT_LOCAL_BUFFLEN - 1;
            xmt_local_buffer->bufferlen = CENTRAL_cbXMT_LOCAL_BUFFLEN;

            // Initialize status buffer (in separate shared memory segment)
            std::memset(status_buffer, 0, sizeof(CentralPCStatus));
            status_buffer->m_iBlockRecording = 0;
            status_buffer->m_nPCStatusFlags = 0;
            status_buffer->m_nNumFEChans = CENTRAL_cbNUM_FE_CHANS;
            status_buffer->m_nNumAnainChans = CENTRAL_cbNUM_ANAIN_CHANS;
            status_buffer->m_nNumAnalogChans = CENTRAL_cbNUM_ANALOG_CHANS;
            status_buffer->m_nNumAoutChans = CENTRAL_cbNUM_ANAOUT_CHANS;
            status_buffer->m_nNumAudioChans = CENTRAL_cbNUM_AUDOUT_CHANS;
            status_buffer->m_nNumAnalogoutChans = CENTRAL_cbNUM_ANALOGOUT_CHANS;
            status_buffer->m_nNumDiginChans = CENTRAL_cbNUM_DIGIN_CHANS;
            status_buffer->m_nNumSerialChans = CENTRAL_cbNUM_SERIAL_CHANS;
            status_buffer->m_nNumDigoutChans = CENTRAL_cbNUM_DIGOUT_CHANS;
            status_buffer->m_nNumTotalChans = CENTRAL_cbMAXCHANS;
            for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
                status_buffer->m_nNspStatus[i] = NSPStatus::NSP_INIT;
                status_buffer->m_nNumNTrodesPerInstrument[i] = 0;
            }
            status_buffer->m_nGeminiSystem = 0;

            // Initialize spike cache buffer (in separate shared memory segment)
            std::memset(spike_buffer, 0, sizeof(CentralSpikeBuffer));
            spike_buffer->flags = 0;
            spike_buffer->chidmax = CENTRAL_cbNUM_ANALOG_CHANS;
            spike_buffer->linesize = sizeof(CentralSpikeCache);
            spike_buffer->spkcount = 0;
            // Initialize each channel's spike cache
            for (uint32_t ch = 0; ch < CENTRAL_cbPKT_SPKCACHELINECNT; ++ch) {
                spike_buffer->cache[ch].chid = ch;
                spike_buffer->cache[ch].pktcnt = CENTRAL_cbPKT_SPKCACHEPKTCNT;
                spike_buffer->cache[ch].pktsize = sizeof(cbPKT_SPK);
                spike_buffer->cache[ch].head = 0;
                spike_buffer->cache[ch].valid = 0;
            }
        }

        is_open = true;
        return Result<void>::ok();
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
                                           const std::string& signal_event_name, Mode mode) {
    ShmemSession session;
    session.m_impl->cfg_name = cfg_name;
    session.m_impl->rec_name = rec_name;
    session.m_impl->xmt_name = xmt_name;
    session.m_impl->xmt_local_name = xmt_local_name;
    session.m_impl->status_name = status_name;
    session.m_impl->spk_name = spk_name;
    session.m_impl->signal_event_name = signal_event_name;
    session.m_impl->mode = mode;

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
    bool active = (m_impl->cfg_buffer->instrument_status[idx] == static_cast<uint32_t>(InstrumentStatus::ACTIVE));
    return Result<bool>::ok(active);
}

Result<void> ShmemSession::setInstrumentActive(cbproto::InstrumentId id, bool active) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    uint8_t idx = id.toIndex();
    m_impl->cfg_buffer->instrument_status[idx] = active
        ? static_cast<uint32_t>(InstrumentStatus::ACTIVE)
        : static_cast<uint32_t>(InstrumentStatus::INACTIVE);

    return Result<void>::ok();
}

Result<cbproto::InstrumentId> ShmemSession::getFirstActiveInstrument() const {
    if (!isOpen()) {
        return Result<cbproto::InstrumentId>::error("Session not open");
    }

    for (uint8_t i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
        if (m_impl->cfg_buffer->instrument_status[i] == static_cast<uint32_t>(InstrumentStatus::ACTIVE)) {
            return Result<cbproto::InstrumentId>::ok(cbproto::InstrumentId::fromIndex(i));
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

    // THE KEY FIX: Use packet.instrument (0-based) as array index
    uint8_t idx = id.toIndex();

    return Result<cbPKT_PROCINFO>::ok(m_impl->cfg_buffer->procinfo[idx]);
}

Result<cbPKT_BANKINFO> ShmemSession::getBankInfo(cbproto::InstrumentId id, uint32_t bank) const {
    if (!isOpen()) {
        return Result<cbPKT_BANKINFO>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<cbPKT_BANKINFO>::error("Invalid instrument ID");
    }

    // Bank parameter is 1-based (matches cbPKT_BANKINFO.bank), convert to 0-based array index
    if (bank == 0 || bank > CENTRAL_cbMAXBANKS) {
        return Result<cbPKT_BANKINFO>::error("Bank number out of range");
    }

    uint8_t idx = id.toIndex();
    return Result<cbPKT_BANKINFO>::ok(m_impl->cfg_buffer->bankinfo[idx][bank - 1]);
}

Result<cbPKT_FILTINFO> ShmemSession::getFilterInfo(cbproto::InstrumentId id, uint32_t filter) const {
    if (!isOpen()) {
        return Result<cbPKT_FILTINFO>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<cbPKT_FILTINFO>::error("Invalid instrument ID");
    }

    // Filter parameter is 1-based (matches cbPKT_FILTINFO.filt), convert to 0-based array index
    if (filter == 0 || filter > CENTRAL_cbMAXFILTS) {
        return Result<cbPKT_FILTINFO>::error("Filter number out of range");
    }

    uint8_t idx = id.toIndex();
    return Result<cbPKT_FILTINFO>::ok(m_impl->cfg_buffer->filtinfo[idx][filter - 1]);
}

Result<cbPKT_CHANINFO> ShmemSession::getChanInfo(uint32_t channel) const {
    if (!isOpen()) {
        return Result<cbPKT_CHANINFO>::error("Session not open");
    }

    if (channel >= CENTRAL_cbMAXCHANS) {
        return Result<cbPKT_CHANINFO>::error("Channel index out of range");
    }

    return Result<cbPKT_CHANINFO>::ok(m_impl->cfg_buffer->chaninfo[channel]);
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

    // THE KEY FIX: Use packet.instrument (0-based) as array index
    uint8_t idx = id.toIndex();
    m_impl->cfg_buffer->procinfo[idx] = info;

    return Result<void>::ok();
}

Result<void> ShmemSession::setBankInfo(cbproto::InstrumentId id, uint32_t bank, const cbPKT_BANKINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    if (bank >= CENTRAL_cbMAXBANKS) {
        return Result<void>::error("Bank index out of range");
    }

    uint8_t idx = id.toIndex();
    m_impl->cfg_buffer->bankinfo[idx][bank] = info;

    return Result<void>::ok();
}

Result<void> ShmemSession::setFilterInfo(cbproto::InstrumentId id, uint32_t filter, const cbPKT_FILTINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID");
    }

    if (filter >= CENTRAL_cbMAXFILTS) {
        return Result<void>::error("Filter index out of range");
    }

    uint8_t idx = id.toIndex();
    m_impl->cfg_buffer->filtinfo[idx][filter] = info;

    return Result<void>::ok();
}

Result<void> ShmemSession::setChanInfo(uint32_t channel, const cbPKT_CHANINFO& info) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    if (channel >= CENTRAL_cbMAXCHANS) {
        return Result<void>::error("Channel index out of range");
    }

    m_impl->cfg_buffer->chaninfo[channel] = info;

    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Packet Routing (THE KEY FIX!)

Result<void> ShmemSession::storePacket(const cbPKT_GENERIC& pkt) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    // CRITICAL: Write ALL packets to receive buffer ring (Central's architecture)
    // This is the streaming data that clients read from
    auto rec_result = m_impl->writeToReceiveBuffer(pkt);
    if (rec_result.isError()) {
        // Log error but don't fail - config updates may still work
        // (In production, might want to track this as a stat)
    }

    // ADDITIONALLY update config buffer for configuration packets
    // (This maintains the config "database" for query operations)

    // Helper lambda to check if packet needs config buffer storage (and thus instrument ID validation)
    // Based on Central's InstNetwork.cpp logic - only packets actually stored to config buffer need valid instrument IDs
    auto isConfigPacket = [](const cbPKT_HEADER& header) -> bool {
        // Config packets are sent on the configuration channel
        if (header.chid != cbPKTCHAN_CONFIGURATION) {
            return false;
        }

        uint16_t type = header.type;

        // Channel config packets (0x40-0x4F range)
        if ((type & 0xF0) == cbPKTTYPE_CHANREP) return true;

        // System config packets (0x10-0x1F range)
        if ((type & 0xF0) == cbPKTTYPE_SYSREP) return true;

        // Other specific config packet types that Central stores
        switch (type) {
            case cbPKTTYPE_GROUPREP:
            case cbPKTTYPE_FILTREP:
            case cbPKTTYPE_PROCREP:
            case cbPKTTYPE_BANKREP:
            case cbPKTTYPE_ADAPTFILTREP:
            case cbPKTTYPE_REFELECFILTREP:
            case cbPKTTYPE_SS_MODELREP:
            case cbPKTTYPE_SS_STATUSREP:
            case cbPKTTYPE_SS_DETECTREP:
            case cbPKTTYPE_SS_ARTIF_REJECTREP:
            case cbPKTTYPE_SS_NOISE_BOUNDARYREP:
            case cbPKTTYPE_SS_STATISTICSREP:
            case cbPKTTYPE_FS_BASISREP:
            case cbPKTTYPE_LNCREP:
            case cbPKTTYPE_REPFILECFG:
            case cbPKTTYPE_REPNTRODEINFO:
            case cbPKTTYPE_NMREP:
            case cbPKTTYPE_WAVEFORMREP:
            case cbPKTTYPE_NPLAYREP:
                return true;
            default:
                return false;
        }
    };

    // Only validate instrument ID for config packets that need to be stored
    if (isConfigPacket(pkt.cbpkt_header)) {
        uint16_t pkt_type = pkt.cbpkt_header.type;
        // Extract instrument ID from packet header
        cbproto::InstrumentId id = cbproto::InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);

        if (!id.isValid()) {
            // Invalid instrument ID for config packet - skip storing to config buffer
            // but still return ok since we already wrote to receive buffer
            return Result<void>::ok();
        }

        // Use packet.instrument as index (mode-independent!)
        uint8_t idx = id.toIndex();

        if ((pkt_type & 0xF0) == cbPKTTYPE_CHANREP) {
            // Channel info packets (0x40-0x4F range)
            const auto* chan_pkt = reinterpret_cast<const cbPKT_CHANINFO*>(&pkt);
            // Channel index is 1-based in packet, but chaninfo array is 0-based
            if (chan_pkt->chan > 0 && chan_pkt->chan <= CENTRAL_cbMAXCHANS) {
                std::memcpy(&m_impl->cfg_buffer->chaninfo[chan_pkt->chan - 1], &pkt, sizeof(cbPKT_CHANINFO));
            }
        }
        else if ((pkt_type & 0xF0) == cbPKTTYPE_SYSREP) {
            // System info packets (0x10-0x1F range) - all store to same sysinfo
            std::memcpy(&m_impl->cfg_buffer->sysinfo, &pkt, sizeof(cbPKT_SYSINFO));
        }
        else if (pkt_type == cbPKTTYPE_GROUPREP) {
            // Store sample group info (group index is 1-based in packet)
            const auto* group_pkt = reinterpret_cast<const cbPKT_GROUPINFO*>(&pkt);
            if (group_pkt->group > 0 && group_pkt->group <= CENTRAL_cbMAXGROUPS) {
                std::memcpy(&m_impl->cfg_buffer->groupinfo[idx][group_pkt->group - 1], &pkt, sizeof(cbPKT_GROUPINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_FILTREP) {
            // Store filter info (filter index is 1-based in packet)
            const auto* filt_pkt = reinterpret_cast<const cbPKT_FILTINFO*>(&pkt);
            if (filt_pkt->filt > 0 && filt_pkt->filt <= CENTRAL_cbMAXFILTS) {
                std::memcpy(&m_impl->cfg_buffer->filtinfo[idx][filt_pkt->filt - 1], &pkt, sizeof(cbPKT_FILTINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_PROCREP) {
            // Store processor info
            std::memcpy(&m_impl->cfg_buffer->procinfo[idx], &pkt, sizeof(cbPKT_PROCINFO));

            // Mark instrument as active when we receive its PROCINFO
            m_impl->cfg_buffer->instrument_status[idx] = static_cast<uint32_t>(InstrumentStatus::ACTIVE);
        }
        else if (pkt_type == cbPKTTYPE_BANKREP) {
            // Store bank info (bank index is 1-based in packet)
            const auto* bank_pkt = reinterpret_cast<const cbPKT_BANKINFO*>(&pkt);
            if (bank_pkt->bank > 0 && bank_pkt->bank <= CENTRAL_cbMAXBANKS) {
                std::memcpy(&m_impl->cfg_buffer->bankinfo[idx][bank_pkt->bank - 1], &pkt, sizeof(cbPKT_BANKINFO));
            }
        }
        else if (pkt_type == cbPKTTYPE_ADAPTFILTREP) {
            // Store adaptive filter info (per-instrument)
            m_impl->cfg_buffer->adaptinfo[idx] = *reinterpret_cast<const cbPKT_ADAPTFILTINFO*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_REFELECFILTREP) {
            // Store reference electrode filter info (per-instrument)
            m_impl->cfg_buffer->refelecinfo[idx] = *reinterpret_cast<const cbPKT_REFELECFILTINFO*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_STATUSREP) {
            // Store spike sorting status (system-wide, in isSortingOptions)
            m_impl->cfg_buffer->isSortingOptions.pktStatus = *reinterpret_cast<const cbPKT_SS_STATUS*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_DETECTREP) {
            // Store spike detection parameters (system-wide)
            m_impl->cfg_buffer->isSortingOptions.pktDetect = *reinterpret_cast<const cbPKT_SS_DETECT*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_ARTIF_REJECTREP) {
            // Store artifact rejection parameters (system-wide)
            m_impl->cfg_buffer->isSortingOptions.pktArtifReject = *reinterpret_cast<const cbPKT_SS_ARTIF_REJECT*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_NOISE_BOUNDARYREP) {
            // Store noise boundary (per-channel, 1-based in packet)
            const auto* noise_pkt = reinterpret_cast<const cbPKT_SS_NOISE_BOUNDARY*>(&pkt);
            if (noise_pkt->chan > 0 && noise_pkt->chan <= CENTRAL_cbMAXCHANS) {
                m_impl->cfg_buffer->isSortingOptions.pktNoiseBoundary[noise_pkt->chan - 1] = *noise_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_SS_STATISTICSREP) {
            // Store spike sorting statistics (system-wide)
            m_impl->cfg_buffer->isSortingOptions.pktStatistics = *reinterpret_cast<const cbPKT_SS_STATISTICS*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_SS_MODELREP) {
            // Store spike sorting model (per-channel, per-unit)
            // Note: Central calls UpdateSortModel() which validates and constrains unit numbers
            // For now, store directly with validation
            const auto* model_pkt = reinterpret_cast<const cbPKT_SS_MODELSET*>(&pkt);
            uint32_t nChan = model_pkt->chan;
            uint32_t nUnit = model_pkt->unit_number;

            // Validate channel and unit numbers (0-based in packet)
            if (nChan < CENTRAL_cbMAXCHANS && nUnit < (cbMAXUNITS + 2)) {
                m_impl->cfg_buffer->isSortingOptions.asSortModel[nChan][nUnit] = *model_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_FS_BASISREP) {
            // Store feature space basis (per-channel)
            // Note: Central calls UpdateBasisModel() for additional processing
            // For now, store directly with validation
            const auto* basis_pkt = reinterpret_cast<const cbPKT_FS_BASIS*>(&pkt);
            uint32_t nChan = basis_pkt->chan;

            // Validate channel number (1-based in packet)
            if (nChan > 0 && nChan <= CENTRAL_cbMAXCHANS) {
                m_impl->cfg_buffer->isSortingOptions.asBasis[nChan - 1] = *basis_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_LNCREP) {
            // Store line noise cancellation info (per-instrument)
            std::memcpy(&m_impl->cfg_buffer->isLnc[idx], &pkt, sizeof(cbPKT_LNC));
        }
        else if (pkt_type == cbPKTTYPE_REPFILECFG) {
            // Store file configuration info (only for specific options)
            const auto* file_pkt = reinterpret_cast<const cbPKT_FILECFG*>(&pkt);
            if (file_pkt->options == cbFILECFG_OPT_REC ||
                file_pkt->options == cbFILECFG_OPT_STOP ||
                file_pkt->options == cbFILECFG_OPT_TIMEOUT) {
                m_impl->cfg_buffer->fileinfo = *file_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_REPNTRODEINFO) {
            // Store n-trode information (1-based in packet)
            const auto* ntrode_pkt = reinterpret_cast<const cbPKT_NTRODEINFO*>(&pkt);
            if (ntrode_pkt->ntrode > 0 && ntrode_pkt->ntrode <= cbMAXNTRODES) {
                m_impl->cfg_buffer->isNTrodeInfo[ntrode_pkt->ntrode - 1] = *ntrode_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_WAVEFORMREP) {
            // Store analog output waveform configuration
            // Based on Central's logic (InstNetwork.cpp:415)
            const auto* wave_pkt = reinterpret_cast<const cbPKT_AOUT_WAVEFORM*>(&pkt);

            // Validate channel number (0-based) and trigger number (0-based)
            if (wave_pkt->chan < AOUT_NUM_GAIN_CHANS && wave_pkt->trigNum < cbMAX_AOUT_TRIGGER) {
                m_impl->cfg_buffer->isWaveform[wave_pkt->chan][wave_pkt->trigNum] = *wave_pkt;
            }
        }
        else if (pkt_type == cbPKTTYPE_NPLAYREP) {
            // Store nPlay information
            m_impl->cfg_buffer->isNPlay = *reinterpret_cast<const cbPKT_NPLAY*>(&pkt);
        }
        else if (pkt_type == cbPKTTYPE_NMREP) {
            // Store NeuroMotive (video/tracking) information
            // Based on Central's logic (InstNetwork.cpp:367-397)
            const auto* nm_pkt = reinterpret_cast<const cbPKT_NM*>(&pkt);

            if (nm_pkt->mode == cbNM_MODE_SETVIDEOSOURCE) {
                // Video source configuration (1-based index in flags field)
                if (nm_pkt->flags > 0 && nm_pkt->flags <= cbMAXVIDEOSOURCE) {
                    std::memcpy(m_impl->cfg_buffer->isVideoSource[nm_pkt->flags - 1].name,
                                nm_pkt->name, cbLEN_STR_LABEL);
                    m_impl->cfg_buffer->isVideoSource[nm_pkt->flags - 1].fps =
                        static_cast<float>(nm_pkt->value) / 1000.0f;
                }
            }
            else if (nm_pkt->mode == cbNM_MODE_SETTRACKABLE) {
                // Trackable object configuration (1-based index in flags field)
                if (nm_pkt->flags > 0 && nm_pkt->flags <= cbMAXTRACKOBJ) {
                    std::memcpy(m_impl->cfg_buffer->isTrackObj[nm_pkt->flags - 1].name,
                                nm_pkt->name, cbLEN_STR_LABEL);
                    m_impl->cfg_buffer->isTrackObj[nm_pkt->flags - 1].type =
                        static_cast<uint16_t>(nm_pkt->value & 0xff);
                    m_impl->cfg_buffer->isTrackObj[nm_pkt->flags - 1].pointCount =
                        static_cast<uint16_t>((nm_pkt->value >> 16) & 0xff);
                }
            }
            // Note: cbNM_MODE_SETRPOS does not exist in upstream cbproto.h
            // If reset functionality is needed, it should be implemented using a different mode
            /*
            else if (nm_pkt->mode == cbNM_MODE_SETRPOS) {
                // Clear all trackable objects
                std::memset(m_impl->cfg_buffer->isTrackObj, 0, sizeof(m_impl->cfg_buffer->isTrackObj));
                std::memset(m_impl->cfg_buffer->isVideoSource, 0, sizeof(m_impl->cfg_buffer->isVideoSource));
            }
            */
        }

        // All recognized config packet types now have storage
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::storePackets(const cbPKT_GENERIC* pkts, size_t count) {
    if (!isOpen()) {
        return Result<void>::error("Session not open");
    }

    for (size_t i = 0; i < count; ++i) {
        auto result = storePacket(pkts[i]);
        if (result.isError()) {
            return result;  // Propagate first error
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

    if (!m_impl->xmt_buffer) {
        return Result<void>::error("Transmit buffer not initialized");
    }

    CentralTransmitBuffer* xmt = m_impl->xmt_buffer;

    // Calculate packet size in uint32_t words
    // packet header is cbPKT_HEADER_32SIZE words (4 words with 64-bit PROCTIME)
    // dlen is payload in uint32_t words
    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    // Check if there's enough space in the ring buffer
    uint32_t head = xmt->headindex;
    uint32_t tail = xmt->tailindex;
    uint32_t buflen = xmt->bufferlen;

    // Calculate available space
    uint32_t used;
    if (head >= tail) {
        used = head - tail;
    } else {
        used = buflen - tail + head;
    }

    uint32_t available = buflen - used - 1;  // -1 to distinguish full from empty

    if (available < pkt_size_words) {
        return Result<void>::error("Transmit buffer full");
    }

    // Copy packet data to buffer (as uint32_t words)
    const uint32_t* pkt_data = reinterpret_cast<const uint32_t*>(&pkt);

    for (uint32_t i = 0; i < pkt_size_words; ++i) {
        xmt->buffer[head] = pkt_data[i];
        head = (head + 1) % buflen;
    }

    // Update head index
    xmt->headindex = head;

    return Result<void>::ok();
}

Result<bool> ShmemSession::dequeuePacket(cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }

    if (!m_impl->xmt_buffer) {
        return Result<bool>::error("Transmit buffer not initialized");
    }

    CentralTransmitBuffer* xmt = m_impl->xmt_buffer;

    uint32_t head = xmt->headindex;
    uint32_t tail = xmt->tailindex;

    // Check if queue is empty
    if (head == tail) {
        return Result<bool>::ok(false);  // Queue is empty
    }

    uint32_t buflen = xmt->bufferlen;

    // Read packet header (4 uint32_t words = 16 bytes)
    // Header contains: time (2 dwords: 0-1), chid/type (dword 2), dlen/instrument/reserved (dword 3)
    // Note: PROCTIME is uint64_t (8 bytes) unless CBPROTO_311 is defined
    uint32_t* pkt_data = reinterpret_cast<uint32_t*>(&pkt);

    if (tail + 4 <= buflen) {
        // Header doesn't wrap
        pkt_data[0] = xmt->buffer[tail];
        pkt_data[1] = xmt->buffer[tail + 1];
        pkt_data[2] = xmt->buffer[tail + 2];
        pkt_data[3] = xmt->buffer[tail + 3];
    } else {
        // Header wraps around
        pkt_data[0] = xmt->buffer[tail];
        pkt_data[1] = xmt->buffer[(tail + 1) % buflen];
        pkt_data[2] = xmt->buffer[(tail + 2) % buflen];
        pkt_data[3] = xmt->buffer[(tail + 3) % buflen];
    }

    // Now we know the packet size from dlen
    // Total packet size = header + payload = cbPKT_HEADER_32SIZE + dlen
    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    // Read the rest of the packet (starting from word 4, since we already read the header)
    tail = (tail + 4) % buflen;  // Advance past the 4-word header we already read
    for (uint32_t i = 4; i < pkt_size_words; ++i) {
        pkt_data[i] = xmt->buffer[tail];
        tail = (tail + 1) % buflen;
    }

    // Update tail index
    xmt->tailindex = tail;
    xmt->transmitted++;

    return Result<bool>::ok(true);  // Successfully dequeued
}

bool ShmemSession::hasTransmitPackets() const {
    if (!m_impl || !m_impl->is_open || !m_impl->xmt_buffer) {
        return false;
    }

    return m_impl->xmt_buffer->headindex != m_impl->xmt_buffer->tailindex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Local Transmit Queue Operations (IPC-only packets)
///////////////////////////////////////////////////////////////////////////////////////////////////

Result<void> ShmemSession::enqueueLocalPacket(const cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->xmt_local_buffer) {
        return Result<void>::error("Local transmit buffer not initialized");
    }

    CentralTransmitBufferLocal* xmt_local = m_impl->xmt_local_buffer;

    // Calculate packet size in uint32_t words
    // packet header is cbPKT_HEADER_32SIZE words (4 words with 64-bit PROCTIME)
    // dlen is payload in uint32_t words
    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    // Check if there's enough space in the ring buffer
    uint32_t head = xmt_local->headindex;
    uint32_t tail = xmt_local->tailindex;
    uint32_t buflen = xmt_local->bufferlen;

    // Calculate available space
    uint32_t used;
    if (head >= tail) {
        used = head - tail;
    } else {
        used = buflen - tail + head;
    }

    uint32_t available = buflen - used - 1;  // -1 to distinguish full from empty

    if (available < pkt_size_words) {
        return Result<void>::error("Local transmit buffer full");
    }

    // Copy packet data to buffer (as uint32_t words)
    const uint32_t* pkt_data = reinterpret_cast<const uint32_t*>(&pkt);

    for (uint32_t i = 0; i < pkt_size_words; ++i) {
        xmt_local->buffer[head] = pkt_data[i];
        head = (head + 1) % buflen;
    }

    // Update head index
    xmt_local->headindex = head;

    return Result<void>::ok();
}

Result<bool> ShmemSession::dequeueLocalPacket(cbPKT_GENERIC& pkt) {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }

    if (!m_impl->xmt_local_buffer) {
        return Result<bool>::error("Local transmit buffer not initialized");
    }

    CentralTransmitBufferLocal* xmt_local = m_impl->xmt_local_buffer;

    uint32_t head = xmt_local->headindex;
    uint32_t tail = xmt_local->tailindex;

    // Check if queue is empty
    if (head == tail) {
        return Result<bool>::ok(false);  // Queue is empty
    }

    uint32_t buflen = xmt_local->bufferlen;

    // Read packet header (4 uint32_t words = 16 bytes)
    // Header contains: time (2 dwords: 0-1), chid/type (dword 2), dlen/instrument/reserved (dword 3)
    // Note: PROCTIME is uint64_t (8 bytes) unless CBPROTO_311 is defined
    uint32_t* pkt_data = reinterpret_cast<uint32_t*>(&pkt);

    if (tail + 4 <= buflen) {
        // Header doesn't wrap
        pkt_data[0] = xmt_local->buffer[tail];
        pkt_data[1] = xmt_local->buffer[tail + 1];
        pkt_data[2] = xmt_local->buffer[tail + 2];
        pkt_data[3] = xmt_local->buffer[tail + 3];
    } else {
        // Header wraps around
        pkt_data[0] = xmt_local->buffer[tail];
        pkt_data[1] = xmt_local->buffer[(tail + 1) % buflen];
        pkt_data[2] = xmt_local->buffer[(tail + 2) % buflen];
        pkt_data[3] = xmt_local->buffer[(tail + 3) % buflen];
    }

    // Now we know the packet size from dlen
    // Total packet size = header + payload = cbPKT_HEADER_32SIZE + dlen
    uint32_t pkt_size_words = cbPKT_HEADER_32SIZE + pkt.cbpkt_header.dlen;

    // Read the rest of the packet (starting from word 4, since we already read the header)
    tail = (tail + 4) % buflen;  // Advance past the 4-word header we already read
    for (uint32_t i = 4; i < pkt_size_words; ++i) {
        pkt_data[i] = xmt_local->buffer[tail];
        tail = (tail + 1) % buflen;
    }

    // Update tail index
    xmt_local->tailindex = tail;
    xmt_local->transmitted++;

    return Result<bool>::ok(true);  // Successfully dequeued
}

bool ShmemSession::hasLocalTransmitPackets() const {
    if (!m_impl || !m_impl->is_open || !m_impl->xmt_local_buffer) {
        return false;
    }

    return m_impl->xmt_local_buffer->headindex != m_impl->xmt_local_buffer->tailindex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PC Status Buffer Access

Result<uint32_t> ShmemSession::getNumTotalChans() const {
    if (!m_impl || !m_impl->is_open) {
        return Result<uint32_t>::error("Session is not open");
    }

    if (!m_impl->status_buffer) {
        return Result<uint32_t>::error("Status buffer not initialized");
    }

    return Result<uint32_t>::ok(m_impl->status_buffer->m_nNumTotalChans);
}

Result<NSPStatus> ShmemSession::getNspStatus(cbproto::InstrumentId id) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<NSPStatus>::error("Session is not open");
    }

    if (!m_impl->status_buffer) {
        return Result<NSPStatus>::error("Status buffer not initialized");
    }

    uint32_t index = id.toIndex();  // Convert 1-based InstrumentId to 0-based array index
    if (index >= CENTRAL_cbMAXPROCS) {
        return Result<NSPStatus>::error("Invalid instrument ID");
    }

    return Result<NSPStatus>::ok(m_impl->status_buffer->m_nNspStatus[index]);
}

Result<void> ShmemSession::setNspStatus(cbproto::InstrumentId id, NSPStatus status) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->status_buffer) {
        return Result<void>::error("Status buffer not initialized");
    }

    uint32_t index = id.toIndex();  // Convert 1-based InstrumentId to 0-based array index
    if (index >= CENTRAL_cbMAXPROCS) {
        return Result<void>::error("Invalid instrument ID");
    }

    m_impl->status_buffer->m_nNspStatus[index] = status;
    return Result<void>::ok();
}

Result<bool> ShmemSession::isGeminiSystem() const {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }

    if (!m_impl->status_buffer) {
        return Result<bool>::error("Status buffer not initialized");
    }

    return Result<bool>::ok(m_impl->status_buffer->m_nGeminiSystem != 0);
}

Result<void> ShmemSession::setGeminiSystem(bool is_gemini) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->status_buffer) {
        return Result<void>::error("Status buffer not initialized");
    }

    m_impl->status_buffer->m_nGeminiSystem = is_gemini ? 1 : 0;
    return Result<void>::ok();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Spike Cache Buffer Access

Result<void> ShmemSession::getSpikeCache(uint32_t channel, CentralSpikeCache& cache) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->spike_buffer) {
        return Result<void>::error("Spike buffer not initialized");
    }

    if (channel >= CENTRAL_cbPKT_SPKCACHELINECNT) {
        return Result<void>::error("Invalid channel number");
    }

    // Copy the entire cache for this channel
    cache = m_impl->spike_buffer->cache[channel];
    return Result<void>::ok();
}

Result<bool> ShmemSession::getRecentSpike(uint32_t channel, cbPKT_SPK& spike) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<bool>::error("Session is not open");
    }

    if (!m_impl->spike_buffer) {
        return Result<bool>::error("Spike buffer not initialized");
    }

    if (channel >= CENTRAL_cbPKT_SPKCACHELINECNT) {
        return Result<bool>::error("Invalid channel number");
    }

    const CentralSpikeCache& cache = m_impl->spike_buffer->cache[channel];

    // Check if there are any valid spikes in the cache
    if (cache.valid == 0) {
        return Result<bool>::ok(false);  // No spikes available
    }

    // Get the most recent spike (the one before head)
    uint32_t recent_idx = (cache.head == 0) ? (cache.pktcnt - 1) : (cache.head - 1);
    spike = cache.spkpkt[recent_idx];

    return Result<bool>::ok(true);  // Spike available
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
        return Result<bool>::ok(true);   // Signal received
    } else if (result == WAIT_TIMEOUT) {
        return Result<bool>::ok(false);  // Timeout
    } else {
        return Result<bool>::error("WaitForSingleObject failed");
    }

#else
    if (m_impl->signal_event == SEM_FAILED) {
        return Result<bool>::error("Signal event not initialized");
    }

    // Use sem_timedwait for timeout support
    timespec ts;
#ifdef __APPLE__
    // macOS doesn't have clock_gettime, use a workaround
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif

    // Add timeout
    long ns = timeout_ms * 1000000L;
    const long NANOSECONDS_PER_SEC = 1000000000L;
    ts.tv_nsec += ns;
    if (ts.tv_nsec >= NANOSECONDS_PER_SEC) {
        ts.tv_sec += ts.tv_nsec / NANOSECONDS_PER_SEC;
        ts.tv_nsec = ts.tv_nsec % NANOSECONDS_PER_SEC;
    }

#ifdef __APPLE__
    // macOS doesn't have sem_timedwait, use sem_trywait with polling
    // This is not ideal but works for our purposes
    int retries = timeout_ms / 10;  // Poll every 10ms
    for (int i = 0; i < retries; ++i) {
        if (sem_trywait(m_impl->signal_event) == 0) {
            return Result<bool>::ok(true);   // Signal received
        }
        usleep(10000);  // Sleep 10ms
    }
    // One final try
    if (sem_trywait(m_impl->signal_event) == 0) {
        return Result<bool>::ok(true);
    }
    return Result<bool>::ok(false);  // Timeout
#else
    int result = sem_timedwait(m_impl->signal_event, &ts);
    if (result == 0) {
        return Result<bool>::ok(true);   // Signal received
    } else if (errno == ETIMEDOUT) {
        return Result<bool>::ok(false);  // Timeout
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
    // On POSIX, semaphores are auto-reset by sem_wait/sem_trywait
    // So this is a no-op for semaphores
    // However, to drain any pending signals, we can call sem_trywait in a loop
    if (m_impl->signal_event == SEM_FAILED) {
        return Result<void>::error("Signal event not initialized");
    }

    // Drain all pending signals
    while (sem_trywait(m_impl->signal_event) == 0) {
        // Keep draining
    }

    return Result<void>::ok();
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Receive buffer reading methods

Result<void> ShmemSession::readReceiveBuffer(cbPKT_GENERIC* packets, size_t max_packets, size_t& packets_read) {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->rec_buffer) {
        return Result<void>::error("Receive buffer not initialized");
    }

    if (!packets || max_packets == 0) {
        return Result<void>::error("Invalid parameters");
    }

    packets_read = 0;

    // Read current writer position (volatile reads)
    uint32_t head_index = m_impl->rec_buffer->headindex;
    uint32_t head_wrap = m_impl->rec_buffer->headwrap;

    // Check if there's new data available
    if (m_impl->rec_tailwrap == head_wrap && m_impl->rec_tailindex == head_index) {
        // No new data
        return Result<void>::ok();
    }

    // Read packets until we catch up to head or reach max_packets
    while (packets_read < max_packets) {
        // Check if we've caught up
        if (m_impl->rec_tailwrap == head_wrap && m_impl->rec_tailindex == head_index) {
            break;
        }

        // Check for buffer overrun (writer lapped us)
        if ((m_impl->rec_tailwrap + 1 == head_wrap && m_impl->rec_tailindex < head_index) ||
            (m_impl->rec_tailwrap + 1 < head_wrap)) {
            // We've been lapped - skip to current head position to recover
            m_impl->rec_tailindex = head_index;
            m_impl->rec_tailwrap = head_wrap;
            return Result<void>::error("Receive buffer overrun - data lost");
        }

        // Read packet size (first dword is packet size in dwords)
        uint32_t pkt_size_dwords = m_impl->rec_buffer->buffer[m_impl->rec_tailindex];

        if (pkt_size_dwords == 0 || pkt_size_dwords > (sizeof(cbPKT_GENERIC) / sizeof(uint32_t))) {
            // Invalid packet size - skip this position
            m_impl->rec_tailindex++;
            if (m_impl->rec_tailindex >= CENTRAL_cbRECBUFFLEN) {
                m_impl->rec_tailindex = 0;
                m_impl->rec_tailwrap++;
            }
            continue;
        }

        // Check if packet would wrap around buffer
        uint32_t end_index = m_impl->rec_tailindex + pkt_size_dwords;

        if (end_index <= CENTRAL_cbRECBUFFLEN) {
            // Packet doesn't wrap - copy directly
            std::memcpy(&packets[packets_read],
                       &m_impl->rec_buffer->buffer[m_impl->rec_tailindex],
                       pkt_size_dwords * sizeof(uint32_t));
        } else {
            // Packet wraps around - copy in two parts
            uint32_t first_part_size = CENTRAL_cbRECBUFFLEN - m_impl->rec_tailindex;
            uint32_t second_part_size = pkt_size_dwords - first_part_size;

            std::memcpy(&packets[packets_read],
                       &m_impl->rec_buffer->buffer[m_impl->rec_tailindex],
                       first_part_size * sizeof(uint32_t));

            std::memcpy(reinterpret_cast<uint32_t*>(&packets[packets_read]) + first_part_size,
                       &m_impl->rec_buffer->buffer[0],
                       second_part_size * sizeof(uint32_t));
        }

        packets_read++;

        // Advance tail pointer
        m_impl->rec_tailindex += pkt_size_dwords;
        if (m_impl->rec_tailindex >= CENTRAL_cbRECBUFFLEN) {
            m_impl->rec_tailindex -= CENTRAL_cbRECBUFFLEN;
            m_impl->rec_tailwrap++;
        }
    }

    return Result<void>::ok();
}

Result<void> ShmemSession::getReceiveBufferStats(uint32_t& received, uint32_t& available) const {
    if (!m_impl || !m_impl->is_open) {
        return Result<void>::error("Session is not open");
    }

    if (!m_impl->rec_buffer) {
        return Result<void>::error("Receive buffer not initialized");
    }

    received = m_impl->rec_buffer->received;

    // Calculate available packets (approximate - based on position difference)
    uint32_t head_index = m_impl->rec_buffer->headindex;
    uint32_t head_wrap = m_impl->rec_buffer->headwrap;

    if (m_impl->rec_tailwrap == head_wrap) {
        if (head_index >= m_impl->rec_tailindex) {
            available = head_index - m_impl->rec_tailindex;
        } else {
            available = 0;  // Head behind tail (shouldn't happen)
        }
    } else if (m_impl->rec_tailwrap + 1 == head_wrap) {
        // One wrap difference
        available = (CENTRAL_cbRECBUFFLEN - m_impl->rec_tailindex) + head_index;
    } else {
        // Multiple wraps - buffer overrun
        available = 0;
    }

    return Result<void>::ok();
}

} // namespace cbshmem
