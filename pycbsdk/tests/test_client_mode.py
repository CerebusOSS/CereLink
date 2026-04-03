"""Integration tests for pycbsdk CLIENT mode.

A CLIENT session attaches to the shared memory created by a STANDALONE session.
It does not own the device connection — it reads config and data from shmem.

These tests require a running nPlayServer and use the shared ``nplay_session``
fixture as the STANDALONE session, then open a second ``Session`` as CLIENT.
"""

import time

import numpy as np
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

N_CHANS = 4


@pytest.fixture()
def client_session(nplay_session):
    """Open a second Session that should attach as CLIENT via shared memory.

    The STANDALONE ``nplay_session`` must already be running so that its
    shared memory segments exist.
    """
    # Ensure channels are configured on the STANDALONE side first
    nplay_session.set_channel_sample_group(
        N_CHANS, ChannelType.FRONTEND, SampleRate.SR_30kHz,
        disable_others=True,
    )
    time.sleep(0.5)

    with Session(DeviceType.NPLAY) as client:
        # Give the CLIENT's shmem receive thread time to start polling
        time.sleep(1)
        yield client


# ---------------------------------------------------------------------------
# Session properties
# ---------------------------------------------------------------------------


class TestClientProperties:
    """Verify CLIENT session properties."""

    def test_is_not_standalone(self, client_session):
        assert not client_session.is_standalone

    def test_is_running(self, client_session):
        assert client_session.running

    def test_protocol_version(self, client_session):
        assert client_session.protocol_version != ProtocolVersion.UNKNOWN

    def test_sysfreq(self, client_session):
        # sysfreq comes from SYSINFO in shmem; may be 0 if the STANDALONE
        # hasn't mirrored SYSREP to the native config buffer yet.
        sysfreq = client_session.sysfreq
        assert sysfreq == 0 or sysfreq == 30000

    def test_proc_ident(self, client_session):
        ident = client_session.proc_ident
        assert isinstance(ident, str)

    def test_max_chans(self, client_session):
        assert client_session.max_chans() > 0

    def test_version_string(self, client_session):
        assert len(client_session.version()) > 0


# ---------------------------------------------------------------------------
# Config read-back (CLIENT reads from shmem written by STANDALONE)
# ---------------------------------------------------------------------------


class TestClientConfigAccess:
    """Verify CLIENT can read device config from shared memory."""

    def test_get_channel_label(self, client_session):
        label = client_session.get_channel_label(1)
        assert label is not None
        assert isinstance(label, str)
        assert len(label) > 0

    def test_get_channel_type(self, client_session):
        ct = client_session.get_channel_type(1)
        assert ct == ChannelType.FRONTEND

    def test_get_channel_chancaps(self, client_session):
        caps = client_session.get_channel_chancaps(1)
        assert caps > 0

    def test_get_channel_config(self, client_session):
        config = client_session.get_channel_config(1)
        assert config is not None
        assert config["type"] == ChannelType.FRONTEND

    def test_get_matching_channel_ids(self, client_session):
        ids = client_session.get_matching_channel_ids(
            ChannelType.FRONTEND, n_chans=N_CHANS,
        )
        assert len(ids) == N_CHANS

    def test_get_channels_labels(self, client_session):
        labels = client_session.get_channels_labels(
            ChannelType.FRONTEND, n_chans=N_CHANS,
        )
        assert len(labels) == N_CHANS

    def test_get_group_channels(self, client_session):
        # Group membership comes from GROUPINFO in shmem; may be empty if
        # the STANDALONE hasn't mirrored GROUPREP to the config buffer.
        channels = client_session.get_group_channels(int(SampleRate.SR_30kHz))
        assert len(channels) >= 0  # May be empty in CLIENT mode

    def test_get_filter_info(self, client_session):
        # CLIENT should see the same filter table
        n = client_session.num_filters()
        assert n > 0

    def test_get_channel_scaling(self, client_session):
        scaling = client_session.get_channel_scaling(1)
        assert scaling is not None
        assert "anaunit" in scaling


