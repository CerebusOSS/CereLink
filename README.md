# CereLink

Blackrock Microsystems Cerebus Link

The software develoment kit for Blackrock Microsystems neural signal processing hardware includes:
* c++ library (cbsdk): cross platform library for two-way communication with hardware
* MATLAB/Octave wrapper (cbmex/cboct): MATLAB executable (mex) to configure and pull data using cbsdk
* Python wrapper (cerebus.cbpy): Python binding for cbsdk to configure, pull data, and receive callbacks
* File conversion utility (n2h5): Converts nsx and nev files to hdf5 format

Downloads are on the [releases page](https://github.com/CerebusOSS/CereLink/releases).

## Build

The BUILD.md document has the most up-to-date build instructions.

## Usage

### Testing with nPlayServer

On Windows, download and install the latest version of Cerebus Central Suite from the [Blackrock Neurotech support website (scroll down)](https://blackrockneurotech.com/support/).

#### Testing with a localhost client

After installing, navigate an explorer Window to `C:\Program Files\Blackrock Microsystems\Cerebus Central Suite\` and run `runNPlayAndCentral.bat`. This will run a device simulator (nPlayServer) and Central on the localhost loopback. cbsdk / cerebus / cbmex should be able to connect as a Slave (Central is the Master) to the nPlay instance.

#### Testing with a networked client

If you want to test using nPlayServer on one computer with CereLink on a secondary computer, then you will have to run nPlayServer with special settings. It will also make your life easier if you change the IP address of the network adapter of your Windows machine to mimic that of a Cerebus device (192.168.137.128 for NSP or 192.168.137.200 for Gemini Hub).

> Run `nPlayServer --help` to get a list of available options.

* Emulating Legacy NSP: `nPlayServer -L --network inst=192.168.137.128:51001 --network bcast=192.168.137.255:51002`
* Emulating Gemini Hub: `nPlayServer -L --network inst=192.168.137.200:51002 --network bcast=192.168.137.255:51002`

### cerebus.cbpy

* Download a wheel from the releases page or [build it yourself](BUILD.md#cerebuscbpy-python-lib-build-instructions).
* Activate a Python environment with pip, Cython, and numpy
* Install the wheel: `pip install path\to\filename.whl`
* Test with `python -c "from cerebus import cbpy; cbpy.open(parameter=cbpy.defaultConParams())"`
    * You might get `RuntimeError: -30, Instrument is offline.`. That's OK, depending on your device and network settings.

## Getting Help

First, read the frequently asked questions and answers in the [project wiki](https://github.com/CerebusOSS/CereLink/wiki).

Second, search the [issues on GitHub](https://github.com/CerebusOSS/CereLink/issues).

Finally, open an issue.

This is a community project and is not officially supported by Blackrock Neurotech.
