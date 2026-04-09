#!/usr/bin/env python3
"""Analog Input Latency Test — measures the end-to-end timing from host
audio output to NSP analog input, demonstrating pycbsdk's clock
synchronisation and timestamp conversion.

Hardware setup
--------------
1. Take a **3.5 mm (headphone) → BNC** cable.
2. Plug the 3.5 mm end into your computer's **headphone jack**.
3. Connect the BNC end to **Analog Input 1** on the NSP front panel.
4. Set your computer's volume to approximately 50 %.  The tone only needs
   to be large enough to cross a simple amplitude threshold — if the
   detected signal is too weak, increase the volume and re-run.

What the test does
------------------
1. Connects to the NSP and configures Analog Input channel 1 for raw
   (30 kHz) sampling with DC coupling.
2. Collects ~1 second of silence to establish a baseline (mean and
   standard deviation of the noise floor).
3. Plays a 400 Hz sine tone (300 ms) through the computer speakers
   ten times, recording ``time.monotonic()`` at the instant each tone
   begins.
4. In the streaming callback, converts each sample's device timestamp
   to ``time.monotonic()`` and watches for the first sample whose
   absolute value exceeds ``baseline_mean + 5 * baseline_std``.
   That crossing time is the **detected onset** of each tone.
5. After all tones have been played, reports the latency
   (``detected_time − play_time``) for each trial plus summary
   statistics (mean, median, range, std).

Expected results
~~~~~~~~~~~~~~~~
Latency includes the computer's audio output buffer, the DAC, the
cable, the NSP's ADC, and the UDP network path.  Typical values are
**5–30 ms** depending on the OS audio stack.  The spread (std) should
be < 5 ms if clock synchronisation is working correctly.

Requirements
~~~~~~~~~~~~
* ``pycbsdk`` (installed from this repository)
* ``numpy``
* ``sounddevice`` — ``pip install sounddevice``

Usage::

    python analog_latency_test.py [--device-type NSP]
"""

from __future__ import annotations

import argparse
import math
import sys
import threading
import time

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
CHANNEL_ID = 1                   # 1-based analog input channel
BASELINE_DURATION_S = 1.0        # Silence duration for baseline estimation
N_WARMUP = 3                     # Warmup tones (not analysed)
N_TRIALS = 10                    # Number of measured tone presentations
POST_TONE_WAIT_S = 0.200         # Extra wait after each tone for ringdown
THRESHOLD_SIGMAS = 5             # Onset threshold = mean + N * std


