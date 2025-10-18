# CereLink

Cerebus Link for Blackrock Neurotech hardware

The software development kit for Blackrock Neurotech neural signal processing hardware includes:
* c++ library (cbsdk): cross-platform library for two-way communication with hardware
* Python binding (cerelink): Python binding for cbsdk to configure, pull data, and receive callbacks
* MATLAB/Octave binding (cbmex/cboct): MATLAB executable (mex) to configure and pull data using cbsdk
* C#/CLI binding
* File conversion utility (n2h5): Converts nsx and nev files to hdf5 format

Downloads are on the [releases page](https://github.com/CerebusOSS/CereLink/releases).

## Build

The [BUILD.md](./BUILD.md) document has the most up-to-date build instructions.

## Usage

You must be connected to a Blackrock Neurotech device (Cerebus, CerePlex, NSP, or Gemini) or a simulator (nPlayServer) to use cbsdk, cbmex, cboct, or cerebus.cbpy.

### Testing with nPlayServer

On Windows, download and install the latest version of Cerebus Central Suite from the [Blackrock Neurotech support website (scroll down)](https://blackrockneurotech.com/support/). You may also wish to download some sample data from this same website.

nPlayServer for other platforms is available upon request. Post an issue in this repository with details about your system configuration, and we will try to help.

#### Testing with nPlayServer on localhost

After installing, navigate an explorer Window to `C:\Program Files\Blackrock Microsystems\Cerebus Central Suite\` and run `runNPlayAndCentral.bat`. This will run a device simulator (nPlayServer) and Central on the localhost loopback. cbsdk / cerebus / cbmex should be able to connect as a Slave (Central is the Master) to the nPlay instance.

#### Testing with nPlayServer on network

If you want to test using nPlayServer on a different computer to better emulate the device, you must first configure the IP addresses of the ethernet adapters of both the client computer with CereLink and the device-computer with nPlayServer. The client computer should be set to 192.168.137.198 or .199. The nPlayServer computer's IP address should be set to 192.168.137.128 to mimic a Cerebus NSP or 192.168.137.200 to mimic a Gemini Hub.

> Run `nPlayServer --help` to get a list of available options.

* Emulating Legacy NSP: `nPlayServer -L --network inst=192.168.137.128:51001 --network bcast=192.168.137.255:51002`
* Emulating Gemini Hub: `nPlayServer -L --network inst=192.168.137.200:51002 --network bcast=192.168.137.255:51002`

### cerebus.cbpy

See [bindings/Python/README.md](./bindings/Python/README.md) for usage and build instructions.

## Getting Help

First, read the frequently asked questions and answers in the [project wiki](https://github.com/CerebusOSS/CereLink/wiki).

Second, search the [issues on GitHub](https://github.com/CerebusOSS/CereLink/issues).

Finally, open an issue.

This is a community project and is not officially supported by Blackrock Neurotech.
