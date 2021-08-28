import os
import ipaddress
import socket

class FlyEnv:

	FLY_HOST_ENV = "FLY_HOST"
	FLY_PORT_ENV = "FLY_PORT"
	FLY_ROOT_ENV = "FLY_ROOT"
	FLY_WORKERS_ENV = "FLY_WORKERS"

	_FLY_DEFAULT_PORT = 2222
	_FLY_DEFAULT_HOST = "127.0.0.1"
	_FLY_DEFAULT_ROOT = "."
	_FLY_DEFAULT_WORKERS = 1

	def __init__(self):
		self._envdict = dict()
		self._envdict.setdefault("host", os.getenv(FlyEnv.FLY_HOST_ENV))
		self._envdict.setdefault("port", os.getenv(FlyEnv.FLY_PORT_ENV))
		self._envdict.setdefault("root", os.getenv(FlyEnv.FLY_ROOT_ENV))
		self._envdict.setdefault("workers", os.getenv(FlyEnv.FLY_WORKERS_ENV))
	
	@property
	def host(self):
		return self._envdict["host"]  if "host" in self._envdict and self._envdict["host"] else FlyEnv._FLY_DEFAULT_HOST

	@host.setter
	def host(self, value):
		if not isinstance(value, str) and value is not None:
			raise TypeError(
				"host must be str type or None."
			)
		if value is None:
			self._envdict["host"] = value
			return
		else:
			try:
				hostname = socket.gethostbyname(value)
				self._envdict["host"] = hostname
			except socket.gaierror:
				raise ValueError(
					f"hostname is invalid value. ({value})"
				)
	
	@property
	def ip_v(self):
		return ipaddress.ip_address(self.host).version

	@property
	def port(self):
		port_str = self._envdict["port"] if "port" in self._envdict and self._envdict["port"] else FlyEnv._FLY_DEFAULT_PORT
		return int(port_str)

	@port.setter
	def port(self, value):
		if not isinstance(value, int) and value is not None:
			raise TypeError(
				"port must be int type or None."
			)
		self._envdict["port"] = value


	@property
	def root(self):
		_root = self._envdict["root"] if "root" in self._envdict and self._envdict["root"] else FlyEnv._FLY_DEFAULT_ROOT
		if not os.path.isdir(_root):
			raise ValueError("root path is invalid value.")
		return os.path.abspath(_root)

	@root.setter
	def root(self, value):
		if not isinstance(value, str) and value is not None:
			raise TypeError(
				"root must be str type or None."
			)
		self._envdict["root"] = value
	
	@property
	def workers(self):
		return self._envdict["workers"] if "workers" in self._envdict and self._envdict["workers"] else FlyEnv._FLY_DEFAULT_WORKERS

	@workers.setter
	def workers(self, value):
		if not isinstance(value, int) and value is not None:
			raise TypeError(
				"workers must be str type or None."
			)
		self._envdict["workers"] = value
