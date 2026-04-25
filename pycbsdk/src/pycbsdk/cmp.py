"""Parse and apply Blackrock ``.cmp`` channel-map files.

A ``.cmp`` file describes one headstage's electrode layout.  Each data row
has the form::

    col row bank electrode [label]

where ``bank`` is a letter (``A``-``H``) and ``electrode`` is 1-based within
the bank.  Rows are not guaranteed to be in channel order; this module sorts
by ``(bank, electrode)`` and assigns each sorted row an absolute 1-based
channel ID starting at a caller-supplied ``start_chan``.

Labels commonly collide across headstages (every file may use
``elec1-1`` … ``elec1-128``), so each label is prefixed with ``"hs{hs_id}-"``.
Pass ``hs_id=0`` (the default) to skip prefixing — appropriate for
single-headstage rigs where the original labels are already unique.

Typical use::

    entries = parse_cmp("/path/to/headstage1.cmp", start_chan=1, hs_id=1)
    for chan_id, entry in sorted(entries.items()):
        print(chan_id, entry.label, entry.position)

To apply CMPs to a live session, prefer :meth:`pycbsdk.Session.load_channel_map`.

Command line::

    python -m pycbsdk.cmp head1.cmp:1:1 head2.cmp:129:2 --device NSP
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CmpEntry:
    """One parsed CMP row, ready to apply to an absolute channel."""

    chan_id: int  # 1-based absolute channel
    position: tuple[int, int, int, int]  # (col, row, bank_idx, electrode)
    label: str  # prefixed (e.g. "hs1-chan3")


def parse_cmp(
    filepath: str | Path,
    start_chan: int = 1,
    hs_id: int = 0,
) -> dict[int, CmpEntry]:
    """Parse a single CMP file and assign absolute channel IDs.

    Args:
        filepath: Path to the ``.cmp`` file.
        start_chan: 1-based channel assigned to the first sorted row.
        hs_id: Headstage identifier; labels are prefixed ``"hs{hs_id}-"``.
            Pass ``0`` (the default) to leave labels un-prefixed.

    Returns:
        Dict mapping absolute 1-based ``chan_id`` → :class:`CmpEntry`.

    Raises:
        ValueError: If the file is malformed or contains no valid rows.
        FileNotFoundError: If the file does not exist.
    """
    raw: list[tuple[int, int, int, int, str]] = []  # (col, row, bank_idx, elec, label)
    description: str | None = None

    with open(filepath) as fh:
        for line in fh:
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            if description is None:
                description = stripped
                continue
            parts = stripped.split()
            if len(parts) < 4:
                raise ValueError(
                    f"{filepath}: malformed row (need 'col row bank elec [label]'): "
                    f"{line.rstrip()!r}"
                )
            bank = parts[2]
            if len(bank) != 1 or not bank[0].isalpha():
                raise ValueError(f"{filepath}: invalid bank letter {bank!r}")
            bank_idx = ord(bank[0].upper()) - ord("A") + 1
            raw.append(
                (
                    int(parts[0]),
                    int(parts[1]),
                    bank_idx,
                    int(parts[3]),
                    parts[4] if len(parts) > 4 else "",
                )
            )

    if not raw:
        raise ValueError(f"{filepath}: no valid entries")

    raw.sort(key=lambda r: (r[2], r[3]))

    # hs_id == 0 means "single headstage, no prefix needed".
    prefix = "" if hs_id == 0 else f"hs{hs_id}-"
    entries: dict[int, CmpEntry] = {}
    for i, (col, row, bank_idx, elec, label) in enumerate(raw):
        chan_id = start_chan + i
        entries[chan_id] = CmpEntry(
            chan_id=chan_id,
            position=(col, row, bank_idx, elec),
            label=f"{prefix}{label}",
        )
    return entries


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _parse_spec(spec: str) -> tuple[Path, int, int]:
    """Parse a ``FILE:START_CHAN:HS_ID`` or ``FILE:START_CHAN`` spec.

    ``HS_ID`` defaults to ``0`` (no label prefix) when omitted; ``START_CHAN``
    defaults to ``1``. The CLI does not auto-stack headstages — callers must
    pass each spec explicitly.
    """
    parts = spec.split(":")
    if not parts or not parts[0]:
        raise argparse.ArgumentTypeError(f"empty spec: {spec!r}")
    path = Path(parts[0])
    if not path.exists():
        raise argparse.ArgumentTypeError(f"{path}: not found")
    start_chan = int(parts[1]) if len(parts) > 1 and parts[1] else 1
    hs_id = int(parts[2]) if len(parts) > 2 and parts[2] else 0
    if len(parts) > 3:
        raise argparse.ArgumentTypeError(
            f"too many fields in spec {spec!r}; expected FILE[:START_CHAN[:HS_ID]]"
        )
    return path, start_chan, hs_id


def _dump(specs: list[tuple[Path, int, int]]) -> None:
    """Parse each spec and print its entries, checking for chan_id collisions."""
    seen: dict[int, tuple[Path, str]] = {}
    for path, start_chan, hs_id in specs:
        entries = parse_cmp(path, start_chan=start_chan, hs_id=hs_id)
        print(
            f"# {path}  start_chan={start_chan}  hs_id={hs_id}  ({len(entries)} chans)"
        )
        for chan_id in sorted(entries):
            e = entries[chan_id]
            col, row, bank_idx, elec = e.position
            if chan_id in seen:
                prev_path, prev_label = seen[chan_id]
                print(
                    f"  # WARNING: chan {chan_id} already claimed by "
                    f"{prev_path.name} as {prev_label!r}",
                    file=sys.stderr,
                )
            seen[chan_id] = (path, e.label)
            print(
                f"  chan {chan_id:>3}  {e.label:<16s}  col={col} row={row} "
                f"bank={bank_idx} elec={elec}"
            )


def _apply(specs: list[tuple[Path, int, int]], device: str, timeout: float) -> None:
    """Connect to a device and call ``session.load_channel_map`` for each spec."""
    # Import lazily so `python -m pycbsdk.cmp --dump` works without a device lib.
    import time

    from pycbsdk import DeviceType, Session
    from pycbsdk.session import _coerce_enum

    device_type = _coerce_enum(DeviceType, device)
    with Session(device_type=device_type) as session:
        deadline = time.monotonic() + timeout
        while not session.running:
            if time.monotonic() > deadline:
                raise TimeoutError(
                    f"Session for {device_type.name} did not start within {timeout}s"
                )
            time.sleep(0.1)
        time.sleep(0.5)  # let initial config settle

        for path, start_chan, hs_id in specs:
            print(f"Loading {path.name}  start_chan={start_chan}  hs_id={hs_id}")
            session.load_channel_map(str(path), start_chan=start_chan, hs_id=hs_id)
        print("Done.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m pycbsdk.cmp",
        description=(
            "Parse or apply Blackrock .cmp channel-map files. "
            "Each SPEC is FILE[:START_CHAN[:HS_ID]] "
            "(defaults: START_CHAN=1, HS_ID=0 → no label prefix)."
        ),
    )
    parser.add_argument(
        "specs",
        nargs="+",
        type=_parse_spec,
        metavar="SPEC",
        help=(
            "one or more FILE:START_CHAN:HS_ID specs "
            "(START_CHAN default 1, HS_ID default 0 → no prefix)"
        ),
    )
    parser.add_argument(
        "--dump",
        action="store_true",
        help="print parsed entries without connecting to a device",
    )
    parser.add_argument(
        "--device",
        default="NPLAY",
        help="device type when applying (default: NPLAY)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="session connection timeout in seconds (default: 10)",
    )
    args = parser.parse_args(argv)

    if args.dump:
        _dump(args.specs)
        return 0

    try:
        _apply(args.specs, device=args.device, timeout=args.timeout)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
