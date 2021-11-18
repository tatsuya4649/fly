
class HTTPException(Exception):
    _status=500

    def __init__(
        self,
        err_content = None,
        headers = None
    ):
        self._err_content = err_content
        if headers is not None:
            self._headers = headers
        else:
            self._headers = list()

    def __str__(self):
        return self._err_content \
                if self._err_content is not None else ""

    @property
    def status_code(self):
        return self._status

    @property
    def headers(self):
        return self._headers

# 4xx Error (Client error)
class HTTP400Exception(HTTPException):
    _status=400
class HTTP401Exception(HTTPException):
    _status=401
class HTTP403Exception(HTTPException):
    _status=403
class HTTP404Exception(HTTPException):
    _status=404
class HTTP405Exception(HTTPException):
    _status=405
class HTTP406Exception(HTTPException):
    _status=406
class HTTP407Exception(HTTPException):
    _status=407
class HTTP408Exception(HTTPException):
    _status=408
class HTTP409Exception(HTTPException):
    _status=409
class HTTP410Exception(HTTPException):
    _status=410
class HTTP411Exception(HTTPException):
    _status=411
class HTTP412Exception(HTTPException):
    _status=412
class HTTP413Exception(HTTPException):
    _status=413
class HTTP414Exception(HTTPException):
    _status=414
class HTTP415Exception(HTTPException):
    _status=415
class HTTP416Exception(HTTPException):
    _status=416
class HTTP417Exception(HTTPException):
    _status=417
class HTTP418Exception(HTTPException):
    _status=418
class HTTP421Exception(HTTPException):
    _status=421
class HTTP422Exception(HTTPException):
    _status=422
class HTTP423Exception(HTTPException):
    _status=423
class HTTP424Exception(HTTPException):
    _status=424
class HTTP425Exception(HTTPException):
    _status=425
class HTTP426Exception(HTTPException):
    _status=426
class HTTP428Exception(HTTPException):
    _status=428
class HTTP429Exception(HTTPException):
    _status=429
class HTTP431Exception(HTTPException):
    _status=431
class HTTP451Exception(HTTPException):
    _status=451

# 5xx Error (Server error)
class HTTP500Exception(HTTPException):
    _status=500
class HTTP501Exception(HTTPException):
    _status=501
class HTTP502Exception(HTTPException):
    _status=502
class HTTP503Exception(HTTPException):
    _status=503
class HTTP504Exception(HTTPException):
    _status=504
class HTTP505Exception(HTTPException):
    _status=505
class HTTP506Exception(HTTPException):
    _status=506
class HTTP507Exception(HTTPException):
    _status=507
class HTTP508Exception(HTTPException):
    _status=508
class HTTP510Exception(HTTPException):
    _status=510
class HTTP511Exception(HTTPException):
    _status=511

