from enum import Enum

class SameSite(Enum):
    Strict="Strict"
    Lax="Lax"
    _None="None"

_COOKIE_GAP = "; "
def header_value_from_cookie(
    name,
    value,
    max_age=None,
    secure=False,
    http_only=False,
    domain=None,
    path=None,
    samesite=None,
):
    if not isinstance(name, str):
        raise TypeError("name and value must be str")

    res = str()
    res += f"{name}={str(value)}"

    if max_age:
        if not isinstance(max_age, int):
            raise TypeError("max_age must be int type.")
        res += _COOKIE_GAP
        res += f"Max-Age={max_age}"

    if secure:
        if not isinstance(secure, bool):
            raise TypeError("secure must be bool type.")
        res += _COOKIE_GAP
        res += "Secure"

    if http_only:
        if not isinstance(http_only, bool):
            raise TypeError("http_only must be bool type.")
        res += _COOKIE_GAP
        res += "HttpOnly"

    if domain:
        if not isinstance(domain, str):
            raise TypeError("domain must be str type.")
        res += _COOKIE_GAP
        res += f"Domain={domain}"

    if path:
        if not isinstance(path, str) or len(path) == 0:
            raise TypeError("path must be str type.")
        res += _COOKIE_GAP
        res += f"Path={path}"

    if samesite:
        if not isinstance(samesite, SameSite):
            raise TypeError("samesite must be SameSite type.")
        res += _COOKIE_GAP
        res += f"SameSite={samesite.value}"

    return res
