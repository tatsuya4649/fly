from fs import FlyFs
from _fly_server import _fly_server
from route import FlyRoute
from env import FlyEnv

class Fly(_fly_server):
	def __init__(
		self,
		fs,
		port=None,
		host=None,
		root=None,
		workers=None,
	):
		self._env = FlyEnv()
		self._env.port = port
		self._env.host = port
		self._port = self._env.port
		self._host = self._env.host

		super().__init__(
			host=self._env.host,
			port=self._env.port,
			ip_v=self._env.ip_v,
		)
		self._fs = fs
		self._route = FlyRoute()
		self._root = root
		self._workers = workers
	
	def run(self):
		pass
	
if __name__ == "__main__":
	fs = FlyFs()
	fly = Fly(
		fs=FlyFs(),
	)
	fly.run()