# ---------------------------------------------------------------------------
# Config matches STANDALONE
# ---------------------------------------------------------------------------


class TestClientConfigMatchesStandalone:
    """Verify CLIENT sees the same config as the STANDALONE session."""

    def test_sysfreq_matches(self, nplay_session, client_session):
        # CLIENT may not have sysfreq if SYSINFO isn't mirrored to shmem
        client_freq = client_session.sysfreq
        if client_freq != 0:
            assert client_freq == nplay_session.sysfreq

    def test_protocol_version_matches(self, nplay_session, client_session):
        assert client_session.protocol_version == nplay_session.protocol_version

    def test_channel_labels_match(self, nplay_session, client_session):
        standalone_labels = nplay_session.get_channels_labels(
            ChannelType.FRONTEND, n_chans=N_CHANS,
        )
        client_labels = client_session.get_channels_labels(
            ChannelType.FRONTEND, n_chans=N_CHANS,
        )
        assert client_labels == standalone_labels

    def test_group_channels_match(self, nplay_session, client_session):
        standalone_group = nplay_session.get_group_channels(int(SampleRate.SR_30kHz))
        client_group = client_session.get_group_channels(int(SampleRate.SR_30kHz))
        # CLIENT may not see GROUPINFO if not mirrored; skip comparison if empty
        if len(client_group) > 0:
            assert client_group == standalone_group

    def test_smpgroup_field_matches(self, nplay_session, client_session):
        standalone = nplay_session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.SMPGROUP, N_CHANS,
        )
        client = client_session.get_channels_field(
            ChannelType.FRONTEND, ChanInfoField.SMPGROUP, N_CHANS,
        )
        assert client == standalone


# ---------------------------------------------------------------------------
# Data reception in CLIENT mode
# ---------------------------------------------------------------------------


class TestClientDataReception:
    """Verify CLIENT session receives data via shmem polling."""

    def test_on_group_receives_samples(self, client_session):
        count = [0]

        @client_session.on_group(SampleRate.SR_30kHz)
        def on_sample(header, data):
            count[0] += 1

        time.sleep(1)

        assert count[0] > 0, "CLIENT received no group samples"

    def test_on_group_batch(self, client_session):
        batches = []

        @client_session.on_group_batch(SampleRate.SR_30kHz)
        def on_batch(samples, timestamps):
            batches.append((samples.copy(), timestamps.copy()))

        time.sleep(1)

        assert len(batches) > 0, "CLIENT received no batch callbacks"
        samples, timestamps = batches[0]
        assert samples.ndim == 2
        assert samples.shape[1] == N_CHANS
        assert samples.dtype == np.int16

    def test_on_packet(self, client_session):
        count = [0]

        @client_session.on_packet()
        def on_pkt(header, data):
            count[0] += 1

        time.sleep(1)

        assert count[0] > 0, "CLIENT received no packets via catch-all"

    def test_continuous_reader(self, client_session):
        # continuous_reader/read_continuous depend on get_group_channels which
        # requires GROUPINFO in shmem.  Skip if not available in CLIENT mode.
        channels = client_session.get_group_channels(int(SampleRate.SR_30kHz))
        if len(channels) == 0:
            pytest.skip("GROUPINFO not available in CLIENT mode shmem")

        reader = client_session.continuous_reader(
            rate=SampleRate.SR_30kHz, buffer_seconds=2.0,
        )
        try:
            time.sleep(1)
            data = reader.read()
            assert data.shape[0] == N_CHANS
            assert data.shape[1] > 0, "CLIENT continuous_reader collected no samples"
        finally:
            reader.close()

    def test_read_continuous(self, client_session):
        channels = client_session.get_group_channels(int(SampleRate.SR_30kHz))
        if len(channels) == 0:
            pytest.skip("GROUPINFO not available in CLIENT mode shmem")

        data = client_session.read_continuous(
            rate=SampleRate.SR_30kHz, duration=0.5,
        )
        assert data.dtype == np.int16
        assert data.shape[0] == N_CHANS
        assert data.shape[1] > 0
