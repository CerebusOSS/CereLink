#!/usr/bin/env python3
"""Analog Input Latency Test — measures the end-to-end timing from host
audio output to device analog/frontend input, demonstrating pycbsdk's clock
synchronisation and timestamp conversion.

Can run against a single device (NSP or HUB1) or both simultaneously to
compare their clock alignment.

Hardware setup
--------------
1. Take a **3.5 mm (headphone) → BNC** cable (with a splitter for BOTH mode).
2. Plug the 3.5 mm end into your computer's **headphone jack**.
3. For the NSP: connect to **Analog Input 1**.
4. For the HUB1: connect to **FrontEnd channel 1** (via a 1/1000 attenuator).
5. Set your computer's volume to approximately 50 %.  The tone only needs
   to be large enough to cross a simple amplitude threshold — if the
   detected signal is too weak, increase the volume and re-run.

What the test does
------------------
1. Connects to one or both devices and configures the appropriate input
   channel for raw (30 kHz) sampling with DC coupling.
2. Collects ~1 second of silence to establish a per-device baseline.
3. Plays a 400 Hz sine tone (300 ms) through the computer speakers
   ten times, recording ``time.monotonic()`` at the instant each tone
   begins.
4. In each device's streaming callback, converts each sample's device
   timestamp to ``time.monotonic()`` and watches for the first sample
   whose absolute value exceeds ``baseline_mean + 5 * baseline_std``.
5. Reports per-device latency (``detected_time − play_time``) and, when
   running both devices, the cross-device onset difference for each trial.

Usage::

    python analog_latency_test.py [--device-type NSP|HUB1|BOTH]

Requirements
~~~~~~~~~~~~
* ``pycbsdk`` (installed from this repository)
* ``numpy``
* ``sounddevice`` — ``pip install sounddevice``
"""

from __future__ import annotations

import argparse
import contextlib
import math
import sys
import threading
import time
from dataclasses import dataclass, field

import numpy as np

try:
    import sounddevice as sd
except ImportError:
    sys.exit(
        "This example requires the 'sounddevice' package.\n"
        "Install it with:  pip install sounddevice"
    )

from pycbsdk import ChannelType, DeviceType, SampleRate, Session


# ---------------------------------------------------------------------------
# Tone parameters
# ---------------------------------------------------------------------------
TONE_FREQ_HZ = 400       # Frequency of the test tone
TONE_DURATION_S = 0.300   # Duration of each tone burst
TONE_AMPLITUDE = 0.5      # Peak amplitude (0–1, relative to full scale)
AUDIO_SAMPLE_RATE = 44100 # Sample rate for audio output

# ---------------------------------------------------------------------------
# Test parameters
# ---------------------------------------------------------------------------
BASELINE_DURATION_S = 1.0        # Silence duration for baseline estimation
N_WARMUP = 3                     # Warmup tones (not analysed)
N_TRIALS = 20                    # Number of measured tone presentations
POST_TONE_WAIT_S = 0.200         # Extra wait after each tone for ringdown
NOISE_FLOOR_SIGMAS = 5           # Minimum threshold as multiple of baseline std
SIGNAL_FRACTION = 0.3            # Threshold as fraction of calibrated peak signal


# ---------------------------------------------------------------------------
# Per-device state
# ---------------------------------------------------------------------------
@dataclass
class DeviceCtx:
    """Holds all per-device state for the latency test."""
    name: str
    dev_type: DeviceType
    channel_id: int
    channel_type: ChannelType

    session: Session | None = field(default=None, repr=False)
    chan_index: int = 0

    # Baseline estimation
    lock: threading.Lock = field(default_factory=threading.Lock)
    baseline_samples: list[np.ndarray] = field(default_factory=list)
    baseline_done: bool = False
    baseline_mean: float = 0.0
    baseline_std: float = 1.0
    threshold: float = math.inf

    # Onset detection — each entry: (trial_index, monotonic_s, device_ns)
    detections: list[tuple[int, float, int]] = field(default_factory=list)
    current_trial: int | None = None
    trial_detected: bool = False
    trial_earliest: float = 0.0  # reject detections with timestamp before this

    # Auto-calibration
    calibrating: bool = False
    calibration_samples: list[np.ndarray] = field(default_factory=list)

    # Monotonicity tracking
    last_mono_time: float | None = None
    mono_violations: int = 0
    mono_worst_ms: float = 0.0
    mono_total_samples: int = 0

    # Clock sync monitoring — each entry: (elapsed_s, offset_ns, uncertainty_ns)
    clock_log: list[tuple[float, int | None, int | None]] = field(
        default_factory=list
    )


