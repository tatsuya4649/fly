import pytest
from fly import FlyRoute
from fly.method import FlyMethod

@pytest.fixture(scope="function", autouse=False)
def __pyroute_init():
    __route = FlyRoute()
    yield __route

def test_init():
    FlyRoute()

def test_routes(__pyroute_init):
    assert(isinstance(__pyroute_init.routes, list))

def hello(): ...

def test_register(__pyroute_init):
    __pyroute_init.register_route("/", hello, "get")

def test_register_method(__pyroute_init):
    __pyroute_init.register_route("/", hello, FlyMethod.GET)

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
        __pyroute_init.register_route(uri, hello, FlyMethod.GET)

@pytest.mark.parametrize(
    "method", [
    None,
    10,
    10.0,
    True,
    [FlyMethod.GET],
    {"method", FlyMethod.GET},
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
        __pyroute_init.register_route("/", func, FlyMethod.GET)
