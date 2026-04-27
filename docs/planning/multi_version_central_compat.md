# Multi-Version Central Compatibility — Design Document

**Status**: Proposal
**Author**: CereLink Development Team
**Date**: 2026-04-25

## 1. Problem

CereLink's `CENTRAL_COMPAT` mode attaches to Central's shared memory and reads/writes
`cbCFGbuffer` using a single struct, `CentralLegacyCFGBUFF`, that mirrors Central's exact
binary layout. Central's layout changes — small but breaking — roughly every two years
when the wire protocol bumps. The current implementation only knows the most recent
layout, so a CereLink build is effectively locked to one Central release.

The pain falls on customers who cannot upgrade Central beyond a particular version
(typically because the rest of their recording stack depends on a known release) but
who would like to use CereLink-based tooling against that older Central.

This document proposes an architecture that lets a single CereLink binary interoperate
with multiple Central versions concurrently, isolates all version-specific code behind
a stable interface, and keeps the most-common path (no Central running) unchanged.

### 1.1 Scope of versions to support

- Newest Central (current): all `DeviceType`s including Gemini Hubs and the newer NSP.
- Up to three older Central releases simultaneously, restricted to **`NPLAY` and
  `LEGACY_NSP` only**. Customers using Gemini hardware can be guided to upgrade
  Central; older versions predate Gemini support anyway.
- Adapters are runtime-selected, not chosen at build time.

### 1.2 Non-goals

- Mirroring Central's spike cache. Not currently consumed; will be added if/when needed.
- Supporting more than one Central instance (`nInstance` 1-3). CereLink remains
  instance-0-only.
- Any change to native (no-Central) `STANDALONE`/`CLIENT` behavior.

## 2. Background

See [shared_memory_architecture.md](shared_memory_architecture.md) for the current
two-mode (NATIVE / CENTRAL_COMPAT) layout, and
[central_shared_memory_layout.md](central_shared_memory_layout.md) for upstream
Central's layout.

The current `ShmemSession` carries a `ShmemLayout` enum (`CENTRAL`, `CENTRAL_COMPAT`,
`NATIVE`) and ~20 accessors, each branching three ways:

```cpp
if (layout == NATIVE)               return nativeCfg()->procinfo;
else if (layout == CENTRAL_COMPAT)  return legacyCfg()->procinfo[idx];
else                                return centralCfg()->procinfo[idx];
```

The `CENTRAL` layout (CereLink's own near-Central struct) is a relic — Central never
reads it, and CereLink never writes it from a position where Central could see it.
This proposal removes it.

The `CENTRAL_COMPAT` branch is the locus of the version problem. This proposal moves
all version-specific code out of `ShmemSession` and behind a `CentralAdapter` interface.

## 3. Design overview

### 3.1 Two modes, two layouts

After this change, a `ShmemSession` is in exactly one of two configurations:

| Configuration            | Backing struct                         | Adapter? |
|--------------------------|----------------------------------------|----------|
| Native                   | `NativeConfigBuffer` (single device)   | None     |
| Central-attached (compat)| version-specific `CentralLayout_vXYZ`  | Required |

`ShmemLayout` collapses to a `bool hasAdapter()` predicate (or, for clarity, an enum
with two values). All version-specific dispatch happens through the adapter.

### 3.2 No relay (yet)

We considered a relay design where one process bridges Central's shmem into native
shmem and all other CereLink processes attach to the native shmem (so client code never
sees Central directly). The relay was rejected as the **first** step because:

- It introduces process election, refcounting across processes, and crash handoff —
  all real complexity with no current feature waiting on it.
- The version-isolation goal is fully achieved by an adapter abstraction alone.
- The same `CentralAdapter` interface designed here is what a future relay would
  consume; nothing in this design closes the door to adding a relay later.

The relay remains a future possibility for features that benefit from a single source
of truth (e.g., spike cache mirroring, derived-state publishing, low-latency in-process
fan-out from a single bridge). See § 9.

### 3.3 What the adapter encapsulates

1. **Config buffer geometry**: total size, field offsets, array dimensions.
2. **Per-device indexing**: how `DeviceType` maps to a Central instrument slot.
3. **Packet translation**: both directions, against this version's wire format.
4. **Capability negotiation**: which `DeviceType`s this version actually supports.
5. **Version identification**: what `(major, minor)` it implements.

The adapter does **not** own shmem mappings, threads, or session state. It is a pure
strategy object — given a pointer into Central's mapped cfg buffer (or a pointer into
the rec/xmt rings), it reads or writes correctly for its version.

