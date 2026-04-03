"""Integration tests for pycbsdk clock synchronization.

Tests the device timestamp, clock offset estimation, clock probing, and
the device_to_monotonic conversion chain against a running nPlayServer.

Uses the shared ``nplay_session`` fixture.  The session has been running
for many seconds by the time these tests execute (after config and data
reception tests), so clock probes have had ample time to complete.

Note: nPlayServer loops a short file (~2s with dnss256), so device
timestamps can wrap.  Tests that compare successive timestamps use short
measurement windows to stay within one pass.
"""

import time

import pytest

from pycbsdk import (
    ChannelType,
    DeviceType,
    SampleRate,
    Session,
)

pytestmark = pytest.mark.integration

# The SDK sends an initial clock probe at handshake completion, then
# periodic probes every 2 seconds.  3 seconds is enough for the initial
# probe + one periodic cycle to complete.
CLOCK_SYNC_WAIT_S = 3


@pytest.fixture(scope="module")
def synced_session(nplay_session):
    """The shared session, after waiting for clock sync probes to complete."""
    time.sleep(CLOCK_SYNC_WAIT_S)
    assert nplay_session.clock_offset_ns is not None, (
        f"clock_offset_ns still None after {CLOCK_SYNC_WAIT_S}s — "
        "nPlayServer may not be echoing NPLAYSET probes"
    )
    return nplay_session


# ---------------------------------------------------------------------------
# Device timestamp
# ---------------------------------------------------------------------------


class TestDeviceTime:
    """Tests for the ``time`` property (most recent device timestamp)."""

    def test_time_nonzero(self, nplay_session):
        """After handshake, device timestamp should be nonzero."""
        t = nplay_session.time
        assert t > 0, f"Device time is {t}, expected > 0"

    def test_time_advances(self, nplay_session):
        """Device timestamp should increase over time."""
        t1 = nplay_session.time
        time.sleep(0.1)
        t2 = nplay_session.time
        if t2 <= t1:
            # File looped; retry
            t1 = t2
            time.sleep(0.1)
            t2 = nplay_session.time
        assert t2 > t1, f"Device time did not advance: {t1} -> {t2}"

    def test_time_rate(self, nplay_session):
        """Device timestamp should advance at roughly real-time rate."""
        t1 = nplay_session.time
        time.sleep(0.3)
        t2 = nplay_session.time

        if t2 <= t1:
            # File looped — retry once
            t1 = t2
            time.sleep(0.3)
            t2 = nplay_session.time

        delta_ns = t2 - t1
        delta_s = delta_ns / 1e9
        assert 0.1 < delta_s < 1.0, (
            f"Device time advanced {delta_s:.3f}s in ~0.3s wall-time"
        )


# ---------------------------------------------------------------------------
# Clock offset
# ---------------------------------------------------------------------------


class TestClockOffset:
    """Tests for clock_offset_ns and clock_uncertainty_ns.

    Uses the ``synced_session`` fixture which waits for clock probes to
    complete before yielding.
    """

    def test_clock_offset_available(self, synced_session):
        """Clock offset must be available after probe completes."""
        offset = synced_session.clock_offset_ns
        assert offset is not None, "clock_offset_ns is None"
        assert isinstance(offset, int)

    def test_clock_uncertainty_available(self, synced_session):
        """Clock uncertainty (half-RTT) must be available."""
        uncertainty = synced_session.clock_uncertainty_ns
        assert uncertainty is not None, "clock_uncertainty_ns is None"
        assert isinstance(uncertainty, int)

    def test_clock_uncertainty_small_on_localhost(self, synced_session):
        """On localhost, half-RTT should be well under 100ms."""
        uncertainty = synced_session.clock_uncertainty_ns
        assert uncertainty is not None
        uncertainty_ms = abs(uncertainty) / 1e6
        assert uncertainty_ms < 100, (
            f"Clock uncertainty {uncertainty_ms:.1f}ms — too large for localhost"
        )


# ---------------------------------------------------------------------------
# Clock probing
# ---------------------------------------------------------------------------


class TestClockProbe:
    """Tests for send_clock_probe."""

    def test_send_probe_succeeds(self, nplay_session):
        """Sending a clock probe should not raise."""
        nplay_session.send_clock_probe()

    def test_multiple_probes(self, nplay_session):
        """Sending multiple probes should not raise or crash."""
        for _ in range(5):
            nplay_session.send_clock_probe()
            time.sleep(0.2)


# ---------------------------------------------------------------------------
# device_to_monotonic conversion
# ---------------------------------------------------------------------------


class TestDeviceToMonotonic:
    """Tests for the device_to_monotonic timestamp conversion."""

    def test_converts_without_error(self, synced_session):
        """device_to_monotonic should succeed."""
        device_ns = synced_session.time
        mono = synced_session.device_to_monotonic(device_ns)
        assert isinstance(mono, float)

    def test_result_close_to_current_monotonic(self, synced_session):
        """Converted time should be close to current time.monotonic()."""
        device_ns = synced_session.time
        mono_converted = synced_session.device_to_monotonic(device_ns)
        mono_now = time.monotonic()

        # The converted time should be in the recent past (within 5s)
        delta = mono_now - mono_converted
        assert -1 < delta < 5, (
            f"device_to_monotonic off by {delta:.3f}s from time.monotonic()"
        )

    def test_monotonicity(self, synced_session):
        """Successive conversions within one playback pass should increase."""
        t1_dev = synced_session.time
        m1 = synced_session.device_to_monotonic(t1_dev)

        time.sleep(0.1)
        t2_dev = synced_session.time
        m2 = synced_session.device_to_monotonic(t2_dev)

        if t2_dev > t1_dev:
            assert m2 > m1, (
                f"Converted times not monotonic: {m1:.6f} -> {m2:.6f}"
            )
        # If t2_dev < t1_dev, the file looped — cannot assert monotonicity
