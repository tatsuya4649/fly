from fly import Fly
import pytest
import asyncio
import httpx
from signal import SIGINT
import os
import conftest
from conftest import *
from random import randint

_HTTP="http"
_HOST="localhost"
_PORT=1234
@pytest.mark.asyncio
async def test_multiple_request(fly_server):
    for i in range(100):
        async with httpx.AsyncClient(
            http1=True,
            timeout=100
        ) as client:
            for i in range(100):
                res = randint(1, 3)

                if res % 5 == 0:
                    res = await client.get(
                        f"{_HTTP}://{_HOST}:{_PORT}/index.html"
                    )
                    assert(res.status_code == 200)
                    assert(res.http_version == http_scheme())
                elif res % 4 == 0:
                    res = await client.get(
                        f"{_HTTP}://{_HOST}:{_PORT}/raise_404"
                    )
                    assert(res.status_code == 404)
                    assert(res.http_version == http_scheme())
                elif res % 3 == 0:
                    res = await client.get(
                        f"{_HTTP}://{_HOST}:{_PORT}"
                    )
                    assert(res.status_code == 200)
                    assert(res.http_version == http_scheme())
                elif res % 2 == 0:
                    res = await client.get(
                        f"{_HTTP}://{_HOST}:{_PORT}/user",
                    )
                    print(res.content)
                    assert(res.status_code == 200)
                    assert(res.http_version == http_scheme())
                else:
                    res = await client.get(
                        f"{_HTTP}://{_HOST}:{_PORT}/__test",
                    )
                    print(res.content)
                    assert(res.status_code == 404)
                    assert(res.http_version == http_scheme())
