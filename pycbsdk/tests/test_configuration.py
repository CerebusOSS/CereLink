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
    """Tests for set_channel_sample_group and related queries."""

    def test_set_frontend_30k(self, nplay_session):
        n_chans = 4
        nplay_session.set_channel_sample_group(
            n_chans, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        time.sleep(0.5)
        channels = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels) >= n_chans

    def test_set_and_verify_smpgroup_field(self, nplay_session):
        n_chans = 4
        nplay_session.set_channel_sample_group(
            n_chans, ChannelType.FRONTEND, SampleRate.SR_10kHz,
            disable_others=True,
        )
        time.sleep(0.5)

        groups = nplay_session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.SMPGROUP, n_chans
        )
        assert all(g == int(SampleRate.SR_10kHz) for g in groups)

    def test_disable_others(self, nplay_session):
        # Enable 4 channels at 30kHz
        nplay_session.set_channel_sample_group(
            4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=False,
        )
        time.sleep(0.5)

        # Now set 2 at 1kHz with disable_others
        nplay_session.set_channel_sample_group(
            2, ChannelType.FRONTEND, SampleRate.SR_1kHz,
            disable_others=True,
        )
        time.sleep(0.5)

        # 30kHz group should be empty (others were disabled)
        channels_30k = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels_30k) == 0

        channels_1k = nplay_session.get_group_channels(int(SampleRate.SR_1kHz))
        assert len(channels_1k) >= 2


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
        nplay_session.set_channel_sample_group(
            4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        time.sleep(0.5)
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
        time.sleep(0.3)

    def test_set_dc_coupling(self, nplay_session):
        nplay_session.set_ac_input_coupling(
            4, ChannelType.FRONTEND, enabled=False,
        )
        time.sleep(0.3)


# ---------------------------------------------------------------------------
# Spike sorting
# ---------------------------------------------------------------------------


class TestSpikeSorting:
    """Tests for spike sorting configuration."""

    def test_set_spike_sorting(self, nplay_session):
        nplay_session.set_channel_spike_sorting(
            4, ChannelType.FRONTEND, sort_options=0,
        )
        time.sleep(0.3)


# ---------------------------------------------------------------------------
# Per-channel configuration (configure_channel)
# ---------------------------------------------------------------------------


class TestConfigureChannel:
    """Tests for the configure_channel convenience method."""

    def test_configure_smpfilter(self, nplay_session):
        orig = nplay_session.get_channel_smpfilter(1)
        nplay_session.configure_channel(1, smpfilter=2)
        time.sleep(0.3)
        assert nplay_session.get_channel_smpfilter(1) == 2
        nplay_session.configure_channel(1, smpfilter=orig)
        time.sleep(0.3)

    def test_configure_spkfilter(self, nplay_session):
        # Read original value so we can restore it afterward (other tests
        # depend on a valid spike filter being configured).
        orig = nplay_session.get_channel_spkfilter(1)
        nplay_session.configure_channel(1, spkfilter=3)
        time.sleep(0.3)
        assert nplay_session.get_channel_spkfilter(1) == 3
        # Restore
        nplay_session.configure_channel(1, spkfilter=orig)
        time.sleep(0.3)

    def test_configure_label(self, nplay_session):
        nplay_session.configure_channel(1, label="TestCh")
        time.sleep(0.3)
        assert nplay_session.get_channel_label(1) == "TestCh"

    def test_configure_multiple_attrs(self, nplay_session):
        orig_filter = nplay_session.get_channel_smpfilter(1)
        orig_label = nplay_session.get_channel_label(1)
        nplay_session.configure_channel(
            1,
            smpfilter=4,
            label="Multi",
        )
        time.sleep(0.3)
        assert nplay_session.get_channel_smpfilter(1) == 4
        assert nplay_session.get_channel_label(1) == "Multi"
        # Restore
        nplay_session.configure_channel(1, smpfilter=orig_filter,
                                        label=orig_label or "chan1")
        time.sleep(0.3)

    def test_configure_unknown_attr_raises(self, nplay_session):
        with pytest.raises(ValueError, match="Unknown channel attribute"):
            nplay_session.configure_channel(1, nonexistent_attr=42)



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

    def test_load_ccf(self, nplay_session, ccf_path):
        nplay_session.load_ccf(str(ccf_path))
        time.sleep(0.5)

    def test_save_load_roundtrip(self, nplay_session):
        nplay_session.set_channel_sample_group(
            4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
            disable_others=True,
        )
        time.sleep(0.5)

        with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
            ccf_file = f.name
        try:
            nplay_session.save_ccf(ccf_file)
            assert Path(ccf_file).stat().st_size > 0

            nplay_session.load_ccf(ccf_file)
            time.sleep(0.5)

            channels = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
            assert len(channels) >= 4
        finally:
            Path(ccf_file).unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# CMP files
# ---------------------------------------------------------------------------


class TestCMP:
    """Tests for channel mapping (CMP) file loading."""

    def test_load_channel_map(self, nplay_session, cmp_path):
        nplay_session.load_channel_map(str(cmp_path))
        time.sleep(0.5)

    def test_load_channel_map_with_bank_offset(self, nplay_session, cmp_path):
        nplay_session.load_channel_map(str(cmp_path), bank_offset=4)
        time.sleep(0.5)

    def test_positions_after_cmp_load(self, nplay_session, cmp_path):
        nplay_session.load_channel_map(str(cmp_path))
        time.sleep(0.5)
        positions = nplay_session.get_channels_positions(
            ChannelType.FRONTEND, n_chans=4
        )
        non_zero = [p for p in positions if any(v != 0 for v in p)]
        assert len(non_zero) > 0


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
        time.sleep(0.3)
        assert nplay_session.spike_length == 48
        assert nplay_session.spike_pretrigger == 12


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
