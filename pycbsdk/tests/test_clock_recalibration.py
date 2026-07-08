"""Unit tests for Session._maybe_recalibrate_monotonic (no hardware).

The monotonic↔steady offset is measured once at session creation, but the two
clocks can diverge over a long session (notably across host sleep on macOS).
These tests drive the drift-detection logic directly with a mocked clock/lib,
using Session.__new__ to bypass the device-connecting constructor.
"""

import types

import pytest

from pycbsdk import Session
import pycbsdk.session as _session_mod


def _bare_session() -> Session:
    """A Session instance with only the fields the recalibration logic needs."""
    s = Session.__new__(Session)
    s._mono_to_steady_offset_ns = 0
    s._last_recal_probe_mono_ns = 0
    return s


def _patch_clock(monkeypatch, *, mono_now: int, steady_now: int) -> dict:
    """Pin _time.monotonic_ns and the lib's steady clock; track recalibration."""
    monkeypatch.setattr(
        _session_mod, "_time", types.SimpleNamespace(monotonic_ns=lambda: mono_now)
    )
    monkeypatch.setattr(
        _session_mod,
        "_get_lib",
        lambda: types.SimpleNamespace(cbsdk_get_steady_clock_ns=lambda: steady_now),
    )
    calls = {"n": 0}

    def _fake_calibrate():
        calls["n"] += 1
        return steady_now - mono_now

    monkeypatch.setattr(
        Session, "_calibrate_monotonic_offset", staticmethod(_fake_calibrate)
    )
    return calls


def test_recalibrates_on_drift(monkeypatch):
    """A large steady↔monotonic divergence (e.g. host sleep) triggers recal."""
    mono = 10 * Session._RECAL_PROBE_INTERVAL_NS
    calls = _patch_clock(monkeypatch, mono_now=mono, steady_now=mono + 5_000_000)  # +5 ms
    s = _bare_session()
    s._last_recal_probe_mono_ns = mono - Session._RECAL_PROBE_INTERVAL_NS - 1  # gate open

    s._maybe_recalibrate_monotonic()

    assert calls["n"] == 1
    assert s._mono_to_steady_offset_ns == 5_000_000
    assert s._last_recal_probe_mono_ns == mono  # probe timestamp advanced


def test_no_recalibration_within_threshold(monkeypatch):
    """A sub-threshold divergence does not trigger a full recalibration."""
    mono = 10 * Session._RECAL_PROBE_INTERVAL_NS
    calls = _patch_clock(monkeypatch, mono_now=mono, steady_now=mono + 100_000)  # +0.1 ms
    s = _bare_session()
    s._last_recal_probe_mono_ns = mono - Session._RECAL_PROBE_INTERVAL_NS - 1  # gate open

    s._maybe_recalibrate_monotonic()

    assert calls["n"] == 0
    assert s._mono_to_steady_offset_ns == 0


def test_probe_interval_gate(monkeypatch):
    """Within the probe interval, no clock probe (or recalibration) happens."""
    mono = 10 * Session._RECAL_PROBE_INTERVAL_NS

    def _boom():
        raise AssertionError("steady clock probed within the gate interval")

    monkeypatch.setattr(
        _session_mod, "_time", types.SimpleNamespace(monotonic_ns=lambda: mono)
    )
    monkeypatch.setattr(
        _session_mod,
        "_get_lib",
        lambda: types.SimpleNamespace(cbsdk_get_steady_clock_ns=_boom),
    )
    s = _bare_session()
    s._last_recal_probe_mono_ns = mono  # just probed → gate closed

    s._maybe_recalibrate_monotonic()  # must return before probing

    assert s._mono_to_steady_offset_ns == 0