## 4. Adapter interface

```cpp
// src/cbshm/include/cbshm/central_adapter.h
namespace cbshm {

struct CentralVersion {
    uint16_t major;
    uint16_t minor;
    constexpr bool operator==(CentralVersion o) const {
        return major == o.major && minor == o.minor;
    }
};

class CentralAdapter {
public:
    virtual ~CentralAdapter() = default;

    // ---- Identity / capability ------------------------------------------
    virtual CentralVersion version() const = 0;
    virtual const char*    name()    const = 0;     // "central-4.12", "central-3.11"
    virtual bool           supports(cbproto::DeviceType) const = 0;

    /// 0-based instrument index in Central's [4] arrays for this device, or -1
    virtual int            instrumentIndex(cbproto::DeviceType) const = 0;

    // ---- Buffer geometry ------------------------------------------------
    virtual size_t cfgBufferSize()  const = 0;
    virtual size_t recBufferBytes() const = 0;      // expected ring size for sanity check
    virtual size_t xmtGlobalBytes() const = 0;
    virtual size_t xmtLocalBytes()  const = 0;

    virtual uint32_t maxBanks()  const = 0;
    virtual uint32_t maxFilts()  const = 0;
    virtual uint32_t maxChans()  const = 0;
    virtual uint32_t maxGroups() const = 0;

    // ---- Config read accessors ------------------------------------------
    // Inputs/outputs are CURRENT-format structs. The adapter unpacks from
    // version-specific buffer to current-format on read; packs current →
    // version-specific on write.
    virtual cbPKT_PROCINFO   readProcInfo  (const void* cfg, int instr_idx) const = 0;
    virtual cbPKT_BANKINFO   readBankInfo  (const void* cfg, int instr_idx, int bank) const = 0;
    virtual cbPKT_FILTINFO   readFilterInfo(const void* cfg, int instr_idx, int filter) const = 0;
    virtual cbPKT_CHANINFO   readChanInfo  (const void* cfg, int channel) const = 0;
    virtual cbPKT_GROUPINFO  readGroupInfo (const void* cfg, int instr_idx, int group) const = 0;
    virtual cbPKT_SYSINFO    readSysInfo   (const void* cfg) const = 0;
    // ... one per cfg packet family the SDK exposes today

    // ---- Config write accessors -----------------------------------------
    virtual void writeProcInfo  (void* cfg, int instr_idx, const cbPKT_PROCINFO&) const = 0;
    virtual void writeBankInfo  (void* cfg, int instr_idx, int bank, const cbPKT_BANKINFO&) const = 0;
    virtual void writeFilterInfo(void* cfg, int instr_idx, int filter, const cbPKT_FILTINFO&) const = 0;
    virtual void writeChanInfo  (void* cfg, int channel, const cbPKT_CHANINFO&) const = 0;
    virtual void writeGroupInfo (void* cfg, int instr_idx, int group, const cbPKT_GROUPINFO&) const = 0;
    virtual void writeSysInfo   (void* cfg, const cbPKT_SYSINFO&) const = 0;

    // ---- Packet translation ---------------------------------------------
    /// Read raw bytes from Central's rec ring, produce a current-format packet.
    /// Returns total bytes consumed from src (header + payload), or 0 if src
    /// does not yet hold a complete packet.
    virtual size_t inboundPacket(const uint8_t* src, size_t src_avail,
                                 cbPKT_GENERIC& out) const = 0;

    /// Take a current-format packet, write the version-specific bytes into
    /// Central's xmt ring at dst. Returns bytes written, or 0 on error.
    virtual size_t outboundPacket(const cbPKT_GENERIC& src,
                                  uint8_t* dst, size_t dst_cap) const = 0;

    // ---- Status segment shape -------------------------------------------
    virtual bool hasGeminiStatus() const = 0;

    /// Convert this version's PROCTIME (ticks-at-30kHz on legacy, ns on Gemini)
    /// to nanoseconds for the SDK's uniform timestamp surface.
    virtual PROCTIME timestampToNs(PROCTIME raw, const cbPKT_SYSINFO&) const = 0;
};

// ---- Factory --------------------------------------------------------------

/// Sniff `version` at offset 0 of cfg and pick an adapter. Returns nullptr
/// if the version is unrecognized.
std::unique_ptr<CentralAdapter> makeCentralAdapter(const void* raw_cfg, size_t cfg_bytes);

/// Build a specific adapter (for tests / explicit overrides).
std::unique_ptr<CentralAdapter> makeCentralAdapter(CentralVersion);

} // namespace cbshm
```

