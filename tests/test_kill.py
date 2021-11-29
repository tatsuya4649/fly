import os
import sys
import signal
import pytest
import asyncio
from fly import Fly
from conftest import get_master_pid

def get_worker_pids(pid_list):
    mid = get_master_pid(pid_list)

    wids = list()
    for i in pid_list:
        if int(i) != mid:
            wids.append(int(i))
    return wids

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def test_spawn_processes(fly_servers_onlyspawn):
    prc = await asyncio.create_subprocess_shell(
        "lsof -i:1234 -t",
        stdout=asyncio.subprocess.PIPE
    )
    __out, _ = await prc.communicate()
    prc = __out.decode("utf-8")
    prcs = prc.split("\n")

    rprcs = list()
    for i in prcs:
        if len(i)>0:
            rprcs.append(int(i))

    yield rprcs

async def check_workers_processes():
    prc = await asyncio.create_subprocess_shell(
        "lsof -i:1234 -t",
        stdout=asyncio.subprocess.PIPE
    )
    __out, _ = await prc.communicate()
    prc = __out.decode("utf-8")
    prcs = prc.split("\n")

    rprcs = list()
    for i in prcs:
        if len(i)>0:
            rprcs.append(int(i))

    return rprcs

@pytest.mark.asyncio
async def test_worker_kill(test_spawn_processes):
    mid = get_master_pid(test_spawn_processes)
    wids = get_worker_pids(test_spawn_processes)
    print(f"fly processes: {test_spawn_processes}")
    print(f"master: {mid}")
    print(f"workers: {wids}")
    if len(wids) < 1:
        raise ValueError

    kill_wid = wids[0]
    print(f"kill worker: {kill_wid}")
    # kill worker
    os.kill(kill_wid, signal.SIGTERM)
    await asyncio.sleep(1.5)

    prcs = await check_workers_processes()
    nmid = get_master_pid(prcs)
    nwids = get_worker_pids(prcs)
    print(f"new fly processes: {prcs}")
    print(f"new master: {nmid}")
    print(f"new workers: {nwids}")

    assert(mid == nmid)
    assert(len(wids) == len(nwids))
    assert(not kill_wid in nwids)

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def test_spawn_over_worker(fly_server_over_worker):
    prc = await asyncio.create_subprocess_shell(
        "lsof -i:1234 -t",
        stdout=asyncio.subprocess.PIPE
    )
    __out, _ = await prc.communicate()
    prc = __out.decode("utf-8")
    prcs = prc.split("\n")

    rprcs = list()
    for i in prcs:
        if len(i)>0:
            rprcs.append(int(i))

    yield rprcs

@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def test_spawn_over_worker_d(fly_server_over_worker_d):
    prc = await asyncio.create_subprocess_shell(
        "lsof -i:1234 -t",
        stdout=asyncio.subprocess.PIPE
    )
    __out, _ = await prc.communicate()
    prc = __out.decode("utf-8")
    prcs = prc.split("\n")

    rprcs = list()
    for i in prcs:
        if len(i)>0:
            rprcs.append(int(i))

    yield rprcs

@pytest.mark.asyncio
async def test_over_max_workers(test_spawn_over_worker):
    wids = get_worker_pids(test_spawn_over_worker)
    print(f"workers: {wids}")
    assert(len(wids) == 10)

@pytest.mark.asyncio
async def test_over_max_workers_d(test_spawn_over_worker_d):
    wids = get_worker_pids(test_spawn_over_worker_d)
    print(f"workers: {wids}")
    assert(len(wids) == 10)
