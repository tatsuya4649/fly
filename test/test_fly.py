import pytest
from fly import Fly
from fly.fly import FlyMethod

@pytest.fixture(scope="function", autouse=False)
def __fly_init():
    __fly = Fly(
        host="localhost",
        port=2223,
        root=".",
        workers=3,
    )
    yield __fly

def test_fly_init():
    Fly(
        host="localhost",
        port=2222,
        root=".",
        workers=3,
    )

def test_fly_init_root_error():
    with pytest.raises(
        ValueError
    ) as raiseinfo:
        Fly(
            host="localhost",
            port=2222,
            root="...",
            workers=3,
        )

@pytest.mark.parametrize(
    "path", [
    10,
    10.0,
    True,
    ["route"],
    {"route": "route"},
])
def test_fly_route_path_error(__fly_init, path):
    with pytest.raises(
        TypeError
    ):
        assert(callable(__fly_init.route(path, FlyMethod.GET)))

def test_fly_route_method_error(__fly_init):
    with pytest.raises(
        TypeError
    ):
        __fly_init.route("/user", "get")

def test_fly_get(__fly_init):
    assert(callable(__fly_init.get("/")))

def test_fly_post(__fly_init):
    assert(callable(__fly_init.post("/")))
