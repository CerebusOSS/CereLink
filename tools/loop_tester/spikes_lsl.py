import numpy as np
import pylsl
import time


wf_orig = [
    [
        0, 34, 68, 96, 123, 191, 258, 291, 325, 291, 258,
        112, -33, -397, -760, -777, -794, -688, -581, -380, -179, 6,
        191, 275, 358, 370, 381, 330, 280, 230, 179, 118, 56,
        28, 0, -11, -22, -28, -33, -16, 0
    ],
    [
        0, -16, -33, -67, -100, -296, -492, -447, -402, 34, 470,
        549, 627, 470, 314, 90, -134, -167, -201, -190, -179, -162,
        -145, -100, -56, -22, 12, 40, 68, 73, 79, 45, 12,
        -5, -22, -22, -22, -16, -11, -5, 0
    ],
    [
        0, -22, -44, -100, -156, -380, -604, -604, -604, -397, -190,
        34, 258, 426, 593, 700, 806, 677, 549, 358, 168, 90,
        12, -22, -56, -67, -78, -84, -89, -89, -89, -89, -89,
        -84, -78, -61, -44, -39, -33, -16, 0
    ]
]
wf_orig = np.array(wf_orig, dtype=np.int16)


def main(num_channels: int = 128, chunk_size: int = 100):
    """
    The pattern during single spikes:

    ```
     ** Ch 1 1-----------2-----------3-----------1-----------2- ... and
     ** Ch 2 ---2-----------3-----------1-----------2---------- ... on
     ** Ch 3 ------3-----------1-----------2-----------3------- ... and
     ** Ch 4 ---------1-----------2-----------3-----------1---- ... on
     ** Ch 5 1-----------2-----------3-----------1-----------2- ... and
    ```
    Waveforms are inserted 7500 samples (0.25 seconds). At each iteration,
    the next waveform (% 3 waveforms) is inserted into the next channel (% 4 channels).
    This repeats until there are 36 spike + gap periods (36 * 7500), for 9 seconds total.

    During bursting:

    ```
     ** Ch 1 1---2---3---1---2---3---1---2---3---1---2---3---1- ... and
     ** Ch 2 1---2---3---1---2---3---1---2---3---1---2---3---1- ... on
     ** Ch 3 1---2---3---1---2---3---1---2---3---1---2---3---1- ... and
     ** Ch 4 1---2---3---1---2---3---1---2---3---1---2---3---1- ... on
     ** Ch 5 1---2---3---1---2---3---1---2---3---1---2---3---1- ... and
    ```

    Bursts begin immediately after the last gap following the 36th spike during the slow period.
    During bursting, a spike occurs in all channels simultaneously with the same waveform.
    Each spike in a burst comprises a spike waveform + gap for 300 total samples (0.01 s).
    Waveforms cycle in order, 1, 2, 3, 1, 2, 3, and so on.
    There are 99 total spikes in a burst for 0.99 second total.

    A burst ends with a 300 sample gap (so 600 from the onset of the last spike in the burst) before the slow period begins again.

    All above spikes are additive on a common background pattern.
    The background pattern is a sum of 3 sinusoids at frequencies of 1.0, 3.0, and 9.0 Hz.

    :param num_channels:
    :param chunk_size:
    :return:
    """
    # Constants
    SAMPLE_RATE = 30_000
    SINGLE_SPIKE_DURATION = 7500  # 0.25 seconds per spike + gap
    NUM_SINGLE_SPIKES = 36
    BURST_SPIKE_DURATION = 300  # 0.01 seconds per spike + gap
    NUM_BURST_SPIKES = 99
    BURST_END_GAP = 300  # Extra gap after burst
    SINE_AMPLITUDE = 894.4

    # Total samples for each period
    single_spike_period_samples = NUM_SINGLE_SPIKES * SINGLE_SPIKE_DURATION  # 270,000 samples (9 seconds)
    burst_period_samples = NUM_BURST_SPIKES * BURST_SPIKE_DURATION + BURST_END_GAP  # 30,000 samples (1 second)
    total_samples = single_spike_period_samples + burst_period_samples  # 300,000 samples (10 seconds)

    print(f"Generating {total_samples / SAMPLE_RATE:.1f} second signal ({total_samples:,} samples) for {num_channels} channels...")

    # Create background: sum of 3 sinusoids at 1.0, 3.0, and 9.0 Hz
    t = np.arange(total_samples) / SAMPLE_RATE
    background = (
            SINE_AMPLITUDE * np.sin(2 * np.pi * 1.0 * t) +
            SINE_AMPLITUDE * np.sin(2 * np.pi * 3.0 * t) +
            SINE_AMPLITUDE * np.sin(2 * np.pi * 9.0 * t)
    ).astype(np.int16)

    # Initialize signal array [samples, channels] via broadcast addition
    signal = background[:, None] + np.zeros((1, num_channels), dtype=np.int16)

    # Generate single spike period (first 9 seconds)
    print("Adding single spikes...")
    for spike_idx in range(NUM_SINGLE_SPIKES):
        # Determine which channel gets this spike (cycles through 4 channels)
        channel = spike_idx % 4

        # Determine which waveform to use (cycles through 3 waveforms)
        waveform_idx = spike_idx % 3
        waveform = wf_orig[waveform_idx]

        # Insert waveform at the start of this spike period
        start_sample = spike_idx * SINGLE_SPIKE_DURATION
        signal[start_sample:start_sample + len(waveform), channel] += waveform

    # Generate burst period (last 1 second)
    print("Adding burst spikes...")
    burst_start = single_spike_period_samples
    for burst_spike_idx in range(NUM_BURST_SPIKES):
        # Determine which waveform to use (cycles through 3 waveforms)
        waveform_idx = burst_spike_idx % 3
        waveform = wf_orig[waveform_idx]

        # Insert waveform into ALL channels simultaneously
        start_sample = burst_start + burst_spike_idx * BURST_SPIKE_DURATION
        signal[start_sample:start_sample + len(waveform), :] += waveform[:, None]

    print(f"Signal prepared. Shape: {signal.shape}")

    outlet = pylsl.StreamOutlet(
        pylsl.StreamInfo(
            name="SpikeTest",
            type="Simulation",
            channel_count=num_channels,
            nominal_srate=SAMPLE_RATE,
            channel_format=pylsl.cf_int16,
            source_id="spikes1234"
        )
    )

    print(f"Now sending data in chunks of {chunk_size} samples...")
    print("Press Ctrl+C to stop")

    # Stream the signal in chunks, maintaining real-time pacing
    sample_idx = 0
    start_time = pylsl.local_clock()

    try:
        while True:
            # Loop through the 10-second cycle continuously
            chunk_start = sample_idx % total_samples
            chunk_end = min(chunk_start + chunk_size, total_samples)

            # Extract chunk
            chunk = signal[chunk_start:chunk_end, :]

            # If we wrapped around, handle the wrap
            if (chunk_start + chunk_size) > total_samples and sample_idx > 0:
                # Need to wrap to beginning
                remainder = (chunk_start + chunk_size) - total_samples
                wrap_chunk = signal[:, :remainder]
                chunk = np.vstack([chunk, wrap_chunk])

            # Send chunk to LSL outlet
            outlet.push_chunk(chunk)

            # Update sample counter
            sample_idx += chunk_size

            # Calculate how long to sleep to maintain real-time pacing
            expected_time = start_time + (sample_idx / SAMPLE_RATE)
            current_time = pylsl.local_clock()
            sleep_time = expected_time - current_time

            if sleep_time > 0:
                time.sleep(sleep_time)
            elif sleep_time < -0.1:
                # Warn if we're falling behind by more than 100ms
                print(f"Warning: Running {-sleep_time:.3f}s behind real-time")

    except KeyboardInterrupt:
        print("\nStopped by user")


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
