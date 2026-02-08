# CereLink Shared Memory Architecture

## Overview

CereLink supports two shared memory modes:

- **Native mode** (first-class): CereLink creates its own per-device shared memory segments.
  Lean, right-sized buffers. No instance index -- devices identified by type. Packets are
  always in the current protocol format.

- **Central compat mode** (fallback): CereLink attaches to Central's existing shared memory
  as a CLIENT. Uses Central's monolithic layout with 4-instrument arrays. Packets may require
  protocol translation from older formats.

Mode is auto-detected at startup: if Central's shared memory exists, use compat mode;
otherwise, use native mode.

**See also**: [Central's shared memory layout](central_shared_memory_layout.md) for the
upstream layout that compat mode interoperates with.

## Device Identification

CereLink identifies devices by **type**, not by numeric instance index. Each device type is
a singleton with fixed, well-known network configuration:

| DeviceType | IP Address | Recv Port | FE Channels |
|------------|-----------|-----------|-------------|
| `LEGACY_NSP` | 192.168.137.128 | 51002 | 256 |
| `NSP` | 192.168.137.128 | 51001 | 256 |
| `HUB1` | 192.168.137.200 | 51002 | 256 |
| `HUB2` | 192.168.137.201 | 51003 | 256 |
| `HUB3` | 192.168.137.202 | 51004 | 256 |
| `NPLAY` | 127.0.0.1 | -- | 256 |

There is no instance index. The device type **is** the identifier. A client opens a session
for a specific device type and receives only that device's data.

```
// No numeric instance index to remember
auto session = SdkSession::open(DeviceType::HUB1);
```

## Architecture Diagram (Native Mode)

```
+------------------------------------------------------------------------------------+
|                                    CEREBUS DEVICE                                   |
|                              (NSP Hardware - UDP Protocol)                          |
+--------------------+-----------------------------------+---------------------------+
                     |                                   |
                     | UDP Packets                       | UDP Packets
                     | (Port varies by device)           | (Port varies by device)
                     v                                   ^
+-------------------------------------------------------------------------------------+
|                           STANDALONE PROCESS (owns device)                           |
|  +--------------------------------------------------------------------------------+ |
|  |                                    THREADS                                      | |
|  |                                                                                 | |
|  |  +----------------+      +----------------+      +-----------------+            | |
|  |  |  UDP Receive   |      |   UDP Send     |      |    Callback     |            | |
|  |  |    Thread      |      |    Thread      |      |   Dispatcher    |            | |
|  |  |  (cbdev)       |      |  (cbdev)       |      |    Thread       |            | |
|  |  +--------+-------+      +--------^-------+      +--------^-------+            | |
|  |           |                       |                       |                     | |
|  |           | Packets               | Dequeue               | Process             | |
|  |           | (translated to        | packets               | callbacks           | |
|  |           |  current protocol)    |                       |                     | |
|  |           v                       |                       |                     | |
|  |  +--------+-----------------------+-----------------------+-------+             | |
|  |  |           onPacketsReceivedFromDevice()                        |             | |
|  |  |                                                                |             | |
|  |  |   1. storePacket() -> rec buffer (ring buffer)                 |             | |
|  |  |   2. storePacket() -> cfg buffer (config updates)              |             | |
|  |  |   3. signalData()  -> signal event  <-----------+              |             | |
|  |  |   4. Enqueue to local SPSC queue                | SIGNAL!      |             | |
|  |  +--------------------------------------------------+             |             | |
|  +--------------------------------------------------------------------------------+ |
+------------------+----------------------------------------------------------------------+
                   |
                   | Writes to (Producer)
                   |
    +==============v==================================================================+
    ||                   NATIVE SHARED MEMORY (per device)                            ||
    ||                   Named: cbshm_{device}_{segment}                              ||
    ||                                                                                ||
    ||  1. cbshm_hub1_cfg          | Config for 1 device (284 channels, ~1 MB)        ||
    ||  2. cbshm_hub1_rec          | Receive ring buffer (~256 MB)                    ||
    ||  3. cbshm_hub1_xmt          | Transmit queue (~5 MB)                           ||
    ||  4. cbshm_hub1_xmt_local    | Local transmit queue (~2 MB)                     ||
    ||  5. cbshm_hub1_status       | Device status (~few KB)                          ||
    ||  6. cbshm_hub1_spk          | Spike cache (272 analog channels)                ||
    ||  7. cbshm_hub1_signal       | Data availability signal                         ||
    ||                                                                                ||
    ||  All packets stored in CURRENT protocol format (translation at cbdev layer)    ||
    ||  Config sized for 1 instrument (no [4] arrays)                                 ||
    ||  Client pays only for devices it opens                                         ||
    +==================================================================================+
                   |
                   | Reads from (Consumer)
                   |
+------------------v-----------------------------------------------------------------------+
|                           CLIENT PROCESS (attaches to shmem)                              |
|  +-------------------------------------------------------------------------------------+ |
|  |  Shared Memory Receive Thread                                                       | |
|  |                                                                                      | |
|  |  while (running) {                                                                   | |
|  |    waitForData(250ms)                 <-- wakeup from signal                         | |
|  |    if (signaled) {                                                                   | |
|  |      readReceiveBuffer()              <-- all packets are this device's              | |
|  |      packet_callback(packets, count)  <-- direct invocation, no queue                | |
|  |    }                                                                                 | |
|  |  }                                                                                   | |
|  +-------------------------------------------------------------------------------------+ |
+-----------------------------------------------------------------------------------------+
```

## Native Mode

### Segment Naming

Each device gets its own set of shared memory segments:

```
cbshm_{device}_{segment}

Examples:
  cbshm_nsp_cfg         cbshm_hub1_cfg         cbshm_hub2_cfg
  cbshm_nsp_rec         cbshm_hub1_rec         cbshm_hub2_rec
  cbshm_nsp_xmt         cbshm_hub1_xmt         cbshm_hub2_xmt
  cbshm_nsp_xmt_local   cbshm_hub1_xmt_local   cbshm_hub2_xmt_local
  cbshm_nsp_status      cbshm_hub1_status      cbshm_hub2_status
  cbshm_nsp_spk         cbshm_hub1_spk         cbshm_hub2_spk
  cbshm_nsp_signal      cbshm_hub1_signal      cbshm_hub2_signal
```

On POSIX, names are prefixed with `/` (e.g., `/cbshm_hub1_rec`).

### Per-Device Config Buffer (Native)

Since each device is a single NSP, the config buffer drops all multi-instrument arrays
to scalars:

```c
typedef struct {
    uint32_t version;
    uint32_t device_type;           // DeviceType enum value

    cbPKT_SYSINFO sysinfo;
    cbPKT_PROCINFO procinfo;                        // was [4], now 1
    cbPKT_BANKINFO bankinfo[NATIVE_MAXBANKS];       // 1 instrument's banks
    cbPKT_GROUPINFO groupinfo[NATIVE_MAXGROUPS];    // 1 instrument's groups
    cbPKT_FILTINFO filtinfo[NATIVE_MAXFILTS];       // 1 instrument's filters
    cbPKT_ADAPTFILTINFO adaptinfo;                  // was [4], now 1
    cbPKT_REFELECFILTINFO refelecinfo;              // was [4], now 1
    cbPKT_LNC isLnc;                                // was [4], now 1

    cbPKT_CHANINFO chaninfo[NATIVE_MAXCHANS];       // 284 channels (1 NSP)

    NativeSPIKE_SORTING isSortingOptions;           // sized for 284 channels
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_NPLAY isNPlay;
    cbVIDEOSOURCE isVideoSource[cbMAXVIDEOSOURCE];
    cbTRACKOBJ isTrackObj[cbMAXTRACKOBJ];
    cbPKT_FILECFG fileinfo;
} NativeConfigBuffer;
```

Channel count per device (`NATIVE_MAXCHANS` with `cbMAXPROCS=1`):

| Type | Count | Formula |
|------|-------|---------|
| Front-end | 256 | `cbNUM_FE_CHANS` |
| Analog input | 16 | `16 * 1` |
| Analog output | 4 | `4 * 1` |
| Audio output | 2 | `2 * 1` |
| Digital input | 1 | `1 * 1` |
| Serial | 1 | `1 * 1` |
| Digital output | 4 | `4 * 1` |
| **Total** | **284** | |

### Per-Device Memory Footprint

| Segment | Central-compat (4 instruments) | Native (1 device) | Savings |
|---------|-------------------------------|-------------------|---------|
| Config buffer | ~4 MB (880 ch, `[4]` arrays) | ~1 MB (284 ch, scalars) | ~75% |
| Receive buffer | ~768 MB (768 FE ch) | ~256 MB (256 FE ch) | ~67% |
| XmtGlobal | ~290 MB (5000 * max-UDP-size) | ~5 MB (5000 * 1024 bytes) | ~98% |
| XmtLocal | ~116 MB (2000 * max-UDP-size) | ~2 MB (2000 * 1024 bytes) | ~98% |
| Spike cache | large (832 analog ch) | ~1/3 (272 analog ch) | ~67% |
| Status | ~few KB | ~few KB | -- |
| **Total** | **~1.2 GB** | **~265 MB** | **~78%** |

The transmit buffers are dramatically smaller because they carry only config/command packets
(max 1024 bytes each), not max-UDP-sized packets. Central's XmtGlobal is drained at 4
packets per 10ms; the buffer only needs capacity for burst scenarios like CCF file loading.

### Packet Format in Native Shared Memory

All packets in native shared memory are in **current protocol format** (4.2+). Protocol
translation happens at the cbdev layer before packets reach shared memory:

```
Device (any protocol) --> cbdev DeviceSession wrapper --> translates to current format
                          --> writes current-format packets to native shmem
                          --> CLIENTs read current format (no translation needed)
```

This means CLIENT processes never need to know what protocol the device speaks.

## Central Compat Mode

When CereLink detects that Central is running (by checking for Central's system mutex
`cbSharedDataMutex`), it attaches to Central's existing shared memory as a CLIENT.

### Segment Names (Central's)

Central uses instance-0 bare names:
- `cbCFGbuffer`, `cbRECbuffer`, `XmtGlobal`, `XmtLocal`, `cbSTATUSbuffer`, `cbSPKbuffer`,
  `cbSIGNALevent`

CereLink only supports Central instance 0. Instances 1-3 are not supported.

### DeviceType to Instrument Index Mapping

Central's config buffer has `[4]` arrays for up to 4 instruments. CereLink must map
`DeviceType` to the correct instrument index in Central's arrays.

The mapping is hardcoded based on Central's compile-time `GEMSTART` setting. The current
Central build uses `GEMSTART == 2`:

| Instrument Index | Device | IP Address | Port |
|-----------------|--------|------------|------|
| 0 | Hub 1 | 192.168.137.200 | 51002 |
| 1 | Hub 2 | 192.168.137.201 | 51003 |
| 2 | Hub 3 | 192.168.137.202 | 51004 |
| 3 | NSP | 192.168.137.128 | 51001 |

**Limitation**: This mapping assumes Central was compiled with `GEMSTART == 2`. An alternate
build (`GEMSTART == 1`) uses a different ordering: NSP=0, Hub1=1, Hub2=2. CereLink does
not currently detect or adapt to alternate GEMSTART configurations. If a future Central
build changes GEMSTART, this mapping must be updated. A runtime discovery mechanism (e.g.,
scanning `procinfo` slots for identifying data) could be added if needed.

For legacy (non-Gemini) NSP systems, only instrument index 0 is used.

### Receive Buffer Filtering

In Central's shared memory, ALL devices' packets go into ONE receive ring buffer. When
CereLink compat mode is reading for a specific device:

1. Read all packets from `cbRECbuffer` (cannot filter at the shmem level)
2. Check each packet's `cbpkt_header.instrument` field
3. Deliver only packets matching the requested device's instrument index

This is less efficient than native mode (where the receive buffer only contains one device's
packets), but the large buffer size makes this a negligible cost.

### Protocol Translation in Compat Mode

Central only supports one protocol version at a time. Packets in Central's shared memory
are in whatever protocol format Central is using, which may be older than CereLink's current
format (4.2+).

When reading from Central's shared memory, CereLink must:
1. Detect the protocol version (from `cbCFGBUFF.sysinfo` version field)
2. Translate packets from old format to current format using `PacketTranslator`
3. Deliver current-format packets to the user callback

When writing to Central's transmit buffer, the reverse translation applies.

This reuses the same `PacketTranslator` infrastructure that `cbdev` uses for direct UDP
connections to older devices.

### Config Buffer Access in Compat Mode

Central's `cbCFGBUFF` has a different field layout than CereLink's `NativeConfigBuffer` or
`cbConfigBuffer` (see "Differences from Central" section below). In compat mode, CereLink
must use a `CentralLegacyCFGBUFF` struct that matches Central's exact field order to read
the config buffer correctly.

Config access for a specific device uses the instrument index:
```
cfg->procinfo[instrument_idx]
cfg->bankinfo[instrument_idx]
cfg->filtinfo[instrument_idx]
// etc.
```

## Mode Auto-Detection

```
SdkSession::open(DeviceType::HUB1)
    |
    +-- Can open Central's "cbSharedDataMutex" (instance 0)?
    |     |
    |     YES --> Central Compat Mode
    |             - Map Central's 7 instance-0 segments
    |             - Use GEMSTART==2 mapping: Hub1 = instrument index 0
    |             - Filter receive buffer by instrument field
    |             - Translate packets if protocol version < current
    |             - Index into [4] arrays for config access
    |
    +-- NO --> Can open "cbshm_hub1_signal"?
    |     |
    |     YES --> Native Client Mode
    |             - Map cbshm_hub1_* segments (read-only)
    |             - All packets are Hub1's (no filtering)
    |             - Packets in current format (no translation)
    |             - Config is single-instrument (no indexing)
    |
    +-- NO --> Native Standalone Mode
              - Create cbshm_hub1_* segments
              - Start cbdev (protocol auto-detect + translation)
              - Write current-format packets to native shmem
              - Other CLIENTs can attach via Native Client Mode
```

## Key Data Flow Paths

### Device -> STANDALONE -> Shared Memory -> CLIENT

1. **NSP device** sends UDP packet
2. **STANDALONE UDP receive thread** catches packet
3. **cbdev** translates packet to current protocol format (if device uses older protocol)
4. **onPacketsReceivedFromDevice()**:
   - `storePacket(pkt)` writes to receive buffer `[headindex]`
   - `headindex++` (advance writer position)
   - `signalData()` -> Set signal event (wake up CLIENTs!)
   - Enqueue to SPSC queue for callback dispatcher thread
5. **CLIENT shmem receive thread** wakes up from `waitForData()`
6. **CLIENT** calls `readReceiveBuffer()`:
   - Read from receive buffer `[tailindex]` to `[headindex]`
   - `tailindex += packets_read` (advance reader position)
   - Parse packets from ring buffer (handle wraparound)
7. **CLIENT** invokes user callback **directly** (no queue, no 2nd thread needed)

### CLIENT -> Shared Memory -> STANDALONE -> Device

1. **Client app** calls `sendPacket(command)`
2. `enqueuePacket()` writes to transmit queue
3. **STANDALONE send thread** dequeues from transmit queue
4. **Send thread** transmits packet via UDP to device

## Synchronization

### STANDALONE (Producer)
- Calls `signalData()` after writing packets
- **Windows**: `SetEvent()` (manual-reset event)
- **POSIX**: `sem_post()` (increment semaphore)

### CLIENT (Consumer)
- Calls `waitForData(250ms)` before reading
- **Windows**: `WaitForSingleObject()` with timeout
- **POSIX**: `sem_timedwait()` (Linux) or polling `sem_trywait()` (macOS)

### Efficiency
- CLIENT sleeps until signaled (no CPU-burning polling!)
- Immediate wakeup when new data arrives

## Ring Buffer Tracking

### Overview
- Receive buffer is a ring buffer with wrap-around capability
- Native mode: 256 * 65536 * 4 - 1 dwords (~256 MB per device)
- Central compat: 768 * 65536 * 4 - 1 dwords (~768 MB, all devices combined)

### Writer (STANDALONE)
- Updates `headindex` - current write position
- Updates `headwrap` - increments each time buffer wraps around

### Reader (CLIENT)
- Tracks own `tailindex` - current read position
- Tracks own `tailwrap` - increments each time reader wraps around
- Each CLIENT maintains independent read position

### Synchronization Logic
- **No new data**: `tailwrap == headwrap && tailindex == headindex`
- **Data available**: `tailindex` < `headindex` (same wrap) or different wrap counters
- **Buffer overrun**: `headwrap > tailwrap + 1` (writer lapped reader - data lost!)

### Packet Format
- First dword of each packet = packet size in dwords
- Variable-length packets
- Handles wraparound mid-packet (copy in two parts)

## Thread Architecture

### STANDALONE Process Threads
1. **UDP Receive Thread** (cbdev) - Receives packets from device, translates protocol
2. **UDP Send Thread** (cbdev) - Sends packets to device
3. **Callback Dispatcher Thread** - Decouples fast UDP receive from slow user callbacks
4. **Main Thread** - User application

**Why separate callback thread?** UDP packets arrive at high rate and OS buffer is limited.
We must dequeue UDP packets quickly to avoid drops. User callbacks can be slow, so we use
an SPSC queue to buffer between fast UDP receive and slow callback processing.

### CLIENT Process Threads (Optimized)
1. **Shared Memory Receive Thread** - Reads from receive buffer AND invokes callbacks directly
2. **Main Thread** - User application

**Why no separate callback thread?** Reading from the receive buffer is not time-critical
(large buffer provides ample buffering). We invoke user callbacks directly, eliminating:
- One extra thread (simpler architecture, less overhead)
- One extra data copy (receive buffer -> callback, no packet_queue needed)

## Differences from Central-Suite's Shared Memory Layout

CereLink's current `cbConfigBuffer` struct is **NOT binary-compatible** with Central's
`cbCFGBUFF`. For a complete description of Central's layout, see
[central_shared_memory_layout.md](central_shared_memory_layout.md).

### cbCFGbuffer / Config Buffer (INCOMPATIBLE)

Field ordering is changed and fields are added/removed:

| # | Central `cbCFGBUFF` | CereLink `cbConfigBuffer` |
|---|---------------------|---------------------------|
| 1 | `version` | `version` |
| 2 | `sysflags` | `sysflags` |
| 3 | **`optiontable`** | **`instrument_status[4]`** (NEW) |
| 4 | **`colortable`** | `sysinfo` |
| 5 | `sysinfo` | `procinfo[4]` |
| 6 | `procinfo[4]` | `bankinfo[4][30]` |
| 7 | `bankinfo[4][30]` | `groupinfo[4][8]` |
| 8 | `groupinfo[4][8]` | `filtinfo[4][32]` |
| 9 | `filtinfo[4][32]` | `adaptinfo[4]` |
| 10 | `adaptinfo[4]` | `refelecinfo[4]` |
| 11 | `refelecinfo[4]` | **`isLnc[4]`** (MOVED earlier) |
| 12 | `chaninfo[880]` | `chaninfo[880]` |
| 13 | `isSortingOptions` | `isSortingOptions` |
| 14 | `isNTrodeInfo[..]` | `isNTrodeInfo[..]` |
| 15 | `isWaveform[..][..]` | `isWaveform[..][..]` |
| 16 | **`isLnc[4]`** | `isNPlay` |
| 17 | `isNPlay` | `isVideoSource[..]` |
| 18 | `isVideoSource[..]` | `isTrackObj[..]` |
| 19 | `isTrackObj[..]` | `fileinfo` |
| 20 | `fileinfo` | **`optiontable`** (MOVED later) |
| 21 | **`hwndCentral`** | **`colortable`** (MOVED later) |
| 22 | -- | (hwndCentral OMITTED) |

Central compat mode requires a separate `CentralLegacyCFGBUFF` struct matching Central's
exact layout to read the config buffer correctly.

### cbSTATUSbuffer / PC Status (PARTIALLY COMPATIBLE)

| Difference | Central `cbPcStatus` | CereLink `CentralPCStatus` |
|-----------|----------------------|----------------------------|
| Type | C++ class (private/public) | Plain C struct |
| `APP_WORKSPACE[10]` | Present (at end) | **Omitted** |
| Overlap | Fields match in order up to `m_nGeminiSystem` | Same |

CereLink's struct is a subset -- safe to read, safe to write (omitted field is at the end).

### cbRECbuffer, XmtGlobal, XmtLocal (COMPATIBLE)

Same field layout:
- `received`, `lasttime`, `headwrap`, `headindex`, `buffer[N]` (receive)
- `transmitted`, `headindex`, `tailindex`, `last_valid_index`, `bufferlen`, `buffer[N]` (transmit)

Central uses flexible array member (`buffer[0]`), CereLink uses fixed-size. Binary layout
matches as long as allocated sizes match.

### cbSPKbuffer, cbSIGNALevent (COMPATIBLE)

Same structure layouts and mechanisms.

### Compatibility Summary

| Segment | Compatible? | Notes |
|---------|-------------|-------|
| cbCFGbuffer | **NO** | Field order differs; need `CentralLegacyCFGBUFF` |
| cbRECbuffer | Yes | Same layout |
| XmtGlobal | Yes | Same layout |
| XmtLocal | Yes | Same layout |
| cbSTATUSbuffer | Partial | CereLink is a subset (missing workspace at end) |
| cbSPKbuffer | Yes | Same layout |
| cbSIGNALevent | Yes | Same mechanism |

## Implementation Status

- All 7 shared memory segments implemented (Central-compat layout)
- cbSIGNALevent synchronization working
- Ring buffer reading logic complete
- CLIENT mode shared memory receive thread implemented
- STANDALONE mode signaling to CLIENT processes
- Thread lifecycle management (start/stop)
- Optimized CLIENT mode (1 thread, 1 data copy)
- All unit tests passing (18 cbshm tests + 28 SDK tests)

### TODO (Native Mode)

- [ ] Define `NativeConfigBuffer` struct (single-instrument)
- [ ] Define `NativeSPIKE_SORTING` struct (284-channel arrays)
- [ ] Implement `cbshm_{device}_{segment}` naming in ShmemSession
- [ ] Implement right-sized receive buffer (256 FE channels)
- [ ] Implement right-sized transmit buffers (1024-byte packet slots)
- [ ] Implement mode auto-detection in SdkSession::open()
- [ ] Implement `CentralLegacyCFGBUFF` for compat mode config access
- [ ] Implement receive buffer instrument filtering for compat mode
- [ ] Implement protocol translation bridge for compat mode reads/writes
- [ ] Update unit tests for dual-mode operation

## Code Locations

- **Shared Memory**: `src/cbshm/`
  - `include/cbshm/shmem_session.h` - Public API (ShmemSession class)
  - `src/shmem_session.cpp` - Implementation
  - `include/cbshm/central_types.h` - Central-compatible buffer structures
  - `include/cbshm/config_buffer.h` - Configuration buffer struct definition

- **SDK Integration**: `src/cbsdk/`
  - `src/sdk_session.cpp` - High-level SDK, mode auto-detection, segment naming
  - Threads, callbacks, packet routing

- **Protocol**: `src/cbproto/`
  - `include/cbproto/instrument_id.h` - Type-safe InstrumentId (1-based internally)
  - `include/cbproto/types.h` - Per-NSP constants (cbMAXPROCS=1, cbNUM_FE_CHANS=256)
  - `include/cbproto/cbproto.h` - Protocol packet definitions
  - `include/cbproto/connection.h` - Protocol version enum

- **Device Layer**: `src/cbdev/`
  - UDP communication with NSP hardware
  - Protocol detection (`src/protocol_detector.cpp`)
  - Protocol translation (`src/packet_translator.cpp`)
  - Per-version session wrappers (`src/device_session_{311,400,410}.cpp`)
  - Used only in STANDALONE mode

## References

- [Central's shared memory layout](central_shared_memory_layout.md)
- Central Suite source: `Central-Suite/cbhwlib/cbhwlib.h` and `cbhwlib.cpp`
- Central instrument assignment: `Central-Suite/CentralCommon/CentralDlg.cpp` (GEMSTART)
- Cerebus Protocol Specification
