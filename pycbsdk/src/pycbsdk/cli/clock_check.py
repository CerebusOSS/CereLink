"""Validate device-to-host clock conversion.

Receives live packets via callbacks, converts each ``header.time`` to
``time.monotonic()`` via ``session.device_to_monotonic()``, and compares
against the actual ``time.monotonic()`` at arrival.  The difference is the
one-way delivery latency — it should be small and stable when clock sync
is working correctly.

Reports the detected protocol version.  Protocol 4.0+ uses native PTP
nanosecond timestamps; protocol 3.11 uses 30 kHz sample counters that the
protocol wrapper upconverts to nanoseconds.  Clock sync should work in both
cases since the probe echo uses the same time base as data packets.

By default listens for event packets (spikes).  Use ``--group`` to listen
on a continuous sample group instead (useful when no spike channels are
configured).

Usage::

    python -m pycbsdk.cli.clock_check HUB1
    python -m pycbsdk.cli.clock_check HUB1 --interval 2.0
    python -m pycbsdk.cli.clock_check HUB1 --group 6
"""

from __future__ import annotations

import argparse
import sys
import time
import threading
from collections import deque

from pycbsdk import DeviceType, SampleRate, Session
from pycbsdk.session import ProtocolVersion


class ClockChecker:
    """Accumulates packet arrival-vs-converted timestamps."""

    def __init__(self, session: Session, window_sec: float):
        self._session = session
        self._window = window_sec
        self._lock = threading.Lock()
        # (arrival_mono, converted_mono, device_ns)
        self._samples: deque[tuple[float, float, int]] = deque()
        self._total = 0

    def on_packet(self, header, data) -> None:
        arrival = time.monotonic()
        try:
            converted = self._session.device_to_monotonic(header.time)
        except Exception:
            return
        with self._lock:
            self._samples.append((arrival, converted, header.time))
            self._total += 1

    def snapshot(self) -> dict | None:
        cutoff = time.monotonic() - self._window
        with self._lock:
            while self._samples and self._samples[0][0] < cutoff:
                self._samples.popleft()
            samples = list(self._samples)
            total = self._total
        if not samples:
            return None

        # latency = arrival - converted  (positive means packet arrived
        # after the converted timestamp, i.e. normal delivery delay)
        latencies = [(a - c) * 1000 for a, c, _ in samples]
        return {
            "n_window": len(samples),
            "n_total": total,
            "min_ms": min(latencies),
            "max_ms": max(latencies),
            "mean_ms": sum(latencies) / len(latencies),
            "last_latency_ms": latencies[-1],
            "last_device_ns": samples[-1][2],
            "last_arrival": samples[-1][0],
        }


