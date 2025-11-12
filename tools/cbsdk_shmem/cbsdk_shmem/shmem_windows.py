"""
Windows implementation of shared memory access using ctypes and kernel32.
"""

import sys
import ctypes
from ctypes import wintypes
from typing import Optional

from .shmem_base import SharedMemoryBase


if sys.platform != 'win32':
    raise ImportError("shmem_windows module requires Windows platform")


# Windows API constants
FILE_MAP_READ = 0x0004
FILE_MAP_WRITE = 0x0002
FILE_MAP_ALL_ACCESS = 0x001F
INVALID_HANDLE_VALUE = -1
PAGE_READWRITE = 0x04
PAGE_READONLY = 0x02


# Load kernel32.dll
kernel32 = ctypes.windll.kernel32


# Define Windows API function signatures
# OpenFileMappingA(dwDesiredAccess, bInheritHandle, lpName) -> HANDLE
kernel32.OpenFileMappingA.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCSTR]
kernel32.OpenFileMappingA.restype = wintypes.HANDLE

# MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap) -> LPVOID
kernel32.MapViewOfFile.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_size_t]
kernel32.MapViewOfFile.restype = wintypes.LPVOID

# UnmapViewOfFile(lpBaseAddress) -> BOOL
kernel32.UnmapViewOfFile.argtypes = [wintypes.LPVOID]
kernel32.UnmapViewOfFile.restype = wintypes.BOOL

# CloseHandle(hObject) -> BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL

# GetLastError() -> DWORD
kernel32.GetLastError.argtypes = []
kernel32.GetLastError.restype = wintypes.DWORD


class WindowsSharedMemory(SharedMemoryBase):
    """Windows implementation using kernel32 file mapping functions."""

    def __init__(self, name: str, size: int, read_only: bool = True):
        super().__init__(name, size, read_only)
        self._handle: Optional[wintypes.HANDLE] = None
        self._mapped_ptr: Optional[int] = None

    def open(self) -> bool:
        """
        Open the shared memory segment using OpenFileMappingA.

        Returns
        -------
        bool
            True if successful, False otherwise
        """
        if self._handle is not None:
            return True  # Already open

        # Determine access rights
        desired_access = FILE_MAP_READ if self.read_only else FILE_MAP_ALL_ACCESS

        # Open existing file mapping
        self._handle = kernel32.OpenFileMappingA(
            desired_access,
            False,  # bInheritHandle
            self.name.encode('ascii')
        )

        if self._handle is None or self._handle == 0:
            error_code = kernel32.GetLastError()
            raise OSError(f"OpenFileMappingA failed for '{self.name}': error {error_code}")

        # Map view of file into address space
        self._mapped_ptr = kernel32.MapViewOfFile(
            self._handle,
            desired_access,
            0,  # dwFileOffsetHigh
            0,  # dwFileOffsetLow
            self.size
        )

        if self._mapped_ptr is None or self._mapped_ptr == 0:
            error_code = kernel32.GetLastError()
            kernel32.CloseHandle(self._handle)
            self._handle = None
            raise OSError(f"MapViewOfFile failed for '{self.name}': error {error_code}")

        # Create memoryview from pointer
        self._buffer = (ctypes.c_char * self.size).from_address(self._mapped_ptr)

        return True

    def close(self) -> None:
        """Close and cleanup the shared memory segment."""
        if self._buffer is not None:
            self._buffer = None

        if self._mapped_ptr is not None:
            kernel32.UnmapViewOfFile(self._mapped_ptr)
            self._mapped_ptr = None

        if self._handle is not None:
            kernel32.CloseHandle(self._handle)
            self._handle = None

    def get_buffer(self) -> Optional[memoryview]:
        """
        Get a memoryview of the shared memory buffer.

        Returns
        -------
        Optional[memoryview]
            Memory view of the buffer, or None if not open
        """
        if self._buffer is None:
            return None
        return memoryview(self._buffer)
