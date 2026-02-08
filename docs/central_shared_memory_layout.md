# Central-Suite Shared Memory Layout

This document describes the shared memory layout used by the upstream Central-Suite application
(the proprietary Windows application from Blackrock Neurotech). CereLink may need to interoperate
with this layout when running as a CLIENT attached to Central's shared memory.

Source of truth: `Central-Suite/cbhwlib/cbhwlib.h` and `Central-Suite/cbhwlib/cbhwlib.cpp`

## Key Concepts: Instance vs Instrument

Central has **two independent indexing dimensions** for multi-device support:

### Instance ID (`nInstance`)

- **Range**: 0 to `cbMAXOPEN - 1` (0 to 3)
- **Purpose**: Selects which **set of shared memory segments** to use. Each instance is a
  completely independent set of all 7 segments. This allows multiple Central applications
  (or multiple CereLink sessions) to run simultaneously.
- **Set via**: `--instance` command-line parameter for Central, or first argument to `cbSdkOpen(nInstance, ...)`
- **Effect on naming**: Instance 0 uses bare names (`"cbRECbuffer"`). Instance N>0 appends
  the number (`"cbRECbuffer1"`, `"cbRECbuffer2"`, etc.)

### Instrument Number (`nInstrument`)

- **Range**: 1 to `cbMAXPROCS` (1-based; `cbNSP1 = 1`)
  - For Central: `cbMAXPROCS = 4` (supports up to 4 NSPs)
  - For NSP firmware: `cbMAXPROCS = 1`
- **Purpose**: Identifies a **physical NSP** within a single instance's shared memory. The
  `cbCFGBUFF` config buffer has arrays dimensioned by `cbMAXPROCS` to hold per-instrument data.
- **In packet headers**: Stored as **0-based** in `cbPKT_HEADER.instrument` (so instrument 1 is
  stored as 0). Functions that take `nInstrument` are 1-based and subtract 1 for indexing.

### Relationship

```
nInstance (0-3)          Selects WHICH shared memory set
  |
  +-- Instance 0 --> "cbRECbuffer", "cbCFGbuffer", ...
  |                    +-- cbCFGBUFF.procinfo[0..3]     <-- nInstrument 1-4
  |                    +-- cbCFGBUFF.chaninfo[0..879]   <-- global channel numbers
  |
  +-- Instance 1 --> "cbRECbuffer1", "cbCFGbuffer1", ...
  |                    +-- (same structure, independent data)
  :
```

### The `cb_library_index` Indirection

There is an indirection layer between `nInstance` and the internal buffer slot `nIdx`:

```c
UINT32 cb_library_index[cbMAXOPEN];    // maps nInstance -> nIdx
UINT32 cb_library_initialized[cbMAXOPEN];

// In cbOpen():
// 1. Find first uninitialized slot -> nIdx
// 2. cb_library_index[nInstance] = nIdx
// 3. Create/open shared memory at slot nIdx
```

Every buffer access goes through this indirection:
```c
UINT32 nIdx = cb_library_index[nInstance];
cbRECBUFF* rec = cb_rec_buffer_ptr[nIdx];
```

## Shared Memory Segments

Central creates **7 named shared memory segments** per instance.

### Naming Convention

| Segment | Base Name | Instance 0 | Instance N (N>0) |
|---------|-----------|------------|-------------------|
| Config buffer | `cbCFGbuffer` | `cbCFGbuffer` | `cbCFGbufferN` |
| Receive buffer | `cbRECbuffer` | `cbRECbuffer` | `cbRECbufferN` |
| Global transmit | `XmtGlobal` | `XmtGlobal` | `XmtGlobalN` |
| Local transmit | `XmtLocal` | `XmtLocal` | `XmtLocalN` |
| PC status | `cbSTATUSbuffer` | `cbSTATUSbuffer` | `cbSTATUSbufferN` |
| Spike cache | `cbSPKbuffer` | `cbSPKbuffer` | `cbSPKbufferN` |
| Signal event | `cbSIGNALevent` | `cbSIGNALevent` | `cbSIGNALeventN` |
| System mutex | `cbSharedDataMutex` | `cbSharedDataMutex` | `cbSharedDataMutexN` |

Implementation: `cbhwlib.cpp` lines 271-399 (`cbOpen()`) and lines 3744-3904 (`CreateSharedObjects()`)

### Segment Details

#### 1. cbCFGBUFF (Configuration Buffer)

The largest and most complex structure. Contains cached configuration for the entire system.