def generate_tone() -> np.ndarray:
    """Create a mono 400 Hz sine wave at the audio sample rate."""
    t = np.arange(int(AUDIO_SAMPLE_RATE * TONE_DURATION_S)) / AUDIO_SAMPLE_RATE
    # Apply a 5 ms raised-cosine ramp to avoid clicks
    ramp_samples = int(0.005 * AUDIO_SAMPLE_RATE)
    ramp = np.ones_like(t)
    ramp[:ramp_samples] = 0.5 * (1 - np.cos(np.pi * np.arange(ramp_samples) / ramp_samples))
    ramp[-ramp_samples:] = ramp[:ramp_samples][::-1]
    return (TONE_AMPLITUDE * np.sin(2 * np.pi * TONE_FREQ_HZ * t) * ramp).astype(np.float32)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--device-type", default="NSP",
                        help="pycbsdk DeviceType name (default: NSP)")
    args = parser.parse_args()

    dev_type = DeviceType[args.device_type.upper()]

    # -----------------------------------------------------------------------
    # 1. Connect and configure
    # -----------------------------------------------------------------------
    print(f"Connecting to {dev_type.name}...")
    with Session(device_type=dev_type) as sess:
        deadline = time.monotonic() + 10
        while not sess.running:
            if time.monotonic() > deadline:
                sys.exit("Session did not start within 10 s — is the device on?")
            time.sleep(0.1)

        print(f"  Processor: {sess.proc_ident}")
        print(f"  Clock uncertainty: {(sess.clock_uncertainty_ns or 0) / 1e6:.2f} ms")
        print()

        # Configure: 1 channel, raw (30 kHz), DC coupling
        sess.set_channel_smpgroup(CHANNEL_ID, SampleRate.SR_RAW)
        sess.set_ac_input_coupling(1, ChannelType.ANALOG_IN, False)
        sess.sync()
        print(f"  Channel {CHANNEL_ID} configured: SR_RAW, DC coupling")

        raw_channels = sess.get_group_channels(int(SampleRate.SR_RAW))
        if CHANNEL_ID not in raw_channels:
            sys.exit(f"Channel {CHANNEL_ID} not in RAW group — check device type "
                     f"(got group: {raw_channels})")

        # Work out which index our channel occupies in the group data array.
        # The on_group_batch callback delivers an (n_samples, n_channels)
        # array where columns correspond to get_group_channels() order.
        chan_index = raw_channels.index(CHANNEL_ID)

        # -------------------------------------------------------------------
        # 2. Prepare tone
        # -------------------------------------------------------------------
        tone = generate_tone()
        print(f"  Tone: {TONE_FREQ_HZ} Hz, {TONE_DURATION_S*1000:.0f} ms, "
              f"{AUDIO_SAMPLE_RATE} Hz audio sample rate")
        print()

        # -------------------------------------------------------------------
        # 3. Register streaming callback
        # -------------------------------------------------------------------
        # Shared state between callback and main thread.
        # The callback runs on the SDK's internal thread, so we use a lock.
        lock = threading.Lock()
        baseline_samples: list[np.ndarray] = []
        baseline_done = False
        baseline_mean = 0.0
        baseline_std = 1.0
        threshold = math.inf  # will be set after baseline

        # Each entry: (trial_index, detected_monotonic_time)
        detections: list[tuple[int, float]] = []
        current_trial: int | None = None  # set by main thread when tone plays
        trial_detected = False

        # Monotonicity tracking: detect backwards jumps in converted timestamps
        last_mono_time: float | None = None
        mono_violations = 0
        mono_worst_ms = 0.0
        mono_total_samples = 0

        @sess.on_group_batch(SampleRate.SR_RAW)
        def on_raw_batch(samples: np.ndarray, timestamps: np.ndarray):
            """Process a batch of raw samples.

            Parameters
            ----------
            samples : ndarray, shape (n_samples, n_channels), dtype int16
                Raw ADC values for all channels in the RAW group.
            timestamps : ndarray, shape (n_samples,), dtype uint64
                Device timestamps in nanoseconds for each sample.
            """
            nonlocal baseline_done, baseline_mean, baseline_std, threshold
            nonlocal current_trial, trial_detected
            nonlocal last_mono_time, mono_violations, mono_worst_ms, mono_total_samples

            # Check every converted timestamp for monotonicity.
            for ts in timestamps:
                t = sess.device_to_monotonic(int(ts))
                mono_total_samples += 1
                if last_mono_time is not None and t < last_mono_time:
                    mono_violations += 1
                    jump_ms = (last_mono_time - t) * 1000
                    mono_worst_ms = max(mono_worst_ms, jump_ms)
                last_mono_time = t

            # Extract our channel's column
            ch_data = samples[:, chan_index].astype(np.float64)

            with lock:
                if not baseline_done:
                    # Accumulate baseline samples
                    baseline_samples.append(ch_data.copy())
                    return

                if current_trial is None or trial_detected:
                    return  # Not in an active trial, or already detected this one

                # Look for the first sample exceeding the threshold.
                abs_data = np.abs(ch_data - baseline_mean)
                above = np.where(abs_data > threshold)[0]
                if len(above) == 0:
                    return

                # Convert the device timestamp of the onset sample to
                # time.monotonic() — this is the key demonstration.
                onset_device_ns = int(timestamps[above[0]])
                onset_monotonic = sess.device_to_monotonic(onset_device_ns)

                detections.append((current_trial, onset_monotonic))
                trial_detected = True

        # -------------------------------------------------------------------
        # 4. Collect baseline
        # -------------------------------------------------------------------
        print(f"Collecting {BASELINE_DURATION_S} s of baseline (silence)...")
        time.sleep(BASELINE_DURATION_S)

        with lock:
            all_baseline = np.concatenate(baseline_samples)
            baseline_mean = float(np.mean(all_baseline))
            baseline_std = float(np.std(all_baseline))
            threshold = THRESHOLD_SIGMAS * baseline_std
            baseline_done = True

        print(f"  Baseline: mean={baseline_mean:.1f}  std={baseline_std:.1f}  "
              f"threshold=±{threshold:.1f}  ({len(all_baseline)} samples)")
        if baseline_std < 1.0:
            print("  WARNING: baseline std is very low — is the cable connected?")
        print()

        # -------------------------------------------------------------------
        # 5–7. Play tones and record play times
        # -------------------------------------------------------------------
        # Use a callback-based output stream for precise timing.
        # The callback fires every time the audio driver needs a new
        # buffer.  When we want to play a tone, we set ``tone_pending``
        # and record ``time.monotonic()`` inside the FIRST callback that
        # copies tone data — that is the closest we can get to the
        # actual moment audio exits the OS mixer.  The remaining
        # latency after that point (driver buffer → DAC → cable → ADC)
        # is hardware-fixed and consistent.
        tone_frames = len(tone)
        tone_pos = 0        # current read position in tone array
        tone_pending = False # main thread sets True to trigger a tone
        play_times: list[float] = []
        play_lock = threading.Lock()

        def audio_callback(outdata, frames, time_info, status):
            nonlocal tone_pos, tone_pending
            with play_lock:
                if tone_pending and tone_pos == 0:
                    # First callback after tone was armed — record the
                    # moment we start filling the driver's buffer.
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

        # Warmup: play a few tones to prime the audio pipeline.
        # CoreAudio (macOS) buffers the first 1-2 callbacks before the
        # DAC actually starts, adding ~250 ms to the first tones.
        print(f"  Warming up ({N_WARMUP} tones)...")
        for _ in range(N_WARMUP):
            with play_lock:
                tone_pos = 0
                tone_pending = True
            time.sleep(TONE_DURATION_S + POST_TONE_WAIT_S)
        # Discard any play_times recorded during warmup
        play_times.clear()
        print()

        for i in range(N_TRIALS):
            with lock:
                current_trial = i
                trial_detected = False

            # Arm the tone: the audio callback will record the play time
            # on the very first buffer fill.
            with play_lock:
                tone_pos = 0
                tone_pending = True

            # Wait for the tone to finish playing out plus margin.
            time.sleep(TONE_DURATION_S + POST_TONE_WAIT_S)

            with lock:
                status = "detected" if trial_detected else "MISSED"
            print(f"  Trial {i+1:2d}/{N_TRIALS}: {status}")

        stream.stop()
        stream.close()

        # Stop looking for onsets
        with lock:
            current_trial = None

        # -------------------------------------------------------------------
        # 8–9. Report results
        # -------------------------------------------------------------------
        print()
        print("=" * 60)
        print("Results")
        print("=" * 60)

        if not detections:
            print("No tones were detected!  Check:")
            print("  • Is the cable connected to Analog Input 1?")
            print("  • Is the computer volume turned up?")
            print("  • Try increasing TONE_AMPLITUDE or decreasing THRESHOLD_SIGMAS.")
            sys.exit(1)

        latencies_ms: list[float] = []
        for trial_idx, detected_t in detections:
            latency_ms = (detected_t - play_times[trial_idx]) * 1000
            latencies_ms.append(latency_ms)
            print(f"  Trial {trial_idx+1:2d}: "
                  f"play={play_times[trial_idx]:.6f}  "
                  f"detect={detected_t:.6f}  "
                  f"latency={latency_ms:+7.2f} ms")

        lat = np.array(latencies_ms)
        print()
        print(f"  Detected:  {len(detections)}/{N_TRIALS} trials")
        print(f"  Mean:      {np.mean(lat):+.2f} ms")
        print(f"  Median:    {np.median(lat):+.2f} ms")
        print(f"  Std:       {np.std(lat):.2f} ms")
        print(f"  Range:     [{np.min(lat):+.2f}, {np.max(lat):+.2f}] ms")
        print()

        uncert = sess.clock_uncertainty_ns
        if uncert is not None:
            print(f"  Clock sync uncertainty: {uncert / 1e6:.2f} ms")
            print(f"  (Latency std should be comparable to this value.)")

        print()
        print(f"  Timestamp monotonicity: {mono_total_samples} samples checked")
        if mono_violations == 0:
            print("  No backwards jumps detected.")
        elif mono_violations <= 2 and mono_worst_ms > 50:
            # A single large jump is the expected one-time correction when
            # the clock sync switches from probe-based to data-packet-based
            # offset estimation (happens ~4-6 s after connect).
            print(f"  {mono_violations} backwards jump(s) "
                  f"(worst: {mono_worst_ms:.1f} ms) — initial clock sync "
                  f"settling, not a steady-state issue.")
        else:
            print(f"  {mono_violations} backwards jumps "
                  f"(worst: {mono_worst_ms:.3f} ms)")

        print()
        print("The latency includes: OS audio buffer → DAC → cable → "
              "NSP ADC → UDP → clock conversion.")


if __name__ == "__main__":
    main()