## 5. Concrete adapters

Each adapter lives in its own translation unit and pulls in only its own legacy layout
header. Adding a new Central version is one new header + one new `.cpp` + one new line
in the factory.

```
src/cbshm/include/cbshm/legacy_layouts/
    central_layout_v412.h      ← matches current Central
    central_layout_v411.h
    central_layout_v40.h       ← pre-Gemini, smaller struct
    central_layout_v311.h
src/cbshm/src/
    central_adapter_v412.cpp
    central_adapter_v411.cpp
    central_adapter_v40.cpp
    central_adapter_v311.cpp
    central_adapter_factory.cpp
```

Layout headers are derived directly from old `cbhwlib.h` snapshots (we have access to
old Central headers and old nPlayServer source) and committed to the tree as historical
artifacts. They are never modified after being committed — they describe a wire
contract with a shipped binary.

### 5.1 Newest version (v4.12, full device support)

```cpp
// central_adapter_v412.cpp
class CentralAdapterV412 : public CentralAdapter {
    using CFG = CentralLayout_v412;
public:
    CentralVersion version() const override { return {4, 12}; }
    const char*    name()    const override { return "central-4.12"; }

    bool supports(DeviceType) const override { return true; }   // all devices
    int  instrumentIndex(DeviceType dt) const override {
        // GEMSTART==2 mapping
        switch (dt) {
            case DeviceType::HUB1: return 0;
            case DeviceType::HUB2: return 1;
            case DeviceType::HUB3: return 2;
            case DeviceType::NSP:  return 3;
            case DeviceType::LEGACY_NSP:
            case DeviceType::NPLAY: return 0;
            default: return -1;
        }
    }

    size_t   cfgBufferSize() const override { return sizeof(CFG); }
    uint32_t maxBanks()      const override { return 30; }
    uint32_t maxChans()      const override { return 880; }

    cbPKT_PROCINFO readProcInfo(const void* cfg, int idx) const override {
        return static_cast<const CFG*>(cfg)->procinfo[idx];
    }
    void writeProcInfo(void* cfg, int idx, const cbPKT_PROCINFO& v) const override {
        static_cast<CFG*>(cfg)->procinfo[idx] = v;
    }
    // ... rest mechanical

    size_t inboundPacket(const uint8_t* src, size_t avail, cbPKT_GENERIC& out) const override {
        // Wire format == current; just copy.
        if (avail < cbPKT_HEADER_SIZE) return 0;
        const auto* h = reinterpret_cast<const cbPKT_HEADER*>(src);
        size_t total = cbPKT_HEADER_SIZE + h->dlen * 4;
        if (avail < total) return 0;
        std::memcpy(&out, src, total);
        return total;
    }
    size_t outboundPacket(const cbPKT_GENERIC& src, uint8_t* dst, size_t cap) const override {
        size_t total = cbPKT_HEADER_SIZE + src.cbpkt_header.dlen * 4;
        if (cap < total) return 0;
        std::memcpy(dst, &src, total);
        return total;
    }

    bool     hasGeminiStatus()                         const override { return true; }
    PROCTIME timestampToNs(PROCTIME raw, const cbPKT_SYSINFO&) const override { return raw; }
};
```

### 5.2 Mid-vintage (v4.1, full device support, older wire format)

