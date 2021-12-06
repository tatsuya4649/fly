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
async def test_create(inotify_file_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    await fly_notice_increment_check(_tmp_notice_size)
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)
    await fly_access_increment_check(_tmp_access_size)

@pytest.mark.asyncio
async def test_delete(inotify_file_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)
    assert(os.path.isfile(_INOF_PATH))
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    os.remove(_INOF_PATH)

    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)
    await fly_access_increment_check(_tmp_access_size)

@pytest.mark.asyncio
async def test_move(inotify_file_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    # create file
    with open(_INOF_PATH, "w") as f:
        f.write("Hello new file!")

    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 200)
    assert(os.path.isfile(_INOF_PATH))
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move file different directory
    shutil.move(_INOF_PATH, '../' + _INOF)
    await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOF}")
    assert(res.status_code == 404)
    await fly_access_increment_check(_tmp_access_size)

    # move to mount directory
    shutil.move('../' + _INOF, _INOF_PATH)
    await fly_notice_increment_check(_tmp_notice_size)

@pytest.mark.asyncio
async def test_create_directory(inotify_dir_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    assert(os.path.isfile(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)
    await fly_access_increment_check(_tmp_access_size)

@pytest.mark.asyncio
async def test_delete_directory(inotify_dir_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    assert not os.path.isfile(_INOD_PATH)
    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)

    # delete directory
    assert os.path.isdir(os.path.dirname(_INOD_PATH))
    shutil.rmtree(os.path.dirname(_INOD_PATH))
    assert not os.path.isdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)

@pytest.mark.asyncio
async def test_move_directory(inotify_dir_remove, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    assert(os.path.isfile(_INOD_PATH))

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move directory
    shutil.move(_INOD_PATH, '../' + _INOF)
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)
    await fly_access_increment_check(_tmp_access_size)

    # move to mount directory
    shutil.move('../' + _INOF, _INOD_PATH)
    await fly_notice_increment_check(_tmp_notice_size)

@pytest.mark.asyncio
async def test_move_directory2(inotify_dir_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    assert(os.path.isfile(_INOD_PATH))

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move directory
    shutil.move(_INOD2_PATH, '../ino')
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
    assert(res.status_code == 404)
    await fly_access_increment_check(_tmp_access_size)

    # move to mount directory
    shutil.move('../ino', _INOD2_PATH)
    await fly_notice_increment_check(_tmp_notice_size)

@pytest.fixture(scope="function", autouse=False)
def make_mnt2():
    if not os.path.isdir("tests/mnt2"):
        os.mkdir("tests/mnt2")
    if not os.path.isfile("tests/mnt2/hello"):
        with open("tests/mnt2/hello", "w") as f:
            f.write("Hello test!")
    yield

@pytest.mark.asyncio
async def test_unmount_dir(inotify_dir_remove, make_mnt2, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/hello")
    assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move mount point
    assert(os.path.isdir('tests/mnt2'))
    if os.path.isdir("../mnt2"):
        shutil.rmtree("../mnt2")
    shutil.move("tests/mnt2", '../mnt2')
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/hello")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move to mount directory
    shutil.move("../mnt2", 'tests/mnt2')
    # not increment test
    await fly_notice_noincrement_check(_tmp_notice_size)

_INODIR = "tests/mnt2/inod"
_INODIR_FILE = "tests/mnt2/inod/hello"
@pytest.mark.asyncio
async def test_mount_unmount(inotify_dir_remove, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()

    # create directory
    os.mkdir(os.path.dirname(_INOD_PATH))
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    with open(_INOD_PATH, "w") as f:
        f.write("Hello new file!")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    assert(os.path.isfile(_INOD_PATH))

    if os.path.isdir(_INODIR):
        shutil.rmtree(_INODIR)
        _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    assert not os.path.isdir(_INODIR)
    assert not os.path.isfile(_INODIR_FILE)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # make directory
    os.mkdir(_INODIR)
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # crate file
    assert not os.path.isfile(_INODIR_FILE)
    with open(_INODIR_FILE, "w") as f:
        f.write("Hello test")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    assert os.path.isdir(_INODIR)
    assert os.path.isfile(_INODIR_FILE)
    # remove directory(move)
    shutil.move(_INODIR, "./inod")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    assert not os.path.isdir(_INODIR)
    assert not os.path.isfile(_INODIR_FILE)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/inod/hello")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    shutil.rmtree("./inod")
    await fly_notice_noincrement_check(_tmp_notice_size)

    # test HTTP
    for i in range(1000):
        async with httpx.AsyncClient(http1=True, timeout=1) as client:
            res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/{_INOD}")
        assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

@pytest.mark.asyncio
async def test_mount_move(make_mnt2, newlog, fly_servers, emerge_log_size_check):
    _tmp_notice_size = fly_notice_log_size()
    _tmp_access_size = fly_access_log_size()

    assert os.path.isdir("tests/mnt2")
    if os.path.isfile("tests/mnt2/test1"):
        os.remove("tests/mnt2/test1")
        _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # create test file on mount point
    if not os.path.isfile("tests/mnt2/test1"):
        with open("tests/mnt2/test1", "w") as f:
            f.write("Hello World")
        _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # test HTTP
    for i in range(1000):
        async with httpx.AsyncClient(http1=True, timeout=1) as client:
            res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
        assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move mount point to different directory
    shutil.move("tests/mnt2", "../mnt2")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move mount point to different directory
    shutil.move("../mnt2", "tests/mnt2")
    _tmp_notice_size = await fly_notice_noincrement_check(_tmp_notice_size)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    assert(os.path.isfile("tests/mnt2/test1"))
    os.remove("tests/mnt2/test1")
    await fly_notice_noincrement_check(_tmp_notice_size)

@pytest.mark.asyncio
async def test_mount_delete(make_mnt2, newlog, fly_servers, emerge_log_size_check):
    _tmp_access_size = fly_access_log_size()
    _tmp_notice_size = fly_notice_log_size()

    assert os.path.isdir("tests/mnt2")
    if os.path.isfile("tests/mnt2/test1"):
        os.remove("tests/mnt2/test1")
        _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)
    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # create test file on mount point
    if not os.path.isfile("tests/mnt2/test1"):
        with open("tests/mnt2/test1", "w") as f:
            f.write("Hello World")
        _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # test HTTP
    for i in range(1000):
        async with httpx.AsyncClient(http1=True, timeout=1) as client:
            res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
        assert(res.status_code == 200)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move mount point to different directory
    shutil.rmtree("tests/mnt2")
    _tmp_notice_size = await fly_notice_increment_check(_tmp_notice_size)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    _tmp_access_size = await fly_access_increment_check(_tmp_access_size)

    # move mount point to different directory
    assert not os.path.isdir("tests/mnt2")
    os.mkdir("tests/mnt2")
    _tmp_notice_size = await fly_notice_noincrement_check(_tmp_notice_size)
    # create test file on mount point
    assert not os.path.isfile("tests/mnt2/test1")
    with open("tests/mnt2/test1", "w") as f:
        f.write("Hello World")
    _tmp_notice_size = await fly_notice_noincrement_check(_tmp_notice_size)

    # test HTTP
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/test1")
    assert(res.status_code == 404)
    await fly_access_increment_check(_tmp_access_size)

    assert(os.path.isfile("tests/mnt2/test1"))
    os.remove("tests/mnt2/test1")
    await fly_notice_noincrement_check(_tmp_notice_size)
