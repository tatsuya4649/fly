import pytest
import asyncio
from signal import SIGTERM, SIGINT
import os
import sys
from fly import Fly
import shutil
try:
    import ssl
except ModuleNotFoundError:
    print("no ssl")


def http_scheme():
    __HTTP = "HTTP/1.1"
    return __HTTP

def http2_scheme():
    __HTTP2 = "HTTP/2"
    return __HTTP2

def crt_path():
    __CRT = "tests/fly_test.crt"
    return __CRT

def key_path():
    __KEY = "tests/fly_test.key"
    return __KEY

def not_have_ssl_crt_key_file():
    return not os.path.isfile(crt_path()) or \
        not os.path.isfile(key_path())

@pytest.fixture(scope="session", autouse=True)
def make_mntdir():
    if not os.path.isdir("tests/mnt"):
        os.mkdir("tests/mnt")
    if not os.path.isdir("tests/mnt2"):
        os.mkdir("tests/mnt2")
    yield

_LOGPATH="tests/log"
@pytest.fixture(scope="function", autouse=True)
def newlog():
    if os.path.isdir(_LOGPATH):
        shutil.rmtree(_LOGPATH)
    os.mkdir(_LOGPATH)
    yield {
        "aceess": f'{_LOGPATH}/fly_access.log',
        "notice": f'{_LOGPATH}/fly_notice.log',
        "error":  f'{_LOGPATH}/fly_error.log'
        }

def fly_notice_log_size():
    notice_log = f"{_LOGPATH}/fly_notice.log"
    if not os.path.isfile(notice_log):
        return 0
    else:
        return os.path.getsize(notice_log)
async def fly_notice_increment_check(pre_len):
    notice_log = f"{_LOGPATH}/fly_notice.log"
    await asyncio.sleep(0.5)
    assert pre_len < os.path.getsize(notice_log)
    return os.path.getsize(notice_log)
async def fly_notice_noincrement_check(pre_len):
    notice_log = f"{_LOGPATH}/fly_notice.log"
    await asyncio.sleep(0.5)
    assert pre_len == os.path.getsize(notice_log)
    return os.path.getsize(notice_log)

def fly_access_log_size():
    access_log = f"{_LOGPATH}/fly_access.log"
    assert os.path.isfile(access_log)
    return os.path.getsize(access_log)
async def fly_access_increment_check(pre_len):
    access_log = f"{_LOGPATH}/fly_access.log"
    await asyncio.sleep(0.5)
    assert pre_len < os.path.getsize(access_log)
    return os.path.getsize(access_log)
async def fly_access_noincrement_check(pre_len):
    access_log = f"{_LOGPATH}/fly_access.log"
    await asyncio.sleep(0.5)
    assert pre_len == os.path.getsize(access_log)
    return os.path.getsize(access_log)

@pytest.fixture(scope="function", autouse=False)
def emerge_log_size_check():
    emerge_log = f"{_LOGPATH}/fly_emerge.log"
    if os.path.isfile(emerge_log):
        _tmp_size = os.path.getsize(emerge_log)
    else:
        _tmp_size = 0
    yield
    if os.path.isfile(emerge_log):
        assert _tmp_size == os.path.getsize(emerge_log)


@pytest.fixture(scope="function", autouse=False)
def access_check(newlog):
    access_log = _LOGPATH + "/fly_access.log"
    if os.path.isfile(access_log):
        _tmp_access_log_size = os.path.getsize(access_log)
    else:
        _tmp_access_log_size = 0
    print(f"~~~~~ now access log size {_tmp_access_log_size} ~~~~~")
    yield _tmp_access_log_size
    assert(_tmp_access_log_size < os.path.getsize(access_log))
    print(f"~~~~~ access log size {os.path.getsize(access_log)} ~~~~~")


ssl_reason = "require SSL cert/key file"
pid_path = "log/fly.pid"

@pytest.fixture(scope="session", autouse=True)
def _pid():
    pid = os.getpid()
    print(f"fly test PID: \"{pid}\"")
    yield pid

