from fly import Fly
import pytest
import asyncio
import httpx
from signal import SIGINT
import os
import conftest
from conftest import *
from random import randint

@pytest.mark.asyncio
async def test_multiple_request(fly_server):
    async with httpx.AsyncClient(
        http1=True,
        timeout=100
    ) as client:
        for i in range(100):
            res = randint(1, 3)

            if res % 3 == 0:
                res = await client.get(
                    "http://localhost:1234",
                )
                assert(res.status_code == 200)
                assert(res.http_version == http_scheme())
            elif res % 2 == 0:
                res = await client.get(
                    "http://localhost:1234/user",
                )
                print(res.content)
                assert(res.status_code == 200)
                assert(res.http_version == http_scheme())
            else:
                res = await client.get(
                    "http://localhost:1234/__test",
                )
                print(res.content)
                assert(res.status_code == 404)
                assert(res.http_version == http_scheme())
