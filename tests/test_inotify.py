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
_INOD2_PATH=f"{_TEST_MOUNT_POINT}/ino"

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

@pytest.mark.asyncio
async def test_delete_directory(inotify_dir_remove, fly_servers):
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))

    print("~~~~~ CREATE DIRECTORY ~~~~~")

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)

    # delete directory
    shutil.rmtree(os.path.dirname(_INOD_PATH))

    print("~~~~~ DELETE DIRECTORY ~~~~~")
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)

@pytest.mark.asyncio
async def test_move_directory(inotify_dir_remove, fly_servers):
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)

    # move directory
    shutil.move(_INOD_PATH, '../' + _INOF)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)

    # move to mount directory
    shutil.move('../' + _INOF, _INOD_PATH)

@pytest.mark.asyncio
async def test_move_directory2(inotify_dir_remove, fly_servers):
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)

    # move directory
    shutil.move(_INOD2_PATH, '../ino')

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)

    # move to mount directory
    shutil.move('../ino', _INOD2_PATH)


@pytest.mark.asyncio
async def test_unmount_dir(inotify_dir_remove, fly_servers):
    assert(os.path.isdir("tests/mnt2"))
    if not os.path.isfile("tests/mnt2/hello"):
        with open("tests/mnt2/hello", "w") as f:
            f.write("Hello test!")

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/hello")
    assert(res.status_code == 200)

    # move mount point
    shutil.move("tests/mnt2", '../mnt2')

    await asyncio.sleep(1)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/hello")
    assert(res.status_code == 404)

    # move to mount directory
    shutil.move("../mnt2", 'tests/mnt2')

_INODIR = "tests/mnt2/inod"
_INODIR_FILE = "tests/mnt2/inod/hello"
@pytest.mark.asyncio
async def test_mount_unmount(inotify_dir_remove, fly_servers):
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))

    if os.path.isdir(_INODIR):
        shutil.rmtree(_INODIR)
    assert not os.path.isdir(_INODIR)
    assert not os.path.isfile(_INODIR_FILE)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 404)

    # make directory
    os.mkdir(_INODIR)
    with open(_INODIR_FILE, "w") as f:
        f.write("Hello test")

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 200)

    # remove directory
    assert os.path.isdir(_INODIR)
    assert os.path.isfile(_INODIR_FILE)
    shutil.move(_INODIR, "./inod")
    assert not os.path.isdir(_INODIR)
    assert not os.path.isfile(_INODIR_FILE)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 404)

    shutil.rmtree("./inod")

    # test HTTP
    for i in range(1000):
        async with httpx.AsyncClient(http1=True, timeout=1) as client:
            res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
        assert(res.status_code == 200)
