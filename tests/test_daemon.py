import pytest
import asyncio
import os
import signal

@pytest.mark.asyncio
async def test_daemon(remove_already_in_use):
    await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test.py -c tests/http_test.conf --daemon")
    await asyncio.sleep(2)

    process = await asyncio.create_subprocess_shell("lsof -i:1234 -t", stdout = asyncio.subprocess.PIPE)
    await process.wait()
    __out, __err = await process.communicate()
    print("\n")
    pro = __out.decode("utf-8")
    print(pro.split('\n'))

    j=0
    for i in pro.split('\n'):
        if (len(i) > 0):
            j+=1
    assert(j == 2)

