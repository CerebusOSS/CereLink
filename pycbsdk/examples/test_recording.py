"""Test Central recording start/stop via CereLink.

Requires:
  - Central running with a device connected
  - libcbsdk.dll built (CBSDK_LIB_PATH env var or on PATH)

Usage:
  python test_recording.py [device_type]
  python test_recording.py HUB1
"""

import sys
import time

sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[1] / "pycbsdk" / "src"))
from pycbsdk import Session

device_type = sys.argv[1] if len(sys.argv) > 1 else "HUB1"

print(f"Creating session ({device_type})...")
session = Session(device_type=device_type)
time.sleep(2)
print(f"Connected. running={session.running}\n")

# Start recording
filename = "cerelink_test"
comment = "CereLink recording test"
print(f"Starting Central recording: filename='{filename}', comment='{comment}'")
session.start_central_recording(filename, comment)
print("  -> start_central_recording() returned OK")
print("  Check Central: file.exe should be recording now.\n")

# Record for 5 seconds
print("Recording for 5 seconds...")
time.sleep(5)

# Stop recording
print("Stopping Central recording...")
session.stop_central_recording()
print("  -> stop_central_recording() returned OK")
print("  Check Central: file.exe should have stopped.\n")

session.close()
print("Done.")
