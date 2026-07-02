# Client Mode Sequence Diagrams

Sequence diagrams for the workflow between CereLink clients and cbsdk, for each of the
three client modes, and the differences in Central-compatible client mode when attached
to older Central versions.

Reflects the code as of PR #190 (multi-version Central compatibility). Key sources:
`src/cbsdk/src/sdk_session.cpp`, `src/cbshm/src/shmem_session.cpp`,
`src/cbshm/src/central_version.cpp`, `src/cbshm/include/cbshm/central_adapters/*`.

**See also**: [Shared memory architecture](shared_memory_architecture.md),
[Multi-version Central compatibility](multi_version_central_compat/README.md).

## Mode selection (common entry point)

Every client — C++ or pycbsdk (`Session()` → `cbsdk_session_create()` →
`SdkSession::create()`) — goes through the same three-way probe:

```mermaid
sequenceDiagram
    participant App as Client app / pycbsdk
    participant SDK as SdkSession::create()
    participant SHM as cbshm::ShmemSession

    App->>SDK: create(config{device_type=HUB1, ...})
    Note over SDK: instrument index from GEMSTART==2 map<br/>(Hub1→0, Hub2→1, Hub3→2, NSP→3, legacy→0)
    SDK->>SHM: create(CLIENT, CENTRAL, instance="", inst)
    alt Central.exe running & its segments open
        SHM-->>SDK: ok → CENTRAL-COMPAT CLIENT mode
    else fails (no Central / non-Windows / wrong version)
        SDK->>SHM: create(CLIENT, NATIVE, "hub1", inst 0)
        alt cbshm_hub1_* segments exist AND owner alive
            Note over SHM: isOwnerAlive(): kill(owner_pid, 0)<br/>rejects stale segments
            SHM-->>SDK: ok → NATIVE CLIENT mode
        else fails or stale
            SDK->>SHM: create(STANDALONE, NATIVE, "hub1", inst 0)
            SHM-->>SDK: creates 7 segments → NATIVE STANDALONE mode
        end
    end
```

Note that the first attempt does not merely test whether Central's shared memory exists:
`ShmemSession::Impl::open()` calls `detectCentralVersion()` first, which requires finding
a running `Central.exe` process (Windows-only). If Central crashed leaving segments
behind, or on any POSIX platform, attempt 1 fails before any segment is touched.

## Mode 1 — NATIVE STANDALONE (owns the device)

```mermaid
sequenceDiagram
    participant App as Client app
    participant SDK as SdkSession
    participant Q as SPSC queue +<br/>callback thread
    participant SHM as ShmemSession<br/>(NATIVE, writer)
    participant DEV as cbdev DeviceSession<br/>(recv + send threads)
    participant NSP as Device (UDP)

    rect rgba(88,110,255,0.15)
    Note over App,NSP: Startup
    App->>SDK: create()
    SDK->>SHM: create(STANDALONE, NATIVE, "hub1")
    Note over SHM: shm_unlink stale → create 7 segments<br/>init cfg (owner_pid), xmt rings, status, spike cache
    SDK->>DEV: createDeviceSession(params)
    Note over DEV: ProtocolDetector probes device →<br/>wraps in DeviceSession_311/400/410 if old protocol
    SDK->>SDK: start(): spawn callback thread,<br/>register recv + datagram callbacks,<br/>DEV.startReceiveThread(), spawn send thread
    SDK->>DEV: handshake (autorun ? performStartupHandshake : requestConfiguration)
    DEV->>NSP: SYSSET/config request
    NSP-->>DEV: SYSREP + config dump (PROCREP, CHANREP...)
    SDK->>DEV: sendClockProbe() (initial)
    end

    rect rgba(46,180,80,0.15)
    Note over App,NSP: Steady state — receive path
    NSP-->>DEV: UDP datagram (device-native protocol)
    Note over DEV: versioned wrapper translates every packet<br/>→ CURRENT format before anyone sees it
    loop each packet in datagram
        DEV->>SDK: receive callback(pkt)
        SDK->>SHM: storePacket → writeToReceiveBuffer<br/>(wrap-marker padding, release-store head)
        SDK->>SHM: mirror config reps: setProcInfo / setSysInfo /<br/>setGroupInfo / setChanInfo (+CMP overlay)
        SDK->>Q: push(pkt)
    end
    DEV->>SDK: datagram-complete callback
    SDK->>DEV: sendClockProbe() (every ~100 ms)
    SDK->>SHM: setClockSync(offset_ns, uncertainty)
    SDK->>SHM: signalData() — sem_post / SetEvent (wakes CLIENTs)
    SDK->>Q: notify callback thread
    Q->>App: dispatchBatch: group-batch callbacks,<br/>then per-packet (event/group/config) callbacks
    end

    rect rgba(255,150,40,0.15)
    Note over App,NSP: Steady state — send path
    App->>SDK: sendPacket(cmd)
    SDK->>SHM: enqueuePacket (time stamped ns, CURRENT format → xmt ring)
    Note over SDK: send thread polls xmt ring
    SDK->>SHM: dequeuePacket
    SDK->>DEV: sendPacket (wrapper translates to device protocol)
    DEV->>NSP: UDP (paced: sleep every 8 packets)
    end
```