# Map CLI argument to one or two device contexts.
DEVICE_PRESETS: dict[str, list[DeviceCtx]] = {
    "NSP": [
        DeviceCtx("NSP", DeviceType.NSP, channel_id=1, channel_type=ChannelType.ANALOG_IN),
    ],
    "HUB1": [
        DeviceCtx("HUB1", DeviceType.HUB1, channel_id=1, channel_type=ChannelType.FRONTEND),
    ],
    "BOTH": [
        DeviceCtx("NSP", DeviceType.NSP, channel_id=1, channel_type=ChannelType.ANALOG_IN),
        DeviceCtx("HUB1", DeviceType.HUB1, channel_id=1, channel_type=ChannelType.FRONTEND),
    ],
}


def generate_tone() -> np.ndarray:
    """Create a mono 400 Hz sine wave at the audio sample rate."""
    t = np.arange(int(AUDIO_SAMPLE_RATE * TONE_DURATION_S)) / AUDIO_SAMPLE_RATE
    # Apply a 5 ms raised-cosine ramp to avoid clicks
    ramp_samples = int(0.005 * AUDIO_SAMPLE_RATE)
    ramp = np.ones_like(t)
    ramp[:ramp_samples] = 0.5 * (1 - np.cos(np.pi * np.arange(ramp_samples) / ramp_samples))
    ramp[-ramp_samples:] = ramp[:ramp_samples][::-1]
    return (TONE_AMPLITUDE * np.sin(2 * np.pi * TONE_FREQ_HZ * t) * ramp).astype(np.float32)


def register_callback(ctx: DeviceCtx) -> None:
    """Register the streaming callback that performs baseline collection
    and onset detection for *ctx*."""
    sess = ctx.session

    @sess.on_group_batch(SampleRate.SR_RAW)
    def on_raw_batch(samples: np.ndarray, timestamps: np.ndarray):
        # -- Monotonicity check on every converted timestamp --
        for ts in timestamps:
            t = sess.device_to_monotonic(int(ts))
            ctx.mono_total_samples += 1
            if ctx.last_mono_time is not None and t < ctx.last_mono_time:
                ctx.mono_violations += 1
                jump_ms = (ctx.last_mono_time - t) * 1000
                ctx.mono_worst_ms = max(ctx.mono_worst_ms, jump_ms)
            ctx.last_mono_time = t

        ch_data = samples[:, ctx.chan_index].astype(np.float64)

        with ctx.lock:
            if not ctx.baseline_done:
                ctx.baseline_samples.append(ch_data.copy())
                return

            if ctx.calibrating:
                ctx.calibration_samples.append(ch_data.copy())
                return

            if ctx.current_trial is None or ctx.trial_detected:
                return

            abs_data = np.abs(ch_data - ctx.baseline_mean)
            above = np.where(abs_data > ctx.threshold)[0]
            if len(above) == 0:
                return

            onset_device_ns = int(timestamps[above[0]])
            onset_monotonic = sess.device_to_monotonic(onset_device_ns)

            # Reject detections from buffered data that predates this trial.
            if onset_monotonic < ctx.trial_earliest:
                return

            ctx.detections.append((ctx.current_trial, onset_monotonic, onset_device_ns))
            ctx.trial_detected = True


def _clock_monitor(
    devices: list[DeviceCtx],
    stop: threading.Event,
    t0: float,
    interval: float = 0.2,
) -> None:
    """Background thread: sample clock_offset_ns / uncertainty per device."""
    while not stop.is_set():
        elapsed = time.monotonic() - t0
        for ctx in devices:
            if ctx.session:
                offset = ctx.session.clock_offset_ns
                uncert = ctx.session.clock_uncertainty_ns
                ctx.clock_log.append((elapsed, offset, uncert))
        stop.wait(interval)


DATA_FALLBACK_UNCERT_NS = 700_000  # matches ONE_WAY_DELAY_ESTIMATE_NS in C++


