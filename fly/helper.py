from .response import Response, JSONResponse, PlainResponse, HTMLResponse
from .types import Request

"""

    Redirect function:
    @params location(str):  Specify the redirect destination URL
    @code(int):             Select redirect status code(3xx)
    @body(str or bytes):    Response message

"""
def redirect(location, status_code=301, body=None):
    if status_code < 300 or status_code >= 400:
        raise ValueError("Status code of redirect must be 3xx.")
    if not isinstance(location, str):
        raise TypeError("location must be str type.")
    if body is not None and not isinstance(body, (str, bytes)):
        raise TypeError("body must be str or byte type in redirect.")

    body_bytes = body.encode("utf-8") if isinstance(body, str) else body
    _response = Response(
        status_code=status_code,
        header=None,
        body=body_bytes,
    )
    _response.add_header("location", location)
    return _response

"""
    Response function.
"""
def response(
        status_code=200,
        header=None,
        body=None,
        content_type="text/plain",
        ):
    res = Response(
            status_code=status_code,
            header=header,
            body=body,
            content_type=content_type,
            )
    return res

"""

    REsponse TextPlain function.
"""
def plain_response(
        status_code=200,
        header=None,
        body=None,
        ):
    res = PlainResponse(
            status_code=status_code,
            header=header,
            body=body,
            )
    return res

"""
    Response HTML function.
"""
def html_response(
        status_code=200,
        header=None,
        body=None,
        ):
    res = HTMLResponse(
            status_code=status_code,
            header=header,
            body=body,
            )
    return res

"""

    Response JSON function.
"""
def json_response(
        status_code=200,
        header=None,
        body=None,
        ):
    res = JSONResponse(
            status_code=status_code,
            header=header,
            body=body,
            )
    return res
