# Gemini Multi-Device Example

This example demonstrates how to use the new cbsdk_v2 API to connect to multiple Gemini devices simultaneously.

## Features

- **Simplified Configuration**: Uses device type enums instead of manual IP/port configuration
- **Automatic Client Address Detection**: Platform-aware client address selection
- **Multi-Device Support**: Connects to both Gemini NSP and Hub1
- **Packet Callbacks**: Real-time packet processing
- **Statistics Monitoring**: Track packets, queue depth, and errors

## Building

```bash
cd build
cmake ..
make gemini_example
```

## Running

Simply run the executable - no command line arguments needed:

```bash
./examples/gemini_example
```

The example will:
1. Create sessions for both Gemini NSP (192.168.137.128:51001) and Hub1 (192.168.137.200:51002)
2. Auto-detect the appropriate client address based on your platform:
   - **macOS**: Binds to 0.0.0.0 (all interfaces)
   - **Linux**: Searches for 192.168.137.x adapter, falls back to 0.0.0.0
   - **Windows**: Binds to 0.0.0.0 (safe default)
3. Start receiving packets from both devices
4. Display the first few packets from each device
5. Print statistics every 5 seconds
6. Run until you press Ctrl+C

## Example Output

```
===========================================
  Gemini Multi-Device Example (cbsdk_v2)
===========================================

Configuring Gemini NSP...
Configuring Gemini Hub1...

Creating NSP session...
Creating Hub1 session...

Setting up packet callbacks...

Starting NSP session...
Starting Hub1 session...

=== Both devices connected! ===
Press Ctrl+C to stop...

[NSP] Packet type: 0x0001, dlen: 32
[NSP] Packet type: 0x0010, dlen: 64
[Hub1] Packet type: 0x0001, dlen: 32
[Hub1] Packet type: 0x0020, dlen: 128

=== Packet Counts (via callbacks) ===
NSP:  1234 packets
Hub1: 567 packets

=== Gemini NSP Statistics ===
Packets received:  1234
Packets in shmem:  1234
Packets queued:    1234
Packets delivered: 1234
Packets dropped:   0
Queue depth:       45 / 128 (current/peak)
Errors (shmem):    0
Errors (receive):  0

=== Gemini Hub1 Statistics ===
Packets received:  567
Packets in shmem:  567
Packets queued:    567
Packets delivered: 567
Packets dropped:   0
Queue depth:       23 / 89 (current/peak)
Errors (shmem):    0
Errors (receive):  0
```

## Key Code Sections

### Simple Device Configuration

```cpp
// Gemini NSP - just specify the device type!
cbsdk::SdkConfig nsp_config;
nsp_config.device_type = cbsdk::DeviceType::GEMINI_NSP;
nsp_config.shmem_name = "gemini_nsp";

// Gemini Hub1
cbsdk::SdkConfig hub1_config;
hub1_config.device_type = cbsdk::DeviceType::GEMINI_HUB1;
hub1_config.shmem_name = "gemini_hub1";
```

No need to specify IP addresses, ports, or client addresses - they're all auto-configured!

### Packet Callbacks

```cpp
nsp_session.setPacketCallback([&counter](const cbPKT_GENERIC* pkts, size_t count) {
    counter.fetch_add(count);
    // Process packets...
});
```

### Error Handling

```cpp
nsp_session.setErrorCallback([](const std::string& error) {
    std::cerr << "[NSP ERROR] " << error << "\n";
});
```

## Advanced: Custom Configuration

If you need non-standard network configuration, you can override the defaults:

```cpp
cbsdk::SdkConfig config;
config.device_type = cbsdk::DeviceType::GEMINI_NSP;  // Start with defaults

// Override specific values
config.custom_device_address = "192.168.1.100";  // Custom device IP
config.custom_client_address = "192.168.1.50";   // Custom client IP
config.custom_device_port = 52001;               // Custom port
```

## Device Types

Available device types:
- `LEGACY_NSP` - Legacy NSP (192.168.137.128, ports 51001/51002)
- `GEMINI_NSP` - Gemini NSP (192.168.137.128, port 51001)
- `GEMINI_HUB1` - Gemini Hub 1 (192.168.137.200, port 51002)
- `GEMINI_HUB2` - Gemini Hub 2 (192.168.137.201, port 51003)
- `GEMINI_HUB3` - Gemini Hub 3 (192.168.137.202, port 51004)
- `NPLAY` - NPlay loopback (127.0.0.1, port 51001)

## Troubleshooting

**No packets received:**
- Ensure devices are powered on and network cables connected
- Check that you're on the 192.168.137.x subnet
- Verify firewall isn't blocking UDP ports 51001-51004

**Permission errors:**
- On Linux, you may need `sudo` for raw socket access
- On macOS, check System Preferences → Security & Privacy

**Build errors:**
- Ensure you built cbsdk_v2: `make cbsdk_v2`
- Check that CMake configuration completed successfully
