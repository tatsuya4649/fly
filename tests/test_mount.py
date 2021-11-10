import pytest
from fly.mount import FlyMount

@pytest.fixture(scope="function", autouse=False)
def __pyfly_mount():
    __mnt = FlyMount()
    yield __mnt

def test_init():
    FlyMount()

def test_mounts(__pyfly_mount):
    assert(isinstance(__pyfly_mount.mounts, list))

def test_mounts_count(__pyfly_mount):
    assert(__pyfly_mount.mounts_count == 0)

@pytest.mark.parametrize(
    "path", [
    10,
    10.0,
    ["path"],
    {"path": "."}
])
def test_mount_type_error(__pyfly_mount, path):
    with pytest.raises(
        TypeError
    ):
        __pyfly_mount.mount(path)

def test_mount_value_error(__pyfly_mount):
    with pytest.raises(
        ValueError
    ):
        __pyfly_mount.mount(".....")

def test_mount(__pyfly_mount):
    assert(__pyfly_mount.mount(".") is None)
    assert(len(__pyfly_mount.mounts) == 1)
