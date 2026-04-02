"""Integration tests for pycbsdk data reception.

These tests exercise the callback-based data path: on_group, on_group_batch,
on_event, on_packet, continuous_reader, and read_continuous, all against a
running nPlayServer instance.

To avoid shmem/port exhaustion from creating too many sessions, tests use
the shared ``nplay_session`` fixture defined in conftest.py.
"""

import time

import numpy as np
import pytest

from pycbsdk import (
    ChannelType,
    DeviceType,
    SampleRate,
    Session,
)

pytestmark = pytest.mark.integration

N_CHANS = 4


def _ensure_30k(session):
    """Make sure 4 channels are configured at 30kHz."""
    session.set_channel_sample_group(
        N_CHANS, ChannelType.FRONTEND, SampleRate.SR_30kHz,
        disable_others=True,
    )
    time.sleep(0.5)


# ---------------------------------------------------------------------------
# on_group callback
# ---------------------------------------------------------------------------


class TestOnGroup:
    """Tests for the on_group (per-sample) callback."""

    def test_receives_samples(self, nplay_session):
        _ensure_30k(nplay_session)
        received = []

        @nplay_session.on_group(SampleRate.SR_30kHz)
        def on_sample(header, data):
            received.append(header.time)

        time.sleep(1)

        assert len(received) > 0, "No group samples received"
        # At 30kHz we expect ~30000 samples/s; with overhead, at least a few thousand
        assert len(received) > 1000

    def test_receives_samples_as_array(self, nplay_session):
        _ensure_30k(nplay_session)
        samples = []

        @nplay_session.on_group(SampleRate.SR_30kHz, as_array=True)
        def on_sample(header, arr):
            samples.append(arr.copy())

        time.sleep(0.5)

        assert len(samples) > 0, "No group samples received"
        # Each sample should be an int16 array with N_CHANS elements
        assert samples[0].dtype == np.int16
        assert samples[0].shape == (N_CHANS,)


# ---------------------------------------------------------------------------
# on_group at a different rate (needs its own session for reconfiguration)
# ---------------------------------------------------------------------------


