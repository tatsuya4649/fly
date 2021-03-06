import sys
import re
from .method import method_from_name, Method
from ._base import _BaseRoute
from .cors import *


class Route():
    URI_RE_SYNTAX_PATTERN       = r"^/(.*/?)*$"
    URI_RE_SUB_REPEAT_PATTERN   = r"(/{2,})"
    def __init__(self):
        self._routes = list()
        self._debug_route = list()

    @property
    def routes(self):
        return self._routes

    def register_route(
            self,
            uri,
            func,
            method,
            debug=True,
            print_request=False,
            **kwargs,
            ):
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

        _base = _BaseRoute(func, debug, print_request)
        _rd = dict()
        _rd.setdefault("uri", uri)
        _rd.setdefault("func", _base.handler)
        _rd.setdefault("orig_func", func)
        _rd.setdefault("method", method_str)
        _rd.setdefault("base", _base)
        if kwargs.get("debug_route") and kwargs.get("debug_route") is True:
            _rd.setdefault("debug_route", True)
        if kwargs.get("only_debug") and kwargs.get("only_debug") is True:
            _rd.setdefault("only_debug", True)

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

    def _print_request_routes(self, print_request=False):
        if not print_request:
            return

        for i in self._routes:
            if i.get("base") is None or not isinstance(i.get("base"), _BaseRoute):
                raise RuntimeError("Not found base key in route.")
            _base = i.get("base")
            _base.print_request = True

    """
    If now in production mode, remove route that is debug_route from routes.
    """
    def _production_routes(self, debug=True):
        if debug:
            return

        product_routes = list()
        for i in self.routes:
            if i.get("debug_route") and i.get("debug_route") is True:
                continue
            else:
                product_routes.append(i)

        self._routes = product_routes

    def remove_only_debug_headers(self, debug=True):
        if debug:
            return

        for i in self.routes:
            if i.get("only_debug") is not None and \
                    i.get("only_debug"):
                self.routes.remove(i)
            if i.get("base") is None:
                raise ValueError("must have base key")

            _base = i.get("base")
            _base.remove_default_headers_with_debug()
