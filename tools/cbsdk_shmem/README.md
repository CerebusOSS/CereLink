# cbsdk_shmem

Pure Python package for accessing CereLink's shared memory buffers.

This package provides platform-specific implementations to access the shared memory buffers created by CereLink (cbsdk) without requiring the C++ bindings. This is useful for:

- Lightweight monitoring of device state
- Read-only access to configuration and data buffers
- Debugging and diagnostic tools
- Integration with pure Python applications

## Architecture

CereLink creates several named shared memory segments:

1. **cbRECbuffer** - Receive buffer for incoming packets
2. **cbCFGbuffer** - Configuration buffer
3. **XmtGlobal** - Global transmit buffer
4. **XmtLocal** - Local transmit buffer
5. **cbSTATUSbuffer** - PC status information
6. **cbSPKbuffer** - Spike cache buffer
7. **cbSIGNALevent** - Signal event (Windows) / Semaphore (POSIX)

For multi-instance support, buffers are named with an instance suffix (e.g., `cbRECbuffer0`, `cbRECbuffer1`).

## Platform Support

- **Windows**: Uses `ctypes.windll.kernel32` for `OpenFileMappingA` and `MapViewOfFile`
- **Linux/macOS**: Uses `mmap` module with POSIX shared memory (`shm_open`)

## Usage

```python
from cbsdk_shmem import CerebusSharedMemory

# Open shared memory for instance 0 (default)
shmem = CerebusSharedMemory(instance=0, read_only=True)

# Access configuration buffer
cfg = shmem.get_config()
print(f"Protocol version: {cfg['version']}")

# Access status buffer
status = shmem.get_status()
print(f"Connection status: {status['status']}")

# Clean up
shmem.close()
```

## Installation

```bash
cd tools/cbsdk_shmem
python -m pip install -e .
```

## Requirements

- Python 3.11+
- NumPy (for structured array access)
- Platform: Windows, Linux, or macOS

## Limitations

- Read-only access recommended (writing requires careful synchronization)
- Requires CereLink (cbsdk) to be running and have created the shared memory
- Structure definitions must match the C++ version exactly