```c
// cbhwlib.h line 1121
typedef struct {
    UINT32          version;
    UINT32          sysflags;
    cbOPTIONTABLE   optiontable;                                    // 32 uint32 values
    cbCOLORTABLE    colortable;                                     // 96 uint32 values
    cbPKT_SYSINFO   sysinfo;
    cbPKT_PROCINFO  procinfo[cbMAXPROCS];                           // [4] per-instrument
    cbPKT_BANKINFO  bankinfo[cbMAXPROCS][cbMAXBANKS];               // [4][30]
    cbPKT_GROUPINFO groupinfo[cbMAXPROCS][cbMAXGROUPS];             // [4][8]
    cbPKT_FILTINFO  filtinfo[cbMAXPROCS][cbMAXFILTS];               // [4][32]
    cbPKT_ADAPTFILTINFO adaptinfo[cbMAXPROCS];                      // [4]
    cbPKT_REFELECFILTINFO refelecinfo[cbMAXPROCS];                  // [4]
    cbPKT_CHANINFO  chaninfo[cbMAXCHANS];                           // [880] all channels
    cbSPIKE_SORTING isSortingOptions;                               // spike sorting state
    cbPKT_NTRODEINFO isNTrodeInfo[cbMAXNTRODES];                    // ntrode config
    cbPKT_AOUT_WAVEFORM isWaveform[AOUT_NUM_GAIN_CHANS][cbMAX_AOUT_TRIGGER];
    cbPKT_LNC       isLnc[cbMAXPROCS];                              // [4] line noise cancellation
    cbPKT_NPLAY     isNPlay;                                        // nPlay info
    cbVIDEOSOURCE   isVideoSource[cbMAXVIDEOSOURCE];                // video sources
    cbTRACKOBJ      isTrackObj[cbMAXTRACKOBJ];                      // trackable objects
    cbPKT_FILECFG   fileinfo;                                       // file recording status
    HANDLE          hwndCentral;                                    // Central window handle (32 or 64 bit)
} cbCFGBUFF;
```

**Key points**:
- `optiontable` and `colortable` come **before** `sysinfo`
- `isLnc[4]` comes **after** `isWaveform`
- `hwndCentral` is last and its size depends on 32-bit vs 64-bit build
- Arrays indexed by `cbMAXPROCS` hold per-instrument data (up to 4 NSPs)
- `chaninfo[cbMAXCHANS]` uses global channel numbers across all instruments

**Channel count (`cbMAXCHANS`)** for Central (`cbMAXPROCS=4`, `cbNUM_FE_CHANS=768`):

| Type | Count | Formula |
|------|-------|---------|
| Front-end | 768 | `cbNUM_FE_CHANS` |
| Analog input | 64 | `16 * cbMAXPROCS` |
| Analog output | 16 | `4 * cbMAXPROCS` |
| Audio output | 8 | `2 * cbMAXPROCS` |
| Digital input | 4 | `1 * cbMAXPROCS` |
| Serial | 4 | `1 * cbMAXPROCS` |
| Digital output | 16 | `4 * cbMAXPROCS` |
| **Total** | **880** | `cbMAXCHANS` |

#### 2. cbRECBUFF (Receive Ring Buffer)

```c
// cbhwlib.h line 975
#define cbRECBUFFLEN   cbNUM_FE_CHANS * 65536 * 4 - 1

typedef struct {
    UINT32 received;                    // total packets received
    PROCTIME lasttime;                  // timestamp of last packet
    UINT32 headwrap;                    // wrap counter for head
    UINT32 headindex;                   // current write position
    UINT32 buffer[cbRECBUFFLEN];        // ring buffer data
} cbRECBUFF;
```

**Size**: For Central (`cbNUM_FE_CHANS=768`): 201,326,591 uint32 words = **~768 MB**

- STANDALONE writes packets here, advances `headindex`
- CLIENT reads from `tailindex` to `headindex` (CLIENT tracks its own tail)
- Wrap-around tracked via `headwrap` counter

#### 3. cbXMTBUFF (Transmit Ring Buffer)

```c
// cbhwlib.h line 991
typedef struct {
    UINT32 transmitted;         // packets sent count
    UINT32 headindex;           // first empty position
    UINT32 tailindex;           // one past last emptied position
    UINT32 last_valid_index;    // max valid index
    UINT32 bufferlen;           // buffer length in uint32 units
    UINT32 buffer[0];           // flexible array member
} cbXMTBUFF;
```

Used for both `XmtGlobal` (packets sent to device) and `XmtLocal` (IPC-only packets).
The `buffer` is a **flexible array member** -- actual size is set at creation time.

- `XmtGlobal`: 5000 max-sized packet slots
- `XmtLocal`: 2000 max-sized packet slots

#### 4. cbPcStatus (PC Status)

