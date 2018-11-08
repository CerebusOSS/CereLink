* `rmdir /S build`
* `mkdir build && cd build`
* `cmake .. -G "Visual Studio 15 2017 Win64" -DQt5_DIR="C:\Qt\5.11.1\msvc2017_64\lib\cmake\Qt5" -DBUILD_HDF5=OFF -DBUILD_CLI=ON`
* `cd ..`
* `rmdir /S dist`
* Open generated build/CBSDK.sln
* Build the INSTALL target. This will likely generate one `setlocal` error you can ignore.

You should now be able to debug TestCLI.

Note that it may be necessary to change the type of library for cbsdk_cli from SHARED to MODULE>
If this becomes necessary, then the target properties for TestCLI will have to change.
Further, in the generated project, it will probably be necessary to first build the INSTALL target,
then possibly remove/readd the cbsdk_cli.dll reference to TestCLI.