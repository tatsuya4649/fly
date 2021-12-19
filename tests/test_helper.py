from fly.helper import *
from fly.response import Response

def test_redicret():
    res = redirect("/")

    assert isinstance(res, Response)

    headers = res.header
    for _hi in headers:
        if _hi["name"] == "location":
            break
    else:
        assert 0
