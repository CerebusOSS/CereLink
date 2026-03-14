"""Configure channels on a Cerebus device.

Sets up to N channels of a given type to sample at a given rate,
and disables sampling on all remaining channels of that type.

Usage::

    python -m pycbsdk.cli.configure_channels                          # defaults: NPLAY FRONTEND 0 SR_RAW
    python -m pycbsdk.cli.configure_channels --device NPLAY --rate 30kHz
    python -m pycbsdk.cli.configure_channels --n-chans 96 --rate RAW --dc-coupling
    python -m pycbsdk.cli.configure_channels --rate NONE               # disable all
    python -m pycbsdk.cli.configure_channels --rate RAW --disable-spikes --dc-coupling
"""

from __future__ import annotations

import argparse
import sys
import time

from pycbsdk import ChannelType, DeviceType, SampleRate, Session
from pycbsdk.session import _coerce_enum, _RATE_ALIASES

# cbAINPSPK_NOSORT — disable spike sorting
_SPKOPTS_NOSORT = 0x0000_0000


def configure(
    device_type: DeviceType,
    channel_type: ChannelType,
    rate: SampleRate,
    n_chans: int = 0,
    disable_spikes: bool = False,
    dc_coupling: bool = False,
    timeout: float = 10.0,
) -> None:
    """Connect to a device and configure channels.

    Args:
        device_type: Device to connect to.
        channel_type: Which channel type to configure.
        rate: Sample rate group (``SampleRate.NONE`` to disable).
        n_chans: Number of channels to configure (0 = all available).
        disable_spikes: Disable spike sorting on configured channels.
        dc_coupling: Switch to DC input coupling on configured channels.
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
        # Let initial config settle
        time.sleep(0.5)

        # Determine actual channel count
        available = len(session.get_matching_channel_ids(channel_type))
        if n_chans <= 0 or n_chans > available:
            n_chans = available

        print(f"Configuring {n_chans}/{available} {channel_type.name} channels "
              f"to {rate.name} ({rate.hz} Hz)" if rate != SampleRate.NONE
              else f"Disabling sampling on {n_chans}/{available} {channel_type.name} channels")

        # Set sample group (disable_others=True to turn off remaining channels)
        session.set_channel_sample_group(n_chans, channel_type, rate, disable_others=True)

        if dc_coupling and rate != SampleRate.NONE:
            session.set_ac_input_coupling(n_chans, channel_type, False)
            print("  AC input coupling disabled (DC mode)")
        if disable_spikes and rate != SampleRate.NONE:
            session.set_channel_spike_sorting(n_chans, channel_type, _SPKOPTS_NOSORT)
            print("  Spike sorting disabled")

        # Wait for config confirmations
        time.sleep(0.5)

        # Verify
        group_chans = session.get_group_channels(int(rate)) if rate != SampleRate.NONE else []
        print(f"Verification: group {rate.name} now has {len(group_chans)} channels")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.configure_channels",
        description="Configure channels on a Cerebus device.",
    )
    parser.add_argument(
        "--device", default="NPLAY",
        help="Device type (default: NPLAY). "
             f"Choices: {', '.join(dt.name for dt in DeviceType)}",
    )
    parser.add_argument(
        "--type", dest="channel_type", default="FRONTEND",
        help="Channel type to configure (default: FRONTEND). "
             f"Choices: {', '.join(ct.name for ct in ChannelType)}",
    )
    parser.add_argument(
        "--rate", default="SR_RAW",
        help="Sample rate group (default: SR_RAW). "
             f"Choices: {', '.join(r.name for r in SampleRate)}",
    )
    parser.add_argument(
        "--n-chans", type=int, default=0,
        help="Number of channels to configure (default: 0 = all available).",
    )
    parser.add_argument(
        "--disable-spikes", action="store_true",
        help="Disable spike sorting on configured channels.",
    )
    parser.add_argument(
        "--dc-coupling", action="store_true",
        help="Switch to DC input coupling on configured channels.",
    )
    parser.add_argument(
        "--timeout", type=float, default=10.0,
        help="Connection timeout in seconds (default: 10).",
    )
    args = parser.parse_args(argv)

    try:
        device_type = _coerce_enum(DeviceType, args.device)
    except (ValueError, TypeError) as e:
        parser.error(str(e))
        return 1

    try:
        channel_type = _coerce_enum(ChannelType, args.channel_type)
    except (ValueError, TypeError) as e:
        parser.error(str(e))
        return 1

    try:
        rate = _coerce_enum(SampleRate, args.rate, _RATE_ALIASES)
    except (ValueError, TypeError) as e:
        parser.error(str(e))
        return 1

    try:
        configure(
            device_type, channel_type, rate, args.n_chans,
            disable_spikes=args.disable_spikes,
            dc_coupling=args.dc_coupling,
            timeout=args.timeout,
        )
        return 0
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
