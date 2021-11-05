from enum import Enum

class FlyMethod(Enum):
    GET = "GET"
    POST = "POST"


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