class TestOnGroupDifferentRate:
    """Test group callback at 1kHz."""

    def test_different_rate(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            session.set_channel_sample_group(
                N_CHANS, ChannelType.FRONTEND, SampleRate.SR_1kHz,
                disable_others=True,
            )
            time.sleep(0.5)

            count = [0]

            @session.on_group(SampleRate.SR_1kHz)
            def on_sample(header, data):
                count[0] += 1

            time.sleep(1)

            # At 1kHz, expect ~1000 samples/s
            assert count[0] > 500
            assert count[0] < 2000  # Sanity upper bound


# ---------------------------------------------------------------------------
# on_group_batch callback
# ---------------------------------------------------------------------------


class TestOnGroupBatch:
    """Tests for the on_group_batch (batched) callback."""

    def test_receives_batches(self, nplay_session):
        _ensure_30k(nplay_session)
        batches = []

        @nplay_session.on_group_batch(SampleRate.SR_30kHz)
        def on_batch(samples, timestamps):
            batches.append((samples.copy(), timestamps.copy()))

        time.sleep(1)

        assert len(batches) > 0, "No batch callbacks received"

        # Every batch should have the same number of channels
        for i, (samples, timestamps) in enumerate(batches):
            assert samples.ndim == 2
            assert samples.shape[1] == N_CHANS, (
                f"Batch {i}: expected {N_CHANS} channels, got {samples.shape[1]}"
            )
            assert samples.dtype == np.int16
            assert timestamps.ndim == 1
            assert timestamps.dtype == np.uint64
            assert samples.shape[0] == timestamps.shape[0], (
                f"Batch {i}: n_samples mismatch between samples and timestamps"
            )

    def test_timestamps_increase(self, nplay_session):
        _ensure_30k(nplay_session)
        all_timestamps = []

        @nplay_session.on_group_batch(SampleRate.SR_30kHz)
        def on_batch(samples, timestamps):
            all_timestamps.extend(timestamps.tolist())

        time.sleep(0.5)

        assert len(all_timestamps) > 100
        # Timestamps should be monotonically increasing
        for i in range(1, min(len(all_timestamps), 1000)):
            assert all_timestamps[i] > all_timestamps[i - 1], (
                f"Timestamp not increasing at index {i}: "
                f"{all_timestamps[i - 1]} -> {all_timestamps[i]}"
            )

    def test_batch_size(self, nplay_session):
        _ensure_30k(nplay_session)
        batch_sizes = []

        @nplay_session.on_group_batch(SampleRate.SR_30kHz)
        def on_batch(samples, timestamps):
            batch_sizes.append(len(timestamps))

        time.sleep(0.5)

        assert len(batch_sizes) > 0
        # Batches should typically contain multiple samples (~30 from one UDP datagram)
        avg_batch = sum(batch_sizes) / len(batch_sizes)
        assert avg_batch > 1, f"Average batch size {avg_batch} — expected batching"


# ---------------------------------------------------------------------------
# on_event callback
# ---------------------------------------------------------------------------


class TestOnEvent:
    """Tests for the on_event (spike/digital event) callback.

    The test data file contains spikes, but spike extraction must be enabled
    on the channels for the device to emit spike event packets.
    """

    # cbAINPSPK_EXTRACT | cbAINPSPK_THRLEVEL — enable spike extraction
    # with analog level threshold detection
    _SPKOPTS = 0x00000001 | 0x00000100

    def test_receives_spike_events(self, nplay_session):
        _ensure_30k(nplay_session)
        nplay_session.set_channel_spike_sorting(
            N_CHANS, ChannelType.FRONTEND, sort_options=self._SPKOPTS,
        )
        time.sleep(0.5)

        events = []

        @nplay_session.on_event(ChannelType.FRONTEND)
        def on_spike(header, data):
            events.append(header.chid)

        time.sleep(2)

        assert len(events) > 0, "No spike events received (spiking enabled)"

    def test_register_all_events(self, nplay_session):
        events = []

        @nplay_session.on_event(None)  # All event channels
        def on_any_event(header, data):
            events.append(header.chid)

        time.sleep(1)


# ---------------------------------------------------------------------------
# on_packet callback (catch-all)
# ---------------------------------------------------------------------------


class TestOnPacket:
    """Tests for the on_packet catch-all callback."""

    def test_receives_all_packets(self, nplay_session):
        count = [0]

        @nplay_session.on_packet()
        def on_pkt(header, data):
            count[0] += 1

        time.sleep(1)

        assert count[0] > 0, "No packets received via catch-all callback"


# ---------------------------------------------------------------------------
# continuous_reader
# ---------------------------------------------------------------------------


class TestContinuousReader:
    """Tests for the ContinuousReader ring buffer."""

    def test_basic_read(self, nplay_session):
        _ensure_30k(nplay_session)
        reader = nplay_session.continuous_reader(
            rate=SampleRate.SR_30kHz, buffer_seconds=5.0,
        )
        try:
            time.sleep(1)

            data = reader.read()
            assert data.dtype == np.int16
            assert data.shape[0] == N_CHANS
            assert data.shape[1] > 0, "No samples accumulated"
        finally:
            reader.close()

    def test_available_and_total(self, nplay_session):
        _ensure_30k(nplay_session)
        reader = nplay_session.continuous_reader(
            rate=SampleRate.SR_30kHz, buffer_seconds=2.0,
        )
        try:
            time.sleep(0.5)

            assert reader.available > 0
            assert reader.total_samples > 0
            assert reader.total_samples >= reader.available
        finally:
            reader.close()

    def test_read_n_samples(self, nplay_session):
        _ensure_30k(nplay_session)
        reader = nplay_session.continuous_reader(
            rate=SampleRate.SR_30kHz, buffer_seconds=5.0,
        )
        try:
            time.sleep(1)

            n = 100
            data = reader.read(n_samples=n)
            assert data.shape == (N_CHANS, n)
        finally:
            reader.close()

    def test_no_channels_raises(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Disable all channels first
            session.set_channel_sample_group(
                0, ChannelType.FRONTEND, SampleRate.NONE,
                disable_others=True,
            )
            time.sleep(0.3)

            with pytest.raises(ValueError, match="No channels configured"):
                session.continuous_reader(rate=SampleRate.SR_2kHz)


# ---------------------------------------------------------------------------
# read_continuous (blocking)
# ---------------------------------------------------------------------------


class TestReadContinuous:
    """Tests for the read_continuous blocking helper."""

    def test_collects_data(self, nplay_session):
        _ensure_30k(nplay_session)
        data = nplay_session.read_continuous(
            rate=SampleRate.SR_30kHz, duration=0.5,
        )

        assert data.dtype == np.int16
        assert data.shape[0] == N_CHANS
        # At 30kHz for 0.5s, expect ~15000 samples
        assert data.shape[1] > 5000

    def test_short_duration(self, nplay_session):
        _ensure_30k(nplay_session)
        data = nplay_session.read_continuous(
            rate=SampleRate.SR_30kHz, duration=0.1,
        )

        assert data.shape[0] == N_CHANS
        assert data.shape[1] > 0


# ---------------------------------------------------------------------------
# Multi-rate configuration (needs its own session)
# ---------------------------------------------------------------------------


class TestMultiRate:
    """Tests for receiving data at multiple sample rates simultaneously."""

    def test_two_rates(self, nplayserver):
        with Session(DeviceType.NPLAY) as session:
            # Configure first 2 channels at 30kHz (disable everything else)
            session.set_channel_sample_group(
                2, ChannelType.FRONTEND, SampleRate.SR_30kHz,
                disable_others=True,
            )
            time.sleep(0.3)

            # Individually assign channels 3-4 to 1kHz via configure_channel.
            fe_ids = session.get_matching_channel_ids(ChannelType.FRONTEND)
            for chan_id in fe_ids[2:4]:
                session.configure_channel(chan_id, smpgroup=SampleRate.SR_1kHz)
            time.sleep(0.5)

            count_30k = [0]
            count_1k = [0]

            @session.on_group(SampleRate.SR_30kHz)
            def on_30k(header, data):
                count_30k[0] += 1

            @session.on_group(SampleRate.SR_1kHz)
            def on_1k(header, data):
                count_1k[0] += 1

            time.sleep(1)

            assert count_30k[0] > 0, "No 30kHz samples received"
            assert count_1k[0] > 0, "No 1kHz samples received"
            # 30kHz should produce ~30x more samples than 1kHz
            ratio = count_30k[0] / max(count_1k[0], 1)
            assert ratio > 10, f"Rate ratio {ratio:.1f} — expected ~30x"


# ---------------------------------------------------------------------------
# on_error callback
# ---------------------------------------------------------------------------


class TestOnError:
    """Tests for the error callback registration."""

    def test_register_error_callback(self, nplay_session):
        errors = []
        nplay_session.on_error(lambda msg: errors.append(msg))

        time.sleep(0.5)
        assert len(errors) == 0