```cpp
// cbhwlib.h line 1048
class cbPcStatus {
public:
    cbPKT_UNIT_SELECTION isSelection[cbMAXPROCS];       // [4]
private:
    INT32 m_iBlockRecording;
    UINT32 m_nPCStatusFlags;
    UINT32 m_nNumFEChans;
    UINT32 m_nNumAnainChans;
    UINT32 m_nNumAnalogChans;
    UINT32 m_nNumAoutChans;
    UINT32 m_nNumAudioChans;
    UINT32 m_nNumAnalogoutChans;
    UINT32 m_nNumDiginChans;
    UINT32 m_nNumSerialChans;
    UINT32 m_nNumDigoutChans;
    UINT32 m_nNumTotalChans;
    NSP_STATUS m_nNspStatus[cbMAXPROCS];                // [4]
    UINT32 m_nNumNTrodesPerInstrument[cbMAXPROCS];      // [4]
    UINT32 m_nGeminiSystem;
    APP_WORKSPACE m_icAppWorkspace[cbMAXAPPWORKSPACES]; // [10] workspace config
};
```

**Note**: This is a **C++ class** (not a plain C struct), which is fragile for cross-compiler
shared memory. The `public`/`private` labels don't affect memory layout in practice, but this
is still a portability concern.

The `APP_WORKSPACE` array at the end is only used by Central's GUI.

#### 5. cbSPKBUFF (Spike Cache)

```c
// cbhwlib.h line 944
typedef struct {
    UINT32 chid;
    UINT32 pktcnt;                                  // 400
    UINT32 pktsize;
    UINT32 head;
    UINT32 valid;
    cbPKT_SPK spkpkt[cbPKT_SPKCACHEPKTCNT];        // [400]
} cbSPKCACHE;

typedef struct {
    UINT32 flags;
    UINT32 chidmax;
    UINT32 linesize;
    UINT32 spkcount;
    cbSPKCACHE cache[cbPKT_SPKCACHELINECNT];        // [cbMAXCHANS] = [880]
} cbSPKBUFF;
```

Performance optimization: caches last 400 spikes per channel to avoid scanning the full
768 MB receive buffer.

#### 6. cbSIGNALevent (Synchronization)

- **Windows**: Named manual-reset event (`CreateEvent`/`SetEvent`/`WaitForSingleObject`)
- **POSIX**: Named semaphore (`sem_open`/`sem_post`/`sem_timedwait`)

STANDALONE signals after writing packets. CLIENTs wait on signal to wake up and read.

#### 7. System Mutex

Named mutex (`cbSharedDataMutex` + instance suffix) used by STANDALONE to claim ownership.
CLIENT checks this mutex to detect if a STANDALONE is running.

## Multi-Instrument Channel Numbering

When multiple NSPs connect within one instance, channels get expanded global numbers.

`cbGetExpandedChannelNumber()` (cbHwlibHi.cpp line 1202): sums `chancount` from preceding
instruments to offset local channel numbers into global space.

`cbGetChanInstrument()` (line 1227): reads `chaninfo[ch-1].cbpkt_header.instrument` to
determine which NSP a global channel belongs to.

## Size Summary

| Segment | Approximate Size |
|---------|-----------------|
| cbCFGBUFF | Several MB (depends on packet struct sizes) |
| cbRECBUFF | ~768 MB |
| XmtGlobal | ~290 MB |
| XmtLocal | ~116 MB |
| cbPcStatus | Few KB |
| cbSPKBUFF | Large (400 spikes * 880 channels) |
| **Total per instance** | **~1.2 GB** |

## Creation Flow

### STANDALONE (cbOpen with bStandAlone=TRUE)

1. Acquire system mutex (`cbSharedDataMutex[N]`)
2. Find first free slot in `cb_library_initialized[]` -> `nIdx`
3. Set `cb_library_index[nInstance] = nIdx`
4. Call `CreateSharedObjects(nInstance)`:
   - `CreateFileMapping()` for each segment with instance-suffixed name
   - `MapViewOfFile()` to get pointers
   - Zero-initialize all buffers
   - Set up spike cache, color tables, transmit buffer lengths
   - `CreateEvent()` for signal event

### CLIENT (cbOpen with bStandAlone=FALSE)

1. Check system mutex exists (Central must be running)
2. Find first free slot -> `nIdx`
3. `OpenFileMapping()` for each segment
4. `MapViewOfFile()` (read-only for rec/cfg, read-write for xmt/status/spk)
5. Set `cb_library_index[nInstance] = nIdx`
6. `cbMakePacketReadingBeginNow()` -- set tail to current head position

## References

- `Central-Suite/cbhwlib/cbhwlib.h` -- Structure definitions, constants
- `Central-Suite/cbhwlib/cbhwlib.cpp` -- cbOpen(), CreateSharedObjects(), buffer management
- `Central-Suite/cbhwlib/cbHwlibHi.cpp` -- High-level helpers (channel mapping, etc.)
- `cbproto/include/cbproto/cbproto.h` -- Protocol constants (cbMAXOPEN, cbNSP1, cbPKT_HEADER)
- `Central-Suite/cbmex/cbsdk.cpp` -- SDK layer (SdkApp, g_app[] array)
- `Central-Suite/CentralCommon/BmiApp.cpp` -- `--instance` command-line parsing
