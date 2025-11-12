#!/usr/bin/env python3
"""
Example: Read status from CereLink shared memory.

This example demonstrates how to access the PC status buffer
from CereLink's shared memory.
"""

import sys
import time
from cbsdk_shmem import CerebusSharedMemory, get_implementation_info


def main():
    """Read and display status from shared memory."""
    instance = 0  # Use instance 0 (default)

    print(f"Attempting to read CereLink shared memory (instance {instance})...")
    print(f"Using implementation: {get_implementation_info()}")
    print("Make sure CereLink (cbsdk) is running!")
    print()

    try:
        with CerebusSharedMemory(instance=instance, read_only=True) as shmem:
            # Try to access status buffer
            status_buf = shmem.get_status_buffer()

            if status_buf is None:
                print("ERROR: Could not open status buffer.")
                print("Is CereLink running?")
                return 1

            print(f"✓ Status buffer opened successfully ({len(status_buf)} bytes)")

            # Try to access config buffer
            config_buf = shmem.get_config_buffer()
            if config_buf is None:
                print("✗ Could not open config buffer")
            else:
                print(f"✓ Config buffer opened successfully ({len(config_buf)} bytes)")

            # Try to access receive buffer
            rec_buf = shmem.get_rec_buffer()
            if rec_buf is None:
                print("✗ Could not open receive buffer")
            else:
                print(f"✓ Receive buffer opened successfully ({len(rec_buf)} bytes)")

            print()
            print("Successfully connected to CereLink shared memory!")
            print()
            print("Note: To parse the actual structures, you'll need to define")
            print("      the C struct layouts using ctypes.Structure or numpy dtypes.")

    except OSError as e:
        print(f"ERROR: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
