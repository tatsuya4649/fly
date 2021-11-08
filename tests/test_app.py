import pytest
import os
import stat
from fly import Fly
from fly.app import FlyMethod

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
def test_fly_inti(path):
    with pytest.raises(
        TypeError
    ):
        Fly(config_path=path)

TEST_PATH="fly"
def test_fly_inti():
    with pytest.raises(
        ValueError
    ):
        Fly(config_path=TEST_PATH)

TEST_READ_PATH="test_read.conf"
def test_fly_inti():
    open(TEST_READ_PATH, "w")
    os.chmod(path=TEST_READ_PATH, mode=stat.S_IWRITE)
    with pytest.raises(
        ValueError
    ):
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
    os.mkdir(TEST_MOUNT)

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
    func = fly_init.route("/", "GET")
    assert(callable(func))

def test_route_method(fly_init):
    func = fly_init.route("/", FlyMethod.GET)
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
