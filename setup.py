from setuptools import setup, Extension

module = Extension(
	"fly_route",
	sources = ["pyroute.c"]
)

setup(
	name="fly",
	version="1.0",
	description="tiny web/app server with C/Python",
	ext_modules = [
		module
	],
)