def print_clock_timeline(ctx: DeviceCtx) -> None:
    """Print clock sync evolution and highlight probe→fallback transition."""
    if not ctx.clock_log:
        return

    # Compute a common base to make offsets readable
    first_offset = next(
        (o for _, o, _ in ctx.clock_log if o is not None), None
    )
    base_ns = first_offset if first_offset is not None else 0

    print(f"--- {ctx.name} clock sync timeline ---")
    if base_ns:
        print(f"  (offsets shown relative to {base_ns / 1e6:+.3f} ms)")

    prev_method: str | None = None
    transition_idx: int | None = None
    probe_offsets: list[int] = []
    data_offsets: list[int] = []

    for i, (elapsed, offset, uncert) in enumerate(ctx.clock_log):
        if offset is None:
            method = "none"
            offset_ms_str = "      N/A"
            uncert_ms_str = "  N/A"
        else:
            method = "data" if uncert == DATA_FALLBACK_UNCERT_NS else "probe"
            rel_ms = (offset - base_ns) / 1e6
            offset_ms_str = f"{rel_ms:+10.3f}"
            uncert_ms_str = f"{uncert / 1e6:5.2f}" if uncert is not None else "  N/A"
            if method == "probe":
                probe_offsets.append(offset)
            else:
                data_offsets.append(offset)

        marker = ""
        if prev_method is not None and method != prev_method:
            marker = f"  ← {prev_method}→{method}"
            if transition_idx is None:
                transition_idx = i
        prev_method = method

        print(f"  t={elapsed:5.1f}s  offset={offset_ms_str} ms  "
              f"uncert={uncert_ms_str} ms  [{method:5s}]{marker}")

    # Summary
    print()
    if probe_offsets:
        p = (np.array(probe_offsets) - base_ns) / 1e6
        print(f"  Probe phase:  {len(probe_offsets)} samples, "
              f"offset range [{np.min(p):+.3f}, {np.max(p):+.3f}] ms (rel), "
              f"spread {np.max(p) - np.min(p):.3f} ms")
    if data_offsets:
        d = (np.array(data_offsets) - base_ns) / 1e6
        print(f"  Data phase:   {len(data_offsets)} samples, "
              f"offset range [{np.min(d):+.3f}, {np.max(d):+.3f}] ms (rel), "
              f"spread {np.max(d) - np.min(d):.3f} ms")
        # Check if floor is holding (offset should only decrease or stay)
        decreases = sum(1 for a, b in zip(data_offsets, data_offsets[1:]) if b < a)
        holds = sum(1 for a, b in zip(data_offsets, data_offsets[1:]) if b == a)
        increases = sum(1 for a, b in zip(data_offsets, data_offsets[1:]) if b > a)
        print(f"  Data floor:   {decreases} decreases, {holds} holds, "
              f"{increases} increases (expect 0 increases if floor is enforced)")
    if transition_idx is not None:
        t_elapsed = ctx.clock_log[transition_idx][0]
        print(f"  Transition:   at t={t_elapsed:.1f}s (sample {transition_idx})")
        # Show offset jump at transition
        pre = ctx.clock_log[transition_idx - 1] if transition_idx > 0 else None
        post = ctx.clock_log[transition_idx]
        if pre and pre[1] is not None and post[1] is not None:
            jump_ms = (post[1] - pre[1]) / 1e6
            print(f"  Offset jump:  {(pre[1]-base_ns)/1e6:+.3f} → "
                  f"{(post[1]-base_ns)/1e6:+.3f} ms (rel) "
                  f"(Δ = {jump_ms:+.3f} ms)")
    elif probe_offsets and not data_offsets:
        print("  No fallback transition — stayed on probes throughout.")
    elif data_offsets and not probe_offsets:
        print("  Started directly in data-fallback mode (probes never reliable).")
    print()


