# CBSDK Build Instructions

Most users will only need to download directly from the [releases page](https://github.com/CerebusOSS/CereLink/releases).

Other users who just want the CLI commands, or if the below instructions aren't working for you, should check out the GitHub Actions [workflow scripts](https://github.com/CerebusOSS/CereLink/blob/master/.github/workflows/build_cbsdk.yml).

Continue here only if you want to build CereLink (and cerebus.cbpy) from source.

## Requirements

We assume you already have a build environment with an appropriate C++ toolchain and CMake installed.

### Qt6

We are working on eliminating the Qt dependency. For now, you must install it.

* Windows:
  * [Qt6](https://www.qt.io/download-open-source/)
    * Get the latest 6.x LTS (last tested with 6.5.2)
    * The version of the library you download should match your compiler (MSVC or MinGW)
* Mac - Use [homebrew](https://brew.sh/)
  * `brew install qt`
* Ubuntu (/Debian)
  * Ubuntu 18.04 not supported.
  * `sudo apt-get update -y` 
  * Ubuntu 20.04 requires a backport ppa: `sudo add-apt-repository ppa:okirby/qt6-backports`
  * `sudo apt-get install qt6-base-dev`

### Matlab (optional)

If you want to build the Matlab wrappers then you will need to have Matlab development libraries available. In most cases, if you have Matlab installed in a default location, then cmake should be able to find it.

## Cmake command line - Try me first

Here are some cmake one-liners that work if your development environment happens to match perfectly. If not, then modify the cmake options according to the [CMake Options](#cmake-options) instructions below.

* Windows:
    * `cmake -B build -S . -G "Visual Studio 16 2019" -DCMAKE_PREFIX_PATH=C:\Qt\6.5.2\msvc2019_64 -DBUILD_STATIC=ON -DCMAKE_INSTALL_PREFIX=../install -DCPACK_PACKAGE_DIRECTORY=../dist`
* MacOS
    * `cmake -B build -S . -DQt6_DIR=$(brew --prefix qt6)/lib/cmake/Qt6 -DCMAKE_INSTALL_PREFIX=${PWD}/install -DCPACK_PACKAGE_DIRECTORY=${PWD}/dist`
    * If you are going to use the Xcode generator then you also need to use the old build system. Append: `-G Xcode -T buildsystem=1`
* Linux
    * `cmake -B build -S . -DCMAKE_INSTALL_PREFIX=${PWD}/install -DCPACK_PACKAGE_DIRECTORY=${PWD}/dist`

Then follow that up with (append a processor # after the -j to use more cores):
* `cmake --build build --config Release -j`
* `cmake --build build --target=install --config Release -j`

And optionally, to build zips or debs:
* `cmake --build build --target package --config Release -j`

The build products should appear in the CereLink/install or CereLink/build/install directory.

Note: This may generate an error related to the CLI builds. Please see further instructions in the [cli\README.md](./cli/README.md).

### CMake Options

* `-G <generator name>`
    * Call `cmake -G` to see a list of available generators.
* `-DQt6_DIR=<path/to/qt/binaries>/lib/cmake/Qt6`
    * This is the path to the folder holding Qt6Config.cmake for the compiler+architecture you are using.
* or `-DCMAKE_PREFIX_PATH=<path/to/qt/binaries>`
    * In my experience, Windows' CMake sometimes cannot find all the Qt modules when using the above option, but it usually works with the change to the prefix path.
* `-DBUILD_STATIC=ON`
    * Whether to build cbsdk_static lib. This is required by the Python and Matlab wrappers.
* `-DBUILD_CBMEX=ON`
    * To build Matlab binaries. Will only build if Matlab development libraries are found.
* `-DBUILD_CBOCT=ON`
    * To build Octave binaries. Will only build if Octave development libraries are found.
* `-DBUILD_TEST=ON`
* `-DBUILD_HDF5=ON`
    * Should only build if HDF5 found, but I have had to manually disable this in my builds.
* `-DMatlab_ROOT_DIR=<path/to/matlab/root>`
    * This should only be necessary if cmake cannot find Matlab automatically.
    * e.g.: `-DMatlab_ROOT_DIR=/Applications/MATLAB_R2016a.app/`
    * Alternatively, you could populate the ../Matlab directories with the Matlab include and lib files.
* `-DCBMEX_INSTALL_PREFIX` can be used to install cbmex to given directory.
* `-DBUILD_CLI=ON`
    * to build the C#/CLI bindings. Buggy.

# cerebus.cbpy (Python lib) Build Instructions

* Open a Terminal or Anaconda prompt and activate your Python environment.
* Your Python environment must match that of the eventual machine that will run the package, and it must already have Cython, numpy, pip, and wheel installed.
  * We do not set these as explicit dependencies on the package to avoid bundling them 
* Change to the CereLink directory.
* If you haven't already, build CereLink following the above instructions.
* Set the QTDIR environment variable:
  * Windows: `set QTDIR=C:\Qt\6.5.2\msvc2019_64`
  * Mac: `set QTDIR==$(brew --prefix qt6)`
* Windows: Close Visual Studio.
* Install locally: `pip install .`
* or, if you are making a wheel to bring to another machine,
  * activate an environment matching the target machine,
  * `pip wheel . -w dist/`
  * The wheels will be in the `dist` folder.
  * See the [Wiki](https://github.com/CerebusOSS/CereLink/wiki/cerebus.cbpy) for more information.
