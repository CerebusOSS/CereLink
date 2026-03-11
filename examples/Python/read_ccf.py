"""
Read a Cerebus Configuration File (.ccf) offline — no device or pycbsdk needed.

CCF files are XML. Each channel's configuration lives under
``/CCF/ChanInfo/ChanInfo_item``. This script shows common patterns for
extracting channel config fields, filtering by channel type, and bulk-
reading a single field across many channels.

Usage:
    python read_ccf.py path/to/config.ccf
"""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Optional


# ---------------------------------------------------------------------------
# Channel-type classification (mirrors cbproto capability flags)
# ---------------------------------------------------------------------------

# Capability flag bits (from cbproto)
_CHAN_EXISTS    = 0x01
_CHAN_CONNECTED = 0x02
_CHAN_ISOLATED  = 0x04
_CHAN_AINP      = 0x100
_CHAN_AOUT      = 0x200
_CHAN_DINP      = 0x400
_CHAN_DOUT      = 0x800
_AOUT_AUDIO    = 0x40
_DINP_SERIAL   = 0x0000FF00  # serial-capable mask


def classify_channel(chancaps: int, ainpcaps: int = 0, aoutcaps: int = 0,
                     dinpcaps: int = 0) -> Optional[str]:
    """Classify a channel by its capability flags.

    Returns one of: "FRONTEND", "ANALOG_IN", "ANALOG_OUT", "AUDIO",
    "DIGITAL_IN", "SERIAL", "DIGITAL_OUT", or None if not connected.
    """
    if (_CHAN_EXISTS | _CHAN_CONNECTED) != (chancaps & (_CHAN_EXISTS | _CHAN_CONNECTED)):
        return None
    if (_CHAN_AINP | _CHAN_ISOLATED) == (chancaps & (_CHAN_AINP | _CHAN_ISOLATED)):
        return "FRONTEND"
    if _CHAN_AINP == (chancaps & (_CHAN_AINP | _CHAN_ISOLATED)):
        return "ANALOG_IN"
    if chancaps & _CHAN_AOUT:
        return "AUDIO" if (aoutcaps & _AOUT_AUDIO) else "ANALOG_OUT"
    if chancaps & _CHAN_DINP:
        return "SERIAL" if (dinpcaps & _DINP_SERIAL) else "DIGITAL_IN"
    if chancaps & _CHAN_DOUT:
        return "DIGITAL_OUT"
    return None


# ---------------------------------------------------------------------------
# XML helpers
# ---------------------------------------------------------------------------

def _int_text(elem: Optional[ET.Element], default: int = 0) -> int:
    """Get integer text content from an XML element."""
    return int(elem.text) if elem is not None and elem.text else default


def _str_text(elem: Optional[ET.Element], default: str = "") -> str:
    """Get string text content from an XML element."""
    return elem.text if elem is not None and elem.text else default


# ---------------------------------------------------------------------------
# Channel info extraction
# ---------------------------------------------------------------------------

@dataclass
class ChanInfo:
    """Subset of channel config fields from a CCF file."""
    chan: int                # 1-based channel number
    label: str
    bank: int
    term: int
    chancaps: int
    ainpcaps: int
    aoutcaps: int
    dinpcaps: int
    channel_type: Optional[str]
    smpgroup: int           # 0=disabled, 1-6
    smpfilter: int
    spkfilter: int
    ainpopts: int
    spkopts: int
    spkthrlevel: int
    lncrate: int
    refelecchan: int
    position: tuple[int, int, int, int]


