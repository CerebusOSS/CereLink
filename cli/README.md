# CereLink C-Sharp Interface

There are two different interfaces provided:

* C++/CLI Interface
* C# Interface using P/Invoke

## Build system

Requires CMake >= 3.12.
Only tested on Windows VS 2017.
Add the `-DBUILD_CLI=ON` option to the build command described in ../BUILD.md.

If you followed the instructions in ../BUILD.md then you will have already built the release and this will have generated some errors.
Next, built the INSTALL target:
* `cmake --build . --config Release --target install`

This will generate at least one `setlocal` error you can ignore.
It will also generate a specific error for the C++/CLI interface. The fix is described below.

## C++/CLI Interface

I couldn't figure out how to use CMake to get the C++/CLI test project (in TestCLI folder) to 
reference the cbsdk_cli.dll properly. So you'll have to do that manually in the project.

Under the TestCLI target, delete the cbsdk_cli reference and add a new reference pointing to
dist/lib64/cbsdk_cli.dll

* Open the build\CBSDK.sln file.
* Change the config from Debug to Release
* Expand the TestCLI target (click on right arrow)
* Expand "References"
* Right click on `cbsdk_cli` and remove it.
* Right click on References and select `Add Reference...`
* In the new dialog window, browse to dist/lib64/cbsdk_cli.dll

## C# Interface using P/Invoke

An example is provided in TestCSharp.

### On Unity

First copy the dlls and Qt dependencies from CereLink/dist/bin to your Unity project folder.
Add to your assets the CereLink/cli/TestCSharp/CereLink.cs and CereLink/cli/UnityExample/CereLinkInterface.cs
Create a new game object and add CereLinkInterface.cs as a script component.
