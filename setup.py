from setuptools import setup, Extension
import subprocess
import os

"""
make fly library
"""
subprocess.run("make lib", shell=True);
#route = Extension(
#	name="_fly_route",
#	sources=["pyroute.c"],
#	library_dirs=["./lib"],
#	libraries=["fly"],
#	runtime_library_dirs=["./lib"],
#)
macros = []
extra_compile_args = []
if os.getenv("DEBUG") is not None:
    macros.append(("DEBUG", "fly"))
    extra_compile_args.append("-g3")
    extra_compile_args.append("-O0")

server = Extension(
	name="_fly_server",
	sources=["pyserver.c"],
	library_dirs=["./lib"],
	libraries=["fly"],
	runtime_library_dirs=["./lib"],
    extra_compile_args = extra_compile_args,
    define_macros = macros,
)
signal = Extension(
	name="_fly_signal",
	sources=["pysignal.c"],
	library_dirs=["./lib"],
	libraries=["fly"],
	runtime_library_dirs=["./lib"],
    extra_compile_args = ["-g", "-O0"],
)

setup(
	name="fly",
	version="1.0",
	description="tiny web/app server with C/Python",
	ext_modules = [
#		route,
		server,
		signal,
	],
)
