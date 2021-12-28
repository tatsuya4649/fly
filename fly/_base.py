import traceback
from .exceptions import *
from .response import Response
from . import types
from .types import *
from ._parse import RequestParser
import sys


class _BaseRoute:
    def __init__(self, handler, debug=True, print_request=False):
        if handler is None:
            raise ValueError("handler must not be None.")
        if not callable(handler):
            raise ValueError("handler must be callable.")

        self._handler = handler
        self._debug = debug
        self._print_request = debug and print_request
        self._parser = RequestParser(handler)
        if hasattr(handler, "default_headers"):
            if not isinstance(handler.default_headers, list):
                raise TypeError("default_headers of handler must be list type.")
            self._default_headers = handler.default_headers
        else:
            self._default_headers = list()
        self._only_debug_default_headers = list()

    @property
    def is_debug(self):
        return self._debug

    def _parse_func_args(self, request):
        _res = self._parser.parse_func_args(request)
        _args = _res["args"]
        _kwargs = _res["kwargs"]
        return self._handler(*_args, **_kwargs)

    def handler(self, request):
        try:
            if self._print_request:
                print(request)
            res = self._parse_func_args(request)
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
            if len(self.default_headers) > 0:
                if not isinstance(res, Response):
                    res = Response(
                            status_code=200,
                            body=res if res is None or isinstance(res, bytes) \
                                    else res.encode("utf-8"),
                            )
            for i in self.default_headers:
                for j in res.header:
                    # Already, response have default header item
                    # No override
                    if j["name"].lower() == i["name"].lower():
                        break
                else:
                    res.add_header(
                            name=i["name"],
                            value=i["value"]
                            )
                    print(res.header)
            return res

    @property
    def print_request(self):
        return self._print_request

    @print_request.setter
    def print_request(self, value):

        self._print_request = value

    def remove_default_headers_with_debug(self):
        for i in self._only_debug_default_headers:
            for j in self._default_headers:
                if i == j["name"]:
                    self._default_headers.remove(j)

    @property
    def default_headers(self):
        return self._default_headers

    @property
    def only_debug_default_headers(self):
        return self._only_debug_default_headers
