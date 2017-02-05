from __future__ import print_function
import os
from os.path import join, dirname
import sys
import numpy
import warnings
from setuptools import find_packages
try:
    from setuptools import setup, Extension
    from Cython.Distutils import build_ext
    HAVE_CYTHON = True
except ImportError as e:
    HAVE_CYTHON = False
    warnings.warn(e.message)
    from distutils.core import setup, Extension
    from distutils.command import build_ext

CYTHON_REQUIREMENT = 'Cython>=0.19.1'

def get_extras():
    # Find all the extra include files, libraries, and link arguments we need to install.
    extra_includes = []
    extra_libs = []
    extra_link_args = []

    if sys.platform == "darwin":
        # Find Qt framework
        qtfwdir = '/usr/local/opt'  # Default search dir
        import subprocess
        p = subprocess.Popen('brew ls --versions qt5', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        hasqt5 = len(p.stdout.read()) > 0
        if hasqt5:
            p = subprocess.Popen('brew --prefix qt5', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            qtfwdir = str(p.stdout.read().decode("utf-8")[:-1]) + "/Frameworks"
        else:
            p = subprocess.Popen('brew ls --versions qt', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            hasqt4 = len(p.stdout.read()) > 0
            if hasqt4:
                p = subprocess.Popen('brew --prefix qt', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                qtfwdir = str(p.stdout.read().decode("utf-8")[:-1]) + "/Frameworks"
        extra_link_args += ['-F', qtfwdir,
                           '-framework', 'QtCore',
                           '-framework', 'QtXml']
        if hasqt5:
            extra_link_args += ['-framework', 'QtConcurrent']

    elif "win32" in sys.platform:
        import platform

        # Must include cbsdk headers
        extra_includes += [os.path.join(cur, 'dist\\include')]
        # Must include stdinit (V2008 does not have it!)
        extra_includes += [os.path.join(cur, 'compat')]
        # Must be explicit about cbsdk link path
        arch = '64' if '64bit' in platform.architecture() else ''
        cur = os.path.dirname(os.path.abspath(__file__))
        extra_link_args += ['/LIBPATH:{path}'.format(path=os.path.join(cur, 'dist\\lib{arch}'.format(arch=arch)))]
        # add winsock, timer, and system libraries
        extra_libs += ["ws2_32", "winmm"]
        extra_libs += ["kernel32", "user32", "gdi32", "winspool", "shell32",
                       "ole32", "oleaut32", "uuid", "comdlg32", "advapi32"]

        # Find Qt library
        def _get_qt_path():
            # Windows does not have a canonical install place, so try some known locations
            path = ''
            try:
                import _winreg
            except ImportError:
                import winreg as _winreg
            try:
                path = os.environ['QTDIR']  # e.g. `set QTDIR=C:\Qt\Qt5.6.0\5.6\msvc2015_64`
            except:
                pass
            if not path:
                try:
                    hk = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Trolltech\Versions', 0)
                    ver = _winreg.EnumKey(hk, 0)
                    _winreg.CloseKey(hk)
                    print('Using installed Qt{ver}'.format(ver=ver))
                    hk = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,
                                         r'SOFTWARE\Trolltech\Versions\{ver}'.format(ver=ver),
                                         0)
                    ii = 0
                    while True:
                        try:
                            install = _winreg.EnumValue(hk, ii)
                            if install[0].lower() == 'installdir':
                                path = install[1]
                                break
                        except:
                            break
                    _winreg.CloseKey(hk)
                    if not path:
                        raise ValueError("InstallDir not found")
                except:
                    raise RuntimeError("Cannot find Qt in registry nor QTDIR is set")

            # Parse qt_path to see if it is qt5
            is_qt5 = False
            tmp = (path, '')
            while tmp[0] is not '':
                tmp = os.path.split(tmp[0])
                mysplit = tmp[1].split('.')
                if len(mysplit) > 1:
                    if mysplit[0][-1] == '5':
                        is_qt5 = True
                        tmp = ('', '')
            return path, is_qt5

        qt_path, is_qt5 = _get_qt_path()
        extra_link_args += ['/LIBPATH:{path}'.format(path=os.path.join(qt_path, 'lib'))]
        extra_includes += [os.path.join(qt_path, 'include')]
        if is_qt5:
            extra_libs += ["Qt5Core", "Qt5Xml", "Qt5Concurrent"]
        else:
            extra_libs += ["QtCore4", "QtXml4"]
    else: #Linux
        extra_libs += ["QtCore", "QtXml"]  # TODO Qt5: + "QtConcurrent"

    return extra_includes, extra_libs, extra_link_args

extra_includes, extra_libs, extra_link_args = get_extras()

extension_kwargs = {
    'sources': ['cerebus/cbpy.pyx', 'cerebus/cbsdk_helper.cpp'],
    'libraries': ["cbsdk_static"] + extra_libs,
    'extra_link_args': extra_link_args,
    'include_dirs': [numpy.get_include()] + extra_includes}
if HAVE_CYTHON:
    extension_kwargs['language'] = 'c++'

cbpy_module = Extension('cerebus.cbpy', **extension_kwargs)

configuration = {
    'name': 'cerelink',
    'version': '0.0.2',
    'description': 'Cerebus Link',
    'long_description':'Cerebus Link Python Package',
    'author': 'dashesy',
    'author_email': 'dashesy@gmail.com',
    'url': 'https://github.com/dashesy/CereLink',
    'packages': find_packages(),
    'install_requires': [CYTHON_REQUIREMENT],
    'ext_modules': [cbpy_module],
    'cmdclass': {'build_ext': build_ext}}

if not HAVE_CYTHON:
    # If the user does not have Cython then they can use cbpyw.cpp if it is distributed (currently not)
    cbpy_module.sources[0] = 'cerebus/cbpyw.cpp'
    configuration.pop('install_requires')

setup(**configuration)