def parse_chaninfo(item: ET.Element) -> ChanInfo:
    """Parse a single ``ChanInfo_item`` XML element."""
    chan = _int_text(item.find("chan"))
    label = _str_text(item.find("label"))
    bank = _int_text(item.find("bank"))
    term = _int_text(item.find("term"))

    caps = item.find("caps")
    chancaps = _int_text(caps.find("chancaps")) if caps is not None else 0
    ainpcaps = _int_text(caps.find("ainpcaps")) if caps is not None else 0
    aoutcaps = _int_text(caps.find("aoutcaps")) if caps is not None else 0
    dinpcaps = _int_text(caps.find("dinpcaps")) if caps is not None else 0

    opts = item.find("options")
    ainpopts = _int_text(opts.find("ainpopts")) if opts is not None else 0
    spkopts = _int_text(opts.find("spkopts")) if opts is not None else 0

    sample = item.find("sample")
    smpgroup = _int_text(sample.find("group")) if sample is not None else 0
    smpfilter = _int_text(sample.find("filter")) if sample is not None else 0

    spike = item.find("spike")
    spkfilter = _int_text(spike.find("filter")) if spike is not None else 0
    thr = spike.find("threshold") if spike is not None else None
    spkthrlevel = _int_text(thr.find("level")) if thr is not None else 0

    lnc = item.find("lnc")
    lncrate = _int_text(lnc.find("rate")) if lnc is not None else 0

    refelecchan = _int_text(item.find("refelecchan"))

    pos_elem = item.find("position")
    if pos_elem is not None:
        pos_items = pos_elem.findall("position_item")
        position = tuple(_int_text(p) for p in pos_items[:4])
        while len(position) < 4:
            position = position + (0,)
    else:
        position = (0, 0, 0, 0)

    return ChanInfo(
        chan=chan, label=label, bank=bank, term=term,
        chancaps=chancaps, ainpcaps=ainpcaps, aoutcaps=aoutcaps,
        dinpcaps=dinpcaps,
        channel_type=classify_channel(chancaps, ainpcaps, aoutcaps, dinpcaps),
        smpgroup=smpgroup, smpfilter=smpfilter, spkfilter=spkfilter,
        ainpopts=ainpopts, spkopts=spkopts, spkthrlevel=spkthrlevel,
        lncrate=lncrate, refelecchan=refelecchan, position=position,
    )


def read_ccf(filepath: str) -> list[ChanInfo]:
    """Parse all channel configs from a CCF file."""
    tree = ET.parse(filepath)
    root = tree.getroot()
    channels = []
    for item in root.findall("ChanInfo/ChanInfo_item"):
        channels.append(parse_chaninfo(item))
    return channels


# ---------------------------------------------------------------------------
# Bulk field extraction (analogous to cbsdk get_channels_field)
# ---------------------------------------------------------------------------

def get_channels_by_type(channels: list[ChanInfo],
                         channel_type: str) -> list[ChanInfo]:
    """Filter channels by type string (e.g. "FRONTEND")."""
    return [ch for ch in channels if ch.channel_type == channel_type]


def get_field(channels: list[ChanInfo], field: str) -> list:
    """Extract a single field from a list of ChanInfo objects.

    Args:
        channels: List of ChanInfo (e.g. from get_channels_by_type).
        field: Attribute name (e.g. "smpgroup", "spkthrlevel", "label").

    Returns:
        List of field values, one per channel.
    """
    return [getattr(ch, field) for ch in channels]


# ---------------------------------------------------------------------------
# Example usage
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <path/to/config.ccf>")
        sys.exit(1)

    filepath = sys.argv[1]
    channels = read_ccf(filepath)
    print(f"Loaded {len(channels)} channels from {filepath}\n")

    # Show channel type distribution
    type_counts: dict[Optional[str], int] = {}
    for ch in channels:
        type_counts[ch.channel_type] = type_counts.get(ch.channel_type, 0) + 1
    print("Channel types:")
    for ct, n in sorted(type_counts.items(), key=lambda x: (x[0] is None, x[0] or "")):
        print(f"  {ct or '(disconnected)':15s}  {n}")
    print()

    # Example: get smpgroup for all FRONTEND channels
    fe = get_channels_by_type(channels, "FRONTEND")
    if fe:
        groups = get_field(fe, "smpgroup")
        print(f"FRONTEND channels ({len(fe)}):")
        print(f"  sample groups: {groups}")
        print(f"  labels: {get_field(fe, 'label')}")

        # Spike thresholds
        thresholds = get_field(fe, "spkthrlevel")
        print(f"  spike thresholds: {thresholds}")

        # Positions
        positions = get_field(fe, "position")
        nonzero = [(ch.chan, ch.position) for ch in fe if any(p != 0 for p in ch.position)]
        if nonzero:
            print(f"  channels with positions: {len(nonzero)}")
            for chan_id, pos in nonzero[:5]:
                print(f"    ch {chan_id}: {pos}")
        else:
            print("  no positions set")


if __name__ == "__main__":
    main()
