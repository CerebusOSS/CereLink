# TODO: cbsdk_shmem Development

## High Priority

### 1. Define C Structure Layouts
The buffer sizes are currently placeholders. Need to:
- [ ] Extract actual structure definitions from `include/cerelink/cbhwlib.h`
- [ ] Create Python equivalents using `ctypes.Structure` or NumPy structured dtypes
- [ ] Define structures for:
  - [ ] `cbRECBUFF` - Receive buffer
  - [ ] `cbCFGBUFF` - Configuration buffer
  - [ ] `cbXMTBUFF` - Transmit buffer
  - [ ] `cbPcStatus` - PC status
  - [ ] `cbSPKBUFF` - Spike cache buffer

### 2. Test on Windows
- [ ] Test `WindowsSharedMemory` implementation
- [ ] Verify `OpenFileMappingA` and `MapViewOfFile` work correctly
- [ ] Test with actual CereLink instance running

### 3. Test on Linux/macOS
- [ ] Test `PosixSharedMemory` implementation on Linux
- [ ] Test `PosixSharedMemory` implementation on macOS (shm_open via ctypes)
- [ ] Verify correct shared memory naming conventions

## Medium Priority

### 4. Structure Parsing Utilities
- [ ] Add helper methods to parse common structures
- [ ] Add example showing how to read protocol version from config buffer
- [ ] Add example showing how to read connection status

### 5. Synchronization
- [ ] Document thread safety considerations
- [ ] Consider adding signal event/semaphore access for notifications
- [ ] Add optional locking mechanisms for read consistency

### 6. Error Handling
- [ ] Improve error messages
- [ ] Add better diagnostics when buffers don't exist
- [ ] Add buffer validation (check magic numbers, versions, etc.)

## Low Priority

### 7. Advanced Features
- [ ] Add write support (with warnings about synchronization)
- [ ] Add buffer monitoring utilities
- [ ] Create diagnostic tools to inspect buffer contents

### 8. Documentation
- [ ] Add detailed API documentation
- [ ] Create tutorial notebook
- [ ] Document structure layouts
- [ ] Add architecture diagram

### 9. Testing
- [ ] Unit tests for buffer name generation
- [ ] Integration tests (requires CereLink running)
- [ ] Mock shared memory for testing without CereLink

### 10. Packaging
- [ ] Add to main CereLink distribution
- [ ] Consider standalone PyPI package
- [ ] Add CI/CD for testing

## Notes

### Buffer Size Determination
To get actual buffer sizes, need to parse C++ structures. Options:
1. Parse header files programmatically
2. Query at runtime from actual shared memory (if available)
3. Manually transcribe structure definitions

### Platform-Specific Considerations
- **Windows**: Uses named file mappings (kernel32)
- **Linux**: Uses `/dev/shm` for POSIX shared memory
- **macOS**: Uses `shm_open` (requires ctypes to libc)

### Future Enhancements
- Consider using `multiprocessing.shared_memory` (Python 3.8+) as base
- Add NumPy structured array views for easier data access
- Create debugging tools to dump buffer contents
