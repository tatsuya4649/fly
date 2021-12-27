from .response import Response
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


