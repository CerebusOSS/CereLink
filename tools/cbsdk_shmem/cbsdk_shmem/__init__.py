"""
Pure Python package for accessing CereLink's shared memory buffers.

This package provides platform-specific implementations to access the shared memory
buffers created by CereLink (cbsdk) without requiring the C++ bindings.
"""

from .shmem import CerebusSharedMemory, get_implementation_info
from .buffer_names import BufferNames

__all__ = ['CerebusSharedMemory', 'BufferNames', 'get_implementation_info']
__version__ = '0.1.0'
