"""nPlay + LSL clock-synchronization stress test.

nPlayServer (``--device=LSL <name>``) replays a live LSL stream onto the Cerebus
protocol, deriving each packet's device timestamp from the stream's ORIGINATING
time scaled to a 30 kHz sample counter.  Unlike file playback (a sample counter
that RESETS every time the short file wraps), the LSL device clock increases
monotonically for the whole run — so CereLink's host-mapped timeline must be
monotonic across the ENTIRE capture, not just within wrap segments.

This is the path where the ±0.6 s backward-jump bug was reported (the cursor
pipeline, 2026-06): the offset estimate recalibrated against a replay clock that
did not advance at true wall-rate, so ``device_to_monotonic`` stepped backward
and stalled any downstream consumer that assumes monotonic time.  Here we stamp
every 30 kHz batch with ``device_to_monotonic(first_ts)`` — exactly as a consumer
does — and require the resulting timeline to be monotonic.

MUTUALLY EXCLUSIVE with the file-playback tests (both use a STANDALONE NPLAY
session, whose shmem is keyed by device type).  Run in isolation::

    pytest -m lsl
"""

import os
import time

import pytest

from pycbsdk import ChannelType, DeviceType, SampleRate, Session

pytestmark = [pytest.mark.integration, pytest.mark.lsl]

N_CHANS = 8
WARMUP_S = 3          # let channels configure and the first offset settle
CAPTURE_S = 15        # long enough to catch a recalibration thrash

# A backward jump in DEVICE time larger than this would mark a clock reset.  The
# LSL origin clock does not wrap, so we expect ZERO of these — asserting it makes
# an unexpected wrap visible instead of silently splitting the timeline.
WRAP_BACK_S = 0.050

# After any segment start, ignore this much device time so a settling offset is
# not mistaken for a bug.  (For LSL there is one segment; this just guards the
# very first samples after warmup.)
GUARD_S = 0.150

# Max tolerated BACKWARD step in the host-mapped timeline.  Healthy localhost
# sync tracks the origin clock ~1:1 and never steps back; the guarded-against bug
# jumps ~600 ms, so 20 ms cleanly separates them while tolerating sub-deadband
# offset corrections.
MONO_BACKSTEP_S = 0.020


def _analyze(dev_ns, mono_s):
    """Walk a captured (device_ns, host_monotonic_s) timeline.

    Splits on device-clock resets (none expected for LSL), skips a short guard at
    each segment start, and counts BACKWARD steps in the host-mapped time beyond
    MONO_BACKSTEP_S.  Returns a stats dict.
    """
    n = len(dev_ns)
    stats = {"n_samples": 0, "n_wraps": 0, "n_segments": 0,
             "n_backsteps": 0, "max_backstep_s": 0.0}

    # Segment boundaries from the device-time series.
    bounds = [0]
    for i in range(1, n):
        if dev_ns[i] - dev_ns[i - 1] < -WRAP_BACK_S * 1e9:
            stats["n_wraps"] += 1
            bounds.append(i)
    bounds.append(n)

    for s in range(len(bounds) - 1):
        begin, end = bounds[s], bounds[s + 1]
        if end <= begin:
            continue
        seg_dev0 = dev_ns[begin]
        prev = None
        evaluated = 0
        first_eval = last_eval = None
        for i in range(begin, end):
            # Skip the post-(re)acquisition guard at the segment start.
            if (dev_ns[i] - seg_dev0) < GUARD_S * 1e9:
                prev = None
                continue
            if first_eval is None:
                first_eval = dev_ns[i]
            last_eval = dev_ns[i]
            evaluated += 1
            if prev is not None:
                backstep = prev - mono_s[i]  # >0 => went backward
                if backstep > stats["max_backstep_s"]:
                    stats["max_backstep_s"] = backstep
                if backstep > MONO_BACKSTEP_S:
                    stats["n_backsteps"] += 1
            prev = mono_s[i]

        # Only count segments with a meaningful post-guard span.
        if first_eval is not None and (last_eval - first_eval) > 0.3e9:
            stats["n_segments"] += 1
            stats["n_samples"] += evaluated

    return stats


def _start_capture(session):
    """Register a 30 kHz batch capture on *session*.

    Stamps each batch's first sample with ``device_to_monotonic`` — the way an
    acquisition consumer maps the device clock onto the host monotonic clock —
    and returns the (device_ns, host_monotonic_s) lists it appends to.
    """
    dev_ns: list[int] = []
    mono_s: list[float] = []

    @session.on_group_batch(SampleRate.SR_30kHz)
    def _on_batch(samples, timestamps):
        if len(timestamps) == 0:
            return
        t0 = int(timestamps[0])
        try:
            m = session.device_to_monotonic(t0)
        except RuntimeError:
            return  # no clock offset yet — the safe fallback
        dev_ns.append(t0)
        mono_s.append(m)

    return dev_ns, mono_s


def _hardware_multidevice_enabled() -> bool:
    """True if the operator opted in to driving a live Hub (CERELINK_HW_MULTIDEVICE)."""
    return bool(os.environ.get("CERELINK_HW_MULTIDEVICE"))


