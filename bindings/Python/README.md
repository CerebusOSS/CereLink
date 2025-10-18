# cerelink

This is a Python wrapper for the CereLink (cbsdk) library to configure, pull data, and receive callbacks from Blackrock Neurotech devices.

> Note: If you do not require Central running on the same computer as your Python Blackrock device client, then consider instead using [pycbsdk](https://github.com/CerebusOSS/pycbsdk).

## Getting Started

* Download a wheel from the releases page or [build it yourself](#build-instructions).
* Activate a Python environment with pip, Cython, and numpy
* Install the wheel: `python -m pip install path\to\filename.whl`
* Test with `python -c "from cerelink import cbpy; cbpy.open(parameter=cbpy.defaultConParams())"`
    * You might get `RuntimeError: -30, Instrument is offline.`. That's OK, depending on your device and network settings.
* See [cerebuswrapper](https://github.com/CerebusOSS/cerebuswrapper) for a tool that provides a simplified interface to cerelink.

## Build Instructions

* If you haven't already, build and install CereLink following the [main BUILD instructions](../../BUILD.md).
    * Windows: If using Visual Studio then close it.
* Open a Terminal or Anaconda prompt and activate your Python environment.
* Your Python environment's Python interpreter (CPython, arch, version) must match that of the eventual machine that will run the package, and it must already have Cython, numpy, pip, setuptools, and wheel installed.
    * We do not set these as explicit dependencies on the package to avoid bundling them so you must install manually.
    * `python -m ensurepip`
    * `python -m pip install --upgrade pip setuptools wheel cython numpy`
* Change to the CereLink directory.
* Install locally: `python -m pip install bindings/Python`
* or, if you are making a wheel to bring to another machine,
    * activate an environment matching the target machine,
    * `python -m pip wheel bindings/Python -w bindings/Python/dist`
    * The wheels will be in the `bindings/Python/dist` folder.
    * See the [Wiki](https://github.com/CerebusOSS/CereLink/wiki/cerelink) for more information.

## Warning about NumPy ABI compatibility

This project is built against NumPy's C-API 1.7. If this becomes incompatible with newer version of NumPy 2.x at some point, then the following change to the pyproject.toml file may be required:

```toml
[build-system]
requires = ["setuptools", "wheel", "cython", "numpy>=2.0.0rc1", "setuptools-scm"]

[project]
dependencies = [
  "numpy>=2.0.0",
]
```

Until then, users may have to limit themselves to Python < 2.0