def print_device_results(ctx: DeviceCtx, play_times: list[float]) -> None:
    """Print per-device latency results."""
    print(f"--- {ctx.name} ---")

    if not ctx.detections:
        print("  No tones detected!  Check cable and volume.")
        print()
        return

    latencies_ms: list[float] = []
    for trial_idx, detected_t, device_ns in ctx.detections:
        latency_ms = (detected_t - play_times[trial_idx]) * 1000
        latencies_ms.append(latency_ms)
        print(f"  Trial {trial_idx + 1:2d}: "
              f"mono={detected_t:.6f}  "
              f"dev_ns={device_ns}  "
              f"latency={latency_ms:+7.2f} ms")

    lat = np.array(latencies_ms)
    print()
    print(f"  Detected:  {len(ctx.detections)}/{N_TRIALS} trials")
    print(f"  Mean:      {np.mean(lat):+.2f} ms")
    print(f"  Median:    {np.median(lat):+.2f} ms")
    print(f"  Std:       {np.std(lat):.2f} ms")
    print(f"  Range:     [{np.min(lat):+.2f}, {np.max(lat):+.2f}] ms")

    uncert = ctx.session.clock_uncertainty_ns
    if uncert is not None:
        print(f"  Clock sync uncertainty: {uncert / 1e6:.2f} ms")

    print()
    print(f"  Timestamp monotonicity: {ctx.mono_total_samples} samples checked")
    if ctx.mono_violations == 0:
        print("  No backwards jumps detected.")
    elif ctx.mono_violations <= 2 and ctx.mono_worst_ms > 50:
        print(f"  {ctx.mono_violations} backwards jump(s) "
              f"(worst: {ctx.mono_worst_ms:.1f} ms) — initial clock sync "
              f"settling, not a steady-state issue.")
    else:
        print(f"  {ctx.mono_violations} backwards jumps "
              f"(worst: {ctx.mono_worst_ms:.3f} ms)")
    print()