`v4.11` and `v4.12` may share the same cfg layout but differ in wire format. Cfg
accessors stay identical to `v4.12`; only the packet-translation methods change to
delegate into the existing `PacketTranslator::translatePayload_410_to_current` path.

```cpp
size_t inboundPacket(const uint8_t* src, size_t avail, cbPKT_GENERIC& out) const override {
    if (avail < HEADER_SIZE_410) return 0;
    const auto* h = reinterpret_cast<const cbPKT_HEADER*>(src);
    size_t total = HEADER_SIZE_410 + h->dlen * 4;
    if (avail < total) return 0;
    std::memcpy(&out, src, HEADER_SIZE_410);
    PacketTranslator::translatePayload_410_to_current(src, reinterpret_cast<uint8_t*>(&out));
    return total;
}
```

### 5.3 Older version (v4.0, NPLAY+LEGACY_NSP only)

Smaller layout struct (no Gemini fields), single-instrument indexing:

```cpp
class CentralAdapterV40 : public CentralAdapter {
    using CFG = CentralLayout_v40;
public:
    CentralVersion version() const override { return {4, 0}; }

    bool supports(DeviceType dt) const override {
        return dt == DeviceType::NPLAY || dt == DeviceType::LEGACY_NSP;
    }
    int instrumentIndex(DeviceType dt) const override {
        return supports(dt) ? 0 : -1;
    }

    bool     hasGeminiStatus() const override { return false; }
    uint32_t maxChans()        const override { return 272; }   // pre-Gemini
    uint32_t maxBanks()        const override { return 16;  }

    size_t inboundPacket(const uint8_t* src, size_t avail, cbPKT_GENERIC& out) const override {
        if (avail < HEADER_SIZE_400) return 0;
        const auto* h = reinterpret_cast<const cbPKT_HEADER_400*>(src);
        size_t total = HEADER_SIZE_400 + h->dlen * 4;
        if (avail < total) return 0;
        translateHeader_400_to_current(*h, out);
        PacketTranslator::translatePayload_400_to_current(src, reinterpret_cast<uint8_t*>(&out));
        return total;
    }
    PROCTIME timestampToNs(PROCTIME ticks, const cbPKT_SYSINFO& sys) const override {
        return ticks * 1000000000ULL / sys.sysfreq;
    }
};
```

`v3.11` follows the same shape as `v4.0` but with `HEADER_SIZE_311` and the
`PacketTranslator::translatePayload_311_to_current` path.

## 6. Version detection

`version` has lived at offset 0 of `cbCFGbuffer` in every released Central. The factory
reads four bytes and dispatches:

```cpp
std::unique_ptr<CentralAdapter> makeCentralAdapter(const void* raw_cfg, size_t bytes) {
    if (bytes < 4) return nullptr;
    uint32_t v = *static_cast<const uint32_t*>(raw_cfg);
    uint16_t major = (v >> 16) & 0xffff;
    uint16_t minor = v & 0xffff;

    if (major == 4 && minor >= 12) return std::make_unique<CentralAdapterV412>();
    if (major == 4 && minor == 11) return std::make_unique<CentralAdapterV411>();
    if (major == 4 && minor == 0)  return std::make_unique<CentralAdapterV40>();
    if (major == 3)                return std::make_unique<CentralAdapterV311>();
    return nullptr;   // unrecognized — caller errors out
}
```

A future fallback to the Windows registry (Central writes an install version key) is
optional and only needed if a version's `version` field is suspect. Not implemented in
the first cut.

The newest adapter accepts `minor >= 12` (forward-tolerant within a major) on the
assumption that a Central minor bump that keeps the wire layout identical does not
require a new adapter. If a minor bump *does* change the layout, we add an adapter and
tighten the inequality.

## 7. ShmemSession refactor

### 7.1 Surface changes

```cpp
// shmem_session.h
class ShmemSession {
public:
    static Result<ShmemSession> create(const SegmentNames& names,
                                       Mode mode,
                                       std::unique_ptr<CentralAdapter> adapter);
    // adapter == nullptr → native single-instrument layout
    // adapter != nullptr → Central-attached layout, dispatched via adapter

    bool                 hasAdapter() const;
    const CentralAdapter* adapter()   const;        // for diagnostics

    // ... all existing accessors remain
};
```

