# cbdev - Device Transport Layer

Low-level C++ library for UDP communication with Cerebus neural recording devices.

## Scope

- UDP socket management (send/receive) with platform-specific handling (Windows/macOS/Linux)
- Automatic protocol version detection and packet translation (3.11, 4.0, 4.1, current)
- Device handshake, configuration request, and run-level control
- Receive thread with callback registration
- Clock synchronization (probe-based offset estimation)

cbdev does **not** handle shared memory (see `cbshm`) or high-level data management (see `cbsdk`).

## Public API

### Headers

| Header             | Contents                                                                                                |
|--------------------|---------------------------------------------------------------------------------------------------------|
| `connection.h`     | `ConnectionParams`, `DeviceType`, `ProtocolVersion`, `ChannelType`, `DeviceRate` enums, factory helpers |
| `device_session.h` | `IDeviceSession` interface, callback types                                                              |
| `device_factory.h` | `createDeviceSession()` factory function                                                                |
| `result.h`         | `Result<T>` error-handling type                                                                         |

### Usage

```cpp
#include <cbdev/device_factory.h>

// Create session for a specific device type (auto-detects protocol)
auto params = cbdev::ConnectionParams::forDevice(cbdev::DeviceType::HUB1);
auto result = cbdev::createDeviceSession(params);
if (result.isError()) { /* handle error */ }
auto device = std::move(result.value());

// Register callback and start receiving
auto handle = device->registerReceiveCallback([](const cbPKT_GENERIC& pkt) {
    // Called on receive thread -- keep fast
});
device->startReceiveThread();

// Synchronous handshake (brings device to RUNNING)
device->performHandshakeSync(std::chrono::milliseconds(2000));

// Send packets, query config, etc.
const auto& chanInfo = *device->getChanInfo(1);
device->sendPacket(myPacket);

// Clock sync
device->sendClockProbe();
auto offset = device->getOffsetNs();  // device_ns - host_ns

// Cleanup
device->stopReceiveThread();
device->unregisterCallback(handle);
```

## Supported Devices

| Device     | Address         | Protocol               |
|------------|-----------------|------------------------|
| Legacy NSP | 192.168.137.128 | 3.11 (auto-translated) |
| Gemini NSP | 192.168.137.128 | 4.x                    |
| Hub1       | 192.168.137.200 | 4.x                    |
| Hub2       | 192.168.137.201 | 4.x                    |
| Hub3       | 192.168.137.202 | 4.x                    |
| nPlay      | 127.0.0.1       | 3.11 or 4.x            |

## Architecture

```
createDeviceSession(params, version)
        │
        ├─ CURRENT ──► DeviceSession (direct UDP)
        ├─ 4.10 ────► DeviceSession_410 → DeviceSession
        ├─ 4.00 ────► DeviceSession_400 → DeviceSession
        ├─ 3.11 ────► DeviceSession_311 → DeviceSession
        └─ UNKNOWN ─► ProtocolDetector → (one of the above)
```

Protocol wrappers (`DeviceSession_*`) extend `DeviceSessionWrapper`, overriding only `receivePackets()` and `sendPacket()` to translate between the device's wire format and the current protocol used internally.
