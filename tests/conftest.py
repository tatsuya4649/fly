import pytest
from fly import Fly
import asyncio
from signal import SIGINT
import os

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

# make fly server (HTTP/1.1)
@pytest.fixture(scope="function", autouse=False)
async def fly_server(fly_remove_pid):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly server (HTTP/1.1)
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server(fly_remove_pid):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test_mini.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly server (HTTP1.1/2 over SSL)
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl(fly_remove_pid):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

# make fly server (HTTP/1.1)
@pytest.fixture(scope="function", autouse=False)
async def fly_mini_server_ssl(fly_remove_pid):
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test_mini.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)


