
from fly_route import _fly_route


class FlyRoute(_fly_route):
	def __init__(self):
		super().__init__()
		self._routes = list()
	
	@property
	def routes(self):
		return self._routes

	def register_route(self, uri, func, method):
		super().register_route(*(uri, method))

		_rd = dict()
		_rd.setdefault("uri", uri) 
		_rd.setdefault("func", func)
		_rd.setdefault("method", method)

		self._routes.append(_rd)
	
if __name__ == "__main__":
	def hello():
		print("Hello World")

	r = FlyRoute()
	r.register_route("/", hello, "get")

	print(r.routes)
