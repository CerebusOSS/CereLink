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
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace cbshmem {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Platform-specific implementation details (Pimpl idiom)
///
struct ShmemSession::Impl {
    Mode mode;
    std::string cfg_name;  // Config buffer name (e.g., "cbCFGbuffer")
    std::string rec_name;  // Receive buffer name (e.g., "cbRECbuffer")
    std::string xmt_name;  // Transmit buffer name (e.g., "XmtGlobal")
    bool is_open;

    // Platform-specific handles (separate segments for config, receive, and transmit)
#ifdef _WIN32
    HANDLE cfg_file_mapping;
    HANDLE rec_file_mapping;
    HANDLE xmt_file_mapping;
#else
    int cfg_shm_fd;
    int rec_shm_fd;
    int xmt_shm_fd;
#endif

    // Pointers to shared memory buffers
    CentralConfigBuffer* cfg_buffer;
    CentralReceiveBuffer* rec_buffer;
    CentralTransmitBuffer* xmt_buffer;

    Impl()
        : mode(Mode::STANDALONE)
        , is_open(false)
#ifdef _WIN32
        , cfg_file_mapping(nullptr)
        , rec_file_mapping(nullptr)
        , xmt_file_mapping(nullptr)
#else
        , cfg_shm_fd(-1)
        , rec_shm_fd(-1)
        , xmt_shm_fd(-1)
#endif
        , cfg_buffer(nullptr)
        , rec_buffer(nullptr)
        , xmt_buffer(nullptr)
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
        if (cfg_file_mapping) CloseHandle(cfg_file_mapping);
        if (rec_file_mapping) CloseHandle(rec_file_mapping);
        if (xmt_file_mapping) CloseHandle(xmt_file_mapping);
        cfg_file_mapping = nullptr;
        rec_file_mapping = nullptr;
        xmt_file_mapping = nullptr;
#else
        // POSIX requires shared memory names to start with "/"
        std::string posix_cfg_name = (cfg_name[0] == '/') ? cfg_name : ("/" + cfg_name);
        std::string posix_rec_name = (rec_name[0] == '/') ? rec_name : ("/" + rec_name);
        std::string posix_xmt_name = (xmt_name[0] == '/') ? xmt_name : ("/" + xmt_name);

        if (cfg_buffer) {
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
        }
        if (rec_buffer) {
            munmap(rec_buffer, sizeof(CentralReceiveBuffer));
        }
        if (xmt_buffer) {
            munmap(xmt_buffer, sizeof(CentralTransmitBuffer));
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

        cfg_shm_fd = -1;
        rec_shm_fd = -1;
        xmt_shm_fd = -1;
#endif

        cfg_buffer = nullptr;
        rec_buffer = nullptr;
        xmt_buffer = nullptr;
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
        // packet header is 2 uint32_t words (time, chid/type/dlen)
        // dlen is in uint32_t words
        uint32_t pkt_size_words = 2 + pkt.cbpkt_header.dlen;

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

#else
        // POSIX (macOS/Linux) implementation - create three separate shared memory segments
        // POSIX requires shared memory names to start with "/"
        std::string posix_cfg_name = (cfg_name[0] == '/') ? cfg_name : ("/" + cfg_name);
        std::string posix_rec_name = (rec_name[0] == '/') ? rec_name : ("/" + rec_name);
        std::string posix_xmt_name = (xmt_name[0] == '/') ? xmt_name : ("/" + xmt_name);

        int flags = (mode == Mode::STANDALONE) ? (O_CREAT | O_RDWR) : O_RDONLY;
        mode_t perms = (mode == Mode::STANDALONE) ? 0644 : 0;
        int prot = (mode == Mode::STANDALONE) ? (PROT_READ | PROT_WRITE) : PROT_READ;

        // In STANDALONE mode, unlink any existing shared memory first
        if (mode == Mode::STANDALONE) {
            shm_unlink(posix_cfg_name.c_str());  // Ignore errors if it doesn't exist
            shm_unlink(posix_rec_name.c_str());
            shm_unlink(posix_xmt_name.c_str());
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
            xmt_buffer->last_valid_index = CENTRAL_cbXMTBUFFLEN - 1;
            xmt_buffer->bufferlen = CENTRAL_cbXMTBUFFLEN;
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

Result<ShmemSession> ShmemSession::create(const std::string& cfg_name, const std::string& rec_name, const std::string& xmt_name, Mode mode) {
    ShmemSession session;
    session.m_impl->cfg_name = cfg_name;
    session.m_impl->rec_name = rec_name;
    session.m_impl->xmt_name = xmt_name;
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

    if (bank >= CENTRAL_cbMAXBANKS) {
        return Result<cbPKT_BANKINFO>::error("Bank index out of range");
    }

    uint8_t idx = id.toIndex();
    return Result<cbPKT_BANKINFO>::ok(m_impl->cfg_buffer->bankinfo[idx][bank]);
}

Result<cbPKT_FILTINFO> ShmemSession::getFilterInfo(cbproto::InstrumentId id, uint32_t filter) const {
    if (!isOpen()) {
        return Result<cbPKT_FILTINFO>::error("Session not open");
    }

    if (!id.isValid()) {
        return Result<cbPKT_FILTINFO>::error("Invalid instrument ID");
    }

    if (filter >= CENTRAL_cbMAXFILTS) {
        return Result<cbPKT_FILTINFO>::error("Filter index out of range");
    }

    uint8_t idx = id.toIndex();
    return Result<cbPKT_FILTINFO>::ok(m_impl->cfg_buffer->filtinfo[idx][filter]);
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

    // Extract instrument ID from packet header
    cbproto::InstrumentId id = cbproto::InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);

    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID in packet");
    }

    // THE KEY FIX: ALWAYS use packet.instrument as index (mode-independent!)
    uint8_t idx = id.toIndex();

    // ADDITIONALLY update config buffer for configuration packets
    // (This maintains the config "database" for query operations)
    uint16_t pkt_type = pkt.cbpkt_header.type;

    if (pkt_type == cbPKTTYPE_PROCREP) {
        // Store processor info
        std::memcpy(&m_impl->cfg_buffer->procinfo[idx], &pkt, sizeof(cbPKT_PROCINFO));

        // Mark instrument as active when we receive its PROCINFO
        m_impl->cfg_buffer->instrument_status[idx] = static_cast<uint32_t>(InstrumentStatus::ACTIVE);

    } else if (pkt_type == cbPKTTYPE_BANKREP) {
        // Store bank info
        const cbPKT_BANKINFO* bank_pkt = reinterpret_cast<const cbPKT_BANKINFO*>(&pkt);
        if (bank_pkt->bank < CENTRAL_cbMAXBANKS) {
            std::memcpy(&m_impl->cfg_buffer->bankinfo[idx][bank_pkt->bank], &pkt, sizeof(cbPKT_BANKINFO));
        }

    } else if (pkt_type == cbPKTTYPE_FILTREP) {
        // Store filter info
        const cbPKT_FILTINFO* filt_pkt = reinterpret_cast<const cbPKT_FILTINFO*>(&pkt);
        if (filt_pkt->filt < CENTRAL_cbMAXFILTS) {
            std::memcpy(&m_impl->cfg_buffer->filtinfo[idx][filt_pkt->filt], &pkt, sizeof(cbPKT_FILTINFO));
        }
    }
    // TODO: Add more config packet types as needed (CHANINFO, GROUPINFO, etc.)

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
    // packet header is 2 uint32_t words (time, chid/type/dlen)
    // dlen is in uint32_t words
    uint32_t pkt_size_words = 2 + pkt.cbpkt_header.dlen;

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

    // Read packet header (2 uint32_t words)
    uint32_t* pkt_data = reinterpret_cast<uint32_t*>(&pkt);

    if (tail + 2 <= buflen) {
        // Header doesn't wrap
        pkt_data[0] = xmt->buffer[tail];
        pkt_data[1] = xmt->buffer[tail + 1];
    } else {
        // Header wraps around
        pkt_data[0] = xmt->buffer[tail];
        pkt_data[1] = xmt->buffer[(tail + 1) % buflen];
    }

    // Now we know the packet size from dlen
    uint32_t pkt_size_words = 2 + pkt.cbpkt_header.dlen;

    // Read the rest of the packet
    for (uint32_t i = 0; i < pkt_size_words; ++i) {
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

} // namespace cbshmem
