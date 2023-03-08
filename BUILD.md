# CBSDK Build Instructions

## Requirements

* Windows:
    * [CMake](https://cmake.org/download/)
        * Make sure to add cmake to the path.
    * [Qt6](https://www.qt.io/download-open-source/)
        * Using the installer, choose the most recent non-preview libraries that match your compiler.
* Mac - Use [homebrew](https://brew.sh/)
    * `brew install cmake qt`
* Ubuntu (/Debian)
    * `sudo apt-get install build-essential cmake qt6-default`

### Matlab (optional)

If you want to build the Matlab wrappers then you will need to have Matlab development libraries available. In most cases, if you have Matlab installed in a default location, then cmake should be able to find it.

## Cmake command line - Try me first

Here are some cmake one-liners that work if your development environment happens to match perfectly. If not, then modify the cmake options according to the [CMake Options](#cmake-options) instructions below.

* Windows:
    * `cmake -B build -S . -G "Visual Studio 16 2019" -DCMAKE_PREFIX_PATH=C:\Qt\6.4.2\msvc2019_64\lib\cmake\Qt6 -DCMAKE_INSTALL_PREFIX=dist -DBUILD_STATIC=ON -DBUILD_CLI=ON`
* MacOS
    * `cmake  -B build -S . -DQt6_DIR=$(brew --prefix qt6)/lib/cmake/Qt6`
    * If you are going to use the Xcode generator then you also need to use the old build system: `-G Xcode -T buildsystem=1`
* Linux
    * `cmake -B build -S . -DCMAKE_INSTALL_PREFIX=dist`

Then follow that up with:
* `cmake --build build --config Release`

The build products should appear in the CereLink/dist directory.

Note: This may generate an error related to the CLI builds. Please see further instructions in the cli\README.md

### CMake Options

* `-G <generator name>`
    * Call `cmake -G` to see a list of available generators.
* `-DQt6_DIR=<path/to/qt/binaries>/lib/cmake/Qt6`
    * This is the path to the folder holding Qt6Config.cmake for the compiler+architecture you are using.
* `-DBUILD_STATIC=ON`
    * Whether to build cbsdk_static lib. This is required by the Python and Matlab wrappers.
* `-DBUILD_CBMEX=ON`
    * To build Matlab binaries. Will only build if Matlab development libraries are found.
* `-DBUILD_CBOCT=ON`
    * To build Octave binaries. Will only build if Octave development libraries are found.
* `-DBUILD_TEST=ON`
* `-DBUILD_HDF5=ON`
    * Should only build if HDF5 found, but I have had to manually disable this in my builds.
* `DMatlab_ROOT_DIR=<path/to/matlab/root>`
    * This should only be necessary if cmake cannot find Matlab automatically.
    * e.g.: `-DMatlab_ROOT_DIR=/Applications/MATLAB_R2016a.app/`
    * Alternatively, you could populate the ../Matlab directories with the Matlab include and lib files.
* `-DCBMEX_INSTALL_PREFIX` can be used to install cbmex to given directory.
* `-DBUILD_CLI=ON`

# cerebus.cbpy (Python lib) Build Instructions

* Open a Terminal or Anaconda prompt and activate your Python environment.
* Your Python environment must already have Cython, numpy, and pip installed.
* Change to the CereLink directory.
* Set the QTDIR environment variable: `set QTDIR=C:\Qt\6.4.2\msvc2019_64`
* Make sure the CereLink Visual Studio project is closed.
* `pip install .`
* or, if you are making a wheel to bring to another machine,
  * activate an environment matching the target machine,
  * `pip wheel . -w dist/bin`
