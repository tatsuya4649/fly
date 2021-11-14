import pytest
import asyncio
from signal import SIGINT
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

@pytest.fixture(scope="function", autouse=False)
def fly_remove_pid():
    if os.path.isfile(pid_path):
        os.remove(pid_path)
    yield
    if os.path.isfile(pid_path):
        os.remove(pid_path)

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def remove_already_in_use():
    process = await asyncio.create_subprocess_shell("lsof -i:1234 -Fp | sed -e 's/^p//'", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    yield

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

# make fly server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test.conf --daemon")
    await asyncio.sleep(0.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -Fp | sed -e 's/^p//'", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield
    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
            os.kill(int(i), SIGINT)
    assert(j == 2)

# make fly server (HTTP/1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test_mini.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test_mini.conf --daemon")
    await asyncio.sleep(0.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -Fp | sed -e 's/^p//'", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield
    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
            os.kill(int(i), SIGINT)
    assert(j == 2)

# make fly server (HTTP1.1/2 over SSL)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly daemon server (HTTP1.1/2 over SSL)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test.conf --daemon")
    await asyncio.sleep(0.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -Fp | sed -e 's/^p//'", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield
    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
            os.kill(int(i), SIGINT)
    assert(j == 2)


# make fly server (HTTP/1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test_mini.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly daemon server (HTTP1.1)
@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl_d(fly_remove_pid, remove_already_in_use):
    await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test_mini.conf --daemon")
    await asyncio.sleep(0.5)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -Fp | sed -e 's/^p//'", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))
    yield
    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
            os.kill(int(i), SIGINT)
    assert(j == 2)