The `ShmemLayout` enum collapses. The `getConfigBuffer()` / `getLegacyConfigBuffer()`
escape hatches return raw `void*` (or are deleted in favor of going through accessors).

### 7.2 Accessor body shape

Every accessor that previously had a 3-way layout branch becomes:

```cpp
Result<cbPKT_PROCINFO> ShmemSession::getProcInfo(InstrumentId id) const {
    if (m_impl->adapter) {
        int idx = id.toIndex();   // 0-based
        return Result<cbPKT_PROCINFO>::ok(
            m_impl->adapter->readProcInfo(m_impl->cfg_buffer_raw, idx));
    }
    // Native single-instrument
    if (id.toIndex() != 0)
        return Result<cbPKT_PROCINFO>::err("native is single-instrument");
    return Result<cbPKT_PROCINFO>::ok(m_impl->nativeCfg()->procinfo);
}
```

The bounds checks (`max_banks`, `max_chans`, `max_filts`) ask the adapter when one is
present, otherwise use the `NATIVE_*` constants.

### 7.3 Receive path

`readReceiveBuffer()` already detects protocol and translates today via direct
`PacketTranslator` calls. The new code routes through the adapter:

```cpp
while (more_to_read()) {
    cbPKT_GENERIC pkt;
    size_t consumed = adapter->inboundPacket(read_ptr, avail, pkt);
    if (consumed == 0) break;          // partial packet at tail
    if (instrument_filter == -1 ||
        pkt.cbpkt_header.instrument == instrument_filter) {
        out_packets[written++] = pkt;
    }
    advance_tail(consumed);
}
```

### 7.4 Transmit path

`enqueuePacket()` symmetrically calls `adapter->outboundPacket()` to format into
Central's xmt ring. No filter is needed on transmit.

### 7.5 What gets deleted

