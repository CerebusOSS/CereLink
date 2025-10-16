# Make sure the CereLink project folder is not on the path,
# otherwise it will attempt to import cerebus from there, instead of the installed python package.
from cerebus import cbpy


group_idx = 5
res, con_info = cbpy.open(parameter=cbpy.defaultConParams())
res, group_info = cbpy.get_sample_group(group_idx)

res, reset = cbpy.trial_config(
    reset=True,
    buffer_parameter={},
    range_parameter={},
    noevent=1,
    nocontinuous=0,
    nocomment=1
)

# First call: let the function allocate buffers
result, data = cbpy.trial_continuous(reset=True, group=group_idx)
print(f"Initial fetch: {data['num_samples']} samples from {data['count']} channels")
print(f"Trial start time: {data.get('trial_start_time', 'N/A')}")

# Extract the buffers for reuse
timestamps_buffer = data['timestamps']
samples_buffer = data['samples']

try:
    while True:
        # Subsequent calls: reuse the allocated buffers
        result, data = cbpy.trial_continuous(
            reset=True,
            group=group_idx,
            timestamps=timestamps_buffer,
            samples=samples_buffer
        )
        if data['num_samples'] > 0:
            print(f"Fetched {data['num_samples']} samples, latest timestamp: {timestamps_buffer[data['num_samples']-1]}")
except KeyboardInterrupt:
    cbpy.close()
