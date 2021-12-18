import sys
import re
from .method import method_from_name, Method
from ._base import _BaseRoute


class Route():
    URI_RE_SYNTAX_PATTERN       = r"^/(.*/?)*$"
    URI_RE_SUB_REPEAT_PATTERN   = r"(/{2,})"
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

        # Check URI syntax
        res = re.match(self.URI_RE_SYNTAX_PATTERN, uri)
        if res is None:
            raise ValueError(
                f"URI syntax error. ({uri})"
            )
        uri = res.group(0)
        # Check URI repeat slash
        uri = re.sub(self.URI_RE_SUB_REPEAT_PATTERN, '/', uri)

        method_str = method if isinstance(method, str) else method.value

        _base = _BaseRoute(func, debug)
        _rd = dict()
        _rd.setdefault("uri", uri)
        _rd.setdefault("func", _base.handler)
        _rd.setdefault("method", method_str)

        for _r in self._routes:
            if _r["method"] == method_str and _r["uri"] == uri:
                raise ValueError("Already registered same route({_r['uri']}, {_r['method']}).")
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
