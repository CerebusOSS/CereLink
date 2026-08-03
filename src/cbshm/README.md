# cbshm - Shared Memory Management

Internal C++ library that manages shared memory segments for inter-process communication
between STANDALONE and CLIENT sessions.

## Responsibilities

- **Two layout modes:** `NATIVE` (CereLink-to-CereLink), `CENTRAL` (attach to
  Central's shared memory)
- **Consistent packet indexing:** Always uses `packet.instrument` as the array index,
  regardless of mode
- **Ring buffer I/O:** Write packets to the receive buffer (STANDALONE), read packets
  from it (CLIENT), with protocol-aware parsing and translation in compat mode
- **Instrument filtering:** In `CENTRAL` mode, filters packets from Central's
  shared receive buffer by instrument index
- **Platform-specific implementations:** Windows (`CreateFileMapping`) and POSIX
  (`shm_open`) shared memory, plus platform-specific signaling

## Key Types

| Type | Purpose |
|------|---------|
| `ShmemSession` | Main API — create/attach segments, read/write buffers, access config |
| `ShmemLayout` | Enum: `CENTRAL`, `NATIVE` |
| `NativeConfigBuffer` | Single-instrument config (284 channels, scalar arrays) |
| `cbCFGBUFF` | Matches Central's exact binary layout for compat mode |

## Key Design Notes

- **Central vs CereLink constants:** Central uses `cbMAXPROCS=4`, `cbNUM_FE_CHANS=768` (in v7.8.0).
  CereLink uses `cbMAXPROCS=1`, `cbNUM_FE_CHANS=256`. The compat types use Central's
  constants; native types use CereLink's.
- **Not exposed to public API:** cbsdk orchestrates cbshm; users don't interact with it
  directly.

## References

- [Shared memory architecture](../../docs/shared_memory_architecture.md)
- [Central's shared memory layout](../../docs/central_shared_memory_layout.md)
