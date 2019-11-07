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
    while True:
        result, data = cbpy.trial_event(reset=True)
        if len(data) > 0:
            print(data)
except KeyboardInterrupt:
    cbpy.close()
