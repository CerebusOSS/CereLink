"""Unit tests for pycbsdk.cmp (channel-map parser and CLI helpers)."""

from __future__ import annotations

from pathlib import Path

import pytest

from pycbsdk import cmp as cmp_mod
from pycbsdk.cmp import CmpEntry, main, parse_cmp


def test_parse_single_file_assigns_chans_from_one(cmp_path: Path):
    entries = parse_cmp(cmp_path)
    assert len(entries) == 96
    assert set(entries) == set(range(1, 97))
    assert all(isinstance(e, CmpEntry) for e in entries.values())


def test_parse_labels_are_prefixed_with_hs_id(cmp_path: Path):
    entries = parse_cmp(cmp_path, hs_id=3)
    for entry in entries.values():
        assert entry.label.startswith("hs3-")


def test_parse_sorts_by_bank_then_electrode(cmp_path: Path):
    entries = parse_cmp(cmp_path)
    # 96-channel file: banks A, B, C each with 32 electrodes.
    # chan 1  → (bank_idx=1, elec=1), chan 33 → (bank_idx=2, elec=1), …
    assert entries[1].position[2:] == (1, 1)
    assert entries[32].position[2:] == (1, 32)
    assert entries[33].position[2:] == (2, 1)
    assert entries[64].position[2:] == (2, 32)
    assert entries[65].position[2:] == (3, 1)
    assert entries[96].position[2:] == (3, 32)


def test_parse_start_chan_offsets_chan_ids(cmp_path: Path):
    entries = parse_cmp(cmp_path, start_chan=129, hs_id=2)
    assert set(entries) == set(range(129, 129 + 96))
    assert entries[129].position[2:] == (1, 1)  # bank A elec 1
    assert all(e.label.startswith("hs2-") for e in entries.values())


def test_parse_unsorted_rows(tmp_path: Path):
    f = tmp_path / "unsorted.cmp"
    f.write_text(
        "// scratch\n"
        "unsorted test\n"
        "0 0 B 1 label_b1\n"
        "0 0 A 3 label_a3\n"
        "0 0 A 1 label_a1\n"
        "0 0 A 2 label_a2\n"
    )
    entries = parse_cmp(f, hs_id=1)
    # (A,1), (A,2), (A,3), (B,1) → chans 1..4
    assert entries[1].label == "hs1-label_a1"
    assert entries[2].label == "hs1-label_a2"
    assert entries[3].label == "hs1-label_a3"
    assert entries[4].label == "hs1-label_b1"


def test_parse_default_hs_id_omits_prefix(cmp_path: Path):
    """Default hs_id=0 leaves labels unmodified."""
    bare = parse_cmp(cmp_path)
    prefixed = parse_cmp(cmp_path, hs_id=3)
    for chan_id, entry in bare.items():
        assert not entry.label.startswith("hs")
        assert prefixed[chan_id].label == f"hs3-{entry.label}"


def test_parse_rejects_missing_file(tmp_path: Path):
    with pytest.raises(FileNotFoundError):
        parse_cmp(tmp_path / "nope.cmp")


def test_parse_rejects_empty_file(tmp_path: Path):
    f = tmp_path / "empty.cmp"
    f.write_text("// just comments\n// still just comments\n")
    with pytest.raises(ValueError, match="no valid entries"):
        parse_cmp(f)


def test_parse_spec_defaults(cmp_path: Path):
    path, start_chan, hs_id = cmp_mod._parse_spec(str(cmp_path))
    assert path == cmp_path
    assert start_chan == 1
    assert hs_id == 0


def test_parse_spec_full(cmp_path: Path):
    path, start_chan, hs_id = cmp_mod._parse_spec(f"{cmp_path}:129:2")
    assert path == cmp_path
    assert start_chan == 129
    assert hs_id == 2


def test_parse_spec_rejects_nonexistent_file():
    with pytest.raises(Exception):  # argparse.ArgumentTypeError
        cmp_mod._parse_spec("/no/such/file.cmp:1:1")


def test_main_dump_prints_entries(cmp_path: Path, capsys):
    rc = main([f"{cmp_path}:1:1", "--dump"])
    assert rc == 0
    stdout = capsys.readouterr().out
    assert "start_chan=1" in stdout
    assert "hs_id=1" in stdout
    assert "chan   1" in stdout  # formatted with width
    assert "hs1-" in stdout
