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

def test_json_response():
    res = json_response()

    assert isinstance(res, JSONResponse)

def test_plain_response():
    res = plain_response()

    assert isinstance(res, PlainResponse)

def test_html_response():
    res = html_response()

    assert isinstance(res, HTMLResponse)
