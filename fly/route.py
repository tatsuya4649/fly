
from fly_route import _fly_route


class FlyRoute(_fly_route):
	def __init__(self):
		super().__init__()
		self._routes = list()
		
	def route(self, *args):
		super().route(*args)

		_rd = dict()
		_rd.setdefault("uri", args[0]) 
		_rd.setdefault("func", args[1])
		_rd.setdefault("method", args[2])

		self._routes.append(_rd)
