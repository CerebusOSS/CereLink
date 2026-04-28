"""Integration tests for pycbsdk device configuration.

These tests exercise channel configuration, CCF/CMP loading, and session
properties against a running nPlayServer instance.

Most tests use the shared ``nplay_session`` fixture (session-scoped) to avoid
shmem/port exhaustion from creating too many sessions.  Tests that need a
fresh session (e.g. disable_others) create one explicitly but should be kept
to a minimum.
"""

import tempfile
import time
from pathlib import Path

import pytest

from pycbsdk import (
    ChanInfoField,
    ChannelType,
    DeviceType,
    ProtocolVersion,
    SampleRate,
    Session,
)

pytestmark = pytest.mark.integration


# ---------------------------------------------------------------------------
# Session lifecycle (these must create their own sessions to test creation)
# ---------------------------------------------------------------------------


class TestSessionLifecycle:
    """Basic session connection and property tests.

    Uses the shared session for read-only queries; creates a fresh session
    only for the connect-and-receive smoke test.
    """

    def test_session_connects_and_receives(self, nplay_session):
        """Smoke test: session is connected and receiving packets."""
        assert nplay_session.running
        time.sleep(1)
        stats = nplay_session.stats
        assert stats.packets_received > 0

    def test_session_is_standalone(self, nplay_session):
        assert nplay_session.is_standalone

    def test_protocol_version(self, nplay_session):
        assert nplay_session.protocol_version != ProtocolVersion.UNKNOWN

    def test_sysfreq(self, nplay_session):
        assert nplay_session.sysfreq == 30000

    def test_version_string(self, nplay_session):
        version = nplay_session.version()
        assert isinstance(version, str)
        assert len(version) > 0

    def test_max_chans(self, nplay_session):
        assert nplay_session.max_chans() > 0
        assert nplay_session.num_fe_chans() > 0
        assert nplay_session.num_analog_chans() >= nplay_session.num_fe_chans()

    def test_proc_ident(self, nplay_session):
        ident = nplay_session.proc_ident
        assert isinstance(ident, str)


# ---------------------------------------------------------------------------
# Channel sample group configuration
# ---------------------------------------------------------------------------


