import os
from pathlib import Path
import sys

import numpy
from setuptools import setup, Extension
from Cython.Build import build_ext


def get_cbsdk_path(root):
    # Check for environment variable first (for CI/CD builds)
    env_path = os.environ.get("CBSDK_INSTALL_PATH")
    if env_path and os.path.exists(env_path):
        return env_path

    if "win32" in sys.platform:
        vs_out = os.path.join(root, "out", "install", "x64-Release")
        if os.path.exists(vs_out):
            return vs_out
    for build_dir in [".", "build", "cmake-build-release"]:
        cbsdk_path = os.path.join(root, build_dir, "install")
        if os.path.exists(cbsdk_path):
            return cbsdk_path


def get_extras():
    # Find all the extra include files, libraries, and link arguments we need to install.
    cur = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(cur, "..", ".."))
    cbsdk_path = get_cbsdk_path(root)

    # Prepare return variables
    x_includes = []
    x_libs = []
    x_link_args = []
    x_compile_args = []

    # C++17 standard (matching CMake build configuration)
    x_compile_args.append("/std:c++17" if "win32" in sys.platform else "-std=c++17")

    # CBSDK
    x_includes.append(os.path.join(cbsdk_path, "include"))
    if sys.platform == "darwin":
        x_link_args += ["-L{path}".format(path=os.path.join(cbsdk_path, "lib"))]
    elif "win32" in sys.platform:
        x_link_args.append("/LIBPATH:" + str(Path(cbsdk_path) / f"lib"))
    else:
        x_link_args.append(f"-L{os.path.join(cbsdk_path, 'lib')}")

    # pugixml for CCF
    for build_dir in [".", "build", "cmake-build-release"]:
        pugixml_path = Path(cbsdk_path).parent / build_dir / "_deps" / "pugixml-build"
        if (pugixml_path / "Release").exists():
            pugixml_path = pugixml_path / "Release"
        if pugixml_path.exists():
            break
    x_link_args.append(
        f"/LIBPATH:{pugixml_path}" if "win32" in sys.platform else f"-L{pugixml_path}"
    )
    x_libs.append("pugixml")

    # Other platform-specific libraries
    if "win32" in sys.platform:
        # Must include stdint (V2008 does not have it!)
        x_includes.append(os.path.join(root, "compat"))
        # add winsock, timer, and system libraries
        x_libs += ["ws2_32", "winmm"]
        x_libs += [
            "kernel32",
            "user32",
            "gdi32",
            "winspool",
            "shell32",
            "ole32",
            "oleaut32",
            "uuid",
            "comdlg32",
            "advapi32",
        ]
    elif "darwin" not in sys.platform:
        x_libs.append("rt")

    return x_includes, x_libs, x_link_args, x_compile_args


extra_includes, extra_libs, extra_link_args, extra_compile_args = get_extras()


setup(
    ext_modules=[
        Extension(
            "cerelink.cbpy",
            sources=["cerelink/cbpy.pyx", "cerelink/cbsdk_helper.cpp"],
            libraries=["cbsdk_static"] + extra_libs,
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
            include_dirs=[numpy.get_include()] + extra_includes,
            language="c++",
            define_macros=[("NPY_NO_DEPRECATED_API", "NPY_1_7_API_VERSION")],
        )
    ],
    cmdclass={"build_ext": build_ext},
    # See pyproject.toml for rest of configuration.
)
