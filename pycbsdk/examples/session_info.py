"""Connect to a device and print session info.

Usage:
  python session_info.py [device_type]
  python session_info.py NPLAY
  python session_info.py HUB1
"""

import sys
import time

sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[1] / "pycbsdk" / "src"))
from pycbsdk import Session, DeviceType, ProtocolVersion
device_type = sys.argv[1] if len(sys.argv) > 1 else "NPLAY"

print(f"Connecting to {device_type}...")
with Session(device_type=device_type) as session:
    # Print a few raw timestamps from data packets
    state = {"count": 0, "max": 10}

    @session.on_group()
    def on_group(header, data):
        if state["count"] < state["max"]:
            t = header.time
            print(f"  group pkt: time={t:>20d}  chid={header.chid}  "
                  f"(as seconds: {t/1e9:.3f}s if ns, {t/30000:.3f}s if 30kHz counts)")
            state["count"] += 1

    @session.on_event(channel_type=None)
    def on_event(header, data):
        if state["count"] < state["max"]:
            t = header.time
            print(f"  event pkt: time={t:>20d}  chid={header.chid}  type=0x{header.type:02x}  "
                  f"(as seconds: {t/1e9:.3f}s if ns, {t/30000:.3f}s if 30kHz counts)")
            state["count"] += 1

    # Wait for handshake / clock sync
    time.sleep(2)

    print(f"\nSession info:")
    print(f"  running:            {session.running}")
    print(f"  protocol_version:   {session.protocol_version!r}")
    print(f"  proc_ident:         {session.proc_ident!r}")
    print(f"  runlevel:           {session.runlevel}")
    print(f"  spike_length:       {session.spike_length}")
    print(f"  spike_pretrigger:   {session.spike_pretrigger}")
    print(f"  clock_offset_ns:    {session.clock_offset_ns}")
    print(f"  clock_uncertainty:  {session.clock_uncertainty_ns}")
    print(f"  stats:              {session.stats}")
