# Make sure the CereLink project folder is not on the path,
# otherwise it will attempt to import cerebus from there, instead of the installed python package.
from cerebus import cbpy


MONITOR_CHAN = 2  # 1-based
AOUT_CHAN = 277

res, con_info = cbpy.open(parameter=cbpy.defaultConParams())

# Reset all analog output channels to not monitor anything
for chan_ix in range(273, 278):
    res = cbpy.analog_out(chan_ix, None, track_last=False)

# Set one analog output chan (277 = audio1) to monitor a single channel.
res = cbpy.analog_out(AOUT_CHAN, MONITOR_CHAN, track_last=False)
print(res)
