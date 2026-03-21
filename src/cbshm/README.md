# cbshm - Shared Memory Management

Internal C++ library that manages shared memory segments for inter-process communication
between STANDALONE and CLIENT sessions.

## Responsibilities

- **Three layout modes:** `NATIVE` (CereLink-to-CereLink), `CENTRAL_COMPAT` (attach to
  Central's shared memory), `CENTRAL` (Central-compatible layout created by CereLink)
- **Consistent packet indexing:** Always uses `packet.instrument` as the array index,
  regardless of mode
- **Ring buffer I/O:** Write packets to the receive buffer (STANDALONE), read packets
  from it (CLIENT), with protocol-aware parsing and translation in compat mode
- **Instrument filtering:** In `CENTRAL_COMPAT` mode, filters packets from Central's
  shared receive buffer by instrument index
- **Platform-specific implementations:** Windows (`CreateFileMapping`) and POSIX
  (`shm_open`) shared memory, plus platform-specific signaling

## Key Types

| Type | Purpose |
|------|---------|
| `ShmemSession` | Main API — create/attach segments, read/write buffers, access config |
| `ShmemLayout` | Enum: `CENTRAL`, `CENTRAL_COMPAT`, `NATIVE` |
| `NativeConfigBuffer` | Single-instrument config (284 channels, scalar arrays) |
| `CentralLegacyCFGBUFF` | Matches Central's exact binary layout for compat mode |
| `cbConfigBuffer` | CereLink's own multi-instrument config layout |

## Key Design Notes

- **Central vs CereLink constants:** Central uses `cbMAXPROCS=4`, `cbNUM_FE_CHANS=768`.
  CereLink uses `cbMAXPROCS=1`, `cbNUM_FE_CHANS=256`. The compat types use Central's
  constants; native types use CereLink's.
- **Not exposed to public API:** cbsdk orchestrates cbshm; users don't interact with it
  directly.

## References

- [Shared memory architecture](../../docs/shared_memory_architecture.md)
- [Central's shared memory layout](../../docs/central_shared_memory_layout.md)
