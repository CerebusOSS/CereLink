"""ConnectionMode selection (#213): NATIVE is the default, CENTRAL is deprecated.

These tests need the cbsdk library but no device: a session that cannot reach
its device or Central fails to open, which is fine -- what is checked is the
warning behaviour around the attempt.
"""

import warnings

import pytest

from pycbsdk import ConnectionMode, DeviceType, Session


def _try_open(**kwargs):
    try:
        s = Session(DeviceType.HUB3, **kwargs)
    except RuntimeError:
        return
    s.close()


def test_connection_mode_values_match_c_enum():
    assert ConnectionMode.NATIVE == 0
    assert ConnectionMode.CENTRAL == 1


def test_central_mode_warns_deprecated():
    with pytest.warns(DeprecationWarning, match="CENTRAL is deprecated"):
        _try_open(connection_mode=ConnectionMode.CENTRAL)


def test_central_mode_accepts_string():
    with pytest.warns(DeprecationWarning):
        _try_open(connection_mode="central")


def test_native_default_does_not_warn():
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        _try_open()


def test_unknown_mode_rejected():
    with pytest.raises((ValueError, KeyError)):
        Session(DeviceType.HUB3, connection_mode="bogus")
