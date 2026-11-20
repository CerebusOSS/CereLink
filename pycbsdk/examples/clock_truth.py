"""Compare CereLink's clock-offset estimate against a PTP ground truth (Linux).

On a host whose clock is PTP-synchronized to the device(s), the true offset
(device_ns - CLOCK_MONOTONIC ns) is just ref_clock - CLOCK_MONOTONIC, where
ref_clock is the PTP-disciplined PHC (or CLOCK_TAI / CLOCK_REALTIME if phc2sys
slaves the system clock).  This prints estimate - truth over time.

A/B the kernel-timestamp experiment by running it once per mode:

    CBDEV_KERNEL_TS=0    python clock_truth.py HUB1 --ref /dev/ptp0
    CBDEV_KERNEL_TS=rx   python clock_truth.py HUB1 --ref /dev/ptp0
    CBDEV_KERNEL_TS=rxpost python clock_truth.py HUB1 --ref /dev/ptp0
    CBDEV_KERNEL_TS=rxtx python clock_truth.py HUB1 --ref /dev/ptp0  # Linux only
"""

from __future__ import annotations

import argparse
import os
import statistics
import time

from pycbsdk import DeviceType, Session


def open_ref(ref: str) -> int:
    if ref == "tai":
        return time.CLOCK_TAI
    if ref == "realtime":
        return time.CLOCK_REALTIME
    fd = os.open(ref, os.O_RDONLY)  # dynamic POSIX clock: FD_TO_CLOCKID
    return ((~fd) << 3) | 3


def true_offset_ns(clk: int) -> int:
    """ref - monotonic, from the tightest of several bracketed reads."""
    best = None
    for _ in range(7):
        m0 = time.monotonic_ns()
        r = time.clock_gettime_ns(clk)
        m1 = time.monotonic_ns()
        if best is None or m1 - m0 < best[0]:
            best = (m1 - m0, r - (m0 + m1) // 2)
    return best[1]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("device", type=str.upper, choices=[d.name for d in DeviceType])
    ap.add_argument("--ref", default="/dev/ptp0",
                    help="/dev/ptpN, tai, realtime, or none (no truth: report drift rate and "
                         "estimate noise around a linear fit; also prints raw mono/offset ns)")
    ap.add_argument("--duration", type=float, default=120.0, help="seconds")
    ap.add_argument("--interval", type=float, default=1.0, help="seconds")
    args = ap.parse_args()

    clk = None if args.ref == "none" else open_ref(args.ref)
    raw: list[tuple[int, int]] = []  # (monotonic_ns, offset_ns) for --ref none
    mode = os.environ.get("CBDEV_KERNEL_TS", "0") or "0"
    errs_us: list[float] = []
    with Session(device_type=DeviceType[args.device]) as session:
        deadline = time.monotonic() + 15
        while session.clock_offset_ns is None:
            if time.monotonic() > deadline:
                raise TimeoutError("no clock offset within 15 s")
            time.sleep(0.25)

        print(f"mode={mode}  ref={args.ref}  t0_wall={time.time():.3f}")
        print(f"{'t (s)':>7s}  {'err (us)':>10s}  {'uncert (us)':>11s}")
        t0 = time.monotonic()
        try:
            while time.monotonic() - t0 < args.duration:
                time.sleep(args.interval)
                est = session.clock_offset_ns
                if est is None:
                    continue
                unc = (session.clock_uncertainty_ns or 0) / 1e3
                if clk is None:
                    mono = time.monotonic_ns()
                    raw.append((mono, est))
                    err = (est - raw[0][1]) / 1e3  # offset change since start
                    print(f"{time.monotonic() - t0:7.1f}  {err:+10.1f}  {unc:11.1f}  {mono} {est}")
                    continue
                err = (est - true_offset_ns(clk)) / 1e3
                errs_us.append(err)
                print(f"{time.monotonic() - t0:7.1f}  {err:+10.1f}  {unc:11.1f}")
        except KeyboardInterrupt:
            pass

    late = raw[15:]  # skip warm-up
    if len(late) >= 3:
        xs = [(m - late[0][0]) / 1e9 for m, _ in late]
        ys = [(o - late[0][1]) / 1e3 for _, o in late]
        fit = statistics.linear_regression(xs, ys)
        res = sorted(abs(y - (fit.slope * x + fit.intercept)) for x, y in zip(xs, ys))
        print(f"\nmode={mode}  n={len(late)}  drift={fit.slope:+.2f} ppm  "
              f"residual sd={statistics.pstdev([y - (fit.slope * x + fit.intercept) for x, y in zip(xs, ys)]):.1f} us  "
              f"p95|res|={res[int(0.95 * (len(res) - 1))]:.1f} us  max|res|={res[-1]:.1f} us")
    if len(errs_us) >= 2:
        a = sorted(abs(e) for e in errs_us)
        print(
            f"\nmode={mode}  n={len(errs_us)}  mean={statistics.mean(errs_us):+.1f} us  "
            f"sd={statistics.stdev(errs_us):.1f} us  median|err|={statistics.median(a):.1f} us  "
            f"p95|err|={a[int(0.95 * (len(a) - 1))]:.1f} us  max|err|={a[-1]:.1f} us"
        )


if __name__ == "__main__":
    main()
