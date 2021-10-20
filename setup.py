from setuptools import setup, Extension
import subprocess

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
server = Extension(
	name="_fly_server",
	sources=["pyserver.c"],
	library_dirs=["./lib"],
	libraries=["fly"],
	runtime_library_dirs=["./lib"],
)
signal = Extension(
	name="_fly_signal",
	sources=["pysignal.c"],
	library_dirs=["./lib"],
	libraries=["fly"],
	runtime_library_dirs=["./lib"],
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
