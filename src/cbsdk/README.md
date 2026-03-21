# cbsdk - SDK Public API

Public C API (with internal C++ implementation) that orchestrates cbdev + cbshm into a
single session interface.

## Responsibilities

- **Session lifecycle:** Create, start, stop, destroy
- **Mode auto-detection:** CENTRAL_COMPAT CLIENT → Native CLIENT → Native STANDALONE
  (three-way fallback)
- **Device handshake:** Protocol detection, configuration request, run-level control
- **Callback system:** Per-type callbacks (event, group, group batch, config, packet)
  dispatched on a dedicated thread via lock-free SPSC queue
- **Configuration access:** Channel info, group info, filters, labels, positions, scaling
- **Commands:** Comments, digital output, analog output monitor, recording control, CCF
  load/save, clock synchronization

## Architecture

```
User code
  ↓ C API (cbsdk.h)
SdkSession (C++)
  ├── cbdev::IDeviceSession  (UDP transport, STANDALONE only)
  └── cbshm::ShmemSession    (shared memory, all modes)
```

### Thread Model

**STANDALONE** (3 threads):
1. UDP receive (cbdev) → shmem store + SPSC queue push
2. Callback dispatcher → drain queue, invoke user callbacks
3. UDP send (cbdev) → drain transmit buffer

**CLIENT** (1 thread):
1. Shmem receive → read ring buffer, invoke user callbacks directly

## Usage (C)

```c
#include <cbsdk/cbsdk.h>

cbsdk_config_t cfg = cbsdk_config_default();
cfg.device_type = CBPROTO_DEVICE_TYPE_HUB1;

cbsdk_session_t session;
cbsdk_session_create(&session, &cfg);
cbsdk_session_start(session);

// Register callbacks, query config, send commands ...

cbsdk_session_stop(session);
cbsdk_session_destroy(session);
```

## References

- [Shared memory architecture](../../docs/shared_memory_architecture.md)
- Python wrapper: [pycbsdk](../../pycbsdk/)
