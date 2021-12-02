from setuptools import setup, Extension
import subprocess
import os
import sys
import re
import platform
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
            result = re.match('\s*__version__\s*=\s*"(\d+.\d+.\d+(.\d+)*)"', line)
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

_OS = platform.system()
_libfly_dir = list()
if _OS == 'Darwin':
    _libfly_dir.append(
        os.path.join(
            os.getcwd(),
            "fly/lib"
    ))
    # -I header of openSSL
    # OpenSSL
    if os.path.isdir("/usr/local/opt/openssl/include"):
        _ssldir = "/usr/local/opt/openssl/include"
    elif os.path.isdir("/usr/local/opt/openssl@3/include"):
        _ssldir = "/usr/local/opt/openssl@3/include"
    elif os.path.isdir("/usr/local/opt/openssl@1.1/include"):
        _ssldir = "/usr/local/opt/openssl@1.1/include"
    else:
        raise RuntimeError("not found openssl on your system.")
    # Zlib
    if os.path.isdir("/usr/local/opt/zlib/include"):
        _zdir = "/usr/local/opt/zlib/include"
    else:
        raise RuntimeError("not found zlib on your system.")
    #Brotli
    if os.path.isdir("/usr/local/opt/brotli/include"):
        _brodir = "/usr/local/opt/brotli/include"
        extra_compile_args.append(f"-I {_brodir}")

    extra_compile_args.append(f"-I {_ssldir}")
    extra_compile_args.append(f"-I {_zdir}")

server = Extension(
	name="fly._fly_server",
	sources=["src/pyserver.c"],
    language='c',
	libraries=["fly"],
	library_dirs=[f"{ os.path.abspath(os.path.dirname(__file__)) }/fly/lib"],
    runtime_library_dirs=_libfly_dir,
    extra_compile_args = extra_compile_args,
    define_macros = macros,
)

setup(
	name="fly-server",
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
    tests_require = [
        "pytest >= 6",
        "pytest-cov >= 3.0.0",
        "pytest-asyncio >= 0.16.0",
        "httpx >= 0.20.0",
        "httpx[http2]",
    ],
    entry_points="""
    [console_scripts]
    fly=fly.main:fly_command_line
    """,
    license_files = ('LICENSE'),
)
