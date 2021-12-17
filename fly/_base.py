import traceback
import inspect
from .exceptions import *
from .response import Response
from . import types
from .types import *
import sys
from urllib.parse import unquote_plus, unquote
import pydantic
import re


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

    @property
    def is_debug(self):
        return self._debug

    """
    Convert lower:

        'User-agent' => 'user_agent'
    """
    def _convert_to_lower(self, header_name):
        _lower = header_name.lower()
        return _lower.replace('-', '_')


    def _parse_query(self, query_string):
        _qlist = list()
        for i in query_string.split("&"):
            _equal_split = iter(i.split("="))
            _sqdict = dict()
            _sqdict["name"] = next(_equal_split, None)
            _sqdict["value"] = next(_equal_split, None)
            _sqdict["name"] = unquote(_sqdict["name"]) if _sqdict["name"] is not None else None
            _sqdict["value"] = unquote(_sqdict["value"]) if _sqdict["value"] is not None else None
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
                try:
                    return int(_p_value)
                except Exception:
                    raise HTTP404Exception
            elif _p_type == FLOAT:
                try:
                    return float(_p_value)
                except Exception:
                    raise HTTP404Exception
            elif _p_type == BOOL:
                try:
                    return bool(_p_value)
                except Exception:
                    raise HTTP404Exception
            else:
                try:
                    return str(_p_value)
                except Exception:
                    raise HTTP404Exception
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

    def _parse_multipart_content_name(self, value):
        pattern = r'^\s*form-data;\s*name\s*=\s*(?:"|\')(.*?)(?:"|\')\s*(?:;?|;\s*filename\s*=\s*(?:"|\')(.*?)(?:"|\'))\s*$'
        _res = re.match(pattern, value)
        if _res is None:
            return None
        else:
            try:
                return _res.group(1)
            except Exception:
                raise HTTP400Exception

    def _parse_multipart_form_data(self, body):
        _res = dict()
        for _bi in body:
            _header = _bi["header"]
            for _hi in _header:
                if _hi["name"].lower() == "content-disposition":
                    _hv = _hi["value"]
                    _name = self._parse_multipart_content_name(_hv)
                    if _name is None:
                        raise HTTP400Exception

                    _res[_name] = _bi["content"]
        return _res

    def _parse_application_x_www_form_urlencoded(self, body):
        _parse = unquote_plus(body.decode("utf-8"))
        _res = dict()
        try:
            for item in _parse.split("&"):
                i = iter(item.split('='))
                name  = next(i, None)
                value = next(i, None)
                _res[name.strip()] = value.strip() \
                        if value is not None else None
        except Exception as e:
            raise HTTP400Exception
        return _res

    def _parse_data_form(self, body, content_type):
        if body is None:
            raise HTTP400Exception

        _ct = content_type.split(';')[0]
        if _ct == "multipart/form-data":
            return self._parse_multipart_form_data(body)
        elif _ct == "application/x-www-form-urlencoded":
            return self._parse_application_x_www_form_urlencoded(body)

        # Unknown content-type in POST method(Form data)
        # If you want other method, should use hint of Body.
        raise HTTP415Exception

    def _get_cookie(self, name, _cookie):
        if _cookie is None:
            return None

        for i in _cookie:
            if self._convert_to_lower(i["name"]) == name:
                return i["value"]
        return None

    def _get_header(self, name, _header):
        if _header is None:
            return None

        for i in _header:
            if self._convert_to_lower(i["name"]) == name:
                return i["value"]
        return None

    def _get_content_type(self, header):
        if header is None:
            return None

        for i in header:
            if i["name"].lower() == "content-type":
                return i["value"]

        return None

    def _get_data_form(self, _type, form_data):
        if form_data is None:
            return None
        try:
            return _type(**form_data)
        except pydantic.error_wrappers.ValidationError:
            raise HTTP400Exception
        except Exception:
            raise HTTP500Exception

    def _check_with_annotation(self, request, key):
        _type = self._func_annotations.get(key)
        if _type is Request:
            if key == "request":
                return request
            else:
                return request.get(key)

        elif _type is Header:
            _header = request.get("header")
            return self._get_header(key, _header)

        elif _type is Body:
            return request.get("body")

        elif _type is Cookie:
            _cookie = request.get("cookie")
            return self._get_cookie(key, _cookie)

        elif _type is Path:
            _pp = request.get("path_params")
            return self._get_path_params(key, _pp)

        elif _type is Query:
            _q = request.get("query")
            return self._get_query_params(key, _q)

        elif issubclass(_type, FormData):
            if "header" not in request.keys():
                raise HTTP400Exception

            _content_type = self._get_content_type(request["header"])
            if _content_type is None:
                # Unsupported Media Type (Unknown content-type)
                raise HTTP415Exception

            _body = request.get("body")
            _res = self._parse_data_form(_body, _content_type)
            return self._get_data_form(_type, _res)
        else:
            _vclass = list()
            for name in types.__all__:
                _vclass.append(name)

            raise TypeError(f"Invalid type \"{_type.__name__}\". Valid class is {','.join(_vclass)}"
            )

    def parse_func_args(self, request):
        _keys = request.keys()
        _ankeys = self._func_annotations.keys()
        _path_params = request.get("path_params")
        _query_params = request.get("query")
        _header = request.get("header")

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
                # 1. Check path parameter
                _res = self._get_path_params(i, _path_params)
                if _res is not None:
                    _args.append(_res)
                    continue

                # 2. Check query parameter
                _res = self._get_query_params(i, _query_params)
                if _res is not None:
                    _args.append(_res)
                    continue

                # 3. Check Cookie
                _res = self._get_cookie(i, _cookie_params)
                if _res is not None:
                    _args.append(_res)
                    continue

                # 4. Check Header
                _res = self._get_header(i, _header)
                if _res is not None:
                    _args.append(_res)
                    continue

                raise KeyError( f"Invalid key error. No such key(\"{i}\") in request dictionary. If hint of class are given, output None to \"{i}\" argument.")

            if i == "request":
                _args.append(request)
            else:
                _args.append(request.get(i))
                request.pop(i)

        # function have such as **kwargs argument
        if self._func_spec.varkw is not None:
            for key in self._func_args:
                if key in request.keys():
                    request.pop(key, None)
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