class TestChannelSampleGroup:
    """Tests for batch (set_sample_group) and per-channel (set_channel_smpgroup)."""

    # --- Batch setter (set_sample_group) ---

    def test_set_all_frontend_30k(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        nplay_session.set_sample_group(
            n_fe, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        nplay_session.sync()
        channels = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels) == n_fe, (
            f"Expected {n_fe} channels in 30kHz group, got {len(channels)}"
        )

    def test_set_all_frontend_30k_data_width(self, nplay_session):
        """Verify the data callback delivers exactly num_fe_chans samples."""
        n_fe = nplay_session.num_fe_chans()
        nplay_session.set_sample_group(
            n_fe, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        nplay_session.sync()

        import numpy as np
        widths = []

        @nplay_session.on_group(SampleRate.SR_30kHz, as_array=True)
        def on_sample(header, arr):
            if len(widths) < 10:
                widths.append(arr.shape[0])

        time.sleep(0.5)
        assert len(widths) > 0, "No group samples received"
        assert all(w == n_fe for w in widths), (
            f"Expected data width {n_fe}, got {widths}"
        )

    def test_set_and_verify_smpgroup_field(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        nplay_session.set_sample_group(
            n_fe, ChannelType.FRONTEND, SampleRate.SR_10kHz,
            disable_others=True,
        )
        nplay_session.sync()

        groups = nplay_session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.SMPGROUP, n_fe
        )
        assert all(g == int(SampleRate.SR_10kHz) for g in groups)

    def test_disable_others(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        # Enable all frontend channels at 30kHz
        nplay_session.set_sample_group(
            n_fe, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=False,
        )
        # Now set 2 at 1kHz with disable_others
        nplay_session.set_sample_group(
            2, ChannelType.FRONTEND, SampleRate.SR_1kHz,
            disable_others=True,
        )
        nplay_session.sync()

        # 30kHz group should be empty (others were disabled)
        channels_30k = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels_30k) == 0

        channels_1k = nplay_session.get_group_channels(int(SampleRate.SR_1kHz))
        assert len(channels_1k) == 2

    # --- Per-channel setter (set_channel_smpgroup) ---
    #
    # These tests use only per-channel setters for setup (no batch clear)
    # to avoid the fragile 256-packet burst that can drop on slow CI VMs.

    def test_single_channel_30k(self, nplay_session):
        """set_channel_smpgroup targets the exact channel ID."""
        nplay_session.set_channel_smpgroup(2, SampleRate.NONE)
        nplay_session.set_channel_smpgroup(2, SampleRate.SR_30kHz)
        nplay_session.sync()

        assert 2 in nplay_session.get_group_channels(int(SampleRate.SR_30kHz))

    def test_single_channel_raw(self, nplay_session):
        """set_channel_smpgroup with SR_RAW sets the RAWSTREAM flag."""
        nplay_session.set_channel_smpgroup(3, SampleRate.NONE)
        nplay_session.set_channel_smpgroup(3, SampleRate.SR_RAW)
        nplay_session.sync()

        channels = nplay_session.get_group_channels(int(SampleRate.SR_RAW))
        assert 3 in channels, f"Channel 3 not in RAW group: {channels}"

    def test_single_channel_disable(self, nplay_session):
        """set_channel_smpgroup with NONE removes the channel from its group."""
        nplay_session.set_channel_smpgroup(1, SampleRate.SR_30kHz)
        nplay_session.sync()
        assert 1 in nplay_session.get_group_channels(int(SampleRate.SR_30kHz))

        nplay_session.set_channel_smpgroup(1, SampleRate.NONE)
        nplay_session.sync()
        assert 1 not in nplay_session.get_group_channels(int(SampleRate.SR_30kHz))

    def test_single_channel_does_not_affect_others(self, nplay_session):
        """Setting channel 2 should not change channel 1's group."""
        nplay_session.set_channel_smpgroup(1, SampleRate.NONE)
        nplay_session.set_channel_smpgroup(2, SampleRate.NONE)
        nplay_session.sync()

        nplay_session.set_channel_smpgroup(1, SampleRate.SR_10kHz)
        nplay_session.set_channel_smpgroup(2, SampleRate.SR_30kHz)
        nplay_session.sync()

        ch_10k = nplay_session.get_group_channels(int(SampleRate.SR_10kHz))
        ch_30k = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert 1 in ch_10k, f"Channel 1 not in 10kHz group: {ch_10k}"
        assert 2 in ch_30k, f"Channel 2 not in 30kHz group: {ch_30k}"
        assert 1 not in ch_30k, f"Channel 1 leaked into 30kHz group"


# ---------------------------------------------------------------------------
# Async config race (ezmsg-blackrock CereLinkProducer.open() sequence)
# ---------------------------------------------------------------------------


class TestAsyncConfigRace:
    """Reproduces the config→immediate-read pattern from ezmsg-blackrock.

    CereLinkProducer.open() calls set_sample_group and
    set_ac_input_coupling (both fire-and-forget), then immediately reads
    get_group_channels and registers callbacks.  If the device hasn't
    processed all the config packets by then, the group list is incomplete
    and the data callback delivers the wrong number of channels.
    """

    N_CHANS = 128  # matches ezmsg-blackrock impedance test

    def test_immediate_group_read_with_sync(self, nplay_session):
        """Config 128 channels at SR_RAW, sync(), then read group list.

        Repeats 20 times to stress-test the sync barrier.  Without sync()
        this race produces incomplete channel lists ~5% of the time.
        """
        failures = []
        for i in range(20):
            # Reset to a known state first
            nplay_session.set_sample_group(
                self.N_CHANS, ChannelType.FRONTEND, SampleRate.NONE,
                disable_others=True,
            )
            nplay_session.sync()

            # Fire-and-forget config
            nplay_session.set_sample_group(
                self.N_CHANS, ChannelType.FRONTEND, SampleRate.SR_RAW,
                disable_others=True,
            )
            nplay_session.set_ac_input_coupling(
                self.N_CHANS, ChannelType.FRONTEND, False,
            )

            # sync() ensures all CHANREP responses are in shmem
            nplay_session.sync()

            channels = nplay_session.get_group_channels(int(SampleRate.SR_RAW))
            if len(channels) != self.N_CHANS:
                failures.append((i, len(channels)))

        assert not failures, (
            f"Incomplete config in {len(failures)}/20 iterations: "
            + ", ".join(f"iter {i}: got {n}" for i, n in failures)
        )

    def test_immediate_data_width(self, nplay_session):
        """Config 128 channels at SR_RAW, sync, register callback, verify width."""
        import numpy as np

        nplay_session.set_sample_group(
            self.N_CHANS, ChannelType.FRONTEND, SampleRate.SR_RAW,
            disable_others=True,
        )
        nplay_session.set_ac_input_coupling(
            self.N_CHANS, ChannelType.FRONTEND, False,
        )
        nplay_session.sync()

        widths = []

        @nplay_session.on_group(SampleRate.SR_RAW, as_array=True)
        def on_sample(header, arr):
            if len(widths) < 20:
                widths.append(arr.shape[0])

        time.sleep(1)

        assert len(widths) > 0, "No raw group samples received"
        # Every delivered sample should have exactly N_CHANS channels.
        bad = [w for w in widths if w != self.N_CHANS]
        assert not bad, (
            f"Expected data width {self.N_CHANS} but got widths {bad} "
            f"(first 20: {widths})"
        )


# ---------------------------------------------------------------------------
# Channel info accessors
# ---------------------------------------------------------------------------


class TestChannelInfo:
    """Tests for individual channel information retrieval."""

    def test_get_channel_label(self, nplay_session):
        label = nplay_session.get_channel_label(1)
        assert label is not None
        assert isinstance(label, str)
        assert len(label) > 0

    def test_get_channel_type(self, nplay_session):
        ct = nplay_session.get_channel_type(1)
        assert ct == ChannelType.FRONTEND

    def test_get_channel_chancaps(self, nplay_session):
        caps = nplay_session.get_channel_chancaps(1)
        assert caps > 0

    def test_get_channel_scaling(self, nplay_session):
        scaling = nplay_session.get_channel_scaling(1)
        assert scaling is not None
        assert "digmin" in scaling
        assert "digmax" in scaling
        assert "anamin" in scaling
        assert "anamax" in scaling
        assert "anagain" in scaling
        assert "anaunit" in scaling
        assert isinstance(scaling["anaunit"], str)

    def test_get_channel_config(self, nplay_session):
        config = nplay_session.get_channel_config(1)
        assert config is not None
        assert config["type"] == ChannelType.FRONTEND
        assert "label" in config
        assert "smpgroup" in config
        assert "chancaps" in config
        assert "smpfilter" in config
        assert "spkfilter" in config

    def test_get_channel_config_invalid(self, nplay_session):
        config = nplay_session.get_channel_config(0)
        assert config is None


# ---------------------------------------------------------------------------
# Bulk channel queries
# ---------------------------------------------------------------------------


class TestBulkChannelQueries:
    """Tests for get_matching_channel_ids and related bulk queries."""

    def test_get_matching_channel_ids(self, nplay_session):
        ids = nplay_session.get_matching_channel_ids(ChannelType.FRONTEND)
        assert len(ids) > 0
        assert all(i >= 1 for i in ids)

    def test_get_matching_channel_ids_limited(self, nplay_session):
        ids = nplay_session.get_matching_channel_ids(ChannelType.FRONTEND, n_chans=4)
        assert len(ids) == 4

    def test_get_channels_labels(self, nplay_session):
        labels = nplay_session.get_channels_labels(ChannelType.FRONTEND, n_chans=4)
        assert len(labels) == 4
        assert all(isinstance(l, str) and len(l) > 0 for l in labels)

    def test_get_channels_field(self, nplay_session):
        caps = nplay_session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.CHANCAPS, n_chans=4
        )
        assert len(caps) == 4
        assert all(c > 0 for c in caps)

    def test_get_channels_positions(self, nplay_session):
        positions = nplay_session.get_channels_positions(
            ChannelType.FRONTEND, n_chans=4
        )
        assert len(positions) == 4
        assert all(len(p) == 4 for p in positions)

    def test_get_group_channels(self, nplay_session):
        nplay_session.set_sample_group(
            4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        nplay_session.sync()
        channels = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels) >= 4

    def test_get_group_label(self, nplay_session):
        label = nplay_session.get_group_label(int(SampleRate.SR_30kHz))
        assert label is not None
        assert isinstance(label, str)


# ---------------------------------------------------------------------------
# AC input coupling
# ---------------------------------------------------------------------------


class TestACInputCoupling:
    """Tests for set_ac_input_coupling."""

    def test_set_ac_coupling(self, nplay_session):
        nplay_session.set_ac_input_coupling(
            4, ChannelType.FRONTEND, enabled=True,
        )
        nplay_session.sync()

    def test_set_dc_coupling(self, nplay_session):
        nplay_session.set_ac_input_coupling(
            4, ChannelType.FRONTEND, enabled=False,
        )
        nplay_session.sync()


# ---------------------------------------------------------------------------
# Spike sorting
# ---------------------------------------------------------------------------


class TestSpikeSorting:
    """Tests for spike sorting configuration."""

    def test_set_spike_sorting(self, nplay_session):
        nplay_session.set_spike_sorting(
            4, ChannelType.FRONTEND, sort_options=0,
        )
        nplay_session.sync()


# ---------------------------------------------------------------------------
# n_configured return + chans=None semantics
# ---------------------------------------------------------------------------


class TestNConfigured:
    """Bulk setters return the count of channels in the post-config state."""

    def test_set_sample_group_returns_count(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        n = nplay_session.set_sample_group(
            n_fe, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
        )
        assert n == n_fe
        # And matches a fresh re-scan via get_group_channels.
        assert n == len(nplay_session.get_group_channels(int(SampleRate.SR_30kHz)))

    def test_set_sample_group_chans_none_is_all(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        n = nplay_session.set_sample_group(
            None, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
        )
        assert n == n_fe

    def test_set_sample_group_partial(self, nplay_session):
        # Configure only the first 2 channels at 1kHz, leave the rest untouched.
        n = nplay_session.set_sample_group(
            2, ChannelType.FRONTEND, SampleRate.SR_1kHz, disable_others=False,
        )
        # Returned count is "channels at SR_1kHz post-config", i.e. exactly 2.
        assert n == 2

    def test_set_ac_input_coupling_returns_count(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        n_on = nplay_session.set_ac_input_coupling(
            None, ChannelType.FRONTEND, True,
        )
        assert n_on == n_fe
        n_off = nplay_session.set_ac_input_coupling(
            None, ChannelType.FRONTEND, False,
        )
        assert n_off == n_fe

    def test_set_spike_extraction_returns_count(self, nplay_session):
        n_fe = nplay_session.num_fe_chans()
        n_on = nplay_session.set_spike_extraction(
            None, ChannelType.FRONTEND, True,
        )
        assert n_on == n_fe
        n_off = nplay_session.set_spike_extraction(
            None, ChannelType.FRONTEND, False,
        )
        assert n_off == n_fe


# ---------------------------------------------------------------------------
# Explicit channel-list mode for bulk setters
# ---------------------------------------------------------------------------


class TestBulkSettersChanList:
    """``chans`` accepts an explicit list of 1-based channel ids."""

    def test_disjoint_ranges_via_two_calls(self, nplay_session):
        """Configure chans 1-2 to one rate, chans 3-4 to another."""
        n_lo = nplay_session.set_sample_group(
            [1, 2], ChannelType.FRONTEND, SampleRate.SR_1kHz,
            disable_others=False,
        )
        n_hi = nplay_session.set_sample_group(
            [3, 4], ChannelType.FRONTEND, SampleRate.SR_2kHz,
            disable_others=False,
        )
        # Returned counts cover all channels of the type matching each rate.
        # With only 4 chans in the nplay fixture, the second call's count
        # is the count of FE chans at SR_2kHz post-config = 2.
        assert n_lo >= 2
        assert n_hi >= 2
        assert 1 in nplay_session.get_group_channels(int(SampleRate.SR_1kHz))
        assert 2 in nplay_session.get_group_channels(int(SampleRate.SR_1kHz))
        assert 3 in nplay_session.get_group_channels(int(SampleRate.SR_2kHz))
        assert 4 in nplay_session.get_group_channels(int(SampleRate.SR_2kHz))

    def test_disjoint_set(self, nplay_session):
        """Non-contiguous list of channels is configured correctly."""
        # First clear any prior state on these chans.
        nplay_session.set_sample_group(
            None, ChannelType.FRONTEND, SampleRate.NONE, disable_others=True,
        )
        nplay_session.set_sample_group(
            [1, 3], ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=False,
        )
        ch_30k = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert 1 in ch_30k
        assert 3 in ch_30k
        assert 2 not in ch_30k

    def test_disable_others_with_list(self, nplay_session):
        """disable_others=True with an explicit list disables every FE chan
        that isn't in the list."""
        n_fe = nplay_session.num_fe_chans()
        # Enable everything at 30kHz first.
        nplay_session.set_sample_group(
            None, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
        )
        # Now keep only chans 1, 2 at 1kHz; disable everything else.
        n = nplay_session.set_sample_group(
            [1, 2], ChannelType.FRONTEND, SampleRate.SR_1kHz, disable_others=True,
        )
        assert n == 2
        ch_1k = nplay_session.get_group_channels(int(SampleRate.SR_1kHz))
        ch_30k = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert sorted(ch_1k) == [1, 2]
        assert ch_30k == [] or set(ch_30k).isdisjoint({1, 2}) and not ch_30k
        if n_fe > 2:
            # The not-listed FE chans should be disabled (smpgroup=0).
            for chan_id in range(3, n_fe + 1):
                assert chan_id not in ch_30k

    def test_list_with_set_ac_input_coupling(self, nplay_session):
        """Explicit list works for set_ac_input_coupling."""
        n_on = nplay_session.set_ac_input_coupling(
            [1, 3], ChannelType.FRONTEND, True,
        )
        # post-config count = at least 2 chans with offset_correct on
        assert n_on >= 2

    def test_list_with_set_spike_extraction(self, nplay_session):
        """Explicit list works for set_spike_extraction."""
        # First enable extraction everywhere
        nplay_session.set_spike_extraction(
            None, ChannelType.FRONTEND, True,
        )
        # Then disable just chans 1, 3
        n_on = nplay_session.set_spike_extraction(
            [1, 3], ChannelType.FRONTEND, False,
        )
        # After: chans 1, 3 disabled; rest enabled. Returned count is the
        # number of FE chans with EXTRACT == False (i.e., 2).
        assert n_on == 2

    def test_empty_list_with_disable_others(self, nplay_session):
        """Empty list + disable_others disables all FE chans."""
        # Enable everything first
        nplay_session.set_sample_group(
            None, ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=True,
        )
        # Empty list with disable_others: nothing configured, all disabled.
        n = nplay_session.set_sample_group(
            [], ChannelType.FRONTEND, SampleRate.SR_1kHz, disable_others=True,
        )
        assert n == 0
        assert nplay_session.get_group_channels(int(SampleRate.SR_30kHz)) == []
        assert nplay_session.get_group_channels(int(SampleRate.SR_1kHz)) == []

    def test_tuple_and_range_accepted(self, nplay_session):
        """chans accepts any iterable of ints, not just list."""
        nplay_session.set_sample_group(
            (1, 2), ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=False,
        )
        nplay_session.set_sample_group(
            range(1, 3), ChannelType.FRONTEND, SampleRate.SR_30kHz, disable_others=False,
        )


# ---------------------------------------------------------------------------
# Async wait_until_running
# ---------------------------------------------------------------------------


class TestWaitUntilRunning:
    """Async runlevel-awaitable.

    The fixture brings the device up to RUNNING before yielding the session,
    so wait_until_running() returns immediately without registering a
    listener; covering the registration path requires fault injection that
    isn't worth the test complexity.
    """

    def test_returns_immediately_when_running(self, nplay_session):
        import asyncio
        from pycbsdk.session import RUNLEVEL_RUNNING

        async def run() -> None:
            assert nplay_session.runlevel >= RUNLEVEL_RUNNING
            await nplay_session.wait_until_running(timeout=1.0)

        asyncio.run(run())

    def test_wait_for_runlevel_immediate(self, nplay_session):
        import asyncio
        from pycbsdk.session import RUNLEVEL_STANDBY

        async def run() -> None:
            await nplay_session.wait_for_runlevel(RUNLEVEL_STANDBY, timeout=1.0)

        asyncio.run(run())


# ---------------------------------------------------------------------------
# Per-channel configuration (configure_channel)
# ---------------------------------------------------------------------------


class TestConfigureChannel:
    """Tests for the configure_channel convenience method."""

    def test_configure_smpfilter(self, nplay_session):
        orig = nplay_session.get_channel_smpfilter(1)
        nplay_session.configure_channel(1, smpfilter=2)
        nplay_session.sync()
        assert nplay_session.get_channel_smpfilter(1) == 2
        nplay_session.configure_channel(1, smpfilter=orig)
        nplay_session.sync()

    def test_configure_spkfilter(self, nplay_session):
        # Read original value so we can restore it afterward (other tests
        # depend on a valid spike filter being configured).
        orig = nplay_session.get_channel_spkfilter(1)
        nplay_session.configure_channel(1, spkfilter=3)
        nplay_session.sync()
        assert nplay_session.get_channel_spkfilter(1) == 3
        # Restore
        nplay_session.configure_channel(1, spkfilter=orig)
        nplay_session.sync()

    def test_configure_label(self, nplay_session):
        nplay_session.configure_channel(1, label="TestCh")
        nplay_session.sync()
        assert nplay_session.get_channel_label(1) == "TestCh"

    def test_configure_multiple_attrs(self, nplay_session):
        orig_filter = nplay_session.get_channel_smpfilter(1)
        orig_label = nplay_session.get_channel_label(1)
        nplay_session.configure_channel(
            1,
            smpfilter=4,
            label="Multi",
        )
        nplay_session.sync()
        assert nplay_session.get_channel_smpfilter(1) == 4
        assert nplay_session.get_channel_label(1) == "Multi"
        # Restore
        nplay_session.configure_channel(1, smpfilter=orig_filter,
                                        label=orig_label or "chan1")
        nplay_session.sync()

    def test_configure_unknown_attr_raises(self, nplay_session):
        with pytest.raises(ValueError, match="Unknown channel attribute"):
            nplay_session.configure_channel(1, nonexistent_attr=42)


# ---------------------------------------------------------------------------
# Per-channel setters (direct API, not configure_channel)
# ---------------------------------------------------------------------------


class TestPerChannelSetters:
    """Tests for individual channel attribute setters with readback."""

    def test_set_channel_ainpopts(self, nplay_session):
        orig = nplay_session.get_channel_ainpopts(1)
        nplay_session.set_channel_ainpopts(1, 0)
        nplay_session.sync()
        assert nplay_session.get_channel_ainpopts(1) == 0
        # Restore
        nplay_session.set_channel_ainpopts(1, orig)
        nplay_session.sync()

    def test_set_channel_lncrate(self, nplay_session):
        orig = nplay_session.get_channel_lncrate(1)
        nplay_session.set_channel_lncrate(1, 2)
        nplay_session.sync()
        assert nplay_session.get_channel_lncrate(1) == 2
        # Restore
        nplay_session.set_channel_lncrate(1, orig)
        nplay_session.sync()

    def test_set_channel_spkthrlevel(self, nplay_session):
        orig = nplay_session.get_channel_spkthrlevel(1)
        nplay_session.set_channel_spkthrlevel(1, -100)
        nplay_session.sync()
        assert nplay_session.get_channel_spkthrlevel(1) == -100
        # Restore
        nplay_session.set_channel_spkthrlevel(1, orig)
        nplay_session.sync()

    def test_set_channel_autothreshold(self, nplay_session):
        # Enable auto-threshold, verify spkopts has the THRAUTO bit set
        nplay_session.set_channel_autothreshold(1, True)
        nplay_session.sync()
        spkopts = nplay_session.get_channel_spkopts(1)
        THRAUTO = 0x00000400
        assert spkopts & THRAUTO, (
            f"THRAUTO bit not set after enable: spkopts=0x{spkopts:08X}"
        )
        # Disable
        nplay_session.set_channel_autothreshold(1, False)
        nplay_session.sync()
        spkopts = nplay_session.get_channel_spkopts(1)
        assert not (spkopts & THRAUTO), (
            f"THRAUTO bit still set after disable: spkopts=0x{spkopts:08X}"
        )

    def test_set_channel_spkopts(self, nplay_session):
        orig = nplay_session.get_channel_spkopts(1)
        # Set a known value (THRLEVEL only)
        THRLEVEL = 0x00000100
        nplay_session.set_channel_spkopts(1, THRLEVEL)
        nplay_session.sync()
        result = nplay_session.get_channel_spkopts(1)
        assert result & THRLEVEL, (
            f"THRLEVEL bit not set: spkopts=0x{result:08X}"
        )
        # Restore
        nplay_session.set_channel_spkopts(1, orig)
        nplay_session.sync()

    def test_set_channel_label(self, nplay_session):
        orig = nplay_session.get_channel_label(1)
        nplay_session.set_channel_label(1, "MyLabel")
        nplay_session.sync()
        assert nplay_session.get_channel_label(1) == "MyLabel"
        # Restore
        nplay_session.set_channel_label(1, orig or "chan1")
        nplay_session.sync()


# ---------------------------------------------------------------------------
# Filter info
# ---------------------------------------------------------------------------


class TestFilterInfo:
    """Tests for filter information retrieval."""

    def test_num_filters(self, nplay_session):
        n = nplay_session.num_filters()
        assert n > 0

    def test_get_filter_info(self, nplay_session):
        info = nplay_session.get_filter_info(0)
        if info is not None:
            assert "label" in info
            assert "hpfreq" in info
            assert "lpfreq" in info

    def test_get_filter_info_invalid(self, nplay_session):
        info = nplay_session.get_filter_info(9999)
        assert info is None


# ---------------------------------------------------------------------------
# CCF files
# ---------------------------------------------------------------------------


class TestCCF:
    """Tests for CCF save/load."""

    def test_save_ccf(self, nplay_session):
        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_out = f.name
        try:
            nplay_session.save_ccf(ccf_out)
            assert Path(ccf_out).stat().st_size > 0
        finally:
            Path(ccf_out).unlink(missing_ok=True)

    @staticmethod
    def _wait_for_smpfilter(session, chan_id, expected, timeout=5.0):
        """Poll until channel smpfilter matches *expected* or timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if session.get_channel_smpfilter(chan_id) == expected:
                return
            time.sleep(0.1)
        assert session.get_channel_smpfilter(chan_id) == expected

    def test_load_ccf_sync(self, nplay_session):
        # Verify on the *last* frontend channel — channel packets are sent
        # in ascending order, so the last one confirms all preceding packets
        # were also applied.  Use a non-zero baseline because CCF skips
        # zero fields.  This test runs before test_load_ccf so the async
        # CCF burst doesn't leave stale packets in the send queue.
        last_ch = nplay_session.num_fe_chans()
        baseline = 2
        canary = 3
        nplay_session.set_channel_smpfilter(last_ch, baseline)
        self._wait_for_smpfilter(nplay_session, last_ch, baseline)

        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_file = f.name
        try:
            nplay_session.save_ccf(ccf_file)
            nplay_session.set_channel_smpfilter(last_ch, canary)
            self._wait_for_smpfilter(nplay_session, last_ch, canary)

            nplay_session.load_ccf_sync(ccf_file, timeout=5.0)
            self._wait_for_smpfilter(nplay_session, last_ch, baseline)
        finally:
            Path(ccf_file).unlink(missing_ok=True)

    def test_load_ccf(self, nplay_session):
        # Async load_ccf is fire-and-forget — it offers no completion
        # guarantee, so we only verify the call succeeds.  The meaningful
        # config-applied verification lives in test_load_ccf_sync above.
        # Save and reload the current config (a no-op) to avoid changing
        # device state that could affect subsequent tests.
        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_file = f.name
        try:
            nplay_session.save_ccf(ccf_file)
            nplay_session.load_ccf(ccf_file)
        finally:
            Path(ccf_file).unlink(missing_ok=True)

    def test_load_ccf_sync_invalid_file(self, nplay_session):
        with pytest.raises(RuntimeError):
            nplay_session.load_ccf_sync("/nonexistent.ccf", timeout=1.0)

    def test_save_load_roundtrip(self, nplay_session):
        nplay_session.set_sample_group(
            4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        nplay_session.sync()

        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_file = f.name
        try:
            nplay_session.save_ccf(ccf_file)
            assert Path(ccf_file).stat().st_size > 0

            nplay_session.load_ccf(ccf_file)
            nplay_session.sync()

            channels = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
            assert len(channels) >= 4
        finally:
            Path(ccf_file).unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# CCF loading — CLIENT mode
# ---------------------------------------------------------------------------


class TestClientCCF:
    """CCF operations through a CLIENT-mode session (shmem only, no device)."""

    @staticmethod
    def _wait_for_smpfilter(session, chan_id, expected, timeout=5.0):
        """Poll until channel smpfilter matches *expected* or timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if session.get_channel_smpfilter(chan_id) == expected:
                return
            time.sleep(0.1)
        assert session.get_channel_smpfilter(chan_id) == expected

    def test_load_ccf_sync(self, nplay_session, client_session):
        """CLIENT-mode load_ccf_sync: exercises SYSREP detection via shmem."""
        last_ch = nplay_session.num_fe_chans()
        baseline = 2
        canary = 3

        # STANDALONE sets the baseline, then saves a CCF snapshot
        nplay_session.set_channel_smpfilter(last_ch, baseline)
        self._wait_for_smpfilter(nplay_session, last_ch, baseline)

        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_file = f.name
        try:
            nplay_session.save_ccf(ccf_file)

            # STANDALONE changes to canary so we can tell when the CCF restores
            nplay_session.set_channel_smpfilter(last_ch, canary)
            self._wait_for_smpfilter(nplay_session, last_ch, canary)

            # CLIENT loads CCF and waits for SYSREP through shmem
            client_session.load_ccf_sync(ccf_file, timeout=5.0)

            # Verify via STANDALONE that the baseline was restored
            self._wait_for_smpfilter(nplay_session, last_ch, baseline)
        finally:
            Path(ccf_file).unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# CMP files
# ---------------------------------------------------------------------------


class TestCMP:
    """Tests for channel mapping (CMP) file loading.

    ``load_channel_map`` writes positions into shmem synchronously, but pushes
    labels to the device asynchronously (one ``CHANSETLABEL`` packet per
    channel; the device echoes a ``CHANREP`` for each). ``session.sync()``
    sends a no-op runlevel SET and waits for the matching ``SYSREPRUNLEV``,
    which guarantees every preceding ``CHANREP`` has been mirrored into shmem.

    The shared ``nplay_session`` plays back the 4-channel ``dnss`` recording,
    so these tests intersect the parsed CMP entries with the frontend channels
    the device actually reports. End-to-end coverage of larger channel counts
    lives in the C++ integration tests against ``dnss256``.
    """

    @staticmethod
    def _frontend_view(session):
        """Return ``{chan_id: (position, label)}`` for present FRONTEND chans."""
        chan_ids = session.get_matching_channel_ids(ChannelType.FRONTEND)
        positions = session.get_channels_positions(ChannelType.FRONTEND)
        labels = session.get_channels_labels(ChannelType.FRONTEND)
        return {c: (p, l) for c, p, l in zip(chan_ids, positions, labels)}

    def test_load_channel_map_smoke(self, nplay_session, cmp_path):
        """Loading a CMP and syncing succeeds without raising."""
        nplay_session.load_channel_map(str(cmp_path))
        nplay_session.sync()

    def test_load_nonexistent_file_raises(self, nplay_session, tmp_path):
        bogus = tmp_path / "does_not_exist.cmp"
        with pytest.raises(RuntimeError):
            nplay_session.load_channel_map(str(bogus))

    def test_positions_match_parsed(self, nplay_session, cmp_path):
        """For each FE chan present on the device, position == parser output."""
        from pycbsdk.cmp import parse_cmp

        nplay_session.load_channel_map(str(cmp_path))
        nplay_session.sync()

        expected = parse_cmp(str(cmp_path))
        view = self._frontend_view(nplay_session)
        intersect = sorted(set(expected) & set(view))
        assert intersect, "no FRONTEND chans overlap parsed CMP entries"

        for chan_id in intersect:
            actual_pos, _ = view[chan_id]
            assert actual_pos == expected[chan_id].position, (
                f"chan {chan_id}: expected {expected[chan_id].position}, got {actual_pos}"
            )

    def test_labels_round_trip_through_device(self, nplay_session, cmp_path):
        """After load + sync, labels read back through the device match the prefix."""
        from pycbsdk.cmp import parse_cmp

        nplay_session.load_channel_map(str(cmp_path), hs_id=7)
        nplay_session.sync()

        expected = parse_cmp(str(cmp_path), hs_id=7)
        view = self._frontend_view(nplay_session)
        intersect = sorted(set(expected) & set(view))
        assert intersect

        for chan_id in intersect:
            _, actual_label = view[chan_id]
            assert actual_label == expected[chan_id].label, (
                f"chan {chan_id}: expected {expected[chan_id].label!r}, "
                f"got {actual_label!r}"
            )
            # All labels carry the hs7 prefix we requested.
            assert actual_label.startswith("hs7-")

    def test_hs_id_prefix_changes_label(self, nplay_session, cmp_path):
        """Loading with a different hs_id rewrites labels on the device."""
        from pycbsdk.cmp import parse_cmp

        cmp_chans = set(parse_cmp(str(cmp_path)))

        nplay_session.load_channel_map(str(cmp_path), hs_id=4)
        nplay_session.sync()
        before = self._frontend_view(nplay_session)

        nplay_session.load_channel_map(str(cmp_path), hs_id=5)
        nplay_session.sync()
        after = self._frontend_view(nplay_session)

        # Only chans the CMP covers carry the hs prefix; the rest keep their
        # device-default labels. Restrict the check to covered chans that are
        # also visible on the device.
        relabeled = sorted(set(before) & set(after) & cmp_chans)
        assert relabeled
        for chan_id in relabeled:
            assert before[chan_id][1].startswith("hs4-")
            assert after[chan_id][1].startswith("hs5-")
            # The original (un-prefixed) label is unchanged across the two loads.
            assert before[chan_id][1].split("-", 1)[1] == after[chan_id][1].split("-", 1)[1]

    def test_start_chan_assignment(
        self, nplay_session, manufacturer_cmp_path
    ):
        """Loading the same file at start_chan=1 vs 129 produces matching layouts.

        Parser output for ``(start_chan=1, hs_id=1)`` and ``(start_chan=129,
        hs_id=2)`` must agree on (col, row, bank, electrode) at offset chans —
        e.g. chan 1 under hs1 has the same position as chan 129 under hs2 — and
        whichever of those chans the device actually reports must reflect the
        right layout.
        """
        from pycbsdk.cmp import parse_cmp

        # Load both headstages so chans 1.. and 129.. are populated.
        nplay_session.load_channel_map(
            str(manufacturer_cmp_path), start_chan=1, hs_id=1
        )
        nplay_session.load_channel_map(
            str(manufacturer_cmp_path), start_chan=129, hs_id=2
        )
        nplay_session.sync()

        hs1 = parse_cmp(str(manufacturer_cmp_path), start_chan=1, hs_id=1)
        hs2 = parse_cmp(str(manufacturer_cmp_path), start_chan=129, hs_id=2)

        # Shared invariant: corresponding (chan, chan+128) entries describe the
        # same electrode and differ only in the hs prefix.
        for sorted_idx in range(min(len(hs1), len(hs2))):
            assert hs1[1 + sorted_idx].position == hs2[129 + sorted_idx].position
            assert hs1[1 + sorted_idx].label == hs2[129 + sorted_idx].label.replace(
                "hs2-", "hs1-", 1
            )

        # And on the device, every present FE chan that falls in either range
        # matches its parsed entry.
        view = self._frontend_view(nplay_session)
        for chan_id, (pos, label) in view.items():
            if chan_id in hs1:
                assert pos == hs1[chan_id].position
                assert label == hs1[chan_id].label
            elif chan_id in hs2:
                assert pos == hs2[chan_id].position
                assert label == hs2[chan_id].label

    def test_overlay_survives_chanrep_refresh(self, nplay_session, cmp_path):
        """A CHANREP echoed back from the device should not erase our overlay.

        ``setChannelConfig`` triggers the device to send a CHANREP for the
        channel we just labeled. The receive path re-applies the CMP overlay
        before mirroring the CHANREP into shmem; otherwise, the position would
        get clobbered by whatever the device thinks (which is not persisted).
        """
        from pycbsdk.cmp import parse_cmp

        nplay_session.load_channel_map(str(cmp_path), hs_id=9)
        nplay_session.sync()

        expected = parse_cmp(str(cmp_path), hs_id=9)
        view = self._frontend_view(nplay_session)
        for chan_id in sorted(set(expected) & set(view)):
            pos, label = view[chan_id]
            assert pos == expected[chan_id].position
            assert label == expected[chan_id].label

    def test_clear_channel_map_resets_labels(self, nplay_session, cmp_path):
        """clear_channel_map() reverts labels to the device default ("chanN")."""
        from pycbsdk.cmp import parse_cmp

        nplay_session.load_channel_map(str(cmp_path), hs_id=11)
        nplay_session.sync()

        expected = parse_cmp(str(cmp_path), hs_id=11)
        view_before = self._frontend_view(nplay_session)
        loaded = sorted(set(expected) & set(view_before))
        assert loaded
        for chan_id in loaded:
            _, label = view_before[chan_id]
            assert label.startswith("hs11-")

        nplay_session.clear_channel_map()
        nplay_session.sync()

        view_after = self._frontend_view(nplay_session)
        for chan_id in loaded:
            _, label = view_after[chan_id]
            assert label == f"chan{chan_id}", (
                f"chan {chan_id}: expected default label, got {label!r}"
            )

    def test_clear_channel_map_resets_positions(self, nplay_session, cmp_path):
        """clear_channel_map() drops the position overlay; positions revert to zero."""
        nplay_session.load_channel_map(str(cmp_path))
        nplay_session.sync()

        view_before = self._frontend_view(nplay_session)
        loaded = [c for c, (pos, _) in view_before.items() if any(p != 0 for p in pos)]
        assert loaded, "expected at least one frontend chan with non-zero position"

        nplay_session.clear_channel_map()
        nplay_session.sync()

        view_after = self._frontend_view(nplay_session)
        for chan_id in loaded:
            pos, _ = view_after[chan_id]
            assert all(p == 0 for p in pos), (
                f"chan {chan_id}: expected zeroed position after clear, got {pos}"
            )

    def test_clear_channel_map_empty_is_noop(self, nplay_session):
        """Calling clear_channel_map() with no map loaded is a no-op."""
        nplay_session.clear_channel_map()  # must not raise


# ---------------------------------------------------------------------------
# Spike length configuration
# ---------------------------------------------------------------------------


class TestSpikeLength:
    """Tests for spike length configuration."""

    def test_get_spike_length(self, nplay_session):
        length = nplay_session.spike_length
        assert length > 0

    def test_get_spike_pretrigger(self, nplay_session):
        pre = nplay_session.spike_pretrigger
        assert pre >= 0

    def test_set_spike_length(self, nplay_session):
        nplay_session.set_spike_length(48, 12)
        nplay_session.sync()
        assert nplay_session.spike_length == 48
        # nPlayServer may clamp pretrigger to its own limits
        assert nplay_session.spike_pretrigger > 0


# ---------------------------------------------------------------------------
# Full config snapshot
# ---------------------------------------------------------------------------


class TestGetConfig:
    """Tests for the get_config bulk accessor."""

    def test_get_config(self, nplay_session):
        config = nplay_session.get_config()
        assert "sysfreq" in config
        assert config["sysfreq"] == 30000
        assert "channels" in config
        assert "groups" in config
        assert "filters" in config
        assert len(config["channels"]) > 0
