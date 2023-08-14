# Make sure the CereLink project folder is not on the path,
# otherwise it will attempt to import cerebus from there, instead of the installed python package.
from cerebus import cbpy


res, con_info = cbpy.open(parameter=cbpy.defaultConParams())
res, group_info = cbpy.get_sample_group(5)

res, reset = cbpy.trial_config(
    reset=True,
    buffer_parameter={'absolute': True},
    range_parameter={},
    noevent=1,
    nocontinuous=0,
    nocomment=1
)

try:
    while True:
        result, data, t_0 = cbpy.trial_continuous(reset=True)
        if len(data) > 0:
            print(data)
except KeyboardInterrupt:
    cbpy.close()
