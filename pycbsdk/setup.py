"""Setup script for pycbsdk.

Forces platform-specific wheel tags (py3-none-{platform}) because the
package bundles a pre-built shared library (cbsdk.dll / libcbsdk.so / libcbsdk.dylib).
"""

from setuptools import setup

try:
    from wheel.bdist_wheel import bdist_wheel

    class PlatformWheel(bdist_wheel):
        """Tag wheel as platform-specific but Python-version-independent."""

        def get_tag(self):
            return "py3", "none", self.plat_name.replace("-", "_").replace(".", "_")

    setup(cmdclass={"bdist_wheel": PlatformWheel})
except ImportError:
    setup()
