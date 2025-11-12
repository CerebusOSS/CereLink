"""
High-level interface to CereLink shared memory buffers.
"""

import sys
from typing import Optional, Dict, Any

from .buffer_names import BufferNames
from .shmem_base import SharedMemoryBase

# Choose implementation based on platform and Python version
#
# Strategy:
# 1. Windows: Use stdlib (simplest, fully compatible)
# 2. POSIX + Python 3.13+: Use stdlib (naming fixed in 3.13)
# 3. POSIX + Python < 3.13: Use platform-specific (naming incompatibility)
#
# See IMPLEMENTATION_COMPARISON.md for detailed explanation

_USE_STDLIB = False  # Can be set to True to force stdlib implementation

if sys.platform == 'win32':
    # Windows: stdlib works perfectly with named file mappings
    try:
        from .shmem_stdlib import StdlibSharedMemory as PlatformSharedMemory
        _implementation = "stdlib (Windows)"
    except ImportError:
        # Fallback to platform-specific if stdlib unavailable
        from .shmem_windows import WindowsSharedMemory as PlatformSharedMemory
        _implementation = "platform-specific (Windows/ctypes)"

elif sys.version_info >= (3, 13) or _USE_STDLIB:
    # Python 3.13+: stdlib naming fixed on all platforms
    # OR: User explicitly requested stdlib implementation
    try:
        from .shmem_stdlib import StdlibSharedMemory as PlatformSharedMemory
        _implementation = f"stdlib (Python {sys.version_info.major}.{sys.version_info.minor})"
    except ImportError:
        # Fallback to platform-specific
        from .shmem_posix import PosixSharedMemory as PlatformSharedMemory
        _implementation = "platform-specific (POSIX)"

else:
    # POSIX + Python < 3.13: Use platform-specific to avoid naming issues
    from .shmem_posix import PosixSharedMemory as PlatformSharedMemory
    _implementation = "platform-specific (POSIX - stdlib has naming issues)"


def get_implementation_info() -> str:
    """
    Get information about which shared memory implementation is being used.

    Returns
    -------
    str
        Description of the current implementation
    """
    return _implementation


class CerebusSharedMemory:
    """
    High-level interface to CereLink's shared memory buffers.

    This class provides access to the various shared memory segments created
    by CereLink (cbsdk) for inter-process communication.

    Parameters
    ----------
    instance : int
        Instance number (0 for default, >0 for multi-instance support)
    read_only : bool
        If True, open buffers in read-only mode (recommended)

    Examples
    --------
    >>> with CerebusSharedMemory(instance=0) as shmem:
    ...     status = shmem.get_status_buffer()
    ...     if status is not None:
    ...         print(f"Buffer size: {len(status)} bytes")
    """

    # Buffer sizes (in bytes) - these match the C++ structure sizes
    # TODO: These should be imported from structure definitions
    BUFFER_SIZES = {
        BufferNames.REC_BUFFER: 524288,     # Placeholder - actual size of cbRECBUFF
        BufferNames.CFG_BUFFER: 1048576,    # Placeholder - actual size of cbCFGBUFF
        BufferNames.XMT_GLOBAL: 131072,     # Placeholder - actual size of cbXMTBUFF + buffer
        BufferNames.XMT_LOCAL: 131072,      # Placeholder - actual size of cbXMTBUFF + buffer
        BufferNames.STATUS: 4096,           # Placeholder - actual size of cbPcStatus
        BufferNames.SPK_BUFFER: 262144,     # Placeholder - actual size of cbSPKBUFF
    }

    def __init__(self, instance: int = 0, read_only: bool = True, verbose: bool = False):
        """
        Initialize the shared memory interface.

        Parameters
        ----------
        instance : int
            Instance number (0 for default)
        read_only : bool
            Open buffers in read-only mode (recommended)
        verbose : bool
            Print implementation information on initialization
        """
        self.instance = instance
        self.read_only = read_only
        self._buffers: Dict[str, PlatformSharedMemory] = {}

        if verbose:
            print(f"cbsdk_shmem using: {_implementation}")

    def _get_or_open_buffer(self, base_name: str) -> Optional[memoryview]:
        """
        Get or open a shared memory buffer.

        Parameters
        ----------
        base_name : str
            Base buffer name (from BufferNames)

        Returns
        -------
        Optional[memoryview]
            Memory view of the buffer, or None if failed to open
        """
        if base_name not in self._buffers:
            # Get full buffer name with instance suffix
            full_name = BufferNames.get_buffer_name(base_name, self.instance)

            # Get buffer size
            size = self.BUFFER_SIZES.get(base_name)
            if size is None:
                raise ValueError(f"Unknown buffer: {base_name}")

            # Create platform-specific shared memory object
            try:
                shmem = PlatformSharedMemory(full_name, size, self.read_only)
                shmem.open()
                self._buffers[base_name] = shmem
            except OSError as e:
                # Buffer doesn't exist or can't be opened
                print(f"Warning: Could not open buffer '{full_name}': {e}")
                return None

        return self._buffers[base_name].get_buffer()

    def get_rec_buffer(self) -> Optional[memoryview]:
        """
        Get the receive buffer (cbRECbuffer).

        Returns
        -------
        Optional[memoryview]
            Memory view of the receive buffer
        """
        return self._get_or_open_buffer(BufferNames.REC_BUFFER)

    def get_config_buffer(self) -> Optional[memoryview]:
        """
        Get the configuration buffer (cbCFGbuffer).

        Returns
        -------
        Optional[memoryview]
            Memory view of the configuration buffer
        """
        return self._get_or_open_buffer(BufferNames.CFG_BUFFER)

    def get_status_buffer(self) -> Optional[memoryview]:
        """
        Get the PC status buffer (cbSTATUSbuffer).

        Returns
        -------
        Optional[memoryview]
            Memory view of the status buffer
        """
        return self._get_or_open_buffer(BufferNames.STATUS)

    def get_spike_buffer(self) -> Optional[memoryview]:
        """
        Get the spike cache buffer (cbSPKbuffer).

        Returns
        -------
        Optional[memoryview]
            Memory view of the spike buffer
        """
        return self._get_or_open_buffer(BufferNames.SPK_BUFFER)

    def get_xmt_global_buffer(self) -> Optional[memoryview]:
        """
        Get the global transmit buffer (XmtGlobal).

        Returns
        -------
        Optional[memoryview]
            Memory view of the global transmit buffer
        """
        return self._get_or_open_buffer(BufferNames.XMT_GLOBAL)

    def get_xmt_local_buffer(self) -> Optional[memoryview]:
        """
        Get the local transmit buffer (XmtLocal).

        Returns
        -------
        Optional[memoryview]
            Memory view of the local transmit buffer
        """
        return self._get_or_open_buffer(BufferNames.XMT_LOCAL)

    def close(self) -> None:
        """Close all open shared memory buffers."""
        for shmem in self._buffers.values():
            shmem.close()
        self._buffers.clear()

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False
