import pytest
import asyncio


@pytest.mark.asyncio
async def test_daemon():
    await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test.conf --daemon")
