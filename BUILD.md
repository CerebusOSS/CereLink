# CBSDK Build Instructions

Most users will only need to download directly from the [releases page](https://github.com/CerebusOSS/CereLink/releases).

| Platform      | C++ Packages (.deb/.zip) | Python Wheels |
|---------------|--------------------------|---------------|
| windows-x64   | ✅                        | ✅             |
| windows-arm64 | ✅                        | ✅             |
| macOS-latest  | ✅                        | ✅             |
| jammy         | ✅                        | ❌             |
| ubuntu-latest | ✅                        | ✅ (manylinux) |
| bookworm-x64  | ✅                        | ❌             |
| linux-arm64   | ✅                        | ✅ (aarch64)   |

Other users who just want the CLI commands, or if the below instructions aren't working for you, should check out the GitHub Actions [workflow scripts](https://github.com/CerebusOSS/CereLink/blob/master/.github/workflows/build_cbsdk.yml).

Continue here only if you want to build CereLink (Python cerelink) from source.

## Requirements

We assume you already have a build environment with an appropriate C++ toolchain and CMake installed.

### Matlab (optional)

If you want to build the Matlab wrappers then you will need to have Matlab development libraries available. In most cases, if you have Matlab installed in a default location, then cmake should be able to find it.

## Cmake command line - Try me first

Here are some cmake one-liners that work if your development environment happens to match perfectly. If not, then modify the cmake options according to the [CMake Options](#cmake-options) instructions below.

* Windows:
    * `cmake -B build -S . -G "Visual Studio 16 2019" -DBUILD_STATIC=ON -DCMAKE_INSTALL_PREFIX=../install -DCPACK_PACKAGE_DIRECTORY=../dist`
* MacOS
    * `cmake -B build -S . -DCMAKE_INSTALL_PREFIX=${PWD}/install -DCPACK_PACKAGE_DIRECTORY=${PWD}/dist`
    * If you are going to use the Xcode generator then you also need to use the old build system. Append: `-G Xcode -T buildsystem=1`
* Linux
    * `cmake -B build -S . -DCMAKE_INSTALL_PREFIX=${PWD}/install -DCPACK_PACKAGE_DIRECTORY=${PWD}/dist`

Then follow that up with (append a processor # after the -j to use more cores):
* `cmake --build build --config Release -j`
* `cmake --build build --target=install --config Release -j`

And optionally, to build zips or debs:
* `cmake --build build --target package --config Release -j`

The build products should appear in the CereLink/install or CereLink/build/install directory.

Note: This may generate an error related to the CLI builds. Please see further instructions in the [wrappers/cli/README.md](wrappers/cli/README.md).

### CMake Options

* `-G <generator name>`
    * Call `cmake -G` to see a list of available generators.
* `-DCBSDK_BUILD_STATIC=ON`
    * Whether to build cbsdk_static lib. This is required by the Python and Matlab wrappers.
* `-DCBSDK_BUILD_CBMEX=ON`
    * To build Matlab binaries. Will only build if Matlab development libraries are found.
* `-DCBSDK_BUILD_CBOCT=ON`
    * To build Octave binaries. Will only build if Octave development libraries are found.
* `-DCBSDK_BUILD_TEST=ON`
* `-DCBSDK_BUILD_SAMPLE=ON`
* `-DCBSDK_BUILD_TOOLS=ON`
    * NSX to HDF5 tool should only build if HDF5 found.
    * A few C++ applications in `tools/loop_tester` to test data pulling stability.
* `-DCBSDK_BUILD_CLI=ON`
    * to build the C#/CLI bindings. Buggy.
* `-DCBPROTO_311=OFF`
    * Set this to ON to compile CereLink to work with NSPs using protocol 3.11 (firmware 7.0x).
    * Matlab and Python wrappers not supported in this mode.
    * Cannot run alongside Central x86 version (if in "Program Files (x86)") because the Qt6 dependency restricts compilation targets to x64. But it works fine on other computers or if Central is not running.

#### Extra Options for Matlab / cbmex

* `-DMatlab_ROOT_DIR=<path/to/matlab/root>`
    * This should only be necessary if cmake cannot find Matlab automatically.
    * e.g.: `-DMatlab_ROOT_DIR=/Applications/MATLAB_R2016a.app/`
    * Alternatively, you could populate the ../Matlab directories with the Matlab include and lib files.
* `-DCBMEX_INSTALL_PREFIX` can be used to install cbmex to given directory.