- `ShmemLayout::CENTRAL` and the `CentralConfigBuffer` (CereLink's near-Central struct).
- The `centralCfg()` accessor in `Impl`.
- The single-version `CentralLegacyCFGBUFF` typedef (replaced by the per-version
  layout headers in `legacy_layouts/`).
- All `else if (layout == CENTRAL_COMPAT)` branches.
- The hardcoded GEMSTART mapping in `sdk_session.cpp` (moves into adapter).
- `getCompatProtocolVersion()` (the adapter is the source of truth).

## 8. SdkSession integration

```cpp
// sdk_session.cpp — open() flow
Result<SdkSession> SdkSession::open(DeviceType dev, const Config& cfg) {

    // 1. Is Central running on this PC?
    if (auto raw_cfg = tryAttachCentralCfg()) {                       // probes mutex + maps cbCFGbuffer
        auto adapter = makeCentralAdapter(raw_cfg.bytes, raw_cfg.size);
        if (!adapter)
            return err("Central is running but its version is unrecognized");
        if (!adapter->supports(dev))
            return err(std::string("Central ") + adapter->name()
                       + " does not support " + name(dev));

        auto shmem = ShmemSession::create(centralSegmentNames(),
                                          Mode::CLIENT,
                                          std::move(adapter));
        return SdkSession::wrap(std::move(shmem), dev);
    }

    // 2. Native: existing CLIENT-vs-STANDALONE election (unchanged)
    return openNative(dev, cfg);
}
```

The `tryAttachCentralCfg()` helper returns the mapped pointer + size on success, used
both to detect Central's presence and to feed the adapter factory.

## 9. Future: adding a relay later

If a future feature needs a single point of translation (spike-cache mirroring,
derived state, ultra-low-latency in-process callback fan-out from one bridge), a
relay layer can be added without touching the adapter contract:

- The first CereLink process to open a device while Central is running acquires a
  per-device named lock, allocates native shmem, attaches Central's shmem, and starts
  a bridge thread that pumps packets through the same `CentralAdapter` and then into
  native shmem.
- Subsequent CereLink processes detect the lock and attach to the native shmem as
  ordinary native CLIENTs.
- On bridge-process exit, the lock auto-releases (POSIX `flock` / Windows
  `WAIT_ABANDONED`) and the next CLIENT promotes itself.
- The low-latency direct-callback path becomes available to in-process consumers of
  the bridge.

The adapter does not change. Only `SdkSession::open()` learns to elect.

## 10. Testing strategy

### 10.1 Unit tests per adapter

For each `CentralAdapter_vXYZ`:

- Round-trip every cfg packet family: write a current-format struct, read it back,
  expect bitwise equality after the version-specific pack/unpack.
- Synthesize a known wire packet (header + payload) and verify `inboundPacket`
  produces the expected current-format `cbPKT_GENERIC`.
- Round-trip every translatable packet type: `outboundPacket` then `inboundPacket`
  → bitwise equal to the original (modulo fields the wire format does not carry).

These tests do not need a running Central — they operate on byte buffers.

### 10.2 Layout-size regression tests

For each `CentralLayout_vXYZ`:

- Static assertion that `sizeof(CentralLayout_vXYZ)` matches the value documented in
  the header (extracted from the corresponding old `cbhwlib.h`).
- Static assertions on offsets of fields that the adapter touches.

This catches accidental changes to a frozen layout struct.

### 10.3 Integration tests

- Run nPlayServer of a known older version on the test host, then attach with
  CereLink and verify reads/writes work end-to-end. (We have access to old
  nPlayServer binaries.)
- Skip integration tests for adapters whose nPlayServer is not available on the
  current test host.

### 10.4 Capability matrix test

- For each (`adapter`, `DeviceType`) pair, verify `supports()` returns the documented
  matrix (newest-only for Hubs/NSP; all-of-them for NPLAY/LEGACY_NSP).

## 11. Migration plan

1. **Add the interface and factory**: introduce `central_adapter.h` and an empty
   `central_adapter_factory.cpp`. No callers yet.
2. **Extract the current adapter**: implement `CentralAdapterV412` from the existing
   `CentralLegacyCFGBUFF` definitions. Move the GEMSTART-indexing helper into it.
   Wire the factory to return it for `major == 4 && minor >= 11`.
3. **Refactor `ShmemSession` accessors one family at a time**: route through
   `m_impl->adapter` when present. Keep the `CENTRAL_COMPAT` branch in parallel until
   all accessors are converted, then remove `CENTRAL_COMPAT` and the legacy
   single-version typedef.
4. **Refactor `SdkSession::open()`** to call `makeCentralAdapter()` and pass it into
   `ShmemSession::create()`. Delete `getCompatProtocolVersion()` and friends.
5. **Add older adapters**: commit `central_layout_v411.h`, `_v40.h`, `_v311.h` as
   frozen artifacts derived from old Central headers. Implement `CentralAdapterV411`,
   `_V40`, `_V311`. Register in the factory. Add their unit tests.
6. **Delete `ShmemLayout::CENTRAL`** and `CentralConfigBuffer` once nothing references
   them. Confirm with a clean build.

Each step is independently shippable; old behavior is preserved until step 6.

## 12. Open questions

- **Adapter granularity**: I am keying adapters on `(major, minor)`. If cfg layout
  and wire format historically bumped at different cadences, it might be cleaner to
  factor into `(cfg_layout_version, wire_version)` pairs and let each adapter
  compose two strategy objects. Verify against the actual history before committing
  to the simpler keying.
- **`procinfo[0].version` semantics**: today's `getCompatProtocolVersion()` reads
  `procinfo[0].version`, not the cfg `version` field. Confirm both are kept in sync
  by Central across all supported versions. If they can diverge, adapter selection
  may need to consult both.
- **Status segment differences**: older versions lack `m_nGeminiSystem` and related
  fields. The adapter exposes `hasGeminiStatus()` for callers; verify the SDK
  surface degrades cleanly when the field is absent.

## 13. References

- [shared_memory_architecture.md](shared_memory_architecture.md) — current architecture
- [central_shared_memory_layout.md](central_shared_memory_layout.md) — Central's layout
- `src/cbproto/include/cbproto/packet_translator.h` — existing version-tagged packet translation
- `src/cbshm/src/shmem_session.cpp` — current `ShmemSession` dispatch sites
- `src/cbshm/include/cbshm/central_types.h` — current `CentralLegacyCFGBUFF`
