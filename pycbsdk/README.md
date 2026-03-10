# pycbsdk

Python bindings for the [CereLink](https://github.com/CerebusOSS/CereLink) SDK,
providing real-time access to Blackrock Neurotech Cerebus neural signal processors.

Built on [cffi](https://cffi.readthedocs.io/) (ABI mode) — no compiler needed at
install time.

## Installation

```bash
pip install pycbsdk

# With numpy support (zero-copy array access for continuous data)
pip install pycbsdk[numpy]
```

## Quick Start

```python
from pycbsdk import Session
import time

with Session("HUB1") as session:
    # Register a callback for spike events
    @session.on_event("FRONTEND")
    def on_spike(header, data):
        print(f"Spike on channel {header.chid} at t={header.time}")

    # Register a callback for 30kHz continuous data
    @session.on_group(5, as_array=True)
    def on_continuous(header, samples):
        # samples is a numpy int16 array of shape (n_channels,)
        print(f"Group packet: {len(samples)} channels")

    time.sleep(10)
    print(session.stats)
```

## Features

- **Callback-driven**: decorator-based registration for event, group, config, and
  catch-all packet callbacks
- **Context manager**: automatic cleanup on exit
- **numpy integration** (optional): zero-copy arrays for continuous data,
  ring buffer accumulator, blocking `read_continuous()` collector
- **Device support**: LEGACY_NSP, NSP, HUB1, HUB2, HUB3, NPLAY

## API Overview

### Session

```python
session = Session(device_type="HUB1", callback_queue_depth=16384)
```

**Callbacks** (decorator style):

| Decorator | Description |
|-----------|-------------|
| `@session.on_event("FRONTEND")` | Spike / event packets for a channel type |
| `@session.on_group(5)` | Continuous sample group (1-6) |
| `@session.on_group(5, as_array=True)` | Same, but data as numpy array |
| `@session.on_config(pkt_type)` | Config / system packets |
| `@session.on_packet()` | All packets (catch-all) |
| `session.on_error(fn)` | Error messages |

**Configuration access**:

- `session.get_channel_label(chan_id)` — channel label string
- `session.get_channel_smpgroup(chan_id)` — channel's sample group (0-6)
- `session.get_group_channels(group_id)` — list of channel IDs in a group
- `session.runlevel` — current device run level
- `Session.max_chans()`, `Session.num_fe_chans()`, `Session.num_analog_chans()`

**Commands**:

- `session.send_comment("marker text", rgba=0xFF0000)` — inject a comment
- `session.set_digital_output(chan_id, value)` — set digital output
- `session.set_runlevel(level)` — change system run level
- `session.set_channel_sample_group(n, "FRONTEND", group_id)` — configure sampling

**Statistics**:

```python
stats = session.stats  # Stats dataclass
print(stats.packets_received, stats.packets_dropped)
session.reset_stats()
```

### numpy Integration

Requires `pip install pycbsdk[numpy]`.

```python
# Blocking data collection
data = session.read_continuous(group_id=5, duration=2.0)
# data.shape == (n_channels, ~60000), dtype int16

# Ring buffer for ongoing collection
reader = session.continuous_reader(group_id=5, buffer_seconds=10)
import time; time.sleep(5)
data = reader.read()        # most recent samples
data = reader.read(1000)    # last 1000 samples
print(reader.total_samples, reader.dropped)
reader.close()
```

## Supported Devices

| Device Type | Description |
|-------------|-------------|
| `LEGACY_NSP` | Legacy NSP (default) |
| `NSP` | NSP |
| `HUB1` | Gemini Hub 1 |
| `HUB2` | Gemini Hub 2 |
| `HUB3` | Gemini Hub 3 |
| `NPLAY` | nPlay |

## Development

```bash
# Build the shared library
cmake -S . -B build -DCBSDK_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cbsdk_shared --config Release

# Install pycbsdk in development mode
cd pycbsdk
pip install -e ".[dev,numpy]"

# Point to the shared library
export CBSDK_LIB_PATH=/path/to/libcbsdk.dll  # or .so / .dylib
```

## License

BSD 2-Clause. See [LICENSE.txt](https://github.com/CerebusOSS/CereLink/blob/master/LICENSE.txt).
