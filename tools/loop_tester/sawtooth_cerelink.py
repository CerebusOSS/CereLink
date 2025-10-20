import logging
import sys
import time

import numpy as np
from cerelink import cbpy

logging.basicConfig(level=logging.NOTSET)
logger = logging.getLogger(__name__)

def main(
    n_chans: int = 128,
):
    smpgroup = 6
    con_info = cbpy.open(parameter=cbpy.defaultConParams())

    for g in range(1, 7):
        # Disable all channels in all groups
        chan_infos = cbpy.get_sample_group(g)
        for ch_info in chan_infos:
            cbpy.set_channel_config(ch_info["chan"], chaninfo={"smpgroup": 0})

    for ch in range(1, n_chans):
        cbpy.set_channel_config(ch, chaninfo={"smpgroup": smpgroup, "ainpopts": 0})

    time.sleep(1.0)

    chan_infos = cbpy.get_sample_group(smpgroup)
    n_chans = len(chan_infos)
    n_buffer = 102400
    timestamps_buffer = np.zeros(n_buffer, dtype=np.uint64)
    samples_buffer = np.zeros((n_buffer, n_chans), dtype=np.int16)

    cbpy.trial_config(activate=True, n_continuous=-1)

    # Track last values from previous chunk for continuity checking
    last_timestamp = None
    last_ch0_sample = None
    last_ch1_sample = None

    try:
        while True:
            # Subsequent calls: reuse the allocated buffers
            data = cbpy.trial_continuous(
                seek=True,
                group=smpgroup,
                timestamps=timestamps_buffer,
                samples=samples_buffer
            )
            if data["num_samples"] > 0:
                ts = data["timestamps"]
                samps = data["samples"]
                n_samps = data["num_samples"]

                # Prepend last values from previous chunk to check cross-chunk continuity
                if last_timestamp is not None:
                    ts_with_prev = np.concatenate([[last_timestamp], ts[:n_samps]])
                    ch0_with_prev = np.concatenate([[last_ch0_sample], samps[:n_samps, 0]])
                    ch1_with_prev = np.concatenate([[last_ch1_sample], samps[:n_samps, 1]])
                else:
                    # First chunk - no previous data
                    ts_with_prev = ts[:n_samps]
                    ch0_with_prev = samps[:n_samps, 0]
                    ch1_with_prev = samps[:n_samps, 1]

                # Check timestamps
                td_good = np.diff(ts_with_prev) == 1
                if not np.all(td_good):
                    bad_idx = np.where(~td_good)[0]
                    for idx in bad_idx:
                        logger.warning(f"Non-consecutive timestamps at index {idx}: {ts_with_prev[idx]} -> {ts_with_prev[idx+1]}")

                # Check channel 0: incrementing sawtooth (wraps from 32767 to -32768)
                ch0_diff = np.diff(ch0_with_prev)
                ch0_expected = np.ones_like(ch0_diff)
                # Data are int16 so the diff result wraps itself around
                if not np.array_equal(ch0_diff, ch0_expected):
                    bad_idx = np.where(ch0_diff != ch0_expected)[0]
                    for idx in bad_idx[:5]:  # Log first 5 issues
                        logger.warning(f"Channel 0 non-consecutive at index {idx}: {ch0_with_prev[idx]} -> {ch0_with_prev[idx+1]} (diff={ch0_diff[idx]})")

                # Check channel 1: decrementing sawtooth (wraps from -32768 to 32767)
                ch1_diff = np.diff(ch1_with_prev)
                ch1_expected = -np.ones_like(ch1_diff)
                if not np.array_equal(ch1_diff, ch1_expected):
                    bad_idx = np.where(ch1_diff != ch1_expected)[0]
                    for idx in bad_idx[:5]:  # Log first 5 issues
                        logger.warning(f"Channel 1 non-consecutive at index {idx}: {ch1_with_prev[idx]} -> {ch1_with_prev[idx+1]} (diff={ch1_diff[idx]})")

                # Check constant channels (2+)
                expected_const_dat = 100 * np.ones((n_samps, 1), dtype=np.int16) * np.arange(3, n_chans + 1)[None, :]
                if not np.array_equal(samps[:n_samps, 2:], expected_const_dat):
                    logger.warning(f"Data mismatch detected in samples[:, 2:]")

                # Save last values for next iteration
                last_timestamp = ts[n_samps - 1]
                last_ch0_sample = samps[n_samps - 1, 0]
                last_ch1_sample = samps[n_samps - 1, 1]
    except KeyboardInterrupt:
        cbpy.close()


if __name__ == "__main__":
    b_try_no_args = False
    try:
        import typer

        typer.run(main)
    except ModuleNotFoundError:
        print("Please install typer to use CLI args; using defaults.")
        b_try_no_args = True
    if b_try_no_args:
        main()
