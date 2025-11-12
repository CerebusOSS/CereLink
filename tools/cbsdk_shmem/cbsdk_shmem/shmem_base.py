"""
Base class for platform-specific shared memory implementations.
"""

from abc import ABC, abstractmethod
from typing import Optional


class SharedMemoryBase(ABC):
    """Abstract base class for platform-specific shared memory access."""

    def __init__(self, name: str, size: int, read_only: bool = True):
        """
        Initialize shared memory access.

        Parameters
        ----------
        name : str
            Name of the shared memory segment
        size : int
            Size of the shared memory segment in bytes
        read_only : bool
            If True, open in read-only mode
        """
        self.name = name
        self.size = size
        self.read_only = read_only
        self._buffer: Optional[memoryview] = None

    @abstractmethod
    def open(self) -> bool:
        """
        Open the shared memory segment.

        Returns
        -------
        bool
            True if successful, False otherwise
        """
        pass

    @abstractmethod
    def close(self) -> None:
        """Close and cleanup the shared memory segment."""
        pass

    @abstractmethod
    def get_buffer(self) -> Optional[memoryview]:
        """
        Get a memoryview of the shared memory buffer.

        Returns
        -------
        Optional[memoryview]
            Memory view of the buffer, or None if not open
        """
        pass

    def __enter__(self):
        """Context manager entry."""
        self.open()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False