def get_master_pid(pid_list):
    if not isinstance(pid_list, list):
        raise TypeError
    if len(pid_list) <= 1:
        raise ValueError

    min_pid=0
    pids = []
    for i in pid_list:
        if isinstance(i, str) and len(i) > 0:
            pids.append(int(i))
        elif isinstance(i, int):
            pids.append(int(i))
        elif isinstance(i, str) and len(i) == 0:
            continue
        else:
            raise TypeError

    return min(pids)

def get_worker_pids(pid_list):
    mid = get_master_pid(pid_list)

    wids = list()
    for i in pid_list:
        if int(i) != mid:
            wids.append(int(i))
    return wids

@pytest.fixture(scope="function", autouse=False)
def fly_remove_pid():
    if os.path.isfile(pid_path):
        os.remove(pid_path)
    yield
    if os.path.isfile(pid_path):
        os.remove(pid_path)

GET_FLY_COMMAND = 'lsof -i:1234 -t'
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=True)
async def remove_already_in_use():
    pid = os.getpid()
    process = await asyncio.create_subprocess_shell(GET_FLY_COMMAND, stdout = asyncio.subprocess.PIPE)
    _out, _ = await process.communicate()
    await process.wait()

    if (len(_out) == 0):
        yield
    else:
        _spout = _out.decode("utf-8").split('\n')
        _spout = list(filter(lambda x: len(x) > 0, _spout))
        _no_this_pids = list(filter(lambda x: x != str(pid), _spout))
        assert(not str(pid) in _no_this_pids)
        process = await asyncio.create_subprocess_shell(f"echo \"{' '.join(_no_this_pids)}\" | xargs kill -KILL")
        await process.wait()
        yield

@pytest.fixture(params=["nodaemon", "daemon"])
def fly_servers_onlyspawn(request):
    if request.param == "nodaemon":
        yield request.getfixturevalue("fly_server_os")
    else:
        yield request.getfixturevalue("fly_server_os_d")

@pytest.fixture(params=["nodaemon", "daemon"])
def fly_servers(request):
    if request.param == "nodaemon":
        yield request.getfixturevalue("fly_server")
    else:
        yield request.getfixturevalue("fly_server_d")


@pytest.fixture(params=["nodaemon", "daemon"])
def fly_mini_servers(request):
    if request.param == "nodaemon":
        yield request.getfixturevalue("fly_mini_server")
    else:
        yield request.getfixturevalue("fly_mini_server_d")

@pytest.fixture(params=["nodaemon", "daemon"])
def fly_servers_ssl(request):
    if request.param == "nodaemon":
        yield request.getfixturevalue("fly_server_ssl")
    else:
        yield request.getfixturevalue("fly_server_ssl_d")

@pytest.fixture(params=["nodaemon", "daemon"])
def fly_mini_servers_ssl(request):
    if request.param == "nodaemon":
        yield request.getfixturevalue("fly_mini_server_ssl")
    else:
        yield request.getfixturevalue("fly_mini_server_ssl_d")

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_os(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test.conf --test")
    await asyncio.sleep(1.5)
    yield process

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_os_d(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test.conf --daemon  --test")
    await asyncio.sleep(1.5)
    yield process

# make fly server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test.conf --test")
    await asyncio.sleep(1.5)
    yield process

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test.conf --daemon --test")
    await asyncio.sleep(1.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield

# make fly server (HTTP/1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test_mini.conf --test")
    await asyncio.sleep(1.5)
    yield process

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test_mini.conf --daemon --test")
    await asyncio.sleep(1.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield

# make fly server (HTTP1.1/2 over SSL)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test.conf --test")
    await asyncio.sleep(1.5)
    yield process

# make fly daemon server (HTTP1.1/2 over SSL)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test.conf --daemon --test")
    await asyncio.sleep(1.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield

# make fly server (HTTP/1.1), SIGINT
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test_mini.conf --test")
    await asyncio.sleep(1.5)
    yield process

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test_mini.conf --daemon --test")
    await asyncio.sleep(1.5)
    yield

# make fly server over workers max
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_over_worker(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test_over.conf --test")
    await asyncio.sleep(1.5)
    yield process

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_over_worker_d(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test_over.conf --daemon --test")
    await asyncio.sleep(1.5)
    yield process
