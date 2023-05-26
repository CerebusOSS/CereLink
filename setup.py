import os
from pathlib import Path
import sys
import platform

import numpy
from setuptools import setup, Extension
from Cython.Build import build_ext


def get_extras():
    cur = os.path.dirname(os.path.abspath(__file__))
    arch = '64' if '64bit' in platform.architecture() else ''
    # Find all the extra include files, libraries, and link arguments we need to install.
    dist_path = os.path.join(cur, 'install')
    print(dist_path, os.path.exists(dist_path))
    if "win32" in sys.platform:
        vs_out = os.path.join(cur, 'out', 'install', 'x64-Release')
        if os.path.exists(vs_out):
            dist_path = vs_out
    elif not os.path.exists(dist_path):
        clion_out = os.path.join(cur, "cmake-build-release", "install")
        if os.path.exists(clion_out):
            dist_path = clion_out

    x_includes = [os.path.join(dist_path, 'include')]  # Must include cbsdk headers
    x_libs = []
    x_link_args = []

    if sys.platform == "darwin":
        x_link_args += ['-L{path}'.format(path=os.path.join(dist_path, 'lib'))]
        # Find Qt framework
        import subprocess
        p = subprocess.Popen('brew --prefix qt', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        qtfwdir = str(p.stdout.read().decode("utf-8")[:-1]) + "/Frameworks"

        x_link_args += ['-F', qtfwdir,
                        '-framework', 'QtCore',
                        '-framework', 'QtXml',
                        '-framework', 'QtConcurrent']

    elif "win32" in sys.platform:
        # Must include stdint (V2008 does not have it!)
        x_includes += [os.path.join(cur, 'compat')]
        # Must be explicit about cbsdk link path
        x_link_args += ['/LIBPATH:{path}'.format(path=os.path.join(dist_path, 'lib{arch}'.format(arch=arch)))]
        # add winsock, timer, and system libraries
        x_libs += ["ws2_32", "winmm"]
        x_libs += ["kernel32", "user32", "gdi32", "winspool", "shell32",
                   "ole32", "oleaut32", "uuid", "comdlg32", "advapi32"]

        # Find Qt library
        def _get_qt_path():
            # Windows does not have a canonical installation location, so we try some common locations.
            path = None
            if "QTDIR" in os.environ:
                path = Path(os.environ['QTDIR'])  # e.g. `set QTDIR=C:\Qt\6.4.3\msvc2019_64`
            elif "Qt6_DIR" in os.environ:
                path = Path(os.environ["Qt6_DIR"])  # C:\Qt\6.4.3\msvc2019_64\lib\cmake\Qt6
                while path.stem in ["Qt6", "cmake", "lib"]:
                    path = path.parents[0]
            else:
                base_paths = [Path(_) for _ in ["C:\\Qt", "D:\\a\\CereLink\\Qt"]]  # Latter used by GitHub Actions
                for bp in base_paths:
                    if bp.exists():
                        qt_install_paths = list(bp.glob("6*"))
                        if len(qt_install_paths) > 0:
                            qt_msvc_paths = list(qt_install_paths[-1].glob("msvc*64"))
                            if len(qt_msvc_paths) > 0:
                                path = qt_msvc_paths[-1]
                                break
            if not path:
                raise RuntimeError("Cannot find Qt. Please set QTDIR or Qt6_DIR environment variable and try again.")
            return path

        qt_path = _get_qt_path()
        x_link_args += [f"/LIBPATH:{qt_path / 'lib'}"]
        x_includes += [f"{qt_path / 'include'}"]
        x_libs += ["Qt6" + _ for _ in ["Core", "Xml", "Concurrent"]]
    else:  # Linux
        x_link_args += ['-L{path}'.format(path=os.path.join(dist_path, 'lib{arch}'.format(arch=arch)))]
        x_libs += ["Qt6" + _ for _ in ["Core", "Xml", "Concurrent"]]

    return x_includes, x_libs, x_link_args


extra_includes, extra_libs, extra_link_args = get_extras()


setup(
    ext_modules=[
        Extension("cerebus.cbpy",
                  sources=["cerebus/cbpy.pyx", "cerebus/cbsdk_helper.cpp"],
                  libraries=["cbsdk_static"] + extra_libs,
                  extra_link_args=extra_link_args,
                  include_dirs=[numpy.get_include()] + extra_includes,
                  language="c++"
                  )
    ],
    cmdclass={
        "build_ext": build_ext
    }
    # See pyproject.toml for rest of configuration.
)
