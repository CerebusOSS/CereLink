"""Unit tests for pycbsdk.cmp (channel-map parser and CLI helpers)."""

from __future__ import annotations

from pathlib import Path

import pytest

from pycbsdk import cmp as cmp_mod
from pycbsdk.cmp import CmpEntry, main, parse_cmp


def test_parse_keys_by_bank_term(cmp_path: Path):
    # 96-channel file: banks A, B, C (1..3) each with 32 electrodes.
    # start_chan=1 → bank offset 0, so keys are (bank, term) for banks 1..3.
    entries = parse_cmp(cmp_path)
    assert len(entries) == 96
    assert all(isinstance(e, CmpEntry) for e in entries.values())
    expected_keys = {(bank, term) for bank in (1, 2, 3) for term in range(1, 33)}
    assert set(entries) == expected_keys
    for (bank, term), entry in entries.items():
        assert entry.bank == bank
        assert entry.term == term


def test_parse_labels_are_verbatim(cmp_path: Path):
    # Labels are taken straight from the file; hs_id never prefixes them.
    entries = parse_cmp(cmp_path, hs_id=3)
    for entry in entries.values():
        assert not entry.label.startswith("hs")
    # The file's first electrode (bank A, term 1) is labelled "chan1".
    assert entries[(1, 1)].label == "chan1"


def test_hs_id_sets_headstage_not_label(cmp_path: Path):
    # Same file with hs_id=0 vs hs_id=3: identical labels, differing headstage.
    bare = parse_cmp(cmp_path)
    with_hs = parse_cmp(cmp_path, hs_id=3)
    for key, entry in with_hs.items():
        assert entry.headstage == 3
        assert bare[key].headstage == 0
        assert entry.label == bare[key].label


def test_parse_start_chan_offsets_banks(cmp_path: Path):
    # start_chan=129 → offset 129 // 32 = 4, so CMP banks A,B,C → device 5,6,7.
    entries = parse_cmp(cmp_path, start_chan=129, hs_id=2)
    assert len(entries) == 96
    assert {bank for bank, _ in entries} == {5, 6, 7}
    # chan1 row "0 5 A 1 chan1" → device (bank 5, term 1). Default index grid
    # → ×400: x=0, y=2000, size=400.
    first = entries[(5, 1)]
    assert (first.x, first.y) == (0, 2000)
    assert first.size == 400
    assert first.label == "chan1"
    assert all(e.headstage == 2 for e in entries.values())


def test_parse_matches_by_bank_term(tmp_path: Path):
    # Row order doesn't matter — entries are keyed by (bank, term).
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
    assert entries[(1, 1)].label == "label_a1"
    assert entries[(1, 2)].label == "label_a2"
    assert entries[(1, 3)].label == "label_a3"
    assert entries[(2, 1)].label == "label_b1"
    assert all(e.headstage == 1 for e in entries.values())


def test_header_declared_size_takes_face_value(tmp_path: Path):
    # A header naming a size column → parsed by name; with size present the
    # col/row/size values are taken at face value (no µm scaling).
    f = tmp_path / "size.cmp"
    f.write_text(
        "// scratch\n"
        "size test\n"
        "//col row bank elec size label\n"
        "0 0 A 1 4 elec_a1\n"
        "1 0 A 2 7 elec_a2\n"
        "2 0 A 3 0 elec_a3\n"
        "3 0 A 4 9\n"  # size=9, no label
    )
    entries = parse_cmp(f, hs_id=5)
    assert len(entries) == 4

    assert entries[(1, 1)].size == 4
    assert entries[(1, 1)].label == "elec_a1"
    assert entries[(1, 2)].size == 7
    assert entries[(1, 2)].label == "elec_a2"
    assert entries[(1, 3)].size == 0
    assert entries[(1, 3)].label == "elec_a3"
    # Size present, no label → empty label.
    assert entries[(1, 4)].size == 9
    assert entries[(1, 4)].label == ""
    # Face value: x is the raw col (no ×400) because a size column was supplied.
    assert (entries[(1, 4)].x, entries[(1, 4)].y) == (3, 0)


def test_header_driven_column_order(tmp_path: Path):
    # The header determines column order — here columns are reordered and a
    # size column is present, so values are taken at face value.
    f = tmp_path / "reorder.cmp"
    f.write_text(
        "// scratch\n"
        "reordered columns\n"
        "//label bank elec col row size\n"
        "foo A 1 7 9 2\n"
    )
    entries = parse_cmp(f)
    assert len(entries) == 1
    e = entries[(1, 1)]
    assert e.label == "foo"
    assert (e.x, e.y, e.size) == (7, 9, 2)


def test_non_uniform_grid_takes_face_value(tmp_path: Path):
    # No size column, but non-uniform spacing (row delta 4) → not a default
    # index grid → face value, size 0.
    f = tmp_path / "nonuniform.cmp"
    f.write_text(
        "// scratch\n"
        "non-uniform\n"
        "//col row bank elec label\n"
        "0 0 A 1 e1\n"
        "0 4 A 2 e2\n"
    )
    entries = parse_cmp(f)
    assert entries[(1, 2)].y == 4  # raw row, not scaled
    assert entries[(1, 2)].size == 0
    assert entries[(1, 1)].size == 0


def test_default_grid_scales_to_microns(tmp_path: Path):
    # No size column + unit-spaced grid → indices scaled ×400, size = 400.
    f = tmp_path / "grid.cmp"
    f.write_text(
        "// scratch\n"
        "unit grid\n"
        "0 0 A 1 e1\n"
        "1 0 A 2 e2\n"
        "0 1 A 3 e3\n"
    )
    entries = parse_cmp(f)
    assert entries[(1, 2)].x == 400  # col 1 → 400 µm
    assert entries[(1, 3)].y == 400  # row 1 → 400 µm
    assert all(e.size == 400 for e in entries.values())


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
    assert "bank=1 term= 1" in stdout  # formatted (bank, term)
    assert "hs=1" in stdout  # headstage column
    assert "chan1" in stdout  # verbatim label, no prefix
