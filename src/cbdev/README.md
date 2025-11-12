# cbdev - Device Transport Layer

**Status:** Phase 3 - ✅ **COMPLETE** (2025-11-11)

## Purpose

Internal C++ library that handles all device communication via UDP sockets.

## Core Functionality

1. **Socket Management**
   - Create/bind/connect UDP sockets
   - Platform-specific socket options (Windows/POSIX)
   - **macOS multi-interface fix included!** (IP_BOUND_IF)

2. **Packet Send/Receive**
   - `sendPacket()` / `sendPackets()` - transmit to device
   - `pollPacket()` - synchronous receive with timeout
   - `startReceiveThread()` - asynchronous receive with callbacks

3. **Device Configuration**
   - Default addresses for Legacy NSP, Gemini NSP, Gemini Hub1/2/3, NPlay
   - Default client address: 192.168.137.199
   - Legacy NSP: different ports (recv=51001, send=51002)
   - Gemini devices: same port for both send & recv
   - NPlay: loopback (127.0.0.1) for both device and client

4. **Statistics & Monitoring**
   - Packet counters (sent/received)
   - Byte counters (sent/received)
   - Error tracking (send_errors, recv_errors)
   - Connection health status

## Key Design Decisions

- **C++ only, internal use:** Not exposed to public API
- **Uses upstream protocol:** Includes cbproto/cbproto.h directly for packet types
- **Callback-based receive:** Flexible for both sync and async use
- **Platform-aware:** Includes macOS IP_BOUND_IF handling
- **Result<T> pattern:** Consistent error handling (no exceptions)
- **Thread-safe statistics:** Mutex-protected counters

## Current Status

- [x] Directory structure created
- [x] CMake integration added (STATIC library, 599KB)
- [x] DeviceSession class designed and implemented
- [x] UDP socket implementation (Windows/POSIX)
- [x] Device address defaults configured
- [x] Callback system implemented
- [x] Statistics tracking added
- [x] 22 unit tests written and passing

## API Preview

```cpp
namespace cbdev {
    struct DeviceConfig {
        std::string address;
        uint16_t recvPort;
        uint16_t sendPort;
    };

    using PacketCallback = std::function<void(const cbPKT_GENERIC*, size_t)>;

    class DeviceSession {
    public:
        Result open(const DeviceConfig& config);
        void close();

        Result sendPacket(const cbPKT_GENERIC& pkt);
        void setPacketCallback(PacketCallback callback);
        Result startReceiveThread();

        bool isConnected() const;
        Stats getStats() const;
    };
}
```

## Implementation Notes

### macOS Multi-Interface Handling

This module will incorporate the macOS networking fix developed earlier:
- Use `0.0.0.0` as client IP when multiple interfaces active
- Skip `SO_DONTROUTE` when binding to specific IP
- Optional `IP_BOUND_IF` for interface binding

See: `src/central/UDPsocket.cpp` lines 154-161, 280-295

## References

- Current UDP code: `src/central/UDPsocket.cpp`, `src/central/Instrument.cpp`
- macOS fix: `README.md` (Platform-Specific Networking Notes)
