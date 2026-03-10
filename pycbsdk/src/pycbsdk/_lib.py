"""
Library loading for pycbsdk.

Finds and loads the cbsdk shared library (libcbsdk.dll / libcbsdk.so / libcbsdk.dylib).
"""

import os
import sys
import cffi

from ._cdef import CDEF

ffi = cffi.FFI()
ffi.cdef(CDEF)


def _find_library() -> str:
    """Find the cbsdk shared library.

    Search order:
    1. CBSDK_LIB_PATH environment variable (explicit path to the .dll/.so/.dylib)
    2. CBSDK_LIB_DIR environment variable (directory containing the library)
    3. Next to this Python package (for bundled wheels)
    4. Common build directories relative to the CereLink repo root
    5. System library paths (via cffi default search)
    """
    # Platform-specific library names
    if sys.platform == "win32":
        lib_names = ["cbsdk.dll", "libcbsdk.dll", "cbsdkd.dll", "libcbsdkd.dll"]
    elif sys.platform == "darwin":
        lib_names = ["libcbsdk.dylib"]
    else:
        lib_names = ["libcbsdk.so"]

    # 1. Explicit path
    explicit = os.environ.get("CBSDK_LIB_PATH")
    if explicit and os.path.isfile(explicit):
        return explicit

    # 2. Explicit directory
    lib_dir = os.environ.get("CBSDK_LIB_DIR")
    if lib_dir:
        for name in lib_names:
            path = os.path.join(lib_dir, name)
            if os.path.isfile(path):
                return path

    # 3. Next to this package (bundled in wheel)
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    for name in lib_names:
        path = os.path.join(pkg_dir, name)
        if os.path.isfile(path):
            return path

    # 4. Common build directories (for development)
    # Walk up from this file to find the CereLink repo root
    repo_root = pkg_dir
    for _ in range(10):
        parent = os.path.dirname(repo_root)
        if parent == repo_root:
            break
        repo_root = parent
        if os.path.isfile(os.path.join(repo_root, "CMakeLists.txt")):
            for build_dir in ["cmake-build-debug", "cmake-build-release", "build"]:
                for subpath in [
                    os.path.join("src", "cbsdk"),
                    "",
                ]:
                    for name in lib_names:
                        path = os.path.join(repo_root, build_dir, subpath, name)
                        if os.path.isfile(path):
                            return path

    # 5. Let cffi/OS try to find it on the system path
    for name in lib_names:
        # Strip lib prefix and extension for dlopen-style search
        return name

    return lib_names[0]  # Fallback, will produce a clear error


def load_library():
    """Load the cbsdk shared library and return the cffi lib object."""
    path = _find_library()

    # On Windows, add DLL search directories for runtime dependencies
    # (e.g., MinGW's libstdc++, libgcc, libwinpthread)
    _dll_dirs = []
    if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
        # Add the directory containing the library itself
        lib_dir = os.path.dirname(os.path.abspath(path))
        if os.path.isdir(lib_dir):
            _dll_dirs.append(os.add_dll_directory(lib_dir))

        # Add directories from CBSDK_DLL_DIRS (semicolon-separated)
        extra_dirs = os.environ.get("CBSDK_DLL_DIRS", "")
        for d in extra_dirs.split(";"):
            d = d.strip()
            if d and os.path.isdir(d):
                _dll_dirs.append(os.add_dll_directory(d))

        # Common MinGW locations
        for mingw_hint in [
            os.environ.get("MINGW_BIN", ""),
            os.path.expandvars(
                r"%LOCALAPPDATA%\Programs\CLion\bin\mingw\bin"
            ),
        ]:
            if mingw_hint and os.path.isdir(mingw_hint):
                _dll_dirs.append(os.add_dll_directory(mingw_hint))

    try:
        return ffi.dlopen(path)
    except OSError as e:
        raise OSError(
            f"Could not load cbsdk shared library: {e}\n"
            f"Searched for: {path}\n"
            f"Set CBSDK_LIB_PATH to the full path of the cbsdk shared library,\n"
            f"or CBSDK_LIB_DIR to the directory containing it.\n"
            f"On Windows, set CBSDK_DLL_DIRS to semicolon-separated directories\n"
            f"containing runtime dependencies (e.g., MinGW bin directory).\n"
            f"Build with: cmake -DCBSDK_BUILD_SHARED=ON"
        ) from e
