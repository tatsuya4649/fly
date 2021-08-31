import pytest
import os
from fly.env import FlyEnv

@pytest.fixture(scope="function", autouse=False)
def __flyenv_init():
    os.environ[FlyEnv.FLY_HOST_ENV] = "localhost"
    os.environ[FlyEnv.FLY_PORT_ENV] = "100"
    os.environ[FlyEnv.FLY_ROOT_ENV] = "."
    os.environ[FlyEnv.FLY_WORKERS_ENV] = "2"
    env = FlyEnv()
    yield env

def test_env_init():
    os.environ[FlyEnv.FLY_HOST_ENV] = "localhost"
    os.environ[FlyEnv.FLY_PORT_ENV] = "100"
    os.environ[FlyEnv.FLY_ROOT_ENV] = "."
    os.environ[FlyEnv.FLY_WORKERS_ENV] = "2"
    env = FlyEnv()

def test_host(__flyenv_init):
    assert(isinstance(__flyenv_init.host, str))

@pytest.mark.parametrize(
    "host", [
    "localhost",
    None
])
def test_host_set(__flyenv_init, host):
    __flyenv_init.host = host

@pytest.mark.parametrize(
    "host", [
    10,
    10.0,
    True,
    ["localhost"],
    {"host": "localhost"},
])
def test_host_set_type_error(__flyenv_init, host):
    with pytest.raises(
        TypeError
    ) as raiseinfo:
        __flyenv_init.host = host

def test_port(__flyenv_init):
    assert(isinstance(__flyenv_init.port, int))

@pytest.mark.parametrize(
    "port", [
    10,
    None,
])
def test_port_set(__flyenv_init, port):
    __flyenv_init.port = port
    
@pytest.mark.parametrize(
    "port", [
    "10",
    ["port"],
    {"port": 10},
])
def test_port_set_type_error(__flyenv_init, port):
    with pytest.raises(
        TypeError
    ) as raiseinfo:
        __flyenv_init.port = port

def test_root(__flyenv_init):
    assert(isinstance(__flyenv_init.root, str))

@pytest.mark.parametrize(
    "root",[
    ".",
    None
])
def test_root_set(__flyenv_init, root):
    __flyenv_init.root = root

@pytest.mark.parametrize(
    "root", [
    10,
    10.0,
    ["root"],
    {"root": "."},
])
def test_root_set_type_error(__flyenv_init, root):
    with pytest.raises(
        TypeError
    ) as raiseinfo:
        __flyenv_init.root = root

def test_workers(__flyenv_init):
    assert(isinstance(__flyenv_init.workers, int))

@pytest.mark.parametrize(
    "workers",[
    10,
    None
])
def test_workers_set(__flyenv_init, workers):
    __flyenv_init.workers = workers

@pytest.mark.parametrize(
    "workers",[
    "10",
    [10],
    {"workers": 10},
])
def test_workers_set_type_error(__flyenv_init, workers):
    with pytest.raises(
        TypeError
    ) as raiseinfo:
        __flyenv_init.workers = workers
