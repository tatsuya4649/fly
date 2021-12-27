from .method import Method
from .types import Request
from .response import Response


ACCESS_CONTROL_ALLOW_ORIGIN="Access-Control-Allow-Origin"
ACCESS_CONTROL_ALLOW_METHODS="Access-Control-Allow-Methods"
ACCESS_CONTROL_ALLOW_HEADERS="Access-Control-Allow-Headers"
ACCESS_CONTROL_EXPOSE_HEADERS="Access-Control-Expose-Headers"
ACCESS_CONTROL_ALLOW_CREDENTIALS="Access-Control-Allow-Credentials"
ACCESS_CONTROL_MAX_AGE="Access-Control-Max-Age"

def CORS(
        app,
        allow_origin="*",
        allow_methods=[
            "GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "PATCH"
            ],
        allow_headers=[],
        allow_credentials=True,
        expose_headers=[],
        max_age=None,
        only_debug=False,
        ):
    app.default_cors = _Cors(
        allow_origin=allow_origin,
        allow_methods=allow_methods,
        allow_headers=allow_headers,
        allow_credentials=allow_credentials,
        expose_headers=expose_headers,
        max_age=max_age,
        only_debug=only_debug,
            )

"""

    Allow CORS function:
"""
def _option(
        func,
        allow_origin,
        allow_methods=[],
        allow_headers=[],
        expose_headers=[],
        allow_credentials=False,
        max_age=None,
        only_debug=True,
        ):

    def _option_for_allow_cors(request: Request):
        res = Response(
                status_code=200,
                )
        res.add_header(ACCESS_CONTROL_ALLOW_ORIGIN, allow_origin)
        if len(allow_methods) > 0:
            res.add_header(ACCESS_CONTROL_ALLOW_METHODS, ','.join(allow_methods))
        if len(allow_headers) > 0:
            res.add_header(ACCESS_CONTROL_ALLOW_HEADERS, ','.join(allow_headers))
        if len(expose_headers) > 0:
            res.add_header(ACCESS_CONTROL_EXPOSE_HEADERS, ','.join(expose_headers))
        if allow_credentials:
            res.add_header(ACCESS_CONTROL_ALLOW_CREDENTIALS, "true")
        if max_age is not None:
            res.add_header(ACCESS_CONTROL_MAX_AGE, str(max_age))
        return res

    _option_for_allow_cors.__name__ = f"{func.__name__} (for CORS)"
    return _option_for_allow_cors