def test_lsl_host_mapped_timeline_monotonic(nplay_lsl_session):
    """device_to_monotonic(first_ts) must be monotonic across the LSL run."""
    session = nplay_lsl_session

    session.set_sample_group(
        N_CHANS, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
    )
    time.sleep(WARMUP_S)

    dev_ns, mono_s = _start_capture(session)
    time.sleep(CAPTURE_S)

    # Snapshot (the callback keeps firing on the shared session).
    dev = list(dev_ns)
    mono = list(mono_s)
    assert len(dev) > 1000, f"Too few batches captured ({len(dev)}) — no data flowing?"

    stats = _analyze(dev, mono)
    print(
        f"\n=== nPlay+LSL timeline: {len(dev)} batches, {stats['n_wraps']} wraps, "
        f"{stats['n_segments']} segments, {stats['n_backsteps']} backstep(s) "
        f"> {MONO_BACKSTEP_S * 1e3:.0f} ms, max backstep = "
        f"{stats['max_backstep_s'] * 1e3:.3f} ms ==="
    )

    # The LSL origin clock is monotonic and never resets.
    assert stats["n_wraps"] == 0, (
        f"Unexpected device-clock reset(s) ({stats['n_wraps']}) on the LSL path — "
        "the origin timestamp should increase monotonically"
    )
    assert stats["n_segments"] > 0, "No evaluable timeline captured"

    assert stats["n_backsteps"] == 0, (
        f"{stats['n_backsteps']} backward jump(s) in nPlay's host-mapped LSL "
        f"timeline (max {stats['max_backstep_s'] * 1e3:.3f} ms). The offset is "
        "recalibrating against the replayed origin clock — the non-monotonic "
        "timestamp bug that stalls downstream RESAMPLE/MERGE."
    )


def test_lsl_nplay_and_hub2_timelines_monotonic(nplay_lsl_session):
    """LSL-fed nPlay + a LIVE Hub2: both device clocks map to one monotonic host clock.

    The cross-device merge topology with nPlay driven from LSL instead of a file.
    nPlay's device clock is the LSL stream's originating time (× 30 kHz) while
    Hub2's is PTP — two unrelated time bases that each ClockSync maps onto the
    SAME host monotonic clock.  If nPlay's LSL-derived timeline goes non-monotonic
    while Hub2's real clock stays clean, a consumer resampling one onto the other
    stalls.  Hardware-gated: bringing up Hub2 runs a handshake that may HARDRESET
    it, so this is opt-in.
    """
    if not _hardware_multidevice_enabled():
        pytest.skip(
            "Set CERELINK_HW_MULTIDEVICE=1 to run against a live Hub2 alongside "
            "the LSL-fed nPlay (the Hub handshake may HARDRESET it)."
        )

    nplay = nplay_lsl_session
    nplay.set_sample_group(
        N_CHANS, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
    )

    with Session(DeviceType.HUB2) as hub2:
        hub2.set_sample_group(
            N_CHANS, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
        )
        time.sleep(WARMUP_S)

        ndev, nmono = _start_capture(nplay)
        hdev, hmono = _start_capture(hub2)
        time.sleep(CAPTURE_S)

        # Snapshot while Hub2 is still alive (its callback stops when it closes).
        ndev, nmono = list(ndev), list(nmono)
        hdev, hmono = list(hdev), list(hmono)

    assert len(ndev) > 1000, f"No LSL-fed nPlay data flowing ({len(ndev)})"
    assert len(hdev) > 1000, f"No Hub2 data flowing ({len(hdev)})"

    ns = _analyze(ndev, nmono)
    hs = _analyze(hdev, hmono)
    print(
        f"\n=== LSL-nPlay: {ns['n_segments']} seg, {ns['n_backsteps']} backstep(s), "
        f"max {ns['max_backstep_s'] * 1e3:.3f} ms | "
        f"Hub2: {hs['n_segments']} seg, {hs['n_backsteps']} backstep(s), "
        f"max {hs['max_backstep_s'] * 1e3:.3f} ms ==="
    )

    # Neither the LSL-origin clock nor Hub2's PTP clock wraps.
    assert ns["n_wraps"] == 0 and hs["n_wraps"] == 0, (
        f"Unexpected device-clock reset(s): LSL-nPlay={ns['n_wraps']}, Hub2={hs['n_wraps']}"
    )
    assert ns["n_segments"] > 0, "No evaluable LSL-nPlay timeline captured"
    assert hs["n_segments"] > 0, "No evaluable Hub2 timeline captured"

    assert hs["n_backsteps"] == 0, (
        f"Live Hub2 host-mapped timeline went non-monotonic (max "
        f"{hs['max_backstep_s'] * 1e3:.3f} ms) — unexpected for a real PTP clock; "
        "check the rig before blaming nPlay."
    )
    assert ns["n_backsteps"] == 0, (
        f"{ns['n_backsteps']} backward jump(s) in the LSL-fed nPlay host-mapped "
        f"timeline (max {ns['max_backstep_s'] * 1e3:.3f} ms) while a real Hub2 "
        "stayed monotonic — the cross-device merge stall reproduces on the LSL path."
    )
