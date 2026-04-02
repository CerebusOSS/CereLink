"""Integration tests for pycbsdk device configuration.

These tests exercise channel configuration, CCF/CMP loading, and session
properties against a running nPlayServer instance.
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
# Session lifecycle
# ---------------------------------------------------------------------------


class TestSessionLifecycle:
    """Basic session connection and property tests."""

    def test_session_connects_and_receives(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            assert session.running
            time.sleep(1)
            stats = session.stats
            assert stats.packets_received > 0

    def test_session_is_standalone(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            assert session.is_standalone

    def test_protocol_version(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # nPlayServer should report a known protocol version
            assert session.protocol_version != ProtocolVersion.UNKNOWN

    def test_sysfreq(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            assert session.sysfreq == 30000

    def test_version_string(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            version = session.version()
            assert isinstance(version, str)
            assert len(version) > 0

    def test_max_chans(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            assert session.max_chans() > 0
            assert session.num_fe_chans() > 0
            assert session.num_analog_chans() >= session.num_fe_chans()

    def test_proc_ident(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            ident = session.proc_ident
            assert isinstance(ident, str)


# ---------------------------------------------------------------------------
# Channel sample group configuration
# ---------------------------------------------------------------------------


class TestChannelSampleGroup:
    """Tests for set_channel_sample_group and related queries."""

    def test_set_frontend_30k(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            n_chans = 4
            session.set_channel_sample_group(
                n_chans, ChannelType.FRONTEND, SampleRate.SR_30kHz,
                disable_others=True,
            )
            # Allow config packets to propagate
            time.sleep(0.5)
            channels = session.get_group_channels(int(SampleRate.SR_30kHz))
            assert len(channels) >= n_chans

    def test_set_and_verify_smpgroup_field(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            n_chans = 4
            session.set_channel_sample_group(
                n_chans, ChannelType.FRONTEND, SampleRate.SR_10kHz,
                disable_others=True,
            )
            time.sleep(0.5)

            # Verify via bulk field query
            groups = session.get_channels_field(
                ChannelType.FRONTEND, ChanInfoField.SMPGROUP, n_chans
            )
            assert all(g == int(SampleRate.SR_10kHz) for g in groups)

    def test_disable_others(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # First enable 4 channels at 30kHz
            session.set_channel_sample_group(
                4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
                disable_others=False,
            )
            time.sleep(0.5)

            # Now set 2 at 1kHz with disable_others
            session.set_channel_sample_group(
                2, ChannelType.FRONTEND, SampleRate.SR_1kHz,
                disable_others=True,
            )
            time.sleep(0.5)

            # 30kHz group should be empty (others were disabled)
            channels_30k = session.get_group_channels(int(SampleRate.SR_30kHz))
            assert len(channels_30k) == 0

            channels_1k = session.get_group_channels(int(SampleRate.SR_1kHz))
            assert len(channels_1k) >= 2


# ---------------------------------------------------------------------------
# Channel info accessors
# ---------------------------------------------------------------------------


class TestChannelInfo:
    """Tests for individual channel information retrieval."""

    def test_get_channel_label(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            label = session.get_channel_label(1)
            assert label is not None
            assert isinstance(label, str)
            assert len(label) > 0

    def test_get_channel_type(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            ct = session.get_channel_type(1)
            assert ct == ChannelType.FRONTEND

    def test_get_channel_chancaps(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            caps = session.get_channel_chancaps(1)
            assert caps > 0  # Front-end channels have capabilities

    def test_get_channel_scaling(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            scaling = session.get_channel_scaling(1)
            assert scaling is not None
            assert "digmin" in scaling
            assert "digmax" in scaling
            assert "anamin" in scaling
            assert "anamax" in scaling
            assert "anagain" in scaling
            assert "anaunit" in scaling
            assert isinstance(scaling["anaunit"], str)

    def test_get_channel_config(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            config = session.get_channel_config(1)
            assert config is not None
            assert config["type"] == ChannelType.FRONTEND
            assert "label" in config
            assert "smpgroup" in config
            assert "chancaps" in config
            assert "smpfilter" in config
            assert "spkfilter" in config

    def test_get_channel_config_invalid(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Channel 0 is invalid (1-based)
            config = session.get_channel_config(0)
            assert config is None


# ---------------------------------------------------------------------------
# Bulk channel queries
# ---------------------------------------------------------------------------


class TestBulkChannelQueries:
    """Tests for get_matching_channel_ids and related bulk queries."""

    def test_get_matching_channel_ids(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            ids = session.get_matching_channel_ids(ChannelType.FRONTEND)
            assert len(ids) > 0
            # IDs should be 1-based
            assert all(i >= 1 for i in ids)

    def test_get_matching_channel_ids_limited(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            ids = session.get_matching_channel_ids(ChannelType.FRONTEND, n_chans=4)
            assert len(ids) == 4

    def test_get_channels_labels(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            labels = session.get_channels_labels(ChannelType.FRONTEND, n_chans=4)
            assert len(labels) == 4
            assert all(isinstance(l, str) and len(l) > 0 for l in labels)

    def test_get_channels_field(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            caps = session.get_channels_field(
                ChannelType.FRONTEND, ChanInfoField.CHANCAPS, n_chans=4
            )
            assert len(caps) == 4
            assert all(c > 0 for c in caps)

    def test_get_channels_positions(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            positions = session.get_channels_positions(
                ChannelType.FRONTEND, n_chans=4
            )
            assert len(positions) == 4
            # Each position is a 4-tuple
            assert all(len(p) == 4 for p in positions)

    def test_get_group_channels(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.set_channel_sample_group(
                4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
                disable_others=True,
            )
            time.sleep(0.5)
            channels = session.get_group_channels(int(SampleRate.SR_30kHz))
            assert len(channels) >= 4

    def test_get_group_label(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            label = session.get_group_label(int(SampleRate.SR_30kHz))
            assert label is not None
            assert isinstance(label, str)


# ---------------------------------------------------------------------------
# AC input coupling
# ---------------------------------------------------------------------------


class TestACInputCoupling:
    """Tests for set_ac_input_coupling."""

    def test_set_ac_coupling(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Should not raise
            session.set_ac_input_coupling(
                4, ChannelType.FRONTEND, enabled=True,
            )
            time.sleep(0.3)

    def test_set_dc_coupling(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.set_ac_input_coupling(
                4, ChannelType.FRONTEND, enabled=False,
            )
            time.sleep(0.3)


# ---------------------------------------------------------------------------
# Spike sorting
# ---------------------------------------------------------------------------


class TestSpikeSorting:
    """Tests for spike sorting configuration."""

    def test_set_spike_sorting(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Should not raise
            session.set_channel_spike_sorting(
                4, ChannelType.FRONTEND, sort_options=0,
            )
            time.sleep(0.3)


# ---------------------------------------------------------------------------
# Per-channel configuration (configure_channel)
# ---------------------------------------------------------------------------


class TestConfigureChannel:
    """Tests for the configure_channel convenience method."""

    def test_configure_smpfilter(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.configure_channel(1, smpfilter=2)
            time.sleep(0.3)
            assert session.get_channel_smpfilter(1) == 2

    def test_configure_spkfilter(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.configure_channel(1, spkfilter=3)
            time.sleep(0.3)
            assert session.get_channel_spkfilter(1) == 3

    def test_configure_label(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.configure_channel(1, label="TestCh")
            time.sleep(0.3)
            assert session.get_channel_label(1) == "TestCh"

    def test_configure_multiple_attrs(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.configure_channel(
                1,
                smpfilter=4,
                label="Multi",
            )
            time.sleep(0.3)
            assert session.get_channel_smpfilter(1) == 4
            assert session.get_channel_label(1) == "Multi"

    def test_configure_unknown_attr_raises(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            with pytest.raises(ValueError, match="Unknown channel attribute"):
                session.configure_channel(1, nonexistent_attr=42)


# ---------------------------------------------------------------------------
# Filter info
# ---------------------------------------------------------------------------


class TestFilterInfo:
    """Tests for filter information retrieval."""

    def test_num_filters(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            n = session.num_filters()
            assert n > 0

    def test_get_filter_info(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            info = session.get_filter_info(0)
            # Filter 0 may or may not have a label, but should not crash
            if info is not None:
                assert "label" in info
                assert "hpfreq" in info
                assert "lpfreq" in info

    def test_get_filter_info_invalid(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            info = session.get_filter_info(9999)
            assert info is None


# ---------------------------------------------------------------------------
# CCF files
# ---------------------------------------------------------------------------


class TestCCF:
    """Tests for CCF save/load."""

    def test_save_ccf(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
                ccf_out = f.name
            try:
                session.save_ccf(ccf_out)
                assert Path(ccf_out).stat().st_size > 0
            finally:
                Path(ccf_out).unlink(missing_ok=True)

    def test_load_ccf(self, nplayserver, ccf_path):
        with Session(DeviceType.NPLAY) as session:
            session.load_ccf(str(ccf_path))
            time.sleep(0.5)

    def test_save_load_roundtrip(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Configure some channels first
            session.set_channel_sample_group(
                4, ChannelType.FRONTEND, SampleRate.SR_30kHz,
                disable_others=True,
            )
            time.sleep(0.5)

            with tempfile.NamedTemporaryFile(suffix=".ccf", delete=False) as f:
                ccf_file = f.name
            try:
                session.save_ccf(ccf_file)
                assert Path(ccf_file).stat().st_size > 0

                # Load it back
                session.load_ccf(ccf_file)
                time.sleep(0.5)

                # Verify channels are still configured
                channels = session.get_group_channels(int(SampleRate.SR_30kHz))
                assert len(channels) >= 4
            finally:
                Path(ccf_file).unlink(missing_ok=True)


# ---------------------------------------------------------------------------
# CMP files
# ---------------------------------------------------------------------------


class TestCMP:
    """Tests for channel mapping (CMP) file loading."""

    def test_load_channel_map(self, nplayserver, cmp_path):
        with Session(DeviceType.NPLAY) as session:
            session.load_channel_map(str(cmp_path))
            time.sleep(0.5)

    def test_load_channel_map_with_bank_offset(self, nplayserver, cmp_path):
        with Session(DeviceType.NPLAY) as session:
            session.load_channel_map(str(cmp_path), bank_offset=4)
            time.sleep(0.5)

    def test_positions_after_cmp_load(self, nplayserver, cmp_path):
        with Session(DeviceType.NPLAY) as session:
            session.load_channel_map(str(cmp_path))
            time.sleep(0.5)
            positions = session.get_channels_positions(
                ChannelType.FRONTEND, n_chans=4
            )
            # After loading a CMP, some positions should be non-zero
            non_zero = [p for p in positions if any(v != 0 for v in p)]
            assert len(non_zero) > 0


# ---------------------------------------------------------------------------
# Spike length configuration
# ---------------------------------------------------------------------------


class TestSpikeLength:
    """Tests for spike length configuration."""

    def test_get_spike_length(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            length = session.spike_length
            assert length > 0

    def test_get_spike_pretrigger(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            pre = session.spike_pretrigger
            assert pre >= 0

    def test_set_spike_length(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.set_spike_length(48, 12)
            time.sleep(0.3)
            assert session.spike_length == 48
            assert session.spike_pretrigger == 12


# ---------------------------------------------------------------------------
# Full config snapshot
# ---------------------------------------------------------------------------


class TestGetConfig:
    """Tests for the get_config bulk accessor."""

    def test_get_config(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            config = session.get_config()
            assert "sysfreq" in config
            assert config["sysfreq"] == 30000
            assert "channels" in config
            assert "groups" in config
            assert "filters" in config
            assert len(config["channels"]) > 0


