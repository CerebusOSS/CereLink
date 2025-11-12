# Can We Use multiprocessing.shared_memory?

## Short Answer

**Yes, but with caveats!**

- ✅ **Windows**: Works perfectly - use stdlib
- ✅ **Python 3.13+**: Works on all platforms - use stdlib
- ❌ **Linux/macOS + Python < 3.13**: Naming incompatibility - use platform-specific code

## The Problem: POSIX Naming

Python's `multiprocessing.shared_memory` adds **prefixes** to shared memory names on POSIX systems (Linux/macOS) in Python versions before 3.13:

| What You Request | What Python Creates | What CereLink Creates | Match? |
|-----------------|---------------------|----------------------|---------|
| `"cbSTATUSbuffer"` (Windows) | `cbSTATUSbuffer` | `cbSTATUSbuffer` | ✅ YES |
| `"cbSTATUSbuffer"` (Linux <3.13) | `/dev/shm/psm_cbSTATUSbuffer` | `/dev/shm/cbSTATUSbuffer` | ❌ NO |
| `"cbSTATUSbuffer"` (macOS <3.13) | `wnsm_cbSTATUSbuffer` | `cbSTATUSbuffer` | ❌ NO |
| `"cbSTATUSbuffer"` (Python 3.13+) | `cbSTATUSbuffer` | `cbSTATUSbuffer` | ✅ YES |

**Result**: Python < 3.13 on Linux/macOS cannot find CereLink's buffers!

## Code Comparison

### Platform-Specific Implementation (~150 lines)

```python
# Windows - uses ctypes + kernel32
kernel32.OpenFileMappingA(...)
kernel32.MapViewOfFile(...)

# Linux - uses mmap + /dev/shm
fd = os.open(f"/dev/shm/{name}", flags)
mmap.mmap(fd, size, ...)

# macOS - uses ctypes + libc.shm_open
libc.shm_open(name.encode(), flags, mode)
```

### Standard Library Implementation (~50 lines)

```python
# Works on all platforms automatically!
shm = shared_memory.SharedMemory(name=name, create=False)
buffer = memoryview(shm.buf)
```

**Result**: 66% less code, but doesn't work on POSIX + old Python.

## Our Solution: Hybrid Approach

The package now **automatically** chooses the best implementation:

```python
# In shmem.py

if sys.platform == 'win32':
    # Windows: Use stdlib (simple + compatible)
    from .shmem_stdlib import StdlibSharedMemory

elif sys.version_info >= (3, 13):
    # Python 3.13+: Use stdlib (naming fixed)
    from .shmem_stdlib import StdlibSharedMemory

else:
    # POSIX + Python < 3.13: Use platform-specific
    from .shmem_posix import PosixSharedMemory
```

## Benefits of This Approach

1. **Windows users** get simple, stdlib-only code (most common platform)
2. **Future-proof** for Python 3.13+ when naming is fixed
3. **Backwards compatible** on Linux/macOS with older Python
4. **Automatic** selection - users don't need to worry about it

## Testing

Run the test script to verify compatibility:

```bash
python test_stdlib_shmem.py
```

This will show:
- Which implementation is being used
- Whether Python adds naming prefixes on your platform
- If it can successfully attach to CereLink's buffers

## Timeline

- **Now (2025)**: Python 3.11/3.12 common → Use hybrid approach
- **2026-2027**: Python 3.13+ becomes common → Mostly stdlib
- **2028+**: Drop Python <3.13 support → Stdlib only (remove platform-specific code)

## Conclusion

**Yes, we can simplify with `multiprocessing.shared_memory`, but not yet!**

- Windows can use it now ✅
- Python 3.13+ can use it now ✅
- Linux/macOS need platform-specific code for a few more years ⏳

The hybrid approach gives us the best of both worlds:
- Simple stdlib code where it works
- Platform-specific fallback where needed
- Future-proof migration path

## What Changed

I've updated the package to:

1. ✅ Added `shmem_stdlib.py` - stdlib implementation
2. ✅ Added `test_stdlib_shmem.py` - compatibility testing
3. ✅ Updated `shmem.py` - hybrid selection logic
4. ✅ Added `IMPLEMENTATION_COMPARISON.md` - detailed comparison
5. ✅ Added `get_implementation_info()` - runtime introspection

Try it:

```python
from cbsdk_shmem import CerebusSharedMemory, get_implementation_info

print(get_implementation_info())
# On Windows: "stdlib (Windows)"
# On macOS with Python 3.12: "platform-specific (POSIX - stdlib has naming issues)"
# On Linux with Python 3.13: "stdlib (Python 3.13)"
```
