"""Standalone 30 kHz int16 LSL stream generator for nPlay+LSL tests.

Launched as a subprocess by the ``nplay_lsl`` fixture.  nPlayServer
(``--device=LSL <name>``) resolves this stream by name and replays it onto the
Cerebus protocol, deriving each packet's device timestamp from the stream's
ORIGINATING time scaled to a 30 kHz sample counter.  We therefore pace pushes to
real time so the originating timestamps advance at the true sample rate — the
condition CereLink's clock sync must map to a monotonic host timeline.

Usage:
    python _lsl_generator.py <stream_name> [n_channels]

Runs until killed (SIGTERM/SIGINT) by the fixture.
"""

import signal
import sys
import time

import numpy as np
import pylsl

SRATE = 30000
CHUNK_SAMPLES = 300  # 10 ms per push at 30 kHz


def main() -> int:
    name = sys.argv[1]
    n_ch = int(sys.argv[2]) if len(sys.argv) > 2 else 8

    info = pylsl.StreamInfo(
        name=name,
        type="EEG",
        channel_count=n_ch,
        nominal_srate=SRATE,
        channel_format=pylsl.cf_int16,
        source_id=f"{name}_src",
    )
    outlet = pylsl.StreamOutlet(info, chunk_size=CHUNK_SAMPLES, max_buffered=360)

    # Exit cleanly on the fixture's terminate() so liblsl tears the outlet down.
    running = {"go": True}

    def _stop(*_a):
        running["go"] = False

    signal.signal(signal.SIGTERM, _stop)
    signal.signal(signal.SIGINT, _stop)

    # A slow sine across channels — content is irrelevant to the timing test,
    # but non-constant data makes the stream easy to eyeball in a viewer.
    phase = np.arange(CHUNK_SAMPLES, dtype=np.float64)
    chan_offsets = np.linspace(0.0, np.pi, n_ch, dtype=np.float64)

    t0 = time.monotonic()
    idx = 0
    while running["go"]:
        angle = 2.0 * np.pi * (phase[:, None] + idx) / 3000.0 + chan_offsets[None, :]
        chunk = (np.sin(angle) * 1000.0).astype(np.int16)
        outlet.push_chunk(chunk)
        idx += CHUNK_SAMPLES
        # Pace to real time: sample idx should be emitted at t0 + idx/SRATE.
        target = t0 + idx / SRATE
        dt = target - time.monotonic()
        if dt > 0:
            time.sleep(dt)

    return 0


if __name__ == "__main__":
    sys.exit(main())
