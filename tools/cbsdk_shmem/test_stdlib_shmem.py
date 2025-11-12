#!/usr/bin/env python3
"""
Test whether multiprocessing.shared_memory can attach to CereLink's shared memory.

This script tests if we can simplify the implementation by using Python's
standard library instead of platform-specific code.
"""

import sys
from multiprocessing import shared_memory


def test_attach_to_cerelink():
    """Test attaching to existing CereLink shared memory."""

    print("Testing multiprocessing.shared_memory with CereLink buffers...")
    print()

    # Try to attach to CereLink's status buffer (instance 0)
    buffer_names = [
        "cbSTATUSbuffer",   # Status buffer
        "cbRECbuffer",      # Receive buffer
        "cbCFGbuffer",      # Config buffer
    ]

    for name in buffer_names:
        print(f"Attempting to attach to '{name}'...")

        try:
            # Try to attach to existing shared memory
            # create=False means we're opening existing memory, not creating new
            shm = shared_memory.SharedMemory(name=name, create=False)

            print(f"  ✓ SUCCESS! Attached to '{name}'")
            print(f"    - Size: {shm.size} bytes")
            print(f"    - Name: {shm.name}")

            # Try to read first few bytes
            data = bytes(shm.buf[:16])
            print(f"    - First 16 bytes: {data.hex()}")

            # Close (but don't unlink since we didn't create it)
            shm.close()
            print()

        except FileNotFoundError:
            print(f"  ✗ Not found (CereLink not running?)")
            print()
        except Exception as e:
            print(f"  ✗ Error: {e}")
            print()

    print("Test complete!")


def test_platform_naming():
    """Check if platform affects naming."""
    print(f"Platform: {sys.platform}")
    print()

    # On POSIX systems, shared_memory might prefix names
    if sys.platform != 'win32':
        print("NOTE: On POSIX systems, Python's SharedMemory may add prefixes.")
        print("      This could cause issues attaching to CereLink's buffers.")
        print()
        print("      Checking naming behavior...")

        try:
            # Create a test segment to see what name it uses
            test_shm = shared_memory.SharedMemory(name="test_naming", create=True, size=4096)
            print(f"      Created memory with requested name: 'test_naming'")
            print(f"      Actual name used: '{test_shm.name}'")

            # Check if it's accessible without prefix
            import os
            if os.path.exists("/dev/shm/test_naming"):
                print(f"      ✓ Accessible as /dev/shm/test_naming")
            elif os.path.exists(f"/dev/shm/{test_shm.name}"):
                print(f"      ✓ Accessible as /dev/shm/{test_shm.name}")
            else:
                print(f"      ? Could not find in /dev/shm/")

            test_shm.close()
            test_shm.unlink()

        except Exception as e:
            print(f"      Error testing: {e}")

        print()


if __name__ == '__main__':
    test_platform_naming()
    test_attach_to_cerelink()
