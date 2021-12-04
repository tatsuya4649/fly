import pytest
import httpx
import os
import shutil
from conftest import *

_HOST="localhost"
_PORT="1234"
_HTTP="http"
_INOF="test_inotify_file.txt"
_INOD="ino/test_inotify_file.txt"
_TEST_MOUNT_POINT="./tests/mnt"
_INOF_PATH=f"{_TEST_MOUNT_POINT}/{_INOF}"
_INOD_PATH=f"{_TEST_MOUNT_POINT}/{_INOD}"

@pytest.fixture(scope="function", autouse=False)
def inotify_file_remove():
    if os.path.isfile(_INOF_PATH):
        os.remove(_INOF_PATH)
    yield
    if os.path.isfile(_INOF_PATH):
        os.remove(_INOF_PATH)

@pytest.fixture(scope="function", autouse=False)
def inotify_dir_remove():
    if os.path.isdir(os.path.dirname(_INOD_PATH)):
        shutil.rmtree(os.path.dirname(_INOD_PATH))
    yield
    if os.path.isdir(os.path.dirname(_INOD_PATH)):
        shutil.rmtree(os.path.dirname(_INOD_PATH))

@pytest.mark.asyncio
async def test_create(inotify_file_remove, fly_servers):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)

    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)

@pytest.mark.asyncio
async def test_delete(inotify_file_remove, fly_servers):
    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)
    assert(os.path.isfile(_INOF_PATH))

    os.remove(_INOF_PATH)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)

@pytest.mark.asyncio
async def test_move(inotify_file_remove, fly_servers):
    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)
    assert(os.path.isfile(_INOF_PATH))

    # move file different directory
    shutil.move(_INOF_PATH, '../' + _INOF)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)

    # move to mount directory 
    shutil.move('../' + _INOF, _INOF_PATH)


@pytest.mark.asyncio
async def test_create_directory(inotify_dir_remove, fly_servers):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)

    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)

