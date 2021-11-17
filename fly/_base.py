import traceback
from .status import *
from .response import Response
import sys


class _BaseRoute:
    def __init__(self, handler):
        if handler is None:
            raise ValueError("handler must not be None.")
        if not callable(handler):
            raise ValueError("handler must be callable.")

        self._handler = handler

    def handler(self, request):
        try:
            return self._handler(request)
        except HTTPResponse as e:
            res = Response(
                status_code=e.status_code,
                body=str(e).encode("utf-8")
            )

            for item in e.headers:
                res.add_header(
                    name=item["name"],
                    value=item["value"]
                )

            return res
        except Exception as e:
            # TODO: change debug and production mode
            res_body = traceback.format_exc().encode("utf-8")
            res = Response(
                status_code=500,
                body=res_body,
            )
            return res
