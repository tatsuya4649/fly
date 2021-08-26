from setuptools import setup, Extension
import subprocess

"""
make fly library
"""
subprocess.run("make lib", shell=True);
module = Extension(
	"fly_route",
	sources = ["pyroute.c"],
	library_dirs = ["./lib"],
	libraries = ["fly"],
	runtime_library_dirs = ["./lib"],
)

setup(
	name="fly",
	version="1.0",
	description="tiny web/app server with C/Python",
	ext_modules = [
		module
	],
)
