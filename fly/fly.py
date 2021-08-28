import sys
import os
from enum import Enum
from fs import FlyFs
from _fly_server import _fly_server
from signal import FlySignal
from route import FlyRoute
from env import FlyEnv

class FlyMethod(Enum):
	GET = "get"
	POST = "post"

class Fly(FlySignal, FlyFs, _fly_server):
	def __init__(
		self,
		port=None,
		host=None,
		root=".",
		workers=None,
	):
		FlySignal.__init__(self)
		FlyFs.__init__(self)
		self._env = FlyEnv()
		self._env.port = port
		self._env.host = port
		self._env.workers = workers
		self._port = self._env.port
		self._host = self._env.host
		self._workers = self._env.workers

		# socket make, bind, listen
		_fly_server.__init__(
			self,
			host=self._env.host,
			port=self._env.port,
			ip_v=self._env.ip_v,
			mounts=self.mounts,
		)
		self._route = FlyRoute()
		self._root = os.path.abspath(root)
		if not os.path.isdir(self._root):
			raise ValueError(
				f"\"{self._root}\" not found. root must be directory."
			)
	
	def route(self, path, method):
		if not isinstance(path, str):
			raise TypeError(
				"path must be str type."
			)
		if not isinstance(method, FlyMethod):
			raise TypeError(
				"method must be FlyMethod."
			)
		def __route(func):
			self._route.register_route(
				uri=path,
				func=func,
				method=method.value
			)
			return func
		return __route

	def get(self, path):
		return self.route(path, FlyMethod.GET)

	def post(self, path):
		return self.route(path, FlyMethod.POST)
	
	def run(self):
		print("\n", file=sys.stderr)
		print(f"    \033[1m*\033[0m Fly Running on \033[1m{self._host}:{self._port}\033[0m (Press CTRL+C to quit)", file=sys.stderr)
		print(f"    \033[1m*\033[0m Fly \033[1m{self._workers}\033[0m workers", file=sys.stderr)
		print(f"    \033[1m*\033[0m Root path ({self._root})", file=sys.stderr)
		print(f"    \033[1m*\033[0m Mount paths ({','.join(self.mounts)})", file=sys.stderr)
		print("\n", file=sys.stderr)
		super().run(self._route)
	
if __name__ == "__main__":
	print(Fly.__mro__)
	fly = Fly()
	@fly.get("/")
	def a():
		print("a")
	
	fly.run()
