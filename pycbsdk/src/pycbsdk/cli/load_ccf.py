"""Load a CCF file to configure a Cerebus device.

Reads a CCF (XML) configuration file and sends its contents to the device.

Usage::

    python -m pycbsdk.cli.load_ccf my_config.ccf
    python -m pycbsdk.cli.load_ccf my_config.ccf --device NPLAY
    python -m pycbsdk.cli.load_ccf my_config.ccf --timeout 15
"""

from __future__ import annotations

import argparse
import sys
import time

from pycbsdk import DeviceType, Session
from pycbsdk.session import _coerce_enum


def load_ccf(
    filename: str,
    device_type: DeviceType,
    timeout: float = 10.0,
    sync: bool = True,
) -> None:
    """Connect to a device and apply a CCF configuration file.

    Args:
        filename: Path to the CCF file to load.
        device_type: Device to connect to.
        timeout: Connection timeout in seconds.
        sync: If True (default), wait for the device to acknowledge the
            configuration before returning.
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

        print(f"Loading CCF {filename!r} onto {device_type.name} ...")
        if sync:
            session.load_ccf_sync(filename, timeout=timeout)
        else:
            session.load_ccf(filename)
        print("CCF loaded successfully.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.load_ccf",
        description="Load a CCF file to configure a Cerebus device.",
    )
    parser.add_argument(
        "filename",
        help="Path to the CCF file to load.",
    )
    parser.add_argument(
        "--device",
        default="NPLAY",
        help="Device type (default: NPLAY). "
        f"Choices: {', '.join(dt.name for dt in DeviceType)}",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Connection timeout in seconds (default: 10).",
    )
    parser.add_argument(
        "--no-sync",
        action="store_true",
        default=False,
        help="Don't wait for the device to acknowledge the configuration.",
    )
    args = parser.parse_args(argv)

    try:
        device_type = _coerce_enum(DeviceType, args.device)
    except (ValueError, TypeError) as e:
        parser.error(str(e))
        return 1

    try:
        load_ccf(
            args.filename, device_type, timeout=args.timeout, sync=not args.no_sync
        )
        return 0
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
