import pytest
import asyncio
from signal import SIGTERM, SIGINT
import os
import sys
from fly import Fly

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

ssl_reason = "require SSL cert/key file"
pid_path = "log/fly.pid"


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

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=True)
async def remove_already_in_use():
    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t | xargs kill -KILL", stdout = asyncio.subprocess.PIPE)
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

    process.send_signal(SIGINT)

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

    __master = get_master_pid(pro.split('\n'))
    os.kill(__master, SIGINT)

# make fly server (HTTP/1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test_mini.conf --test")
    await asyncio.sleep(1.5)
    yield process
    process.send_signal(SIGTERM)

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

    __master = get_master_pid(pro.split('\n'))
    os.kill(__master, SIGINT)

# make fly server (HTTP1.1/2 over SSL)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test.conf --test")
    await asyncio.sleep(1.5)
    yield process
    process.send_signal(SIGINT)

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

    __master = get_master_pid(pro.split('\n'))
    os.kill(__master, SIGINT)


# make fly server (HTTP/1.1), SIGINT
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test_mini.conf --test")
    await asyncio.sleep(1.5)
    yield process
    process.send_signal(SIGTERM)

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/https_test_mini.conf --daemon --test")
    await asyncio.sleep(1.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield

    __master = get_master_pid(pro.split('\n'))
    os.kill(__master, SIGINT)
    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
            os.kill(int(i), SIGTERM)
    assert(j == 2)

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