def all_allow_cors(func):
    app = func._application
    route = func.route
    _route = app.options(
            path=route["uri"],
            debug_route=route["debug_route"],
            only_debug=False,
            )
    allow_origin = "*"
    allow_methods=["GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "PATCH"]
    allow_headers = []
    expose_headers = []
    allow_credentials = True
    max_age = None
    _option_handler = _option(
            func=func,
            allow_origin=allow_origin,
            allow_methods=allow_methods,
            allow_headers=allow_headers,
            expose_headers=expose_headers,
            allow_credentials=allow_credentials,
            max_age=max_age,
            )
    _route(_option_handler)

    _base = None
    for i in app.routes:
        if i["uri"] == route["uri"] and \
                i["method"] == (route["method"].value if isinstance(route["method"], Method) else route["method"]):
                    _base = i["base"]
    _base._default_headers.append({
        "name": ACCESS_CONTROL_ALLOW_ORIGIN,
        "value": allow_origin,
    })
    _base._only_debug_default_headers.append(ACCESS_CONTROL_ALLOW_ORIGIN)
    if len(allow_methods) > 0:
        _base._default_headers.append({
            "name": ACCESS_CONTROL_ALLOW_METHODS,
            "value": ','.join(allow_methods),
        })
    if len(allow_headers) > 0:
        _base._default_headers.append({
            "name": ACCESS_CONTROL_ALLOW_HEADERS,
            "value": ','.join(allow_headers),
        })
    if len(expose_headers) > 0:
        _base._default_headers.append({
            "name": ACCESS_CONTROL_EXPOSE_HEADERS,
            "value": ','.join(expose_headers),
        })
    if allow_credentials:
        _base._default_headers.append({
            "name": ACCESS_CONTROL_ALLOW_CREDENTIALS,
            "value": 'true',
        })
    return func

def allow_cors(
        allow_origin="*",
        allow_methods=[],
        allow_headers=[],
        expose_headers=[],
        allow_credentials=False,
        max_age=None,
        only_debug=True,
        ):
    if not isinstance(allow_methods, list):
        raise TypeError("allow_methods must be list type.")
    if not isinstance(allow_headers, list):
        raise TypeError("allow_headers must be list type.")
    if not isinstance(expose_headers, list):
        raise TypeError("expose_headers must be list type.")
    if not isinstance(allow_credentials, bool):
        raise TypeError("allow_credentials must be bool type.")
    if max_age is not None and not isinstance(max_age, int):
        raise TypeError("max_age must be int type.")

    _sm = list()
    for i in allow_methods:
        if not isinstance(i, (str, Method)):
            raise TypeError("allow_methods item must be str or Method type.")

        _sm.append(i if isinstance(i, str) else i.value)
    allow_methods = _sm
    for i in allow_headers:
        if not isinstance(i, str):
            raise TypeError("allow_headers item must be str type.")
    for i in expose_headers:
        if not isinstance(i, str):
            raise TypeError("expose_headers item must be str type.")
    def _allow_cors(func):
        if not hasattr(func, "_application") or \
                not hasattr(func, "route"):
                    raise ValueError(f"""

    function({func.__name__}) is invalid error. allow_cors decorator shuld be set above route(get, post, etc...) decorator.

ex.

    @allow_cors(allow_origin="http://localhost:8080")
    @app.get("/")
    def index():
        return None
                    """)
        app = func._application
        if only_debug and not app.is_debug:
            return func

        route = func.route
        _route = app.options(
                path=route["uri"],
                debug_route=route["debug_route"],
                only_debug=only_debug,
                )
        _option_handler = _option(
                func=func,
                allow_origin=allow_origin,
                allow_methods=allow_methods,
                allow_headers=allow_headers,
                expose_headers=expose_headers,
                allow_credentials=allow_credentials,
                max_age=max_age,
                )
        _route(_option_handler)

        _base = None
        for i in app.routes:
            if i["uri"] == route["uri"] and \
                    i["method"] == (route["method"].value \
                    if isinstance(route["method"], Method) else route["method"]):
                        _base = i["base"]
        _base._default_headers.append({
            "name": ACCESS_CONTROL_ALLOW_ORIGIN,
            "value": allow_origin,
        })
        _base._only_debug_default_headers.append(ACCESS_CONTROL_ALLOW_ORIGIN)
        if len(allow_methods) > 0:
            _base._default_headers.append({
                "name": ACCESS_CONTROL_ALLOW_METHODS,
                "value": ','.join(allow_methods),
            })
            if only_debug:
                _base._only_debug_default_headers.append(ACCESS_CONTROL_ALLOW_METHODS)
        if len(allow_headers) > 0:
            _base._default_headers.append({
                "name": ACCESS_CONTROL_ALLOW_HEADERS,
                "value": ','.join(allow_headers),
            })
            if only_debug:
                _base._only_debug_default_headers.append(ACCESS_CONTROL_ALLOW_HEADERS)
        if len(expose_headers) > 0:
            _base._default_headers.append({
                "name": ACCESS_CONTROL_EXPOSE_HEADERS,
                "value": ','.join(expose_headers),
            })
            if only_debug:
                _base._only_debug_default_headers.append(ACCESS_CONTROL_EXPOSE_HEADERS)
        if allow_credentials:
            _base._default_headers.append({
                "name": ACCESS_CONTROL_ALLOW_CREDENTIALS,
                "value": 'true',
            })
            if only_debug:
                _base._only_debug_default_headers.append(ACCESS_CONTROL_ALLOW_CREDENTIALS)
        return func
    return _allow_cors


class _Cors:
    def __init__(
        self,
        allow_origin="*",
        allow_methods=[
            "GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "PATCH"
            ],
        allow_headers=[],
        allow_credentials=True,
        expose_headers=[],
        max_age=None,
        only_debug=False,
            ):
        self._allow_origin = allow_origin
        self._allow_methods = allow_methods
        self._allow_headers = allow_headers
        self._allow_credentials = allow_credentials
        self._expose_headers = expose_headers
        self._only_debug = only_debug
        self._max_age = max_age

    def _apply_route(self, app, route):
        _base = route["base"]
        _debug_route = True if route.get("debug_route") else False
        _route = app.options(
                path=route["uri"],
                debug_route=_debug_route,
                only_debug=self._only_debug,
                )
        _func = route["orig_func"]
        _option_handler = _option(
                func=_func,
                allow_origin=self._allow_origin,
                allow_methods=self._allow_methods,
                allow_headers=self._allow_headers,
                expose_headers=self._expose_headers,
                allow_credentials=self._allow_credentials,
                max_age=self._max_age,
                )
        _route(_option_handler)

    def apply_route(self, app, routes):
        if not isinstance(routes, list):
            raise TypeError("routes must be list type.")

        for _route in routes:
            _uri = _route["uri"]
            if _route["method"] == Method.OPTIONS or \
                    _route["method"] == "OPTIONS":
                        continue
            for j in routes:
                if j["uri"] == _uri:
                    if j["method"] == Method.OPTIONS or \
                            j["method"] == "OPTIONS":
                                break
            else:
                self._apply_route(app, _route)
            continue
