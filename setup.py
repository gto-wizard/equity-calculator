from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import os
import sys
import subprocess
import sysconfig
import glob

class CMakeBuildExt(build_ext):
    def build_extension(self, ext):
        # Ensure the build directory exists
        build_dir = os.path.abspath(self.build_temp)
        os.makedirs(build_dir, exist_ok=True)

        # Get the source directory (where CMakeLists.txt is)
        source_dir = os.path.dirname(os.path.abspath(__file__))

        # Print debugging information
        print(f"Source directory: {source_dir}")
        print(f"Build directory: {build_dir}")
        print(f"Extension path: {self.get_ext_fullpath(ext.name)}")

        # Configure CMake
        cmake_args = [
            "cmake",
            source_dir,
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={build_dir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
        ]
        
        # Configure
        subprocess.check_call(cmake_args, cwd=build_dir)
        
        # Build
        subprocess.check_call(["cmake", "--build", "."], cwd=build_dir)

        # Get the extension suffix for this platform
        ext_suffix = sysconfig.get_config_var('EXT_SUFFIX')
        if not ext_suffix:
            ext_suffix = '.so'

        # Look for the built file
        so_file = os.path.join(build_dir, f"{ext.name}{ext_suffix}")
        if not os.path.exists(so_file):
            # Try alternative locations and names
            alt_paths = [
                os.path.join(build_dir, f"{ext.name}.cpython-*-darwin.so"),  # macOS pattern
                os.path.join(build_dir, f"lib{ext.name}{ext_suffix}"),
                os.path.join(build_dir, "Debug", f"{ext.name}{ext_suffix}"),
                os.path.join(build_dir, "Release", f"{ext.name}{ext_suffix}"),
            ]
            
            # Use glob to find files matching patterns
            import glob
            found_files = []
            for pattern in alt_paths:
                found_files.extend(glob.glob(pattern))
            
            if found_files:
                so_file = found_files[0]  # Use the first match
            else:
                raise RuntimeError(
                    f"Built extension file not found. Expected: {so_file}\n"
                    f"Searched patterns: {alt_paths}\n"
                    f"Directory contents: {os.listdir(build_dir)}"
                )

        self.copy_file(so_file, self.get_ext_fullpath(ext.name))

ext_modules = [
    Extension(
        name="eqcalc",
        sources=["src/eqcalc.cc"],
        include_dirs=["src"],
        language="c++"
    )
]

setup(
    name="eqcalc",
    version="0.1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": CMakeBuildExt},
    packages=["eqcalc"],
    include_package_data=True,
)