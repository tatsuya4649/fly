from _fly_route import _fly_route
from .method import FlyMethod, method_from_name


class FlyRoute(_fly_route):
    def __init__(self):
        super().__init__()
        self._routes = list()

    @property
    def routes(self):
        return self._routes

    def register_route(self, uri, func, method):
        if not isinstance(uri, str):
            raise TypeError(
                "uri must be str type."
            )
        if not isinstance(method, (str, FlyMethod)):
            raise TypeError(
                "mwethod must be str type."
            )
        if not callable(func) and func is not None:
            raise TypeError(
                "func must be callable or None."
            )

        method_str = method if isinstance(method, str) else method.value
        super().register_route(*(uri, method_str))

        _rd = dict()
        _rd.setdefault("uri", uri)
        _rd.setdefault("func", func)
        _rd.setdefault("method", method_from_name(method))

        self._routes.append(_rd)
