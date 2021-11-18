import pytest
import os
import stat
from fly import Fly
from fly.app import Method

@pytest.fixture(scope="function", autouse=False)
def fly_init():
    __f = Fly()
    yield __f

def test_fly_init():
    Fly()

@pytest.mark.parametrize(
    "path", [
    200,
    200.0,
    True,
    {},
    [],
    b"fly/conf"
])
def test_fly_init_path_error(path):
    with pytest.raises(
        TypeError
    ):
        Fly(config_path=path)

@pytest.mark.parametrize(
    "debug", [
    200,
    200.0,
    {},
    [],
    b"fly/conf"
])
def test_fly_debug_error(debug):
    with pytest.raises(
        TypeError
    ):
        Fly(debug=debug)

TEST_PATH="fly"
def test_fly_init_path_value_error():
    with pytest.raises(
        ValueError
    ):
        Fly(config_path=TEST_PATH)

TEST_READ_PATH="test_read.conf"
def test_fly_iniit_path_perm_error():
    open(TEST_READ_PATH, "w")
    os.chmod(path=TEST_READ_PATH, mode=stat.S_IWRITE)
    if not os.access(TEST_READ_PATH, os.R_OK):
        with pytest.raises(
            ValueError
        ):
            Fly(config_path=TEST_READ_PATH)
    else:
        Fly(config_path=TEST_READ_PATH)

    os.remove(TEST_READ_PATH)

def test_fly_singleton():
    a = Fly()
    b = Fly()
    c = Fly()
    d = Fly()

    assert(a == b == c == d)

TEST_MOUNT = "__fly_test_mount_dir"
@pytest.fixture(scope="function", autouse=False)
def fly_mount(fly_init):
    os.makedirs(TEST_MOUNT, exist_ok=True)

    yield fly_init

    os.rmdir(TEST_MOUNT)

def test_mount(fly_mount):
    fly_mount.mount(TEST_MOUNT)

def test_mount_value_error(fly_mount):
    with pytest.raises(
        ValueError
    ):
        fly_mount.mount(TEST_MOUNT + "test")

def test_route(fly_init):
    func = fly_init.route("/", Method.GET)
    assert(callable(func))

@pytest.mark.parametrize(
    "route", [
    b"route",
    1,
    1.0,
    [],
    {},
])
def test_route_type_error(fly_init, route):
    with pytest.raises(TypeError):
        fly_init.route(route, Method.GET)

@pytest.mark.parametrize(
    "func", [
    "func",
    b"func",
    1,
    1.0,
    [],
    {},
])
def test_route_func_error(fly_init, func):
    _route = fly_init.route("/", Method.GET)
    with pytest.raises(TypeError):
        _route(func)

def hello():
    return "Hello"

def test_route(fly_init):
    _route = fly_init.route("/", Method.GET)
    _route(hello)

def test_route_method(fly_init):
    func = fly_init.route("/", Method.GET)
    assert(callable(func))

def test_get(fly_init):
    func = fly_init.get("/")
    assert(callable(func))

def test_post(fly_init):
    func = fly_init.post("/")
    assert(callable(func))

def test_head(fly_init):
    func = fly_init.head("/")
    assert(callable(func))

def test_options(fly_init):
    func = fly_init.options("/")
    assert(callable(func))

def test_put(fly_init):
    func = fly_init.put("/")
    assert(callable(func))

def test_delete(fly_init):
    func = fly_init.delete("/")
    assert(callable(func))

def test_connect(fly_init):
    func = fly_init.connect("/")
    assert(callable(func))

def test_trace(fly_init):
    func = fly_init.trace("/")
    assert(callable(func))

def test_patch(fly_init):
    func = fly_init.patch("/")
    assert(callable(func))
