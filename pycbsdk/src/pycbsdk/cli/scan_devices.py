"""Scan for Cerebus devices and report configuration.

Usage::

    python -m pycbsdk.cli.scan_devices              # scan all known device types
    python -m pycbsdk.cli.scan_devices NPLAY HUB1    # scan specific devices
    python -m pycbsdk.cli.scan_devices --timeout 5   # custom timeout (seconds)
"""

from __future__ import annotations

import argparse
import sys
import time
from collections import Counter

from pycbsdk import (
    Session,
    DeviceType,
    ChannelType,
    SampleRate,
    ChanInfoField,
    ProtocolVersion,
)

# Spike processing extract bit (cbAINPSPK_EXTRACT)
_SPKOPTS_EXTRACT = 0x0000_0001


def _device_type_names() -> list[str]:
    """Return device type names in scan order, excluding CUSTOM."""
    return [dt.name for dt in DeviceType if dt != DeviceType.CUSTOM]


def _format_hz(hz: int) -> str:
    if hz >= 1000:
        return f"{hz // 1000}kHz"
    return f"{hz}Hz"


def scan_device(device_type: DeviceType, timeout: float = 3.0) -> dict | None:
    """Try to connect to a device and collect its configuration.

    Returns a dict of info on success, or None if connection fails.
    """
    try:
        with Session(device_type=device_type) as session:
            # Give the handshake time to complete
            deadline = time.monotonic() + timeout
            while not session.running and time.monotonic() < deadline:
                time.sleep(0.1)

            if not session.running:
                return None

            info: dict = {}
            info["device_type"] = device_type.name
            info["protocol_version"] = session.protocol_version
            info["proc_ident"] = session.proc_ident
            info["runlevel"] = session.runlevel
            info["sysfreq"] = session.sysfreq
            info["spike_length"] = session.spike_length
            info["spike_pretrigger"] = session.spike_pretrigger
            info["clock_offset_ns"] = session.clock_offset_ns
            info["clock_uncertainty_ns"] = session.clock_uncertainty_ns

            # --- Channel summary by type ---
            chan_summary = {}
            for ct in ChannelType:
                ids = session.get_matching_channel_ids(ct)
                if not ids:
                    continue
                groups = session.get_channels_field(ct, ChanInfoField.SMPGROUP)
                spkopts = session.get_channels_field(ct, ChanInfoField.SPKOPTS)

                # Count channels enabled in each sample group
                group_counts: Counter[int] = Counter()
                for g in groups:
                    if g > 0:
                        group_counts[g] += 1

                enabled_by_rate: dict[str, int] = {}
                for gid, cnt in sorted(group_counts.items()):
                    try:
                        rate = SampleRate(gid)
                        label = _format_hz(rate.hz)
                    except ValueError:
                        label = f"group{gid}"
                    enabled_by_rate[label] = cnt

                spike_enabled = sum(1 for s in spkopts if s & _SPKOPTS_EXTRACT)

                chan_summary[ct.name] = {
                    "total": len(ids),
                    "sampling_enabled": sum(group_counts.values()),
                    "by_rate": enabled_by_rate,
                    "spike_enabled": spike_enabled,
                }
            info["channels"] = chan_summary

            # --- Sample group summary ---
            group_summary = {}
            for gid in range(1, 7):
                chans = session.get_group_channels(gid)
                if not chans:
                    continue
                label = session.get_group_label(gid) or ""
                try:
                    rate = SampleRate(gid)
                    hz = rate.hz
                except ValueError:
                    hz = 0
                group_summary[gid] = {
                    "label": label,
                    "rate": _format_hz(hz) if hz else "?",
                    "n_channels": len(chans),
                }
            info["groups"] = group_summary

            stats = session.stats
            info["packets_received"] = stats.packets_received
            info["packets_dropped"] = stats.packets_dropped

            return info

    except Exception as e:
        # Connection failed — device not present
        return {"error": str(e), "device_type": device_type.name}


def print_report(info: dict) -> None:
    """Print a human-readable report for one device."""
    dt = info["device_type"]
    if "error" in info:
        print(f"  {dt}: connection failed ({info['error']})")
        return

    proto = info["protocol_version"]
    proto_str = proto.name if isinstance(proto, ProtocolVersion) else str(proto)
    ident = info["proc_ident"] or "(unknown)"

    print(f"  {dt}: {ident}")
    print(f"    Protocol:         {proto_str}")
    print(f"    Runlevel:         {info['runlevel']}")
    print(f"    System freq:      {info['sysfreq']} Hz")
    print(
        f"    Spike length:     {info['spike_length']} samples "
        f"(pretrigger: {info['spike_pretrigger']})"
    )

    offset = info["clock_offset_ns"]
    uncertainty = info["clock_uncertainty_ns"]
    if offset is not None:
        print(f"    Clock offset:     {offset} ns (uncertainty: {uncertainty} ns)")
    else:
        print(f"    Clock offset:     (not synced)")

    print(
        f"    Packets:          {info['packets_received']} received, "
        f"{info['packets_dropped']} dropped"
    )

    channels = info.get("channels", {})
    if channels:
        print(f"    Channels:")
        for ct_name, summary in channels.items():
            total = summary["total"]
            enabled = summary["sampling_enabled"]
            spike = summary["spike_enabled"]
            by_rate = summary["by_rate"]

            rate_parts = [f"{cnt}@{rate}" for rate, cnt in by_rate.items()]
            rate_str = ", ".join(rate_parts) if rate_parts else "none"

            parts = [f"{total} total"]
            if enabled > 0:
                parts.append(f"{enabled} sampling ({rate_str})")
            if spike > 0:
                parts.append(f"{spike} spike-enabled")

            print(f"      {ct_name:12s}  {', '.join(parts)}")

    groups = info.get("groups", {})
    if groups:
        print(f"    Sample groups:")
        for gid, g in groups.items():
            label = g["label"]
            print(
                f"      Group {gid} ({g['rate']:>5s}): "
                f"{g['n_channels']} channels"
                f"{f'  [{label}]' if label else ''}"
            )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cli.scan_devices",
        description="Scan for Cerebus devices and report configuration.",
    )
    parser.add_argument(
        "devices",
        nargs="*",
        metavar="DEVICE",
        help=f"Device types to scan (default: all). "
        f"Choices: {', '.join(_device_type_names())}",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=3.0,
        help="Connection timeout per device in seconds (default: 3)",
    )
    args = parser.parse_args(argv)

    if args.devices:
        try:
            targets = [DeviceType[d.upper()] for d in args.devices]
        except KeyError as e:
            parser.error(
                f"Unknown device type: {e}. Choices: {', '.join(_device_type_names())}"
            )
            return 1  # unreachable
    else:
        targets = [dt for dt in DeviceType if dt != DeviceType.CUSTOM]

    print(f"Scanning {len(targets)} device type(s)...\n")

    found = 0
    for dt in targets:
        info = scan_device(dt, timeout=args.timeout)
        if info is None:
            print(f"  {dt.name}: no response")
        else:
            if "error" not in info:
                found += 1
            print_report(info)
        print()

    print(f"Found {found} device(s).")
    return 0 if found > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
