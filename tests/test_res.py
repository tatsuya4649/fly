import pytest
from fly.response import *

def test_response_init():
    Response()

@pytest.mark.parametrize(
    "status_code", [
    "200",
    [100],
    {"status_code": 200},
    200.0,
])
def test_response_status_code_type_error(status_code):
    with pytest.raises(
        TypeError
    ):
        assert(Response(status_code=status_code));

@pytest.mark.parametrize(
    "header", [
    "header",
    100,
    {"header": []},
    200.0,
])
def test_response_header_type_error(header):
    with pytest.raises(
        TypeError
    ):
        assert(Response(header=header));

@pytest.mark.parametrize(
    "body", [
    "body",
    100,
    [],
    {"body": []},
    200.0,
])
def test_response_body_type_error(body):
    with pytest.raises(
        TypeError
    ):
        assert(Response(body=body));

@pytest.mark.parametrize(
    "content_type", [
    b"text/plain",
    100,
    [],
    {"content_type": "text/plain"},
    200.0,
])
def test_response_body_type_error(content_type):
    with pytest.raises(
        TypeError
    ):
        assert(Response(content_type=content_type));

def test_plain_response():
    pr = PlainResponse()
    assert(pr.content_type == "text/plain")

def test_html_response():
    hr = HTMLResponse()
    assert(hr.content_type == "text/html")

def test_json_response():
    jr = JSONResponse()
    assert(jr.content_type == "application/json")

def test_json_body():
    response = {
        "response": 1,
    }
    jr = JSONResponse(body=response)

def test_json_body_error():
    response = "res"
    with pytest.raises(
        TypeError
    ):
        JSONResponse(body=response)
