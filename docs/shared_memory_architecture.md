# CereLink Shared Memory Architecture

## Overview

CereLink implements a Central-compatible shared memory architecture that enables efficient multi-process access to Cerebus data. This document describes the data flow between the device, STANDALONE process, CLIENT processes, and the shared memory segments.

## Architecture Diagram

```
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    CEREBUS DEVICE                                          │
│                              (NSP Hardware - UDP Protocol)                                 │
└────────────────────┬───────────────────────────────────┬───────────────────────────────────┘
                     │                                   │
                     │ UDP Packets                       │ UDP Packets
                     │ (Port 51002)                      │ (Port 51001)
                     ▼                                   ▲
┌───────────────────────────────────────────────────────────────────────────────────────────────┐
│                           STANDALONE PROCESS (owns device)                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────────────────┐ │
│  │                                    THREADS                                               │ │
│  │                                                                                          │ │
│  │  ┌────────────────┐      ┌────────────────┐      ┌─────────────────┐                     │ │
│  │  │  UDP Receive   │      │   UDP Send     │      │    Callback     │                     │ │
│  │  │    Thread      │      │    Thread      │      │   Dispatcher    │                     │ │
│  │  │  (cbdev)       │      │  (cbdev)       │      │    Thread       │                     │ │
│  │  └────────┬───────┘      └────────▲───────┘      └────────▲────────┘                     │ │
│  │           │                       │                       │                              │ │
│  │           │ Packets               │ Dequeue               │ Process                      │ │
│  │           │ from device           │ packets               │ callbacks                    │ │
│  │           ▼                       │                       │                              │ │
│  │  ┌────────────────────────────────┴───────────────────────┴────────┐                     │ │
│  │  │           onPacketsReceivedFromDevice()                         │                     │ │
│  │  │                                                                 │                     │ │
│  │  │   1. storePacket() → cbRECbuffer (ring buffer)                  │                     │ │
│  │  │   2. storePacket() → cbCFGbuffer (config updates)               │                     │ │
│  │  │   3. signalData() → cbSIGNALevent ◄────────────┐                │                     │ │
│  │  │   4. Enqueue to local packet_queue             │ SIGNAL!        │                     │ │
│  │  └────────────────────────────────────────────────┘                │                     │ │
│  └──────────────────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────┬────────────────────────────────────────────────────────────────────────────┘
                   │
                   │ Writes to (Producer)
                   │
    ╔══════════════▼══════════════════════════════════════════════════════════════════════╗
    ║                          SHARED MEMORY SEGMENTS                                     ║
    ║                     (Central-Compatible Architecture)                               ║
    ║                                                                                     ║
    ║  1. cbCFGbuffer                  │ Configuration database (PROCINFO, CHANINFO, etc.)║
    ║     [CentralConfigBuffer]        │ 4 instrument slots, indexed by packet.instrument ║
    ║                                  │                                                  ║
    ║  2. cbRECbuffer                  │ Receive ring buffer (~200MB)                     ║
    ║     [CentralReceiveBuffer]       │ Ring buffer with headindex/headwrap              ║
    ║     - headindex: Writer position (STANDALONE writes here)                           ║
    ║     - headwrap: Wrap counter                                                        ║
    ║     - tailindex: Reader position (CLIENT reads from here) [tracked per-client]      ║
    ║     - tailwrap: Reader wrap counter [tracked per-client]                            ║
    ║                                  │                                                  ║
    ║  3. XmtGlobal                    │ Global transmit queue (packets → device)         ║
    ║     [CentralTransmitBuffer]      │ Ring buffer for outgoing commands                ║
    ║                                  │ Both STANDALONE and CLIENT can enqueue           ║
    ║                                  │ STANDALONE's send thread dequeues and transmits  ║
    ║                                  │                                                  ║
    ║  4. XmtLocal                     │ Local transmit queue (IPC-only packets)          ║
    ║     [CentralTransmitBufferLocal] │ For inter-process communication                  ║
    ║                                  │ NOT sent to device                               ║
    ║                                  │                                                  ║
    ║  5. cbSTATUSbuffer               │ PC status information                            ║
    ║     [CentralPCStatus]            │ NSP status, channel counts, Gemini flag          ║
    ║                                  │                                                  ║
    ║  6. cbSPKbuffer                  │ Spike cache buffer (performance optimization)    ║
    ║     [CentralSpikeBuffer]         │ Last 400 spikes per channel (768 channels)       ║
    ║                                  │ Avoids scanning 200MB cbRECbuffer for recent spikes ║
    ║                                  │                                                  ║
    ║  7. cbSIGNALevent                │ Data availability signal (synchronization)       ║
    ║     [Windows: Named Event (manual-reset) | POSIX: Named Semaphore]                  ║
    ║     - STANDALONE signals when new data written                                      ║
    ║     - CLIENT waits on signal instead of polling                                     ║
    ║     - Efficient inter-process notification                                          ║
    ╚══════════════╤══════════════════════════════════════════════════════════════════════╝
                   │
                   │ Reads from (Consumer)
                   │
┌──────────────────▼─────────────────────────────────────────────────────────────────────────────┐
│                           CLIENT PROCESS (attaches to shmem)                                   │
│  ┌──────────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                                    THREADS                                               │  │
│  │                                                                                          │  │
│  │  ┌────────────────────────────────────────────────────────────────┐                      │  │
│  │  │  Shared Memory Receive Thread                                  │                      │  │
│  │  │                                                                │                      │  │
│  │  │  while (running) {                                             │                      │  │
│  │  │    // Wait for signal from STANDALONE (no polling!)            │                      │  │
│  │  │    waitForData(250ms) ◄────────────────────────────────────────┼──── WAKEUP!          │  │
│  │  │         │                                                      │      from signal     │  │
│  │  │         ▼                                                      │                      │  │
│  │  │    if (signaled) {                                             │                      │  │
│  │  │      readReceiveBuffer()                                       │                      │  │
│  │  │      // Read from cbRECbuffer (no copy to queue!)              │                      │  │
│  │  │      // tailindex → headindex                                  │                      │  │
│  │  │                                                                │                      │  │
│  │  │      // Invoke user callback DIRECTLY (no queue, no 2nd thread)│                      │  │
│  │  │      packet_callback(packets, count) ──────────────────────────┼──► User Application  │  │
│  │  │    }                                                           │                      │  │
│  │  │  }                                                             │                      │  │
│  │  └────────────────────────────────────────────────────────────────┘                      │  │
│  │                                                                                          │  │
│  │  Benefits:                                                                               │  │
│  │  • Only 1 thread (not 2) - simpler, less overhead                                        │  │
│  │  • Only 1 data copy (cbRECbuffer → callback) instead of 2                                │  │
│  │  • Reading from shmem is not time-critical (200MB buffer!)                               │  │
│  │  • User can also enqueue packets to XmtGlobal (commands to device)                       │  │
│  └──────────────────────────────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Key Data Flow Paths

### Device → STANDALONE → Shared Memory → CLIENT

1. **NSP device** sends UDP packet (port 51002)
2. **STANDALONE UDP receive thread** catches packet
3. **onPacketsReceivedFromDevice()**:
   - `storePacket(pkt)` writes to `cbRECbuffer[headindex]`
   - `headindex++` (advance writer position)
   - `signalData()` → Set `cbSIGNALevent` (wake up CLIENTs!)
4. **CLIENT shmem receive thread** wakes up from `waitForData()`
5. **CLIENT** calls `readReceiveBuffer()`:
   - Read from `cbRECbuffer[tailindex]` to `cbRECbuffer[headindex]`
   - `tailindex += packets_read` (advance reader position)
   - Parse packets from ring buffer (handle wraparound)
6. **CLIENT** enqueues packets to local `packet_queue`
7. **Callback dispatcher thread** invokes user callback

### CLIENT → Shared Memory → STANDALONE → Device

1. **Client app** calls `sendPacket(command)`
2. `enqueuePacket()` writes to `XmtGlobal` transmit queue
3. **STANDALONE send thread** dequeues from `XmtGlobal`
4. **Send thread** transmits packet via UDP to device (port 51001)

## Synchronization (cbSIGNALevent)

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
- Matches Central's `cbWaitforData()` behavior

## Ring Buffer Tracking

### Overview
- `cbRECbuffer` is a ring buffer with wrap-around capability
- ~200MB buffer size (`CENTRAL_cbRECBUFFLEN` = 768 * 65536 * 4 - 1 dwords)

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

## Process Modes

### STANDALONE Mode
- **First process** to start
- **Creates** shared memory segments
- **Owns** device connection (UDP threads)
- **Writes** to `cbRECbuffer` (producer)
- **Signals** `cbSIGNALevent` when data written
- **Dequeues** from `XmtGlobal` and transmits to device

### CLIENT Mode
- **Subsequent processes** that attach
- **Attaches** to existing shared memory
- **No device** connection (no UDP threads)
- **Reads** from `cbRECbuffer` (consumer)
- **Waits** on `cbSIGNALevent` for new data
- **Enqueues** to `XmtGlobal` to send commands (STANDALONE transmits them)

### Auto-Detection
The SDK automatically detects mode:
1. Try CLIENT mode first (attach to existing)
2. If fails, fall back to STANDALONE mode (create new)

## Thread Architecture

### STANDALONE Process Threads
1. **UDP Receive Thread** (cbdev) - Receives packets from device (MUST BE FAST!)
2. **UDP Send Thread** (cbdev) - Sends packets to device
3. **Callback Dispatcher Thread** - Decouples fast UDP receive from slow user callbacks
4. **Main Thread** - User application

**Why separate callback thread?** UDP packets arrive at high rate and OS buffer is limited. We must dequeue UDP packets quickly to avoid drops. User callbacks can be slow, so we use packet_queue to buffer between fast UDP receive and slow callback processing.

### CLIENT Process Threads (Optimized)
1. **Shared Memory Receive Thread** - Reads from cbRECbuffer AND invokes user callbacks directly
2. **Main Thread** - User application

**Why no separate callback thread?** Reading from cbRECbuffer is not time-critical (200MB buffer provides ample buffering). We can afford to invoke user callbacks directly, eliminating:
- One extra thread (simpler architecture, less overhead)
- One extra data copy (cbRECbuffer → callback, no packet_queue needed)

## Shared Memory Segments Detail

### 1. cbCFGbuffer (Configuration Database)
- **Type**: `CentralConfigBuffer`
- **Size**: ~few MB
- **Contains**:
  - System info, processor info (4 instruments)
  - Bank info, filter info, group info
  - Channel info (all 828 channels)
- **Access**: Read/write by both processes
- **Indexed by**: `packet.instrument` (1-based, cbNSP1-cbNSP4)

### 2. cbRECbuffer (Receive Ring Buffer)
- **Type**: `CentralReceiveBuffer`
- **Size**: ~200MB (768 * 65536 * 4 - 1 dwords)
- **Contains**: Incoming packets from device
- **Access**:
  - STANDALONE writes (producer)
  - CLIENT reads (consumer)
- **Ring buffer**: Wraps around, tracked by head/tail indices

### 3. XmtGlobal (Global Transmit Queue)
- **Type**: `CentralTransmitBuffer`
- **Size**: 5000 packet slots
- **Contains**: Outgoing commands to device
- **Access**:
  - Both processes enqueue
  - STANDALONE dequeues and transmits

### 4. XmtLocal (Local Transmit Queue)
- **Type**: `CentralTransmitBufferLocal`
- **Size**: 2000 packet slots
- **Contains**: Inter-process communication packets
- **Access**: Local IPC only, NOT sent to device

### 5. cbSTATUSbuffer (PC Status)
- **Type**: `CentralPCStatus`
- **Size**: ~few KB
- **Contains**:
  - NSP status per instrument
  - Channel counts
  - Gemini system flag
- **Access**: Read/write by both processes

### 6. cbSPKbuffer (Spike Cache)
- **Type**: `CentralSpikeBuffer`
- **Size**: ~few MB
- **Contains**: Last 400 spikes per channel (768 channels)
- **Purpose**: Performance optimization - avoid scanning 200MB cbRECbuffer
- **Access**: Written by STANDALONE, read by both

### 7. cbSIGNALevent (Synchronization)
- **Type**: Named Event (Windows) / Named Semaphore (POSIX)
- **Purpose**: Efficient inter-process notification
- **Operations**:
  - `signalData()` - Producer notifies consumers
  - `waitForData()` - Consumer waits for notification
  - `resetSignal()` - Clear pending signals

## Benefits of This Architecture

1. **Efficient Multi-Process Access**: Multiple applications can read Cerebus data simultaneously
2. **Zero Polling Overhead**: CLIENT processes sleep until signaled (saves CPU)
3. **Central Compatibility**: External tools (Raster, Oscilloscope) work seamlessly
4. **Independent Read Positions**: Each CLIENT tracks its own position in ring buffer
5. **Bi-Directional Communication**: Both processes can send commands to device
6. **Robust Overrun Handling**: Detects and recovers from buffer overruns
7. **Optimized CLIENT Mode**: Single thread, single data copy (cbRECbuffer → callback)
8. **Smart Thread Architecture**: Fast UDP receive decoupled in STANDALONE, simplified in CLIENT

## Implementation Status

- ✅ All 7 shared memory segments implemented
- ✅ cbSIGNALevent synchronization working
- ✅ Ring buffer reading logic complete
- ✅ CLIENT mode shared memory receive thread implemented
- ✅ STANDALONE mode signaling to CLIENT processes
- ✅ Thread lifecycle management (start/stop)
- ✅ Optimized CLIENT mode (1 thread, 1 data copy)
- ✅ All unit tests passing (18 cbshm tests + 28 SDK tests)

## Code Locations

- **Shared Memory**: `src/cbshm/`
  - `include/cbshm/shmem_session.h` - Public API
  - `src/shmem_session.cpp` - Implementation
  - `include/cbshm/central_types.h` - Buffer structures

- **SDK Integration**: `src/cbsdk_v2/`
  - `src/sdk_session.cpp` - High-level SDK using shared memory
  - Threads, callbacks, packet routing

- **Device Layer**: `src/cbdev/`
  - UDP communication with NSP hardware
  - Used only in STANDALONE mode

## References

- Central Suite architecture (upstream)
- Cerebus Protocol Specification
- cbhwlib.h (upstream reference implementation)
