///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   native_peer.h
/// @author CereLink Development Team
///
/// @brief  Read-only access to another session's NATIVE config segment
///
/// A full ShmemSession maps all seven segments and, in CLIENT mode, writes to
/// some of them.  Code that only needs to glance at a peer device's published
/// state (e.g. its clock offset, for cross-device consensus) should not pay for
/// that or take write access, so this maps the config segment alone, read-only.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_NATIVE_PEER_H
#define CBSHM_NATIVE_PEER_H

#include <cbshm/native_types.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cbshm {

/// @brief Whether a process with this PID currently exists
///
/// On POSIX this is kill(pid, 0), counting EPERM (exists, not ours to signal)
/// as alive.  On Windows it is OpenProcess, which also succeeds for a process
/// that has exited while some other handle to its process object is still
/// open -- acceptable for staleness detection, where the cost of a false
/// "alive" is reading a value that stops changing.
///
/// @param pid Process id; 0 is never a valid owner and returns false
bool isProcessAlive(uint32_t pid);

/// @brief Read-only mapping of a NATIVE config segment owned by another session
///
/// Not thread-safe; the peer writes the mapped fields concurrently, so callers
/// read them as best-effort snapshots.
class NativeConfigReader {
public:
    NativeConfigReader() = default;
    ~NativeConfigReader();

    NativeConfigReader(const NativeConfigReader&) = delete;
    NativeConfigReader& operator=(const NativeConfigReader&) = delete;

    /// @brief Map the config segment a NATIVE STANDALONE publishes for @p device_tag
    ///
    /// @param device_tag The NATIVE name qualifier passed to ShmemSession::create
    ///                   (e.g. "hub1"), so the name matches what the owner created.
    /// @return true if the segment exists, is large enough and was written with
    ///         this build's config layout; false otherwise (the reader is left closed).
    bool open(const std::string& device_tag);

    /// @brief Unmap the segment (no-op if not open)
    void close();

    [[nodiscard]] bool isOpen() const { return m_view != nullptr; }

    /// @brief The mapped config, or nullptr if not open
    [[nodiscard]] const NativeConfigBuffer* config() const {
        return static_cast<const NativeConfigBuffer*>(m_view);
    }

    /// @brief Whether the segment's recorded owner process is still running
    ///
    /// Returns true when the owner is unknown (owner_pid == 0, a segment from
    /// before liveness tracking), false when not open.
    [[nodiscard]] bool isOwnerAlive() const;

private:
    const void* m_view = nullptr;
#ifdef _WIN32
    void* m_mapping = nullptr;  ///< HANDLE from OpenFileMappingA
#else
    size_t m_size = 0;
#endif
};

} // namespace cbshm

#endif // CBSHM_NATIVE_PEER_H
