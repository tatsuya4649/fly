from ._fly_server import _fly_response

class __metaresponse(type):
    REQUIRED_ATTR = [
        "status_code",
        "header",
        "body",
        "content_type",
    ]

    def __new__(cls, name, bases, attributes):
        for i in cls.REQUIRED_ATTR:
            for __b in bases:
                if not hasattr(__b, i):
                    raise NotImplementedError(
                        f"{cls} must be implemented \"{i}\""
                    )
        return type.__new__(cls, name, bases, attributes)


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


class Response(_Response, metaclass=__metaresponse):
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
        self._status_code = status_code
        self._content_type = content_type
        self._header = list()
        if header is not None:
            if not isinstance(header, list):
                raise TypeError("header must be list type.")

            for i in header:
                if not isinstance(i, dict):
                    raise TypeError("header element must be dict type.")
                if i.get("name") is None or i.get("value") is None:
                    raise ValueError("header element must have \"name\" key and \"value\" key")

        if body is not None:
            if not isinstance(body, bytes):
                raise TypeError("body must be bytes type.")
            self._body = body
        else:
            self._body = bytes()


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
        self._header.add(hdr_elem)

class PlainResponse(Response):
    def __init__(
        self,
        status_code=200,
        header=None,
        body=None,
    ):
        if not isinstance(body, str):
            raise TypeError("body must be str type.")
        super().__init__(
            status_code,
            header,
            body.encode("utf-8"),
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
        if body is not None and not isinstance(body, str):
            raise TypeError("body must be str type.")
        super().__init__(
            status_code,
            header,
            body.encode("utf-8") if body is not None else None,
            content_type="application/json"
        )
