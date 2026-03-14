"""Live spike rate monitor for Cerebus devices.

Connects to a device, listens for spike events, and periodically prints
per-channel firing rate statistics.

Usage::

    python -m pycbsdk.cli.spike_rates                  # defaults: NPLAY, 1s window
    python -m pycbsdk.cli.spike_rates HUB1             # specify device
    python -m pycbsdk.cli.spike_rates --interval 2.0   # update every 2s
    python -m pycbsdk.cli.spike_rates --top 10         # show top-10 channels
"""

from __future__ import annotations

import argparse
import sys
import time
import threading
from collections import deque

from pycbsdk import Session, DeviceType, ChannelType, ChanInfoField


# cbAINPSPK flags
_SPKOPTS_EXTRACT  = 0x0000_0001
_SPKOPTS_THRAUTO  = 0x0000_0400
_SPKOPTS_HOOPSORT = 0x0001_0000
_SPKOPTS_PCASORT  = 0x0006_0000  # all PCA manual + auto


def _format_rate(hz: float) -> str:
    if hz >= 100:
        return f"{hz:7.0f}"
    elif hz >= 10:
        return f"{hz:7.1f}"
    else:
        return f"{hz:7.2f}"


class SpikeRateMonitor:
    """Accumulates spike events and computes windowed firing rates."""

    def __init__(self, session: Session, channel_ids: list[int],
                 window_sec: float = 1.0):
        self._session = session
        self._window_sec = window_sec
        self._channel_ids = channel_ids
        self._ch_index = {ch: i for i, ch in enumerate(channel_ids)}
        n = len(channel_ids)

        self._lock = threading.Lock()
        # Per-channel ring buffers of monotonic-clock spike times (seconds).
        # Converted from device timestamps via session.device_to_monotonic(),
        # falling back to time.monotonic() if clock sync isn't ready.
        self._spike_times: list[deque[float]] = [deque() for _ in range(n)]
        self._total_spikes = [0] * n

    def on_spike(self, header, data) -> None:
        """Callback for spike event packets."""
        chid = header.chid
        idx = self._ch_index.get(chid)
        if idx is None:
            return

        now = time.monotonic()
        try:
            t = self._session.device_to_monotonic(header.time)
            # Reject if converted time is far from wall clock (e.g. clock sync
            # not converged, or NPlay looping with stale offset).
            if abs(t - now) > self._window_sec * 2:
                t = now
        except Exception:
            t = now

        with self._lock:
            self._spike_times[idx].append(t)
            self._total_spikes[idx] += 1

    def _prune(self) -> None:
        """Remove spikes older than the window. Must hold self._lock."""
        cutoff = time.monotonic() - self._window_sec
        for buf in self._spike_times:
            while buf and buf[0] < cutoff:
                buf.popleft()

    def snapshot(self) -> dict:
        """Return current rate statistics."""
        with self._lock:
            self._prune()
            counts = [len(buf) for buf in self._spike_times]
            total = list(self._total_spikes)

        rates = [c / self._window_sec for c in counts]
        active = [(self._channel_ids[i], rates[i], total[i])
                  for i in range(len(self._channel_ids)) if counts[i] > 0]
        active.sort(key=lambda x: x[1], reverse=True)

        n_active = len(active)
        mean_rate = sum(rates) / len(rates) if rates else 0.0
        total_rate = sum(rates)
        max_rate = max(rates) if rates else 0.0
        min_active = min(r for _, r, _ in active) if active else 0.0
        total_spikes = sum(total)

        return {
            "rates": rates,
            "active": active,
            "n_channels": len(self._channel_ids),
            "n_active": n_active,
            "mean_rate": mean_rate,
            "total_rate": total_rate,
            "max_rate": max_rate,
            "min_active_rate": min_active,
            "total_spikes": total_spikes,
        }


