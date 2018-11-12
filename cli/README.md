# CereLink C-Sharp Interface

There are two different interfaces provided:

* C++/CLI Interface
* C# Interface using P/Invoke

## Build system

Requires CMake >= 3.12.
Only tested on Windows VS 2017.
Add the `-DBUILD_CLI=ON` option to the build command described in ../BUILD.md.

Build the INSTALL target. This will generate at least one `setlocal` error you can ignore.
It will also generate a specific error for the C++/CLI interface described below.

## C++/CLI Interface

I couldn't figure out how to use CMake to get the C++/CLI test project (in TestCLI folder) to 
reference the cbsdk_cli.dll properly. So you'll have to do that manually in the project.
Under the TestCLI target, delete the cbsdk_cli reference and add a new reference pointing to
dist/lib64/cbsdk_cli.dll

## C# Interface using P/Invoke

An example is provided in TestCSharp.

### On Unity

First copy the dlls and Qt dependencies from CereLink/dist/bin to your Unity project folder.
Add to your assets the CereLink/cli/TestCSharp/CereLink.cs and CereLink/cli/UnityExample/CereLinkInterface.cs
Create a new game object and add CereLinkInterface.cs as a script component.