Four threads total: cbdev UDP receive, cbdev-driven callbacks feeding the SPSC queue,
the SDK callback dispatcher, and the SDK send thread (plus main).

## Mode 2 — NATIVE CLIENT (attaches to a CereLink STANDALONE)

```mermaid
sequenceDiagram
    participant App as Client app
    participant SDK as SdkSession
    participant SHM as ShmemSession<br/>(NATIVE, reader)
    participant SA as STANDALONE process
    participant NSP as Device

    rect rgba(88,110,255,0.15)
    Note over App,SA: Startup
    App->>SDK: create()
    SDK->>SHM: create(CLIENT, NATIVE, "hub1")
    Note over SHM: attach cbshm_hub1_* (rec/status/spk read-only,<br/>cfg + xmt read-write), tail synced to current head
    SDK->>SHM: isOwnerAlive()? (kill(owner_pid,0))
    SDK->>SDK: start(): spawn ONE shmem-receive thread
    SDK->>SHM: rebuildChannelTypeCache() from shmem chaninfo
    end

    rect rgba(46,180,80,0.15)
    Note over App,NSP: Steady state — receive
    NSP-->>SA: UDP → translated to CURRENT → rec ring
    SA-->>SHM: signalData()
    SHM-->>SDK: waitForData(250ms) returns true
    loop drain ring until empty (batches of 128)
        SDK->>SHM: readReceiveBuffer()
        Note over SHM: CURRENT format already — no translation,<br/>no instrument filtering (single-device ring),<br/>skip chid=0/type=0 wrap markers
        SDK->>SDK: scan batch: SYSREP → runlevel,<br/>NPLAYREP → complete client clock probe
        SDK->>App: dispatchBatch → user callbacks (same thread)
    end
    end

    rect rgba(255,150,40,0.15)
    Note over App,NSP: Send + client clock sync
    App->>SDK: sendPacket(cmd)
    SDK->>SHM: enqueuePacket → cbshm_hub1_xmt
    SA->>SHM: dequeuePacket (its send thread)
    SA->>NSP: UDP
    Note over SDK: every ~2 s: enqueue NPLAYSET probe the same way —<br/>NPLAYREP returns via rec ring → ClockSync sample
    end
```

No callback thread — the ~256 MB ring absorbs bursts, so callbacks run directly on the
shmem-receive thread.

## Mode 3 — CENTRAL-COMPAT CLIENT, latest Central (7.8 / protocol 4.2)

This is the baseline that the versioned adapter machinery runs through even when no
translation is needed:

```mermaid
sequenceDiagram
    participant App as Client app
    participant SDK as SdkSession
    participant SHM as ShmemSession<br/>(CENTRAL layout)
    participant VER as central_version.cpp
    participant ADP as central::Adapter (v7_8)
    participant CTL as Central.exe
    participant NSP as Devices (up to 4)

    rect rgba(88,110,255,0.15)
    Note over App,CTL: Startup
    App->>SDK: create(HUB1) — instrument idx 0 (GEMSTART==2)
    SDK->>SHM: create(CLIENT, CENTRAL, instance="", inst)
    SHM->>VER: detectCentralVersion()
    VER->>CTL: enumerate processes → find Central.exe →<br/>read VersionInfo.ProductVersion
    VER-->>SHM: "7.8.x" → CentralVersion::CURRENT → protocol 4.2
    Note over SHM: BootstrapAdapter(CURRENT) supplies buffer sizes
    SHM->>CTL: OpenFileMapping: cbCFGbuffer, cbRECbuffer,<br/>XmtGlobal, XmtLocal, cbSTATUSbuffer, cbSPKbuffer, cbSIGNALevent
    SHM->>ADP: construct with raw pointers into each segment
    Note over SHM: tail ← head (only NEW packets)
    SDK->>SDK: start(): shmem-receive thread,<br/>rebuildChannelTypeCache via adapter
    end

    rect rgba(46,180,80,0.15)
    Note over App,NSP: Steady state — receive
    NSP-->>CTL: UDP (all instruments) → Central writes RAW device<br/>packets into ONE shared cbRECbuffer, SetEvent(~100/s)
    CTL-->>SDK: waitForData wakes
    loop drain ring
        SDK->>SHM: readReceiveBuffer()
        Note over SHM: protocol == CURRENT → parse 16-byte header,<br/>copy packet as-is (no translation)<br/>non-Gemini system: header.time ticks→ns via sysfreq<br/>then FILTER: instrument != 0 → discard (after copy)
        SDK->>App: dispatchBatch → callbacks
    end
    end

    rect rgba(255,150,40,0.15)
    Note over App,NSP: Config access & send
    App->>SDK: getChanInfo(n)
    SDK->>SHM: getChanInfo → layout==CENTRAL
    SHM->>ADP: getChanInfo(buf, idx)
    Note over ADP: translate Central's cbPKT_CHANINFO struct →<br/>cbproto type (7.8: field-for-field copy)
    App->>SDK: sendPacket(cmd)
    SDK->>SHM: enqueuePacket (ns→ticks if non-Gemini) → XmtGlobal
    CTL->>NSP: Central's own loop drains XmtGlobal (4 pkts/10 ms)
    end
```