def clock_check(
    device_type: DeviceType,
    interval: float = 1.0,
    group: int | None = None,
    timeout: float = 10.0,
) -> None:
    """Connect to a device and print clock-conversion diagnostics.

    Args:
        device_type: Device to connect to.
        interval: Seconds between reports.
        group: If set, listen on this sample group instead of events.
        timeout: Connection timeout in seconds.
    """
    with Session(device_type=device_type) as session:
        deadline = time.monotonic() + timeout
        while not session.running:
            if time.monotonic() > deadline:
                raise TimeoutError(
                    f"Session for {device_type.name} did not start within {timeout}s"
                )
            time.sleep(0.1)

        proto = session.protocol_version
        ts_kind = (
            "30 kHz ticks (upconverted to ns)"
            if proto == ProtocolVersion.V3_11
            else "PTP nanoseconds"
        )
        print(
            f"Connected to {device_type.name}  protocol: {proto.name}  "
            f"timestamps: {ts_kind}"
        )

        if proto == ProtocolVersion.UNKNOWN:
            print(
                "WARNING: Protocol version unknown — results may be unreliable.",
                file=sys.stderr,
            )

        # Wait for at least one clock sync
        print("Waiting for clock sync ...")
        while session.clock_offset_ns is None:
            if time.monotonic() > deadline:
                raise TimeoutError("Clock sync did not arrive within timeout")
            session.send_clock_probe()
            time.sleep(0.25)

        checker = ClockChecker(session, window_sec=interval)

        if group is not None:
            rate = SampleRate(group)
            source = f"group {rate.name}"

            @session.on_group(rate)
            def _on_group(header, data):
                checker.on_packet(header, data)
        else:
            source = "events (all channel types)"

            @session.on_event(None)
            def _on_event(header, data):
                checker.on_packet(header, data)

        print(f"Listening on {source} ...")

        # Wait for at least one packet to arrive so we can sanity-check
        # that data-packet timestamps and clock-probe timestamps share
        # the same time base.
        pkt_deadline = time.monotonic() + timeout
        while True:
            snap = checker.snapshot()
            if snap is not None:
                break
            if time.monotonic() > pkt_deadline:
                raise TimeoutError(
                    f"No packets received within {timeout}s — try --group"
                )
            time.sleep(0.1)

        first_latency_ms = snap["mean_ms"]
        offset_ns = session.clock_offset_ns
        offset_s = offset_ns / 1e9 if offset_ns is not None else float("nan")
        print(
            f"\nClock sanity check (first packets):\n"
            f"  Clock offset:     {offset_s:+.6f} s  "
            f"(device_ns - host_steady_ns)\n"
            f"  First data pkt:   {snap['last_device_ns']} ns  "
            f"({snap['last_device_ns'] / 1e9:.6f} s)\n"
            f"  Delivery latency: {first_latency_ms:+.3f} ms  "
            f"(arrival_mono - device_to_monotonic(pkt.time))"
        )
        if abs(first_latency_ms) > 1000:
            print(
                f"\n  WARNING: Latency magnitude > 1 s — data packet "
                f"timestamps and clock probe\n  responses appear to use "
                f"different time bases.  device_to_monotonic() results\n"
                f"  will not be meaningful for this device.",
                file=sys.stderr,
            )

        print(f"\nReporting every {interval}s ...\n")

        hdr = (
            f"{'Pkts':>6s}  {'Offset (ms)':>14s}  {'Uncert (ms)':>11s}  "
            f"{'Latency min':>11s}  {'mean':>8s}  {'max':>8s}  {'last':>8s}"
        )
        print(hdr)
        print("─" * len(hdr))

        all_means: list[float] = []
        try:
            while True:
                time.sleep(interval)
                snap = checker.snapshot()

                offset_ns = session.clock_offset_ns
                uncert_ns = session.clock_uncertainty_ns
                offset_ms = offset_ns / 1e6 if offset_ns is not None else float("nan")
                uncert_ms = uncert_ns / 1e6 if uncert_ns is not None else float("nan")

                if snap is None:
                    print(
                        f"{'0':>6s}  {offset_ms:>14.3f}  {uncert_ms:>11.3f}"
                        f"  (no packets)"
                    )
                    continue

                all_means.append(snap["mean_ms"])
                print(
                    f"{snap['n_window']:>6d}  {offset_ms:>14.3f}  {uncert_ms:>11.3f}  "
                    f"{snap['min_ms']:>+11.3f}  {snap['mean_ms']:>+8.3f}  "
                    f"{snap['max_ms']:>+8.3f}  {snap['last_latency_ms']:>+8.3f}"
                )

        except KeyboardInterrupt:
            print()

        if all_means:
            print(
                f"\nReports: {len(all_means)}  "
                f"Mean latency  min: {min(all_means):+.3f} ms  "
                f"max: {max(all_means):+.3f} ms  "
                f"overall: {sum(all_means) / len(all_means):+.3f} ms"
            )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.clock_check",
        description="Validate device-to-host clock conversion.",
    )
    parser.add_argument(
        "device",
        nargs="?",
        default="NPLAY",
        help="Device type (default: NPLAY)",
    )
    parser.add_argument(
        "--interval",
        "-i",
        type=float,
        default=1.0,
        help="Seconds between reports (default: 1.0)",
    )
    parser.add_argument(
        "--group",
        "-g",
        type=int,
        default=None,
        help="Listen on sample group N instead of events. "
        f"Choices: {', '.join(f'{r.value}={r.name}' for r in SampleRate if r.value > 0)}",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Connection timeout in seconds (default: 10)",
    )
    args = parser.parse_args(argv)

    try:
        device_type = DeviceType[args.device.upper()]
    except KeyError:
        names = ", ".join(dt.name for dt in DeviceType if dt != DeviceType.CUSTOM)
        parser.error(f"Unknown device: {args.device}. Choices: {names}")
        return 1

    try:
        clock_check(device_type, args.interval, group=args.group, timeout=args.timeout)
        return 0
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
