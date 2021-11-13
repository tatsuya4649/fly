import pytest
from fly.route import Route
from fly.method import Method

@pytest.fixture(scope="function", autouse=False)
def __pyroute_init():
    __route = Route()
    yield __route

def test_init():
    Route()

def test_routes(__pyroute_init):
    assert(isinstance(__pyroute_init.routes, list))

def hello(): ...

def test_register(__pyroute_init):
    __pyroute_init.register_route("/", hello, "GET")

def test_register_method(__pyroute_init):
    __pyroute_init.register_route("/", hello, Method.GET)

@pytest.mark.parametrize(
    "uri", [
    10,
    10.0,
    ["uri"],
    {"uri": "/"},
    None,
])
def test_register_uri_type_error(__pyroute_init, uri):
    with pytest.raises(
        TypeError
    ):
        __pyroute_init.register_route(uri, hello, Method.GET)

@pytest.mark.parametrize(
    "method", [
    None,
    10,
    10.0,
    True,
    [Method.GET],
    {"method", Method.GET},
])
def test_register_method_type_error(__pyroute_init, method):
    with pytest.raises(
        TypeError
    ):
        __pyroute_init.register_route("/", hello, method)

@pytest.mark.parametrize(
    "func", [
    10,
    19.0,
    "function",
    [hello],
    {"func": hello},
])
def test_register_func_type_error(__pyroute_init, func):
    with pytest.raises(
        TypeError
    ):
        __pyroute_init.register_route("/", func, Method.GET)
