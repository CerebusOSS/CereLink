"""Parse and apply Blackrock ``.cmp`` channel-map files.

A ``.cmp`` file describes one headstage's electrode layout.  A data row holds
``col row bank electrode [size] [label]``.  When a column-header comment line
immediately precedes the data (e.g. ``//col row bank elec label`` or
``//col row bank elec size label``) it is the ground truth for column order;
names are matched case-insensitively (col/c, row/r, bank/b, elec/e, size/s,
label/l), so the order is not assumed.  With no header the legacy positional
order ``col row bank electrode [label]`` is used.

Units: when no size column is supplied and the col/row values form a
unit-indexed grid (the smallest non-zero delta among the distinct col/row
values is 1 — so some electrodes are adjacent, the manufacturer default), they
are interpreted as electrode indices: ``size`` becomes 1 and x/y/size are
scaled by the 400 µm Utah-array electrode pitch.  Larger deltas (e.g. the gap
between two arrays in a multi-array map) are allowed and scale through.
Otherwise (a size column is present, or no two electrodes are unit-spaced)
col/row/size are taken at face value.

Entries are keyed by device ``(bank, term)`` — the same join key the C++ SDK
uses to match rows to live channels — not by an ordinal channel ID.  Each
row's bank letter is shifted by ``start_chan // 32`` banks so a CMP can target
a specific headstage's banks (``start_chan=129`` → ``+4`` banks → CMP bank A
maps to device bank E).

Labels are stored verbatim.  ``hs_id`` identifies the headstage and is stored
in each entry's ``headstage`` field (it is **not** mixed into the label — the
stored headstage id already disambiguates labels reused across headstages).

This module mirrors the C++ parser in ``src/cbsdk/src/cmp_parser.{h,cpp}``; the
device is configured through that C++ path (``Session.load_channel_map`` hands
it the file path), so this module is for inspection/CLI use.

Typical use::

    entries = parse_cmp("/path/to/headstage1.cmp", start_chan=1, hs_id=1)
    for (bank, term), entry in sorted(entries.items()):
        print(bank, term, entry.label, entry.x, entry.y)

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
    """One parsed CMP row.

    The geometry fields populate ``cbPKT_CHANINFO.position[0..3]``; the device
    ``(bank, term)`` are the keys used to find which channel the row applies to.
    """

    x: int  # electrode x (µm for index grids, else face value) → position[0]
    y: int  # electrode y (µm for index grids, else face value) → position[1]
    size: int  # electrode size, same units as x/y (0 = unspecified) → position[2]
    headstage: int  # 1-based headstage id (== hs_id; 0 = none) → position[3]
    bank: int  # 1-based device bank (CMP bank + start_chan // 32); match key
    term: int  # 1-based terminal within bank (CMP electrode); match key
    label: str  # verbatim label (e.g. "chan12")


# Blackrock Utah arrays have 400 µm inter-electrode spacing; default index grids
# are scaled by this to convert electrode indices to micrometers.
_PITCH_UM = 400

_COLUMN_ALIASES = {
    "col": "col",
    "column": "col",
    "c": "col",
    "x": "col",
    "row": "row",
    "r": "row",
    "y": "row",
    "bank": "bank",
    "b": "bank",
    "elec": "elec",
    "electrode": "elec",
    "e": "elec",
    "term": "elec",
    "terminal": "elec",
    "size": "size",
    "s": "size",
    "sz": "size",
    "label": "label",
    "l": "label",
    "name": "label",
}


def _classify_column(tok: str) -> str:
    """Map a header token to a column kind (or ``"unknown"``)."""
    return _COLUMN_ALIASES.get(tok.strip().strip("',\"").lower(), "unknown")


def _resolve_columns(header: str | None) -> tuple[list[str], bool]:
    """Resolve column order from a header comment, else the legacy order.

    Returns (columns, has_size).
    """
    if header:
        cols = [_classify_column(t) for t in header.split()]
        if {"col", "row", "bank", "elec"} <= set(cols):
            return cols, ("size" in cols)
    return ["col", "row", "bank", "elec", "label"], False


def _parse_row(tokens: list[str], columns: list[str]) -> dict | None:
    """Parse one data row per ``columns``. Returns None to skip a bad row."""
    out = {"col": None, "row": None, "bank": None, "elec": None, "size": 0, "label": ""}
    for kind, tok in zip(columns, tokens):
        if kind in ("col", "row", "elec", "size"):
            try:
                out[kind] = int(tok)
            except ValueError:
                return None
        elif kind == "bank":
            if not tok[:1].isalpha():
                return None
            out["bank"] = ord(tok[0].upper()) - ord("A") + 1
        elif kind == "label":
            out["label"] = tok
        # unknown: ignore
    if None in (out["col"], out["row"], out["bank"], out["elec"]):
        return None
    return out


def _is_unit_indexed_grid(raw: list[dict]) -> bool:
    """True if the smallest non-zero delta among distinct col/row values is 1.

    A single unit step is enough — larger deltas (e.g. the gap between two
    arrays in a multi-array map) are allowed and scale through.
    """
    deltas: set[int] = set()
    for key in ("col", "row"):
        vals = sorted({r[key] for r in raw})
        deltas.update(b - a for a, b in zip(vals, vals[1:]))
    return 1 in deltas


def parse_cmp(
    filepath: str | Path,
    start_chan: int = 1,
    hs_id: int = 0,
) -> dict[tuple[int, int], CmpEntry]:
    """Parse a single CMP file into entries keyed by device ``(bank, term)``.

    Args:
        filepath: Path to the ``.cmp`` file.
        start_chan: 1-based channel selecting the target banks; the CMP's bank
            letters are shifted by ``start_chan // 32`` banks (1 → banks A…,
            129 → banks E…).
        hs_id: Headstage identifier stored in each entry's ``headstage`` field
            (0 = none). Does not affect labels.

    Returns:
        Dict mapping ``(bank, term)`` → :class:`CmpEntry`.

    Raises:
        ValueError: If the file contains no valid rows.
        FileNotFoundError: If the file does not exist.
    """
    raw: list[dict] = []
    description_seen = False
    header_candidate: str | None = None
    columns: list[str] | None = None
    has_size = False

    with open(filepath) as fh:
        for line in fh:
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("//"):
                # A comment after the description may be the column header.
                if description_seen:
                    header_candidate = stripped[2:]
                continue
            if not description_seen:
                description_seen = True
                header_candidate = None
                continue
            # First data row: lock in the column order.
            if columns is None:
                columns, has_size = _resolve_columns(header_candidate)
            entry = _parse_row(stripped.split(), columns)
            if entry is not None:
                raw.append(entry)

    if not raw:
        raise ValueError(f"{filepath}: no valid entries")

    # No size column + a unit-indexed grid → electrode indices: size is 1 and
    # x/y/size scale by the 400 µm pitch. Otherwise take values at face value.
    scale = not has_size and _is_unit_indexed_grid(raw)
    factor = _PITCH_UM if scale else 1

    bank_offset = start_chan // 32
    entries: dict[tuple[int, int], CmpEntry] = {}
    for r in raw:
        dev_bank = r["bank"] + bank_offset
        term = r["elec"]
        entries[(dev_bank, term)] = CmpEntry(
            x=r["col"] * factor,
            y=r["row"] * factor,
            size=_PITCH_UM if scale else (r["size"] if has_size else 0),
            headstage=hs_id,
            bank=dev_bank,
            term=term,
            label=r["label"],
        )
    return entries


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _parse_spec(spec: str) -> tuple[Path, int, int]:
    """Parse a ``FILE:START_CHAN:HS_ID`` or ``FILE:START_CHAN`` spec.

    ``HS_ID`` defaults to ``0`` when omitted; ``START_CHAN`` defaults to ``1``.
    The CLI does not auto-stack headstages — callers must pass each spec
    explicitly.
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
    """Parse each spec and print its entries, checking for (bank, term) collisions."""
    seen: dict[tuple[int, int], tuple[Path, str]] = {}
    for path, start_chan, hs_id in specs:
        entries = parse_cmp(path, start_chan=start_chan, hs_id=hs_id)
        print(
            f"# {path}  start_chan={start_chan}  hs_id={hs_id}  ({len(entries)} chans)"
        )
        for key in sorted(entries):
            e = entries[key]
            if key in seen:
                prev_path, prev_label = seen[key]
                print(
                    f"  # WARNING: (bank {e.bank}, term {e.term}) already claimed by "
                    f"{prev_path.name} as {prev_label!r}",
                    file=sys.stderr,
                )
            seen[key] = (path, e.label)
            print(
                f"  bank={e.bank} term={e.term:>2}  {e.label:<16s}  "
                f"x={e.x} y={e.y} size={e.size} hs={e.headstage}"
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
            "(defaults: START_CHAN=1, HS_ID=0)."
        ),
    )
    parser.add_argument(
        "specs",
        nargs="+",
        type=_parse_spec,
        metavar="SPEC",
        help="one or more FILE:START_CHAN:HS_ID specs (START_CHAN default 1, HS_ID default 0)",
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
