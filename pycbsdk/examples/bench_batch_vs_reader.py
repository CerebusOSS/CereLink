#!/usr/bin/env python3
"""
Benchmark: ContinuousReader vs on_group_batch callback.

Connects to an nPlay instance (or live device) and collects 30 kHz RAW data
for a configurable duration using both approaches, then reports timing and
throughput metrics.

Results on Macbook M4 Pro when using HUB1 Raw (30 kHz) 256-ch:

  ┌────────────────────┬──────────┬───────────┐
  │       Method       │ CPU time │ % of wall │
  ├────────────────────┼──────────┼───────────┤
  │ on_group(as_array) │ 0.586s   │ 11.7%     │
  ├────────────────────┼──────────┼───────────┤
  │ ContinuousReader   │ 0.562s   │ 11.2%     │
  ├────────────────────┼──────────┼───────────┤
  │ on_group_batch     │ 0.466s   │ 9.3%      │
  └────────────────────┴──────────┴───────────┘

Usage:
    python bench_batch_vs_reader.py [--device NPLAY] [--seconds 5]
"""

import argparse
import os
import time
import threading
import numpy as np
from pycbsdk import Session, DeviceType, SampleRate


def bench_continuous_reader(device_type: str, seconds: float):
    """Collect data using ContinuousReader (per-packet callback internally)."""
    with Session(device_type) as session:
        n_channels = len(session.get_group_channels(int(SampleRate.SR_RAW)))
        print(f"  Channels: {n_channels}")

        reader = session.continuous_reader(rate=SampleRate.SR_RAW, buffer_seconds=seconds + 2)
        session.reset_stats()

        cpu0 = time.process_time()
        t0 = time.perf_counter()
        time.sleep(seconds)
        t1 = time.perf_counter()
        cpu1 = time.process_time()

        data = reader.read()
        stats = session.stats
        reader.close()

    elapsed = t1 - t0
    cpu_time = cpu1 - cpu0
    n_samples = data.shape[1]
    print(f"  Elapsed:  {elapsed:.3f} s")
    print(f"  CPU time: {cpu_time:.3f} s  ({cpu_time/elapsed*100:.1f}% of wall)")
    print(f"  Samples:  {n_samples}")
    print(f"  Rate:     {n_samples / elapsed:.0f} samples/s")
    print(f"  Shape:    {data.shape}")
    print(f"  Received: {stats.packets_received} pkts")
    print(f"  Delivered:{stats.packets_delivered} pkts")
    print(f"  Dropped:  {stats.packets_dropped} pkts")
    return n_samples, elapsed, cpu_time


def bench_group_batch(device_type: str, seconds: float):
    """Collect data using on_group_batch (batch callback)."""
    batch_count = [0]

    with Session(device_type) as session:
        n_channels = len(session.get_group_channels(int(SampleRate.SR_RAW)))
        print(f"  Channels: {n_channels}")

        # Pre-allocate ring buffer — [samples, channels], C-contiguous
        max_samples = int(30000 * (seconds + 2))
        ring = np.zeros((max_samples, n_channels), dtype=np.int16)
        write_pos = [0]

        @session.on_group_batch(SampleRate.SR_RAW)
        def on_batch(samples, timestamps):
            # samples: (n_samples, n_channels), timestamps: (n_samples,)
            n = samples.shape[0]
            pos = write_pos[0]
            if pos + n <= max_samples:
                ring[pos:pos + n, :] = samples
            write_pos[0] = pos + n
            batch_count[0] += 1

        session.reset_stats()

        cpu0 = time.process_time()
        t0 = time.perf_counter()
        time.sleep(seconds)
        t1 = time.perf_counter()
        cpu1 = time.process_time()

        stats = session.stats

    elapsed = t1 - t0
    cpu_time = cpu1 - cpu0
    n_samples = write_pos[0]
    print(f"  Elapsed:  {elapsed:.3f} s")
    print(f"  CPU time: {cpu_time:.3f} s  ({cpu_time/elapsed*100:.1f}% of wall)")
    print(f"  Samples:  {n_samples}")
    print(f"  Rate:     {n_samples / elapsed:.0f} samples/s")
    print(f"  Batches:  {batch_count[0]}")
    if batch_count[0] > 0:
        print(f"  Avg batch:{n_samples / batch_count[0]:.1f} samples/batch")
    print(f"  Received: {stats.packets_received} pkts")
    print(f"  Delivered:{stats.packets_delivered} pkts")
    print(f"  Dropped:  {stats.packets_dropped} pkts")
    return n_samples, elapsed, cpu_time


def bench_on_group_numpy(device_type: str, seconds: float):
    """Collect data using on_group(as_array=True) — per-packet numpy callback."""
    with Session(device_type) as session:
        n_channels = len(session.get_group_channels(int(SampleRate.SR_RAW)))
        print(f"  Channels: {n_channels}")

        max_samples = int(30000 * (seconds + 2))
        ring = np.zeros((n_channels, max_samples), dtype=np.int16)
        write_pos = [0]

        @session.on_group(SampleRate.SR_RAW, as_array=True)
        def on_sample(header, arr):
            pos = write_pos[0]
            if pos < max_samples:
                ring[:, pos] = arr
            write_pos[0] = pos + 1

        session.reset_stats()

        cpu0 = time.process_time()
        t0 = time.perf_counter()
        time.sleep(seconds)
        t1 = time.perf_counter()
        cpu1 = time.process_time()

        stats = session.stats

    elapsed = t1 - t0
    cpu_time = cpu1 - cpu0
    n_samples = write_pos[0]
    print(f"  Elapsed:  {elapsed:.3f} s")
    print(f"  CPU time: {cpu_time:.3f} s  ({cpu_time/elapsed*100:.1f}% of wall)")
    print(f"  Samples:  {n_samples}")
    print(f"  Rate:     {n_samples / elapsed:.0f} samples/s")
    print(f"  Callbacks:{n_samples}")
    print(f"  Received: {stats.packets_received} pkts")
    print(f"  Delivered:{stats.packets_delivered} pkts")
    print(f"  Dropped:  {stats.packets_dropped} pkts")
    return n_samples, elapsed, cpu_time


def main():
    parser = argparse.ArgumentParser(description="Benchmark batch vs reader")
    parser.add_argument("--device", default="NPLAY", help="Device type (default: NPLAY)")
    parser.add_argument("--seconds", type=float, default=5.0, help="Collection duration (default: 5)")
    args = parser.parse_args()

    device = args.device.upper()
    secs = args.seconds

    print(f"=== Benchmark: {device}, {secs}s of 30kHz RAW data ===")
    print(f"    PID {os.getpid()}\n")

    print("[1] on_group(as_array=True) — per-packet numpy callback")
    n0, t0, c0 = bench_on_group_numpy(device, secs)

    time.sleep(1)

    print(f"\n[2] ContinuousReader — per-packet callback into ring buffer")
    n1, t1, c1 = bench_continuous_reader(device, secs)

    time.sleep(1)

    print(f"\n[3] on_group_batch — batch callback")
    n2, t2, c2 = bench_group_batch(device, secs)

    print(f"\n{'='*60}")
    print(f"{'Method':<35} {'Samples':>8} {'CPU':>7} {'Drop%':>7}")
    print(f"{'-'*60}")
    for label, n, t, c in [
        ("on_group(as_array)", n0, t0, c0),
        ("ContinuousReader", n1, t1, c1),
        ("on_group_batch", n2, t2, c2),
    ]:
        drop_pct = (1 - n / (30000 * t)) * 100
        print(f"  {label:<33} {n:>8} {c:>6.3f}s {drop_pct:>6.2f}%")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
