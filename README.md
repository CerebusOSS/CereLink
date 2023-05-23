CereLink
========

Blackrock Microsystems Cerebus Link

The software develoment kit for Blackrock Microsystems neural signal processing hardware includes:

c++ library (cbsdk): cross platform library for two-way communication with hardware

MATLAB/Octave wrapper (cbmex/cboct): MATLAB executable (mex) to configure and pull data using cbsdk

Python wrapper (cerebus.cbpy): Python binding for cbsdk to configure, pull data, and receive callbacks

File conversion utility (n2h5): Converts nsx and nev files to hdf5 format

Downloads are on the [releases page](https://github.com/CerebusOSS/CereLink/releases).

# Project wiki

https://github.com/CerebusOSS/CereLink/wiki

# Build

The BUILD.md document has the most up-to-date build instructions.

# Usage

## Testing on nPlayServer

On Windows, download and install the latest version of Cerebus Central Suite from the [Blackrock Neurotech research/support/software](https://blackrockneurotech.com/research/support/#manuals-and-software-downloads) website.
After installing, navigate an explorer Window to `C:\Program Files\Blackrock Microsystems\Cerebus Central Suite\` and run `runNPlayAndCentral.bat`. This will run a device simulator (nPlayServer) and Central on the localhost loopback. cbsdk / cerebus / cbmex should be able to connect as a Slave (Central is the Master) to the nPlay instance.

## cerebus.cbpy

To install the package, activate your Python environment and install the wheel downloaded from the release page that matches your NSP firmware version, your target platform, and Python version. If there is not a matching wheel on the Releases page then you will have to [build](BUILD.md) it yourself.

When installing cerebus.cbpy, the main numpy and Cython dependencies should install automatically.

At runtime, the Qt libraries are also required. On Linux and Mac, it is usually sufficient to install Qt6 at the system level with apt or homebrew. In Windows, it _should_ be sufficient to have Qt6Core.dll on the path somewhere, but I've found this to be unreliable depending on the Python environment manager. The most reliable approach is to copy Qt6Core.dll into the same folder as your .pyd file, which should be in `path/to/python/env/Lib/site-packages/cerebus`.
