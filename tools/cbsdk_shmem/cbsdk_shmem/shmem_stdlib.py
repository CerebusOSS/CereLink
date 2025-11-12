"""
Simplified implementation using Python's multiprocessing.shared_memory (Python 3.8+).

This is a much simpler alternative to the platform-specific implementations,
using Python's standard library cross-platform shared memory support.
"""

import sys
from typing import Optional
from multiprocessing import shared_memory

from .shmem_base import SharedMemoryBase


# Require Python 3.8+ for multiprocessing.shared_memory
if sys.version_info < (3, 8):
    raise ImportError("shmem_stdlib requires Python 3.8+ for multiprocessing.shared_memory")


class StdlibSharedMemory(SharedMemoryBase):
    """
    Simplified implementation using multiprocessing.shared_memory.

    This approach uses Python's standard library instead of platform-specific
    code, which is much simpler and more maintainable.

    Advantages:
    - Cross-platform (Windows, Linux, macOS)
    - No ctypes or platform-specific code needed
    - Handles naming conventions automatically
    - Automatically determines size when attaching

    Potential Issues:
    - On POSIX, Python may add prefixes to shared memory names
    - May not work if CereLink uses non-standard naming conventions
    """

    def __init__(self, name: str, size: int, read_only: bool = True):
        super().__init__(name, size, read_only)
        self._shm: Optional[shared_memory.SharedMemory] = None

    def open(self) -> bool:
        """
        Open existing shared memory segment.

        Returns
        -------
        bool
            True if successful, False otherwise

        Raises
        ------
        FileNotFoundError
            If the shared memory segment does not exist
        """
        if self._shm is not None:
            return True  # Already open

        try:
            # Attach to existing shared memory (create=False)
            # Size will be determined from the existing segment
            self._shm = shared_memory.SharedMemory(
                name=self.name,
                create=False  # We're opening existing memory from CereLink
            )

            # Verify size if we expected a specific size
            if self.size > 0 and self._shm.size < self.size:
                self.close()
                raise ValueError(
                    f"Shared memory '{self.name}' size mismatch: "
                    f"expected at least {self.size} bytes, got {self._shm.size}"
                )

            # Update size to actual size
            self.size = self._shm.size

            # Create memoryview
            self._buffer = memoryview(self._shm.buf)

            return True

        except FileNotFoundError:
            # Shared memory doesn't exist
            raise FileNotFoundError(
                f"Shared memory '{self.name}' not found. "
                f"Is CereLink running?"
            )
        except Exception as e:
            if self._shm is not None:
                self._shm.close()
                self._shm = None
            raise OSError(f"Failed to open shared memory '{self.name}': {e}")

    def close(self) -> None:
        """
        Close the shared memory segment.

        Note: We use close() instead of unlink() because we didn't create
        the memory - CereLink did. Unlinking would destroy it for all processes.
        """
        if self._buffer is not None:
            self._buffer.release()
            self._buffer = None

        if self._shm is not None:
            self._shm.close()  # Detach from memory
            # Do NOT call shm.unlink() - we didn't create it!
            self._shm = None

    def get_buffer(self) -> Optional[memoryview]:
        """
        Get a memoryview of the shared memory buffer.

        Returns
        -------
        Optional[memoryview]
            Memory view of the buffer, or None if not open
        """
        return self._buffer


# Additional notes about platform compatibility:
#
# Windows:
# --------
# - Uses named file mappings (CreateFileMappingW/OpenFileMappingW)
# - Name format: exactly as specified (e.g., "cbSTATUSbuffer")
# - Should work seamlessly with CereLink's shared memory
#
# Linux:
# ------
# - Uses POSIX shared memory via /dev/shm/
# - Python 3.8-3.12: Names are prefixed with "psm_" (e.g., /dev/shm/psm_cbSTATUSbuffer)
# - Python 3.13+: No prefix, uses name as-is
# - This WILL cause issues with CereLink on Python < 3.13!
#
# macOS:
# ------
# - Uses POSIX shm_open()
# - Python adds "wnsm_" prefix on macOS
# - This WILL cause issues with CereLink!
#
# Workaround for POSIX naming:
# ----------------------------
# If the standard library naming doesn't work on POSIX, we can:
# 1. Monkey-patch the name after creation
# 2. Use the platform-specific implementations instead
# 3. Create symlinks in /dev/shm/
# 4. Wait for Python 3.13+ where this is fixed
