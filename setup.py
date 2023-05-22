from __future__ import print_function
import os
from pathlib import Path
import sys
import platform

import numpy
from setuptools import find_packages
from setuptools import setup, Extension
from Cython.Build import build_ext


def get_extras():
    cur = os.path.dirname(os.path.abspath(__file__))
    arch = '64' if '64bit' in platform.architecture() else ''
    # Find all the extra include files, libraries, and link arguments we need to install.
    dist_path = os.path.join(cur, 'install')
    if "win32" in sys.platform:
        vs_out = os.path.join(cur, 'out', 'install', 'x64-Release')
        if os.path.exists(vs_out):
            dist_path = vs_out

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
        # For Qt linking at run time, check `qtchooser -print-env`
        # If it is not pointing to correct QtDir, then edit /usr/lib/x86_64-linux-gnu/qt-default/qtchooser/default.conf
        # /opt/Qt/5.9/gcc_64/bin
        # /opt/Qt/5.9/gcc_64/lib
        # This should also take care of finding the link dir at compile time, (i.e. -L/path/to/qt not necessary)
        # but we need to specify lib names, and these are version dependent.
        import subprocess
        import re
        p = subprocess.Popen('qmake -v', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        res = p.stdout.read().decode()
        if not res.startswith("Qt version"):
            p = subprocess.Popen('qmake6 -v', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            res = p.stdout.read().decode()
        qt_ver = re.findall('Qt version .', res)[0][-1]
        x_libs += ["Qt" + qt_ver + _ for _ in ["Core", "Xml", "Concurrent"]]

    return x_includes, x_libs, x_link_args


extra_includes, extra_libs, extra_link_args = get_extras()

extension_kwargs = {
    'sources': ['cerebus/cbpy.pyx', 'cerebus/cbsdk_helper.cpp'],
    'libraries': ["cbsdk_static"] + extra_libs,
    'extra_link_args': extra_link_args,
    'include_dirs': [numpy.get_include()] + extra_includes,
    'language': 'c++'
}

cbpy_module = Extension('cerebus.cbpy', **extension_kwargs)

setup(
    name='cerebus',
    version='0.2',
    description='Cerebus Link',
    long_description='Cerebus Link Python Package',
    author='Chadwick Boulay',
    author_email='chadwick.boulay@gmail.com',
    setup_requires=['Cython', 'numpy', 'wheel'],
    install_requires=['Cython', 'numpy'],
    url='https://github.com/CerebusOSS/CereLink',
    packages=find_packages(),
    cmdclass={
        'build_ext': build_ext,
    },
    ext_modules=[
        cbpy_module,
    ],
)