def print_comparison(devices: list[DeviceCtx]) -> None:
    """Print cross-device onset comparison (BOTH mode)."""
    # Build trial_idx → (monotonic_s, device_ns) maps per device.
    det_maps = [
        {idx: (mono, dev_ns) for idx, mono, dev_ns in ctx.detections}
        for ctx in devices
    ]
    common_trials = sorted(
        set.intersection(*(set(d.keys()) for d in det_maps))
    )

    d0, d1 = devices[0].name, devices[1].name

    print("=" * 60)
    print("Cross-device comparison (PTP / device timestamps)")
    print("=" * 60)

    if not common_trials:
        print("  No trials detected by both devices — cannot compare.")
        return

    ptp_diffs_ms: list[float] = []
    for trial_idx in common_trials:
        ns0 = det_maps[0][trial_idx][1]
        ns1 = det_maps[1][trial_idx][1]
        diff_ms = (ns1 - ns0) / 1e6
        ptp_diffs_ms.append(diff_ms)
        print(f"  Trial {trial_idx + 1:2d}: "
              f"{d0}={ns0}  {d1}={ns1}  "
              f"Δ({d1}−{d0})={diff_ms:+.2f} ms")

    ptp = np.array(ptp_diffs_ms)
    print()
    print(f"  Mean Δ:    {np.mean(ptp):+.2f} ms")
    print(f"  Median Δ:  {np.median(ptp):+.2f} ms")
    print(f"  Std Δ:     {np.std(ptp):.2f} ms")
    print(f"  Range Δ:   [{np.min(ptp):+.2f}, {np.max(ptp):+.2f}] ms")
    print()
    print("  If PTP Δ ≈ 0, both device clocks agree and the bias is in")
    print("  device_to_monotonic.  If PTP Δ ≠ 0, the PTP clocks themselves")
    print("  are misaligned.")

    print()
    print("=" * 60)
    print("Cross-device comparison (converted monotonic timestamps)")
    print("=" * 60)

    mono_diffs_ms: list[float] = []
    for trial_idx in common_trials:
        t0 = det_maps[0][trial_idx][0]
        t1 = det_maps[1][trial_idx][0]
        diff_ms = (t1 - t0) * 1000
        mono_diffs_ms.append(diff_ms)
        print(f"  Trial {trial_idx + 1:2d}: "
              f"{d0}={t0:.6f}  {d1}={t1:.6f}  "
              f"Δ({d1}−{d0})={diff_ms:+.2f} ms")

    mono = np.array(mono_diffs_ms)
    print()
    print(f"  Mean Δ:    {np.mean(mono):+.2f} ms")
    print(f"  Median Δ:  {np.median(mono):+.2f} ms")
    print(f"  Std Δ:     {np.std(mono):.2f} ms")
    print(f"  Range Δ:   [{np.min(mono):+.2f}, {np.max(mono):+.2f}] ms")
    print()
    print("  Positive Δ means the second device detects the tone later.")
    print("  Mono Δ − PTP Δ = bias introduced by device_to_monotonic.")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--device-type",
        default="NSP",
        choices=["NSP", "HUB1", "BOTH"],
        help="Device(s) to test (default: NSP)",
    )
    args = parser.parse_args()

    devices = DEVICE_PRESETS[args.device_type.upper()]

    # ------------------------------------------------------------------
    # 1. Connect all devices
    # ------------------------------------------------------------------
    with contextlib.ExitStack() as stack:
        for ctx in devices:
            print(f"Connecting to {ctx.name}...")
            ctx.session = stack.enter_context(Session(device_type=ctx.dev_type))

        deadline = time.monotonic() + 10
        for ctx in devices:
            while not ctx.session.running:
                if time.monotonic() > deadline:
                    sys.exit(f"[{ctx.name}] Session did not start within 10 s "
                             "— is the device on?")
                time.sleep(0.1)
            print(f"  [{ctx.name}] Processor: {ctx.session.proc_ident}")
            print(f"  [{ctx.name}] Clock uncertainty: "
                  f"{(ctx.session.clock_uncertainty_ns or 0) / 1e6:.2f} ms")
        print()

        # Start clock sync monitor immediately
        clock_stop = threading.Event()
        clock_t0 = time.monotonic()
        clock_thread = threading.Thread(
            target=_clock_monitor,
            args=(devices, clock_stop, clock_t0),
            daemon=True,
        )
        clock_thread.start()

        # --------------------------------------------------------------
        # 2. Configure channels and register callbacks
        # --------------------------------------------------------------
        for ctx in devices:
            sess = ctx.session
            sess.set_channel_smpgroup(ctx.channel_id, SampleRate.SR_RAW)
            sess.sync()
            sess.set_ac_input_coupling(1, ctx.channel_type, False)
            sess.sync()

            raw_channels = sess.get_group_channels(int(SampleRate.SR_RAW))
            if ctx.channel_id not in raw_channels:
                sys.exit(f"[{ctx.name}] Channel {ctx.channel_id} not in RAW "
                         f"group (got: {raw_channels})")
            ctx.chan_index = raw_channels.index(ctx.channel_id)

            print(f"  [{ctx.name}] Channel {ctx.channel_id} configured: "
                  f"SR_RAW, DC coupling ({ctx.channel_type.name})")
            register_callback(ctx)

        # --------------------------------------------------------------
        # 3. Prepare tone
        # --------------------------------------------------------------
        tone = generate_tone()
        print(f"\n  Tone: {TONE_FREQ_HZ} Hz, {TONE_DURATION_S * 1000:.0f} ms, "
              f"{AUDIO_SAMPLE_RATE} Hz audio sample rate\n")

        # --------------------------------------------------------------
        # 4. Collect baseline (all devices simultaneously)
        # --------------------------------------------------------------
        print(f"Collecting {BASELINE_DURATION_S} s of baseline (silence)...")
        time.sleep(BASELINE_DURATION_S)

        for ctx in devices:
            with ctx.lock:
                all_baseline = np.concatenate(ctx.baseline_samples)
                ctx.baseline_mean = float(np.mean(all_baseline))
                ctx.baseline_std = float(np.std(all_baseline))
                ctx.baseline_done = True
            print(f"  [{ctx.name}] Baseline: mean={ctx.baseline_mean:.1f}  "
                  f"std={ctx.baseline_std:.1f}  "
                  f"({len(all_baseline)} samples)")
            if ctx.baseline_std < 1.0:
                print(f"  [{ctx.name}] WARNING: baseline std is very low "
                      "— is the cable connected?")
        print()

        # --------------------------------------------------------------
        # 5. Set up audio output with callback-based timing
        # --------------------------------------------------------------
        tone_frames = len(tone)
        tone_pos = 0
        tone_pending = False
        play_times: list[float] = []
        play_lock = threading.Lock()

        def audio_callback(outdata, frames, time_info, status):
            nonlocal tone_pos, tone_pending
            with play_lock:
                if tone_pending and tone_pos == 0:
                    play_times.append(time.monotonic())
                    tone_pending = False

                if tone_pos < tone_frames:
                    end = min(tone_pos + frames, tone_frames)
                    n = end - tone_pos
                    outdata[:n, 0] = tone[tone_pos:end]
                    outdata[n:, 0] = 0.0
                    tone_pos = end
                else:
                    outdata[:, 0] = 0.0

        stream = sd.OutputStream(
            samplerate=AUDIO_SAMPLE_RATE,
            channels=1,
            dtype="float32",
            latency="low",
            callback=audio_callback,
        )
        stream.start()
        print(f"  Audio output latency: {stream.latency * 1000:.1f} ms")

        # --------------------------------------------------------------
        # 6. Warmup tones
        # --------------------------------------------------------------
        print(f"  Warming up ({N_WARMUP} tones)...")
        for _ in range(N_WARMUP):
            with play_lock:
                tone_pos = 0
                tone_pending = True
            time.sleep(TONE_DURATION_S + POST_TONE_WAIT_S)
        play_times.clear()

        # --------------------------------------------------------------
        # 7. Auto-calibrate thresholds: play one tone with a warm audio
        #    pipeline and measure peak signal level on each device.
        # --------------------------------------------------------------
        print("  Calibrating thresholds...")
        for ctx in devices:
            with ctx.lock:
                ctx.calibrating = True
        with play_lock:
            tone_pos = 0
            tone_pending = True
        time.sleep(TONE_DURATION_S + POST_TONE_WAIT_S)
        play_times.clear()

        for ctx in devices:
            with ctx.lock:
                ctx.calibrating = False
                if ctx.calibration_samples:
                    cal_data = np.concatenate(ctx.calibration_samples)
                    peak_dev = float(
                        np.max(np.abs(cal_data - ctx.baseline_mean))
                    )
                    noise_floor = NOISE_FLOOR_SIGMAS * ctx.baseline_std
                    ctx.threshold = max(noise_floor, SIGNAL_FRACTION * peak_dev)
                else:
                    peak_dev = 0.0
                    ctx.threshold = NOISE_FLOOR_SIGMAS * ctx.baseline_std
            if peak_dev > 0:
                print(f"  [{ctx.name}] Peak: {peak_dev:.1f}  "
                      f"Threshold: ±{ctx.threshold:.1f} "
                      f"({ctx.threshold / ctx.baseline_std:.1f}σ, "
                      f"{100 * ctx.threshold / peak_dev:.0f}% of peak)")
            else:
                print(f"  [{ctx.name}] WARNING: no calibration signal, "
                      f"using {NOISE_FLOOR_SIGMAS}σ threshold")
        print()

        # --------------------------------------------------------------
        # 7. Measured trials
        # --------------------------------------------------------------
        for i in range(N_TRIALS):
            now = time.monotonic()
            for ctx in devices:
                with ctx.lock:
                    ctx.current_trial = i
                    ctx.trial_detected = False
                    ctx.trial_earliest = now

            with play_lock:
                tone_pos = 0
                tone_pending = True

            time.sleep(TONE_DURATION_S + POST_TONE_WAIT_S)

            statuses = []
            for ctx in devices:
                with ctx.lock:
                    statuses.append(
                        "detected" if ctx.trial_detected else "MISSED"
                    )
            status_str = "  ".join(
                f"{ctx.name}={s}" for ctx, s in zip(devices, statuses)
            )
            print(f"  Trial {i + 1:2d}/{N_TRIALS}: {status_str}")

        stream.stop()
        stream.close()

        for ctx in devices:
            with ctx.lock:
                ctx.current_trial = None

        # Stop clock monitor
        clock_stop.set()
        clock_thread.join(timeout=2)

        # --------------------------------------------------------------
        # 8. Report results
        # --------------------------------------------------------------
        print()
        print("=" * 60)
        print("Results")
        print("=" * 60)
        print()

        for ctx in devices:
            print_clock_timeline(ctx)

        any_detected = False
        for ctx in devices:
            if ctx.detections:
                any_detected = True
            print_device_results(ctx, play_times)

        if not any_detected:
            print("No tones were detected on any device!  Check:")
            print("  • Are the cables connected?")
            print("  • Is the computer volume turned up?")
            print("  • Try increasing TONE_AMPLITUDE or decreasing "
                  "THRESHOLD_SIGMAS.")
            sys.exit(1)

        if len(devices) > 1:
            print_comparison(devices)

        print()
        print("The latency includes: OS audio buffer → DAC → cable → "
              "device ADC → UDP → clock conversion.")


if __name__ == "__main__":
    main()