def print_stats(snap: dict, top_n: int = 0, show_channels: bool = True) -> None:
    """Print a compact rate summary."""
    n_ch = snap["n_channels"]
    n_active = snap["n_active"]
    mean = snap["mean_rate"]
    total = snap["total_rate"]
    active = snap["active"]

    # Header line
    print(f"\033[2J\033[H", end="")  # clear screen
    print(f"Spike Rate Monitor  |  {n_active}/{n_ch} channels active  |  "
          f"{snap['total_spikes']} total spikes")
    print(f"{'─' * 72}")

    # Aggregate stats
    if n_active > 0:
        print(f"  Mean rate (all channels):  {_format_rate(mean)} Hz")
        print(f"  Mean rate (active only):   {_format_rate(total / n_active)} Hz")
        print(f"  Total spike rate:          {_format_rate(total)} Hz")
        print(f"  Range (active):            {_format_rate(snap['min_active_rate'])} – "
              f"{_format_rate(snap['max_rate'])} Hz")
    else:
        print(f"  No spikes detected in window.")

    if show_channels and active:
        print(f"\n  {'Chan':>6s}  {'Rate (Hz)':>9s}  {'Total':>8s}  {'Bar'}")
        print(f"  {'─' * 6}  {'─' * 9}  {'─' * 8}  {'─' * 40}")

        display = active[:top_n] if top_n > 0 else active
        max_rate = active[0][1] if active else 1.0
        for chid, rate, total_count in display:
            bar_len = int(40 * rate / max_rate) if max_rate > 0 else 0
            bar = "█" * bar_len
            print(f"  {chid:>6d}  {_format_rate(rate)}  {total_count:>8d}  {bar}")

        if top_n > 0 and len(active) > top_n:
            print(f"  ... and {len(active) - top_n} more active channels")

    print()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.spike_rates",
        description="Live spike rate monitor for Cerebus devices.",
    )
    parser.add_argument(
        "device", nargs="?", default="NPLAY",
        help="Device type (default: NPLAY)",
    )
    parser.add_argument(
        "--interval", "-i", type=float, default=1.0,
        help="Update interval in seconds (default: 1.0)",
    )
    parser.add_argument(
        "--window", "-w", type=float, default=None,
        help="Rate window in seconds (default: same as interval)",
    )
    parser.add_argument(
        "--top", "-n", type=int, default=0,
        help="Show only top N channels by rate (default: all active)",
    )
    parser.add_argument(
        "--no-channels", action="store_true",
        help="Hide per-channel breakdown, show only aggregate stats",
    )
    args = parser.parse_args(argv)

    window = args.window if args.window is not None else args.interval

    try:
        device_type = DeviceType[args.device.upper()]
    except KeyError:
        names = ", ".join(dt.name for dt in DeviceType if dt != DeviceType.CUSTOM)
        parser.error(f"Unknown device: {args.device}. Choices: {names}")
        return 1

    print(f"Connecting to {device_type.name}...")
    try:
        with Session(device_type=device_type) as session:
            # Find spike-enabled frontend channels
            fe_ids = session.get_matching_channel_ids(ChannelType.FRONTEND)
            spkopts = session.get_channels_field(ChannelType.FRONTEND, ChanInfoField.SPKOPTS)
            spike_ids = [ch for ch, opts in zip(fe_ids, spkopts)
                         if opts & _SPKOPTS_EXTRACT]

            if not spike_ids:
                print("No channels with spike processing enabled.")
                return 1

            # Summarise threshold config
            n_auto = sum(1 for opts in spkopts if opts & _SPKOPTS_EXTRACT and opts & _SPKOPTS_THRAUTO)
            n_manual = len(spike_ids) - n_auto
            n_sorting = sum(1 for opts in spkopts
                            if opts & _SPKOPTS_EXTRACT and opts & (_SPKOPTS_HOOPSORT | _SPKOPTS_PCASORT))
            print(f"Monitoring {len(spike_ids)} spike-enabled channels "
                  f"({n_auto} auto-threshold, {n_manual} manual-threshold, "
                  f"{n_sorting} with sorting)")

            monitor = SpikeRateMonitor(session, spike_ids, window_sec=window)

            @session.on_event(ChannelType.FRONTEND)
            def _on_spike(header, data):
                monitor.on_spike(header, data)

            try:
                while True:
                    time.sleep(args.interval)
                    snap = monitor.snapshot()
                    print_stats(snap, top_n=args.top,
                                show_channels=not args.no_channels)
            except KeyboardInterrupt:
                print("\nStopped.")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
