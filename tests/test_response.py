import pytest
from fly.response import _Response, Response, PlainResponse, HTMLResponse

class Error_Response(_Response):
    pass

er = Error_Response()

def test_response_status_code_error():
    with pytest.raises(NotImplementedError):
        er.status_code

def test_response_header_error():
    with pytest.raises(NotImplementedError):
        er.header

def test_response_header_len_error():
    with pytest.raises(NotImplementedError):
        er.header_len

def test_response_body_error():
    with pytest.raises(NotImplementedError):
        er.body

def test_response_content_tye_error():
    with pytest.raises(NotImplementedError):
        er.content_type

@pytest.fixture(scope="function")
def fly_response_init():
    res = Response()
    yield res

@pytest.mark.parametrize(
    "body", [
    "body",
    1,
    1.0,
    [],
    {},
])
def test_response_init_body_type_error(body):
    with pytest.raises(TypeError):
        Response(
            body = body
        )

@pytest.mark.parametrize(
    "header", [
    "header",
    b"header",
    1,
    1.0,
    {},
])
def test_response_init_header_type_error(header):
    with pytest.raises(TypeError):
        Response(
            header = header
        )

def test_response_status_code(fly_response_init):
    assert(isinstance(fly_response_init.status_code, int))

def test_response_header(fly_response_init):
    assert(isinstance(fly_response_init.header, list))

def test_response_header_len(fly_response_init):
    assert(isinstance(fly_response_init.header_len, int))

def test_response_body(fly_response_init):
    assert(isinstance(fly_response_init.body, bytes))

def test_response_add_header(fly_response_init):
    fly_response_init.add_header("Hello", "World")

@pytest.mark.parametrize(
    "body", [
    1,
    1.0,
    b"body",
    [],
    {},
])
def test_plainresponse_init_type_error(body):
    with pytest.raises(TypeError):
        PlainResponse(
            body = body
        )

@pytest.mark.parametrize(
    "body", [
    1,
    1.0,
    b"body",
    [],
    {},
])
def test_htmlresponse_init_type_error(body):
    with pytest.raises(TypeError):
        HTMLResponse(
            body = body
        )
