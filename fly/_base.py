import traceback
import inspect
from .exceptions import *
from .response import Response
from . import types
from .types import *
import sys

"""

fly path params type:

    int     : 0
    float   : 1
    bool    : 2
    str     : 3

ex.
    'path_params': [{'param': 'user_id', 'type': 0, 'value': '10'}]
                                                ~~~          ~~~~
                                                int type    int value

"""
INT         = 0
FLOAT       = 1
BOOL        = 2
STR         = 3

class _BaseRoute:
    def __init__(self, handler, debug=True):
        if handler is None:
            raise ValueError("handler must not be None.")
        if not callable(handler):
            raise ValueError("handler must be callable.")

        self._handler = handler
        self._debug = debug

        self._func_spec = inspect.getfullargspec(handler)
        self._func_args = self._func_spec.args
        self._func_arg_count = len(self._func_spec.args)
        self._func_annotations = self._func_spec.annotations
        print(self._func_annotations)

    @property
    def is_debug(self):
        return self._debug

    def _parse_query(self, query_string):
        _qlist = list()
        for i in query_string.split("&"):
            _equal_split = iter(i.split("="))
            _sqdict = dict()
            _sqdict["name"] = next(_equal_split, None)
            _sqdict["value"] = next(_equal_split, None)
            _qlist.append(_sqdict)
        return _qlist

    def _get_query_params(self, name, _query_params):
        if _query_params is None:
            return None

        for i in self._parse_query(_query_params):
            if self._convert_to_lower(i["name"]) == name:
                return i["value"]
        return None

    def _get_path_params(self, name, _path_params):
        if _path_params is None:
            return None

        for _p in _path_params:
            _p_param = self._convert_to_lower(_p["param"])
            if name != _p_param:
                continue
            _p_value = _p["value"]
            _p_type = _p["type"]
            if _p_type == INT:
                return int(_p_value)
            elif _p_type == FLOAT:
                return float(_p_value)
            elif _p_type == BOOL:
                return bool(_p_value)
            else:
                return str(_p_value)
        return None

    def _parse_cookie(self, _cookie):
        if _cookie is None:
            return []

        res = list()
        for i in _cookie:
            for item in i.split(';'):
                _c = iter(item.split('='))
                name  = next(_c, None)
                value = next(_c, None)
                res.append({
                    'name': name.strip() if name is not None else None,
                    'value': value.strip() if value is not None else None
                })
        return res

    def _get_cookie(self, name, _cookie):
        for i in _cookie:
            if i["name"] == name:
                return i["value"]
        return None

    """
    Convert lower:

        'User-agent' => 'user_agent'
    """
    def _convert_to_lower(self, header_name):
        _lower = header_name.lower()
        return _lower.replace('-', '_')

    def _check_with_annotation(self, request, key):
        print(request)
        _type = self._func_annotations.get(key)
        if _type is Request:
            if key == "request":
                return request
            else:
                return request.get(key)

        elif _type is Header:
            _header = request.get("header")
            if _header is None:
                return None

            for _hi in _header:
                if self._convert_to_lower(_hi["name"]) == key:
                    return _hi["value"]

            return None
        elif _type is Body:
            return request.get("body")

        elif _type is Cookie:
            _cookie = request.get("body")
            if _cookie is None:
                return None

            for _ci in _cookie:
                if self._convert_to_lower(_ci["name"]) == key:
                    return _ci["value"]

            return None

        elif _type is Path:
            _pp = request.get("path_params")
            return self._get_path_params(key, _pp)

        elif _type is Query:
            _q = request.get("query")
            return self._get_query_params(key, _q)
        else:
            _vclass = list()
            for name, obj in inspect.getmembers(types):
                print(name)
                if inspect.isclass(obj):
                    _vclass.append(name)

            raise TypeError(f"""
    Invalid type \"{_type.__name__}\".

    Valid class is {','.join(_vclass)}
            """
            )

    def parse_func_args(self, request):
        _keys = request.keys()
        _ankeys = self._func_annotations.keys()
        _path_params = request.get("path_params")
        _query_params = request.get("query")

        request["cookie"] = self._parse_cookie(request.get("cookie"))
        _cookie_params = request.get("cookie")
        _args = list()
        _kwargs = dict()
        for i in self._func_args:
            # Check annotation
            # ex. def index(header: Header):
            if i in _ankeys:
                _args.append(self._check_with_annotation(request, i))
                continue

            # Check without annotation
            # ex. def index(header):
            if i not in _keys and i != "request":
                if _path_params is None:
                    raise KeyError(f"Invalid key error. No such key(\"{i}\") in request dictionary.")
                # Check path parameter
                _res = self._get_path_params(i, _path_params)
                if _res is not None:
                    _args.append(_res)
                    continue

                # Check query parameter
                _res = self._get_query_params(i, _query_params)
                if _res is not None:
                    _args.append(_res)
                    continue

                # Check Cookie
                _res = self._get_cookie(i, _cookie_params)
                if _res is not None:
                    _args.append(_res)
                    continue
                raise KeyError(f"Invalid key error. No such key(\"{i}\") in request dictionary.")

            if i == "request":
                _args.append(request)
            else:
                _args.append(request.get(i))
                request.pop(i)

        # function have such as **kwargs argument
        if self._func_spec.varkw is not None:
            _kwargs = request

        return self._handler(*_args, **_kwargs)

    def handler(self, request):
        try:
            res = self.parse_func_args(request)
            if self.is_debug:
                if isinstance(res, Response):
                    if res.header is not None and   \
                            not isinstance(res.header, list):
                        raise TypeError(
                            "header of response must be list type."
                        )
                    if res.body is not None and  \
                            not isinstance(res.body, bytes):
                        raise TypeError(
                            "body of response must be bytes type."
                        )
                else:
                    if res is not None and \
                            not isinstance(res, (str, bytes)):
                        raise TypeError(
                            "response must be Response or None or str or bytes type."
                        )

        except HTTPException as e:
            res = Response(
                status_code=e.status_code,
                body=str(e).encode("utf-8") if len(str(e)) > 0 else None
            )

            for item in e.headers:
                res.add_header(
                    name=item["name"],
                    value=item["value"]
                )
        except Exception as e:
            if self.is_debug:
                res_body = traceback.format_exc().encode("utf-8")
            else:
                res_body = None

            res = Response(
                status_code=500,
                body=res_body,
            )
        finally:
            return res
