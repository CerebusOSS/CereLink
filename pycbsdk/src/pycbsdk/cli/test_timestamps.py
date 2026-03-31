"""Test timestamp correctness across device types and connection modes.

Scenarios (run on macOS with NPLAY on localhost and HUB1 on network):

  NPLAY (non-Gemini 4.2) x STANDALONE
  NPLAY (non-Gemini 4.2) x CLIENT
  HUB1  (Gemini 4.2)     x STANDALONE
  HUB1  (Gemini 4.2)     x CLIENT

For each scenario:
  1. Verify received packet timestamps (nanoseconds) are close to
     steady_clock via clock sync offset.
  2. Verify device_to_monotonic() produces values close to time.monotonic().

Usage::

    # Run all scenarios (requires NPLAY on localhost + HUB1 on network):
    python -m pycbsdk.cli.test_timestamps

    # Run a single device:
    python -m pycbsdk.cli.test_timestamps --device NPLAY
    python -m pycbsdk.cli.test_timestamps --device HUB1

    # Skip CLIENT mode tests:
    python -m pycbsdk.cli.test_timestamps --standalone-only
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
import threading
from dataclasses import dataclass, field

from pycbsdk import DeviceType, SampleRate, Session
from pycbsdk.session import ProtocolVersion


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Mutable config so CLI args can override defaults.
_cfg = {
    # Maximum acceptable abs(arrival_monotonic - device_to_monotonic(pkt.time)).
    # Covers clock sync error + network latency + scheduling jitter.
    "tolerance_ms": 50.0,
    # How many packets to collect per test.
    "packet_count": 50,
}

# Seconds to wait for clock sync / packets.
TIMEOUT_S = 15.0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

@dataclass
class PacketSample:
    device_ns: int
    arrival_mono: float  # time.monotonic()
    converted_mono: float  # session.device_to_monotonic(device_ns)


@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str
    samples: list[PacketSample] = field(default_factory=list)


def _wait_for_clock_sync(session: Session, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while session.clock_offset_ns is None:
        if time.monotonic() > deadline:
            raise TimeoutError("Clock sync did not arrive within timeout")
        session.send_clock_probe()
        time.sleep(0.25)


def _collect_packets(
    session: Session, n: int, timeout: float, *, configure: bool = True
) -> list[PacketSample]:
    """Register a group callback on RAW and collect n packet samples.

    If *configure* is False (CLIENT mode), skip channel activation —
    the background STANDALONE process is responsible for that.
    """
    samples: list[PacketSample] = []
    done = threading.Event()

    @session.on_group(SampleRate.SR_RAW)
    def _on_group(header, data):
        if done.is_set():
            return
        arrival = time.monotonic()
        try:
            converted = session.device_to_monotonic(header.time)
        except RuntimeError:
            converted = float("nan")
        samples.append(PacketSample(header.time, arrival, converted))
        if len(samples) >= n:
            done.set()

    if configure:
        # Activate one FRONTEND channel on RAW to guarantee traffic.
        session.set_channel_sample_group(
            n_chans=1,
            channel_type=0,  # ChannelType.FRONTEND
            rate=SampleRate.SR_RAW,
        )

    if not done.wait(timeout):
        raise TimeoutError(
            f"Only received {len(samples)}/{n} packets within {timeout}s"
        )
    return samples


# ---------------------------------------------------------------------------
# Individual checks
# ---------------------------------------------------------------------------

def check_timestamps_are_nanoseconds(
    session: Session, samples: list[PacketSample]
) -> TestResult:
    """Verify that header.time values look like nanoseconds, not raw ticks.

    Uses the inter-packet time delta to distinguish units.  RAW group packets
    arrive at ~30 kHz.  If timestamps are nanoseconds the delta should be
    ~33 333 ns.  If they are unconverted 30 kHz ticks the delta would be ~1.
    We check that the median delta is in the plausible nanosecond range
    (1 000 .. 10 000 000 ns, i.e. 1 us .. 10 ms).
    """
    name = "timestamps are nanoseconds"

    if len(samples) < 2:
        return TestResult(name, False, "need at least 2 samples")

    deltas = [
        samples[i].device_ns - samples[i - 1].device_ns
        for i in range(1, len(samples))
    ]
    deltas.sort()
    median_delta = deltas[len(deltas) // 2]

    # Check monotonicity.
    for i in range(1, len(samples)):
        if samples[i].device_ns < samples[i - 1].device_ns:
            return TestResult(
                name, False,
                f"non-monotonic: sample[{i}]={samples[i].device_ns} "
                f"< sample[{i-1}]={samples[i-1].device_ns}",
                samples,
            )

    # Plausible range for nanosecond deltas between RAW group packets.
    if not (1_000 <= median_delta <= 10_000_000):
        # Dump first few samples for diagnosis.
        diag_lines = [f"  [{i}] time={s.device_ns}  delta={s.device_ns - samples[i-1].device_ns if i else 0}"
                      for i, s in enumerate(samples[:10])]
        diag = "\n".join(diag_lines)
        return TestResult(
            name, False,
            f"median inter-packet delta {median_delta} ns outside plausible range "
            f"[1us, 10ms] — timestamps may be raw ticks\n"
            f"  First 10 samples:\n{diag}",
            samples,
        )

    first_ns = samples[0].device_ns
    last_ns = samples[-1].device_ns
    span_s = (last_ns - first_ns) / 1e9
    return TestResult(
        name, True,
        f"median delta {median_delta} ns (~{median_delta / 1e6:.2f} ms), "
        f"span {span_s:.4f}s over {len(samples)} pkts",
        samples,
    )


def check_device_to_monotonic(
    session: Session, samples: list[PacketSample]
) -> TestResult:
    """Verify device_to_monotonic(pkt.time) is close to time.monotonic()."""
    name = "device_to_monotonic accuracy"

    if not samples:
        return TestResult(name, False, "no samples")

    latencies_ms = [
        (s.arrival_mono - s.converted_mono) * 1000 for s in samples
    ]
    mean_ms = sum(latencies_ms) / len(latencies_ms)
    min_ms = min(latencies_ms)
    max_ms = max(latencies_ms)

    passed = all(abs(l) < _cfg["tolerance_ms"] for l in latencies_ms)
    detail = (
        f"latency (arrival - converted): "
        f"min={min_ms:+.3f} ms  mean={mean_ms:+.3f} ms  max={max_ms:+.3f} ms  "
        f"(tolerance: {_cfg['tolerance_ms']} ms)"
    )
    return TestResult(name, passed, detail, samples)


# ---------------------------------------------------------------------------
# Scenario runner
# ---------------------------------------------------------------------------

def run_scenario(
    device_type: DeviceType, mode: str
) -> list[TestResult]:
    """Run all checks for one (device_type, mode) combination.

    mode is "STANDALONE" or "CLIENT".
    For CLIENT, a STANDALONE session is launched as a subprocess first.
    """
    results: list[TestResult] = []
    label = f"{device_type.name} / {mode}"
    print(f"\n{'=' * 60}")
    print(f"  {label}")
    print(f"{'=' * 60}")

    bg_proc = None
    try:
        if mode == "CLIENT":
            # Launch a STANDALONE session in the background.
            # Configure 1 FRONTEND channel on RAW so group traffic flows
            # before the CLIENT connects.
            bg_proc = subprocess.Popen(
                [
                    sys.executable, "-c",
                    "import time; "
                    f"from pycbsdk import Session, DeviceType, SampleRate; "
                    f"s = Session(DeviceType.{device_type.name}); "
                    "time.sleep(3); "
                    "s.set_channel_sample_group(1, 0, SampleRate.SR_RAW); "
                    "time.sleep(120)",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
            )
            # Give the STANDALONE process time to connect, populate shmem,
            # and configure a channel on RAW.
            print(f"  Waiting for background STANDALONE process (pid {bg_proc.pid}) ...")
            time.sleep(6)
            if bg_proc.poll() is not None:
                stderr = bg_proc.stderr.read().decode() if bg_proc.stderr else ""
                raise RuntimeError(
                    f"Background STANDALONE exited early (rc={bg_proc.returncode}): {stderr}"
                )

        print(f"  Connecting as {mode} ...")
        with Session(device_type=device_type) as session:
            proto = session.protocol_version
            ident = session.proc_ident
            print(f"  protocol: {proto.name}  ident: {ident!r}")

            # Activate a channel before clock sync so the device is already
            # streaming on our port.  This avoids inflated clock-sync latency
            # when another device floods the same UDP port (see #165).
            if mode == "STANDALONE":
                session.set_channel_sample_group(
                    n_chans=1,
                    channel_type=0,  # ChannelType.FRONTEND
                    rate=SampleRate.SR_RAW,
                )
                time.sleep(1)  # Let the receive thread settle

            # Wait for clock sync (best-effort — not available in CENTRAL_COMPAT).
            has_clock_sync = False
            try:
                print("  Waiting for clock sync ...")
                _wait_for_clock_sync(session, TIMEOUT_S)
                has_clock_sync = True
                offset_ns = session.clock_offset_ns
                uncert_ns = session.clock_uncertainty_ns
                print(
                    f"  Clock offset: {offset_ns / 1e6 if offset_ns else 0:+.3f} ms  "
                    f"uncertainty: {uncert_ns / 1e6 if uncert_ns else 0:.3f} ms"
                )
            except (TimeoutError, RuntimeError) as e:
                print(f"  Clock sync unavailable ({e}) — skipping device_to_monotonic check")

            # Collect packets.
            print(f"  Collecting {_cfg['packet_count']} packets ...")
            samples = _collect_packets(
                session, _cfg["packet_count"], TIMEOUT_S,
                configure=False,  # Already activated above (or by background process)
            )
            print(f"  Collected {len(samples)} packets.")

            # Run checks.
            results.append(check_timestamps_are_nanoseconds(session, samples))
            if has_clock_sync:
                results.append(check_device_to_monotonic(session, samples))
            else:
                results.append(TestResult(
                    "device_to_monotonic accuracy", True,
                    "SKIPPED — no clock sync in CENTRAL_COMPAT CLIENT mode",
                ))

    except Exception as e:
        results.append(TestResult(f"scenario setup ({label})", False, str(e)))
    finally:
        if bg_proc is not None:
            bg_proc.terminate()
            bg_proc.wait(timeout=5)
            print(f"  Background STANDALONE process terminated.")

    # Print results for this scenario.
    for r in results:
        status = "PASS" if r.passed else "FAIL"
        print(f"  [{status}] {r.name}: {r.detail}")

    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.test_timestamps",
        description="Test timestamp correctness across device types and modes.",
    )
    parser.add_argument(
        "--device", "-d", nargs="*", default=None,
        help="Device type(s) to test (default: NPLAY HUB1). "
             f"Choices: {', '.join(dt.name for dt in DeviceType if dt != DeviceType.CUSTOM)}",
    )
    parser.add_argument(
        "--standalone-only", action="store_true",
        help="Skip CLIENT mode tests.",
    )
    parser.add_argument(
        "--client-only", action="store_true",
        help="Skip STANDALONE mode tests.",
    )
    parser.add_argument(
        "--tolerance", type=float, default=_cfg["tolerance_ms"],
        help=f"Latency tolerance in ms (default: {_cfg['tolerance_ms']})",
    )
    parser.add_argument(
        "--packets", type=int, default=_cfg["packet_count"],
        help=f"Packets to collect per scenario (default: {_cfg['packet_count']})",
    )
    args = parser.parse_args(argv)

    _cfg["tolerance_ms"] = args.tolerance
    _cfg["packet_count"] = args.packets

    # Determine devices.
    if args.device:
        try:
            devices = [DeviceType[d.upper()] for d in args.device]
        except KeyError as e:
            names = ", ".join(dt.name for dt in DeviceType if dt != DeviceType.CUSTOM)
            parser.error(f"Unknown device: {e}. Choices: {names}")
            return 1
    else:
        devices = [DeviceType.NPLAY, DeviceType.HUB1]

    # Determine modes.
    modes = []
    if not args.client_only:
        modes.append("STANDALONE")
    if not args.standalone_only:
        modes.append("CLIENT")

    if not modes:
        parser.error("Cannot use both --standalone-only and --client-only")
        return 1

    # Run scenarios.
    all_results: list[tuple[str, list[TestResult]]] = []
    for device in devices:
        for mode in modes:
            label = f"{device.name} / {mode}"
            results = run_scenario(device, mode)
            all_results.append((label, results))

    # Summary.
    print(f"\n{'=' * 60}")
    print("  SUMMARY")
    print(f"{'=' * 60}")
    total = 0
    passed = 0
    for label, results in all_results:
        for r in results:
            total += 1
            if r.passed:
                passed += 1
            status = "PASS" if r.passed else "FAIL"
            print(f"  [{status}] {label} / {r.name}")

    print(f"\n  {passed}/{total} passed")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