Key contrasts with native client mode: the ring holds **raw device packets from all
instruments** (filtering discards other instruments' packets *after* fully
copying/translating them), config reads go through a struct-translating adapter instead
of direct pointer reads, and instrument status is assumed always-active / read-only.

## How the flow changes with older Central versions

The skeleton above is identical for 7.0–7.7; the deltas are confined to three seams —
version resolution at open, the adapter behind every config accessor, and per-packet
translation in the ring buffer paths:

```mermaid
sequenceDiagram
    participant SDK as SdkSession
    participant SHM as ShmemSession
    participant ADP as v7_x Adapter
    participant PT as cbproto::PacketTranslator
    participant CTL as Central (old)

    Note over SHM,CTL: Open: version → adapter selection
    SHM->>CTL: read Central.exe ProductVersion
    alt 7.7 / 7.6 → protocol 4.1
        SHM->>ADP: v7_7 / v7_6 BootstrapAdapter + Adapter
    else 7.5 → protocol 4.0
        SHM->>ADP: v7_5 (16-byte header, reordered fields, 8-bit type)
    else 7.0 → protocol 3.11
        SHM->>ADP: v7_0 (8-byte header, uint32 tick timestamps)
    else major < 7
        SHM-->>SDK: error "update Central" → SDK falls back to NATIVE modes
    end
    Note over SHM,ADP: BootstrapAdapter returns VERSION-SPECIFIC buffer sizes<br/>(segment mapping would be misaligned with wrong sizes)

    Note over SDK,PT: Receive: readReceiveBuffer (per packet)
    SDK->>SHM: readReceiveBuffer()
    alt protocol 3.11
        SHM->>SHM: parse 8-byte HEADER_311, copy raw to temp buf
        SHM->>SHM: header→current: time = ticks·1e9/30000,<br/>type u8→u16, instrument := 0
        SHM->>PT: translatePayload_311_to_current<br/>(NPLAY, COMMENT, CHANINFO, SYSPROTOCOLMONITOR resize)
    else protocol 4.0
        SHM->>SHM: parse HEADER_400, copy raw to temp buf
        SHM->>SHM: header→current (field reorder, type widened)
        SHM->>PT: translatePayload_400_to_current
    else protocol 4.1
        SHM->>SHM: header identical — copy in place
        SHM->>PT: translatePayload_410_to_current (payload-only diffs)
    else protocol 4.2 (Central 7.8)
        SHM->>SHM: straight copy, no PacketTranslator
    end
    Note over SHM: then (4.0+, non-Gemini): time ticks→ns via sysfreq<br/>then instrument filter — same as current

    Note over SDK,CTL: Transmit: enqueuePacket (reverse translation)
    SDK->>SHM: enqueuePacket(current-format pkt, time=ns)
    alt 3.11
        SHM->>PT: current→311 header (ns→30 kHz ticks) + payload
    else 4.0 / 4.1
        SHM->>PT: current→400/410 (+ ns→ticks when non-Gemini)
    end
    SHM->>CTL: write legacy-format packet into XmtGlobal ring

    Note over SDK,ADP: Config: every get/set goes through the versioned adapter
    SDK->>SHM: getProcInfo / getChanInfo / getSpikeCache / getPcStatus ...
    SHM->>ADP: v7_x translates that version's struct layout ↔ cbproto types<br/>(array-size clamping/zero-fill via copyArr helpers)
```

### Version-to-behavior summary

| Central app version | Protocol | Header in `cbRECbuffer` | Packet translation | Config/status/spike access |
|---|---|---|---|---|
| 7.8, and any unrecognized 7.x minor | 4.2 (CURRENT) | 16 B, ns timestamps | none | `central` (v7_8) adapter, ~identity copy |
| 7.7, 7.6 | 4.1 | 16 B, identical to current | payload-only (4 packet types) | v7_7 / v7_6 adapters |
| 7.5 | 4.0 | 16 B, reordered, u8 type | header + payload both ways | v7_5 adapter |
| 7.0 | 3.11 | 8 B, u32 ticks @30 kHz | header + payload + timestamp unit conversion; instrument forced to 0 | v7_0 adapter |
| major < 7 | — | — | attach refused → SDK falls back to native modes | — |
