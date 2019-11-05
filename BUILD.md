# CBSDK Build Instructions

## Using the solution file (deprecated)

This method isn't really supported anymore because it is not future proof,
and as such this description is here for legacy purposes but may be insufficient
for newcomers.

You can try using the provided CereLink.sln file in Visual Studio 2005.
This seems to require an environment variable called QTDIR to be set and pointing to Qt4.

If you are building mex files then this also requires the Matlab folder
to be populated with Matlab include and library files. You will probably need to match
the Matlab version with the Visual Studio version for compiler compatibility.

## Using the CMake build system (supported)

### Dependencies

* Windows:
    * [CMake](https://cmake.org/download/)
        * Make sure to add cmake to the path.
    * [Qt5](https://www.qt.io/download-open-source/)
        * Using the installer, choose the most recent non-preview libraries that match your compiler.
* Mac - Use [homebrew](https://brew.sh/)
    * `brew install cmake qt`
* Ubuntu (/Debian)
    * `sudo apt-get install build-essential cmake qt5-default`

#### Matlab (optional)

If you want to build then Matlab wrappers then you will need to have Matlab development libraries available.
In most cases, if you have Matlab installed in a default location, then cmake should be able to find it.

### Create your build directory

Use Terminal or Windows users should use "x64 Native Tools Command Prompt for VS 2017".
`cd` into the CereLink directory.
If the `build` directory already exists then delete it
(Win: `rmdir /S build`, Others: `rm -Rf build`).

`mkdir build && cd build`
    
### Try me first

Here are some cmake one-liners that work if your development environment
happens to match perfectly. If not, then modify the cmake options following the instructions below.

* Windows:
    * `cmake .. -G "Visual Studio 15 2017 Win64" -DQt5_DIR="C:\Qt\5.12.0\msvc2017_64\lib\cmake\Qt5" -DBUILD_HDF5=OFF`
* MacOS
    * `cmake .. -DQt5_DIR=$(brew --prefix qt5)/lib/cmake/Qt5`
* Linux
    * `cmake ..`
    
Then follow that up with
```
cmake --build . --config Release
```
The build products should appear in the CereLink/dist directory.
    
### CMake Options

Note: If you want to learn more about CMake or try VS 2017's fancy integrated CMake,
then read [here](http://preshing.com/20170511/how-to-build-a-cmake-based-project/)
for a primer.

* `-G <generator name>`
    * [Generator](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#cmake-generators)
* `-DQt5_DIR=<path/to/qt/binaries>/lib/cmake/Qt5`
    * This is the path to the folder holding Qt5Config.cmake for the compiler+architecture you are using.
* `-DBUILD_STATIC=ON`
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

# cerebus (Python lib) Build Instructions

* Open a Terminal or Anaconda prompt and activate your Python environment.
* Your Python environment must already have Cython installed and pip should be installed too.
* Change to the CereLink directory.
* Set the QTDIR environment variable: `set QTDIR=C:\Qt\5.12.0\msvc2017_64`
* `pip install .`
