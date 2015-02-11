from __future__ import print_function
from distutils.command import build_ext as _build_ext
import os
from os.path import join, dirname
from setuptools import Extension, find_packages, setup
try:
    from Cython.Distutils import build_ext
except ImportError:
    build_ext = None
import numpy
import sys

# Make sure on OSX we bring in the framework
if sys.platform == "darwin":
    extra_link_args=['-F', '/usr/local/lib/', '-framework', 'QtCore', '-framework', 'QtXml']
    libraries = ["cbsdk_static"]
else:
    extra_link_args= []
    libraries = ["cbsdk_static", "QtCore", "QtXml"]

extra_includes = []
if "win32" in sys.platform:
    # Windows does not have a canonical install place, so try some known locations
    import platform
    import _winreg
    def _get_qt_path():
        path = ''
        try:
            path = os.environ['QTDIR']
        except:
            pass
        if not path:
            try:
                hk = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Trolltech\Versions', 0)
                ver = _winreg.EnumKey(hk, 0)
                _winreg.CloseKey(hk)
                print('Using installed Qt{ver}'.format(ver=ver))
                hk = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Trolltech\Versions\{ver}'.format(ver=ver), 0)
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
        
        return path
    
    arch = '64' if '64bit' in platform.architecture() else ''
    cur = os.path.dirname(os.path.abspath(__file__))
    qt_path = _get_qt_path()
    extra_link_args= ['/LIBPATH:{path}'.format(
                        path=os.path.join(cur, 'dist\\lib{arch}'.format(arch=arch))),
                      '/LIBPATH:{path}'.format(
                        path=os.path.join(qt_path, 'lib'))]
    extra_includes = [os.path.join(cur, 'dist\\include'), os.path.join(qt_path, 'include')]
    
    print(extra_link_args)
    print(extra_includes)

CYTHON_REQUIREMENT = 'Cython==0.19.1'

if build_ext:
    cbpy_module = Extension(
                            'cerebus.cbpy',
                            ['cerebus/cbpyw.pyx',
                             'cerebus/cbpy.cpp', 
                            ],
                            libraries=libraries,
                            extra_link_args=extra_link_args,
                            include_dirs=[numpy.get_include()] + extra_includes,
                            language="c++",             # generate C++ code
    )
else:
    cbpy_module = Extension(
                            'cerebus.cbpy',
                            ['cerebus/cbpyw.cpp',
                             'cerebus/cbpy.cpp', 
                            ],
                            libraries=libraries,
                            extra_link_args=extra_link_args,
                            include_dirs=[numpy.get_include()] + extra_includes,
    )
    
    class build_ext(_build_ext.build_ext):
        def initialize_options(self):
            self.cwd = None
            return _build_ext.build_ext.initialize_options(self)

        def finalize_options(self):
            self.cwd = os.getcwd()
            return _build_ext.build_ext.finalize_options(self)

        def run(self):
            assert os.getcwd() == self.cwd, \
                'Must be in package root: %s' % self.cwd
            print("""
            --> Cython is not installed. Can not compile .pyx files. <--
            Unfortunately, setuptools does not allow us to install
            Cython prior to this point, so you'll have to do it yourself
            and run this command again, if you want to recompile your
            .pyx files.

            `pip install {cython}` should suffice.

            ------------------------------------------------------------
            """.format(
                cython=CYTHON_REQUIREMENT,
            ))
            assert os.path.exists(
                os.path.join(self.cwd, 'cerebus', 'cbpyw.cpp')), \
                'Source file not found!'
            return _build_ext.build_ext.run(self)



setup(
    name='cerelink',
    version='0.0.1',
    description='Cerebus Link',
    long_description='Cerebus Link Python Package',
    author='dashesy',
    author_email='dashesy@gmail.com',
    url='https://github.com/dashesy/CereLink',
    packages=find_packages(),
    cmdclass={
        'build_ext': build_ext,
    },
    ext_modules=[
        cbpy_module,
    ],
)
