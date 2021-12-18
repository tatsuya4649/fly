import pytest
from fly.route import Route
from fly.method import Method

@pytest.fixture(scope="function", autouse=False)
def _pyroute_init():
    _route = Route()
    yield _route

def test_init():
    Route()

def test_routes(_pyroute_init):
    assert(isinstance(_pyroute_init.routes, list))

def hello(): ...

def test_register(_pyroute_init):
    _pyroute_init.register_route("/", hello, "GET")

def test_register_method(_pyroute_init):
    _pyroute_init.register_route("/", hello, Method.GET)

@pytest.mark.parametrize(
    "uri", [
    10,
    10.0,
    ["uri"],
    {"uri": "/"},
    None,
])
def test_register_uri_type_error(_pyroute_init, uri):
    with pytest.raises(
        TypeError
    ):
        _pyroute_init.register_route(uri, hello, Method.GET)

@pytest.mark.parametrize(
    "method", [
    None,
    10,
    10.0,
    True,
    [Method.GET],
    {"method", Method.GET},
])
def test_register_method_type_error(_pyroute_init, method):
    with pytest.raises(
        TypeError
    ):
        _pyroute_init.register_route("/", hello, method)

@pytest.mark.parametrize(
    "func", [
    10,
    19.0,
    "function",
    [hello],
    {"func": hello},
])
def test_register_func_type_error(_pyroute_init, func):
    with pytest.raises(
        TypeError
    ):
        _pyroute_init.register_route("/", func, Method.GET)


def test_uri_syntax(_pyroute_init):
    TEST_URI="/test/uri/syntax"
    _pyroute_init.register_route(TEST_URI, hello, Method.GET)

def test_uri_syntax_error(_pyroute_init):
    TEST_URI="test/uri/syntax"
    with pytest.raises(ValueError):
        _pyroute_init.register_route(TEST_URI, hello, Method.GET)

def test_uri_repeat_syntax(_pyroute_init):
    TEST_URI="////test//uri/syntax////"
    _pyroute_init.register_route(TEST_URI, hello, Method.GET)

    routes = _pyroute_init.routes
    assert routes[-1]["uri"] == "/test/uri/syntax/"
