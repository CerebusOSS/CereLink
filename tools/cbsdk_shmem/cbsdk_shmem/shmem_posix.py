"""
POSIX implementation of shared memory access using mmap and shm_open.
"""

import sys
import os
import mmap
from typing import Optional

from .shmem_base import SharedMemoryBase


if sys.platform == 'win32':
    raise ImportError("shmem_posix module requires POSIX platform (Linux/macOS)")


class PosixSharedMemory(SharedMemoryBase):
    """POSIX implementation using shm_open and mmap."""

    def __init__(self, name: str, size: int, read_only: bool = True):
        super().__init__(name, size, read_only)
        self._fd: Optional[int] = None
        self._mmap: Optional[mmap.mmap] = None

    def open(self) -> bool:
        """
        Open the shared memory segment using shm_open.

        Returns
        -------
        bool
            True if successful, False otherwise
        """
        if self._fd is not None:
            return True  # Already open

        # Open shared memory object
        # Note: On POSIX, shared memory names should start with '/'
        shm_name = f"/{self.name}" if not self.name.startswith('/') else self.name

        try:
            # Open with appropriate flags
            flags = os.O_RDONLY if self.read_only else os.O_RDWR

            # shm_open is not directly available in Python, so we use os.open on /dev/shm
            # On Linux, POSIX shared memory is typically at /dev/shm/<name>
            # On macOS, we need to use the shm_open system call via ctypes

            if sys.platform == 'darwin':
                # macOS - use ctypes to call shm_open
                import ctypes
                import ctypes.util

                libc_name = ctypes.util.find_library('c')
                if libc_name is None:
                    raise OSError("Could not find libc")

                libc = ctypes.CDLL(libc_name)

                # int shm_open(const char *name, int oflag, mode_t mode)
                shm_open = libc.shm_open
                shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]
                shm_open.restype = ctypes.c_int

                mode = 0o444 if self.read_only else 0o660
                self._fd = shm_open(shm_name.encode('utf-8'), flags, mode)

                if self._fd < 0:
                    raise OSError(f"shm_open failed for '{shm_name}': errno {ctypes.get_errno()}")
            else:
                # Linux - use /dev/shm directly
                shm_path = f"/dev/shm{shm_name}"
                self._fd = os.open(shm_path, flags)

            # Get the size (should match self.size, but let's verify)
            stat_info = os.fstat(self._fd)
            actual_size = stat_info.st_size

            if actual_size < self.size:
                self.close()
                raise OSError(f"Shared memory '{shm_name}' size mismatch: expected {self.size}, got {actual_size}")

            # Memory map the file descriptor
            prot = mmap.PROT_READ if self.read_only else (mmap.PROT_READ | mmap.PROT_WRITE)
            self._mmap = mmap.mmap(self._fd, self.size, flags=mmap.MAP_SHARED, prot=prot)

            return True

        except OSError as e:
            if self._fd is not None:
                os.close(self._fd)
                self._fd = None
            raise OSError(f"Failed to open shared memory '{shm_name}': {e}")

    def close(self) -> None:
        """Close and cleanup the shared memory segment."""
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None

        if self._fd is not None:
            os.close(self._fd)
            self._fd = None

    def get_buffer(self) -> Optional[memoryview]:
        """
        Get a memoryview of the shared memory buffer.

        Returns
        -------
        Optional[memoryview]
            Memory view of the buffer, or None if not open
        """
        if self._mmap is None:
            return None
        return memoryview(self._mmap)
