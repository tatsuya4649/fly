#from _fly_route import _fly_route
from .method import method_from_name, Method
from ._base import _BaseRoute
import sys


class Route():
    def __init__(self):
        self._routes = list()

    @property
    def routes(self):
        return self._routes

    def register_route(self, uri, func, method, debug=True):
        if not isinstance(uri, str):
            raise TypeError(
                "uri must be str type."
            )
        if not isinstance(method, (str, Method)):
            raise TypeError(
                "mwethod must be str type."
            )
        if not callable(func) and func is not None:
            raise TypeError(
                "func must be callable or None."
            )
        if not isinstance(debug, bool):
            raise TypeError(
                "debug must be bool type."
            )

        method_str = method if isinstance(method, str) else method.value

        _base = _BaseRoute(func, debug)
        _rd = dict()
        _rd.setdefault("uri", uri)
        _rd.setdefault("func", _base.handler)
        _rd.setdefault("method", method_str)

        self._routes.append(_rd)

    def _change_route(self, uri, method, func):
        if not isinstance(uri, str):
            raise TypeError(
                "uri must be str type."
            )
        if not isinstance(method, (str, Method)):
            raise TypeError(
                "mwethod must be str type."
            )
        if not callable(func) and func is not None:
            raise TypeError(
                "func must be callable or None."
            )

        method_str = method if isinstance(method, str) else method.value

        for route in self._routes:
            if route.get("method") is None or \
                    route.get("uri") is None or \
                    route.get("func") is None:
                raise ValueError(f"invalid route {route}")

            if route["method"] == method_str and \
                    route["uri"] == uri:
                route["func"] = func
