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
    std::string name;
    bool is_open;

    // Platform-specific handles
#ifdef _WIN32
    HANDLE file_mapping;
#else
    int shm_fd;
#endif

    // Pointers to shared memory buffers
    CentralConfigBuffer* cfg_buffer;
    CentralReceiveBuffer* rec_buffer;
    CentralTransmitBuffer* xmt_buffer;

    Impl()
        : mode(Mode::STANDALONE)
        , is_open(false)
#ifdef _WIN32
        , file_mapping(nullptr)
#else
        , shm_fd(-1)
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
        if (file_mapping) CloseHandle(file_mapping);
        file_mapping = nullptr;
#else
        if (cfg_buffer) {
            munmap(cfg_buffer, sizeof(CentralConfigBuffer));
        }
        if (shm_fd >= 0) {
            ::close(shm_fd);
            if (mode == Mode::STANDALONE) {
                // POSIX requires shared memory names to start with "/"
                std::string posix_name = (name[0] == '/') ? name : ("/" + name);
                shm_unlink(posix_name.c_str());
            }
        }
        shm_fd = -1;
#endif

        cfg_buffer = nullptr;
        rec_buffer = nullptr;
        xmt_buffer = nullptr;
        is_open = false;
    }

    Result<void> open() {
        if (is_open) {
            return Result<void>::error("Session already open");
        }

#ifdef _WIN32
        // Windows implementation
        DWORD access = (mode == Mode::STANDALONE) ? PAGE_READWRITE : PAGE_READONLY;

        file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            access,
            0,
            sizeof(CentralConfigBuffer),
            name.c_str()
        );

        if (!file_mapping) {
            return Result<void>::error("Failed to create file mapping");
        }

        DWORD map_access = (mode == Mode::STANDALONE) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
        cfg_buffer = static_cast<CentralConfigBuffer*>(
            MapViewOfFile(file_mapping, map_access, 0, 0, sizeof(CentralConfigBuffer))
        );

        if (!cfg_buffer) {
            CloseHandle(file_mapping);
            file_mapping = nullptr;
            return Result<void>::error("Failed to map view of file");
        }

#else
        // POSIX (macOS/Linux) implementation
        // POSIX requires shared memory names to start with "/"
        std::string posix_name = (name[0] == '/') ? name : ("/" + name);

        int flags = (mode == Mode::STANDALONE) ? (O_CREAT | O_RDWR) : O_RDONLY;
        mode_t perms = (mode == Mode::STANDALONE) ? 0644 : 0;

        // In STANDALONE mode, unlink any existing shared memory first
        if (mode == Mode::STANDALONE) {
            shm_unlink(posix_name.c_str());  // Ignore errors if it doesn't exist
        }

        shm_fd = shm_open(posix_name.c_str(), flags, perms);
        if (shm_fd < 0) {
            return Result<void>::error("Failed to open shared memory: " + std::string(strerror(errno)));
        }

        if (mode == Mode::STANDALONE) {
            // Set size for standalone mode
            if (ftruncate(shm_fd, sizeof(CentralConfigBuffer)) < 0) {
                std::string err_msg = "Failed to set shared memory size: " + std::string(strerror(errno));
                ::close(shm_fd);
                shm_fd = -1;
                return Result<void>::error(err_msg);
            }
        }

        int prot = (mode == Mode::STANDALONE) ? (PROT_READ | PROT_WRITE) : PROT_READ;
        cfg_buffer = static_cast<CentralConfigBuffer*>(
            mmap(nullptr, sizeof(CentralConfigBuffer), prot, MAP_SHARED, shm_fd, 0)
        );

        if (cfg_buffer == MAP_FAILED) {
            ::close(shm_fd);
            shm_fd = -1;
            cfg_buffer = nullptr;
            return Result<void>::error("Failed to map shared memory");
        }
#endif

        // Initialize buffer in standalone mode
        if (mode == Mode::STANDALONE) {
            std::memset(cfg_buffer, 0, sizeof(CentralConfigBuffer));
            cfg_buffer->version = cbVERSION_MAJOR * 100 + cbVERSION_MINOR;

            // Mark all instruments as inactive initially
            for (int i = 0; i < CENTRAL_cbMAXPROCS; ++i) {
                cfg_buffer->instrument_status[i] = static_cast<uint32_t>(InstrumentStatus::INACTIVE);
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

Result<ShmemSession> ShmemSession::create(const std::string& name, Mode mode) {
    ShmemSession session;
    session.m_impl->name = name;
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

    // Extract instrument ID from packet header
    cbproto::InstrumentId id = cbproto::InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);

    if (!id.isValid()) {
        return Result<void>::error("Invalid instrument ID in packet");
    }

    // THE KEY FIX: ALWAYS use packet.instrument as index (mode-independent!)
    uint8_t idx = id.toIndex();

    // Route based on packet type
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
    // TODO: Add more packet types as needed

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

} // namespace cbshmem
