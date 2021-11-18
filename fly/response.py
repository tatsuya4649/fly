from ._fly_server import _fly_response
import json
from cookie import *
from exceptions import *


class _Response(_fly_response):

    @property
    def status_code(self):
        raise NotImplementedError(
            "_Response must have status_code attr"
        )

    @property
    def header(self):
        raise NotImplementedError(
            "_Response must have header attr"
        )

    @property
    def header_len(self):
        raise NotImplementedError(
            "_Response must have header attr"
        )

    @property
    def body(self):
        raise NotImplementedError(
            "_Response must have body attr"
        )

    @property
    def content_type(self):
        raise NotImplementedError(
            "_Response must have content_type attr"
        )


class Response(_Response):
    """
    All Response subclass must have 5 attributes.

    - status_code: default 200
    - header: default: []
    - body: default: bytes()
    - content_type: default: text/plain

    """
    def __init__(
        self,
        status_code=200,
        header=None,
        body=None,
        content_type="text/plain",
    ):
        if not isinstance(status_code, int):
            raise TypeError("status_code must be int type.")
        if header is not None and not isinstance(header, (list)):
            raise TypeError("status_code must be list type.")
        if not isinstance(content_type, str):
            raise TypeError("content_type must be str type.")
        if body is not None and not isinstance(body, (bytes)):
            raise TypeError("body must be bytes type.")

        self._status_code = status_code
        self._content_type = content_type
        self._header = list()
        self._body = body if body is not None else bytes()

    @property
    def status_code(self):
        return self._status_code

    @property
    def header(self):
        return self._header

    @property
    def header_len(self):
        return len(self._header)

    @property
    def body(self):
        return self._body

    @property
    def content_type(self):
        return self._content_type

    def add_header(self, name, value):
        hdr_elem = dict()
        hdr_elem["name"] = name
        hdr_elem["value"] = value
        self._header.append(hdr_elem)

    def set_cookie(
        self,
        name,
        value,
        **kwards
    ):
        hdr_elem = dict()
        hdr_elem["name"] = "Set-Cookie"
        value = header_value_from_cookie(name, value, **kwards)
        hdr_elem["value"] = value
        self._header.append(hdr_elem)

class PlainResponse(Response):
    def __init__(
        self,
        status_code=200,
        header=None,
        body=None,
    ):
        if body is not None and not isinstance(body, str):
            raise TypeError("body must be str type.")

        super().__init__(
            status_code,
            header,
            body.encode("utf-8") if isinstance(body, str) else None,
            content_type="text/plain"
        )

class HTMLResponse(Response):
    def __init__(
        self,
        status_code=200,
        header=None,
        body=None,
    ):
        if body is not None and not isinstance(body, str):
            raise TypeError("body must be str type.")

        super().__init__(
            status_code,
            header,
            body.encode("utf-8") if body is not None else None,
            content_type="text/html"
        )

class JSONResponse(Response):
    def __init__(
        self,
        status_code=200,
        header=None,
        body=None,
    ):
        if body is not None and not isinstance(body, dict) and \
                not isinstance(body, list):
            raise TypeError("body must be list/dict type.")

        print(json.dumps(body))
        super().__init__(
            status_code,
            header,
            json.dumps(body).encode("utf-8") \
                    if body is not None and len(body) > 0 \
                    else None,
            content_type="application/json"
        )
