# Make sure the CereLink project folder is not on the path,
# otherwise it will attempt to import cerebus from there, instead of the installed python package.
from cerebus import cbpy


res, con_info = cbpy.open(parameter=cbpy.defaultConParams())
res, reset = cbpy.trial_config(
    reset=True,
    buffer_parameter={},
    range_parameter={},
    noevent=0,
    nocontinuous=1,
    nocomment=1
)

try:
    spk_cache = {}
    while True:
        result, data = cbpy.trial_event(reset=True)
        if len(data) > 0:
            for ev in data:
                chid = ev[0]
                ev_dict = ev[1]
                timestamps = ev_dict["timestamps"]
                print(f"Ch {chid} unit 0 has {len(timestamps[0])} events.")
                if chid not in spk_cache:
                    spk_cache[chid] = cbpy.SpikeCache(channel=chid)
                temp_wfs, unit_ids = spk_cache[chid].get_new_waveforms()
                print(f"Waveform shape: {temp_wfs.shape} on unit_ids {unit_ids}")
except KeyboardInterrupt:
    cbpy.close()
