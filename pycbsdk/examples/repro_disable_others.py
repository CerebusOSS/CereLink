"""Repro for the bulk-setter `disable_others=True` hang report.

Mirrors the call sequence reported as hanging indefinitely in
ezmsg-blackrock:

    session.set_sample_group(
        2, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True
    )

Each phase is timed and logged so a hang shows up as the last printed
line.  Run with:

    python repro_disable_others.py NPLAY
    python repro_disable_others.py HUB2
    python repro_disable_others.py HUB1
"""

import asyncio
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from pycbsdk import ChannelType, DeviceType, SampleRate, Session


def _stamp(t0: float, label: str) -> None:
    print(f"  +{time.monotonic() - t0:6.3f}s  {label}", flush=True)


async def main(device_type: str) -> None:
    print(f"Connecting to {device_type}...", flush=True)
    t0 = time.monotonic()

    with Session(device_type=device_type) as session:
        _stamp(t0, "session created")

        await session.wait_until_running(timeout=10.0)
        _stamp(t0, f"runlevel reached RUNNING ({session.runlevel})")

        n_fe = session.num_fe_chans()
        _stamp(t0, f"num_fe_chans={n_fe}")

        # The exact call reported as hanging.
        session.set_sample_group(
            2, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True
        )
        _stamp(t0, "set_sample_group returned")

        session.sync()
        _stamp(t0, "sync returned")

        ch_30k = session.get_group_channels(int(SampleRate.SR_30kHz))
        _stamp(t0, f"30kHz group: {sorted(ch_30k)}")

        # Sanity: only chans 1, 2 should be at 30kHz.
        assert sorted(ch_30k) == [1, 2], (
            f"Expected [1, 2] at 30kHz, got {sorted(ch_30k)}"
        )
        print("OK", flush=True)


if __name__ == "__main__":
    device_type = sys.argv[1] if len(sys.argv) > 1 else "NPLAY"
    asyncio.run(main(device_type))
