from setuptools import setup, Extension
import subprocess
import os
import sys
import re
from glob import glob

def __run(command_name):
    if not isinstance(command_name, list):
        raise TypeError("must be list type.")

    res = subprocess.call(command_name, shell=False)
    if res != 0:
        raise RuntimeError("failure to make libfly.so")

def version_from_init():
    with open("./fly/__init__.py", "r") as f:
        lines = f.readlines()
        for line in lines:
            result = re.match('\s*__version__\s*=\s*"(\d+.\d+.\d+)"', line)
            if result is not None:
                return result.group(1)
    raise RuntimeError("not found version in fly/__init__.py")

def get_packages(package):
    return [
        dirpath
        for dirpath, dirnames, filenames in os.walk(package)
        if os.path.exists(os.path.join(dirpath, "__init__.py"))
    ]

macros = []
extra_compile_args = []
if os.getenv("DEBUG") is not None:
    print("DEBUG MODE")
    macros.append(("DEBUG", "fly"))
    extra_compile_args.append("-g3")
    extra_compile_args.append("-O0")
else:
    """
    make fly library
    """
    __run(["./configure"])
    __run(["make"])
    __run(["make", "install"])
    extra_compile_args.append("-O3")

server = Extension(
	name="fly._fly_server",
	sources=["src/pyserver.c"],
    language='c',
	libraries=["fly"],
	library_dirs=["fly/lib"],
	runtime_library_dirs=["lib"],
    extra_compile_args = extra_compile_args,
    define_macros = macros,
)

setup(
	name="fly_server",
	version=version_from_init(),
	description="lightweight web framework",
	ext_modules = [
		server,
	],
    packages = get_packages("fly"),
    include_package_data=True,
    package_data = {
        "fly": glob('fly/lib/*'),
    },
    install_requires = [
        "click>=7.1.0",
        "jinja2>=3.0.0",
    ],
    entry_points="""
    [console_scripts]
    fly=fly.main:fly_command_line
    """,
    license_files = ('LICENSE'),
)
