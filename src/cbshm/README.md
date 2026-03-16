# cbshm - Shared Memory Management

**Status:** Phase 2 - ✅ **COMPLETE** (2025-11-11)

## Purpose

Internal C++ library that manages shared memory with **Central-compatible layout**.

**Key Responsibility:** Provide consistent shared memory layout regardless of mode!

## Core Functionality

1. **Mode Detection**
   - Detect if Central is running (client mode)
   - Or operate standalone (direct device connection)

2. **Correct Indexing** (THE KEY FIX!)
   ```cpp
   // ALWAYS use packet.instrument as index (mode-independent!)
   Result<void> storePacket(const cbPKT_GENERIC& pkt) {
       InstrumentId id = InstrumentId::fromPacketField(pkt.cbpkt_header.instrument);
       uint8_t idx = id.toIndex();  // 0-based index for array access

       // Store at procinfo[idx], bankinfo[idx], etc.
       // Works in BOTH standalone and client mode!
       memcpy(&m_cfg_ptr->procinfo[idx], &pkt, sizeof(cbPKT_PROCINFO));
   }
   ```

3. **Packet Routing**
   - `storePacket()`: Route incoming packet to correct shmem location
   - Uses packet.instrument field consistently (no mode switching!)

4. **Config Management**
   - Read/write PROCINFO, CHANINFO, etc.
   - Uses Central-compatible layout (cbMAXPROCS=4, cbNUM_FE_CHANS=768)

## Key Design Decisions

- **C++ only, internal use:** Not exposed to public API
- **Encapsulates indexing logic:** Central fix for the bug
- **Platform-specific:** Different implementations for Windows/macOS/Linux
- **No user-facing discovery:** cbsdk handles "which instrument to use" logic

## Current Status

- [x] Directory structure created
- [x] CMake integration added
- [x] ShmemSession class API designed (2025-11-11)
- [x] Upstream shared memory structures examined
- [x] Platform-specific shared memory code implemented (Windows/POSIX)
- [x] Instrument status management implemented
- [x] Configuration read/write implemented
- [x] Packet routing implemented (THE KEY FIX!)
- [x] Build system working (475KB library)
- [x] Unit tests written and passing (18 tests)

**Phase 2 Status:** ✅ **COMPLETE**
**Build:** ✅ Compiles successfully (475KB library)
**Test Coverage:** ✅ 18 tests, 100% passing

## Key Insights from Upstream Analysis

**Critical Discovery:** Central uses different constants than NSP!

- **NSP (upstream/cbproto/cbproto.h):**
  - `cbMAXPROCS = 1` (single processor)
  - `cbNUM_FE_CHANS = 256` (channels for one NSP)

- **Central (upstream/cbhwlib/cbhwlib.h):**
  - `cbMAXPROCS = 4` (up to 4 processors)
  - `cbNUM_FE_CHANS = 768` (channels for up to 4 NSPs)

**Design Decision:** cbshm MUST use Central constants to ensure compatibility!

## API (implemented in include/cbshm/shmem_session.h)

```cpp
namespace cbshm {
    class ShmemSession {
    public:
        static Result<ShmemSession> create(const std::string& name, Mode mode);

        // Instrument management
        Result<bool> isInstrumentActive(InstrumentId id) const;
        Result<void> setInstrumentActive(InstrumentId id, bool active);
        Result<InstrumentId> getFirstActiveInstrument() const;

        // Config access
        Result<cbPKT_PROCINFO> getProcInfo(InstrumentId id) const;
        Result<cbPKT_CHANINFO> getChanInfo(uint32_t channel) const;

        // Packet routing (THE KEY FIX!)
        Result<void> storePacket(const cbPKT_GENERIC& pkt);
        Result<void> storePackets(const cbPKT_GENERIC* pkts, size_t count);
    };
}
```

## References

- Design document: `docs/refactor_plan.md` (Phase 2)
- Current shared memory: `src/cbhwlib/cbhwlib.cpp`
