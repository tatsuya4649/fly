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
        timeout=1
    ) as client:
        for i in range(100):
            if randint(0, 1):
                res = await client.get(
                    "http://localhost:1234",
                )
                assert(res.status_code == 200)
                assert(res.is_error is False)
                assert(res.http_version == http_scheme())
            else:
                res = await client.get(
                    "http://localhost:1234/user",
                )
                print(res.content)
                assert(res.status_code == 200)
                assert(res.is_error is False)
                assert(res.http_version == http_scheme())
