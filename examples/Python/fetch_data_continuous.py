# Make sure the CereLink/bindings/Python folder is not on the path,
# otherwise it will attempt to import cerelink from there, instead of the installed python package.
import time

import numpy as np
from cerelink import cbpy


def main():
    group_idx = 6
    con_info = cbpy.open(parameter=cbpy.defaultConParams())

    for g in range(1, 7):
        # Disable all channels in all groups
        chan_infos = cbpy.get_sample_group(g)
        for ch_info in chan_infos:
            cbpy.set_channel_config(ch_info["chan"], chaninfo={"smpgroup": 0})

    for ch in range(1, 9):
        # Enable first 8 channels in group 6, no dc offset
        cbpy.set_channel_config(ch, chaninfo={"smpgroup": group_idx, "ainpopts": 0})

    time.sleep(1.0)  # Wait for settings to take effect

    chan_infos = cbpy.get_sample_group(group_idx)
    n_chans = len(chan_infos)
    n_buffer = 102400
    timestamps_buffer = np.zeros(n_buffer, dtype=np.uint64)
    samples_buffer = np.zeros((n_buffer, n_chans), dtype=np.int16)

    cbpy.trial_config(activate=True, n_continuous=-1)
    try:
        while True:
            # Subsequent calls: reuse the allocated buffers
            data = cbpy.trial_continuous(
                reset_clock=False,
                seek=True,
                group=group_idx,
                timestamps=timestamps_buffer,
                samples=samples_buffer,
                num_samples=n_buffer,
            )
            if data["num_samples"] > 0:
                print(f"Fetched {data['num_samples']} samples, latest timestamp: {data['timestamps'][-1]}")
    except KeyboardInterrupt:
        cbpy.close()


if __name__ == "__main__":
    main()
