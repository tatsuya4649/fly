import pytest
from fly import FlyFs

@pytest.fixture(scope="function", autouse=False)
def __pyfly_fs():
    __fs = FlyFs()
    yield __fs

def test_fs_init():
    FlyFs()

def test_fs_mounts(__pyfly_fs):
    assert(isinstance(__pyfly_fs.mounts, list))

@pytest.mark.parametrize(
    "path", [
    10,
    10.0,
    ["path"],
    {"path": "."}
])
def test_fs_mount_type_error(__pyfly_fs, path):
    with pytest.raises(
        TypeError
    ):
        __pyfly_fs.mount(path)

def test_fs_mount_value_error(__pyfly_fs):
    with pytest.raises(
        ValueError
    ):
        __pyfly_fs.mount(".....")

def test_fs_mount(__pyfly_fs):
    assert(__pyfly_fs.mount(".") is None)
    assert(len(__pyfly_fs.mounts) == 1)
