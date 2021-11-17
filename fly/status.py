
class HTTPResponse(Exception):
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
        return self._err_content if self._err_content else ""

    @property
    def status_code(self):
        return self._status

    @property
    def headers(self):
        return self._headers

# 4xx Error (Client error)
class HTTP400Response(HTTPResponse):
    _status=400
class HTTP401Response(HTTPResponse):
    _status=401
class HTTP403Response(HTTPResponse):
    _status=403
class HTTP404Response(HTTPResponse):
    _status=404
class HTTP405Response(HTTPResponse):
    _status=405
class HTTP406Response(HTTPResponse):
    _status=406
class HTTP407Response(HTTPResponse):
    _status=407
class HTTP408Response(HTTPResponse):
    _status=408
class HTTP409Response(HTTPResponse):
    _status=409
class HTTP410Response(HTTPResponse):
    _status=410
class HTTP411Response(HTTPResponse):
    _status=411
class HTTP412Response(HTTPResponse):
    _status=412
class HTTP413Response(HTTPResponse):
    _status=413
class HTTP414Response(HTTPResponse):
    _status=414
class HTTP415Response(HTTPResponse):
    _status=415
class HTTP416Response(HTTPResponse):
    _status=416
class HTTP417Response(HTTPResponse):
    _status=417
class HTTP418Response(HTTPResponse):
    _status=418
class HTTP421Response(HTTPResponse):
    _status=421
class HTTP422Response(HTTPResponse):
    _status=422
class HTTP423Response(HTTPResponse):
    _status=423
class HTTP424Response(HTTPResponse):
    _status=424
class HTTP425Response(HTTPResponse):
    _status=425
class HTTP426Response(HTTPResponse):
    _status=426
class HTTP428Response(HTTPResponse):
    _status=428
class HTTP429Response(HTTPResponse):
    _status=429
class HTTP431Response(HTTPResponse):
    _status=431
class HTTP451Response(HTTPResponse):
    _status=451

# 5xx Error (Server error)
class HTTP500Response(HTTPResponse):
    _status=500
class HTTP501Response(HTTPResponse):
    _status=501
class HTTP502Response(HTTPResponse):
    _status=502
class HTTP503Response(HTTPResponse):
    _status=503
class HTTP504Response(HTTPResponse):
    _status=504
class HTTP505Response(HTTPResponse):
    _status=505
class HTTP506Response(HTTPResponse):
    _status=506
class HTTP507Response(HTTPResponse):
    _status=507
class HTTP508Response(HTTPResponse):
    _status=508
class HTTP510Response(HTTPResponse):
    _status=510
class HTTP511Response(HTTPResponse):
    _status=511

