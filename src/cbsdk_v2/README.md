# cbsdk_v2 - New SDK Public API

**Status:** Phase 4 - Not Started

## Purpose

Public C API that orchestrates cbdev + cbshmem to provide a clean, stable interface for users.

**Key Goal:** Hide all multi-instrument and indexing complexity from users!

## Core Functionality

1. **Session Management**
   - `cbSdkOpen_v2()` / `cbSdkClose_v2()`
   - Automatic mode detection (standalone vs. client)
   - Device name resolution ("Hub1" → IP address)

2. **Orchestration**
   - Manages lifecycle of cbshmem + cbdev
   - Routes packets: device → shmem → user callbacks
   - Handles mode-specific logic internally

3. **Configuration API**
   - `cbSdkGetProcInfo_v2()` - uses first active instrument
   - `cbSdkGetChanInfo_v2()` / `cbSdkSetChanInfo_v2()`
   - All config operations abstracted from multi-instrument complexity

4. **Callback System**
   - Register user callbacks for packets
   - Thread-safe callback invocation
   - Flexible callback management

## Key Design Decisions

- **C API:** Public interface is pure C for ABI stability
- **C++ Implementation:** Internal SdkSession class in C++
- **Hide Complexity:** Users never see InstrumentId, indexing, or mode details
- **Stable API:** Can evolve cbshmem/cbdev without breaking users

## Current Status

- [x] Directory structure created
- [x] CMake integration added (placeholder)
- [ ] SdkSession class (C++) designed
- [ ] C API wrappers implemented
- [ ] Mode detection logic implemented
- [ ] Packet routing implemented
- [ ] Callback system implemented
- [ ] Device name resolution implemented
- [ ] Integration tests written

## API Preview

### C API (Public)

```c
// cbsdk_v2.h
typedef uint32_t cbSdkHandle;

typedef struct {
    cbSdkConnectionType conType;  // DEFAULT, CENTRAL, STANDALONE
    const char* deviceAddress;    // Optional: override default
    uint16_t deviceRecvPort;      // Optional: override default
    uint16_t deviceSendPort;      // Optional: override default
} cbSdkConnectionInfo;

// Open session
cbSdkResult cbSdkOpen_v2(
    uint32_t instance,
    const cbSdkConnectionInfo* pConnection,
    cbSdkHandle* pHandle
);

// Close session
cbSdkResult cbSdkClose_v2(cbSdkHandle handle);

// Get config (simplified - no instrument IDs!)
cbSdkResult cbSdkGetProcInfo_v2(
    cbSdkHandle handle,
    cbPROCINFO* pInfo
);

cbSdkResult cbSdkGetChanInfo_v2(
    cbSdkHandle handle,
    uint16_t channel,
    cbCHANINFO* pInfo
);
```

### C++ Implementation (Internal)

```cpp
// sdk_session.cpp
class SdkSession {
public:
    cbSdkResult open(const cbSdkConnectionInfo* pConn) {
        // 1. Determine mode (standalone vs. client)
        // 2. Open cbshmem
        // 3. If standalone: open cbdev and start receive thread
        // 4. Register packet callback: cbdev → cbshmem → user
    }

    cbSdkResult getProcInfo(cbPROCINFO* pInfo) {
        // Uses cbshmem::getFirstProcInfo()
        // User never knows about multi-instrument complexity!
    }

private:
    cbshmem::ShmemSession m_shmem;
    cbdev::DeviceSession m_device;
};
```

## Migration from Old API

Users will transition from:
```c
cbSdkOpen(INST, conType, con);  // Old API
```

To:
```c
cbSdkOpen_v2(instance, &conInfo, &handle);  // New API
```

During Phase 5, we'll provide migration guide and compatibility shims.

## References

- Design document: `docs/refactor_plan.md` (Phase 4)
- Current SDK: `src/cbsdk/cbsdk.cpp`, `include/cerelink/cbsdk.h`
