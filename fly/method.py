from enum import Enum

class FlyMethod(Enum):
    GET         = "GET"
    POST        = "POST"
    HEAD        = "HEAD"
    OPTIONS     = "OPTIONS"
    PUT         = "PUT"
    DELETE      = "DELETE"
    CONNECT     = "CONNECT"
    TRACE       = "TRACE"
    PATCH       = "PATCH"

def method_from_name(method):
    if isinstance(method, FlyMethod):
        return method
    if not isinstance(method, str):
        raise TypeError(
            "method must be str or FlyMethod type."
        )
    for x in FlyMethod:
        if x.value == method:
            return x
    raise ValueError(
        "invalud method."
    )
