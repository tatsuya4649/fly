from fly import Fly
import pytest
import asyncio
import httpx
import conftest
from conftest import *

_HOST="localhost"
_PORT=1234
_HTTP="http"
_HTTPS="https"

@pytest.mark.parametrize(
    "enc_type", [
        "gzip",
        "br",
        "identity",
        "*",
    ]
)
@pytest.mark.parametrize(
    "ae", [
        "Accept-Encoding",
        "Accept-encoding",
        "accept-encoding",
    ]
)
@pytest.mark.asyncio
async def test_http(fly_servers_enc, ae, enc_type, emerge_log_size_check, access_check):
    headers = {
        ae: enc_type,
    }
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(
                f"{_HTTP}://{_HOST}:{_PORT}/",
                headers=headers
        )

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    if enc_type == "*":
        assert(res.headers["content-encoding"] == "gzip")
    else:
        assert(res.headers["content-encoding"] == enc_type)

@pytest.mark.parametrize(
    "enc_type", [
        "gzip",
        "br",
        "identity",
        "*",
    ]
)
@pytest.mark.parametrize(
    "ae", [
        "Accept-Encoding",
        "Accept-encoding",
        "accept-encoding",
    ]
)
@pytest.mark.asyncio
async def test_https(fly_servers_enc_ssl, ae, enc_type, emerge_log_size_check, access_check):
    headers = {
        ae: enc_type,
    }
    async with httpx.AsyncClient(
            http1=True,
            http2=False,
            verify=False,
            timeout=1) as client:
        res = await client.get(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                headers=headers
        )

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    if enc_type == "*":
        assert(res.headers["content-encoding"] == "gzip")
    else:
        assert(res.headers["content-encoding"] == enc_type)

@pytest.mark.parametrize(
    "enc_type", [
        "gzip",
        "br",
        "identity",
        "*",
    ]
)
@pytest.mark.parametrize(
    "ae", [
        "Accept-Encoding",
        "Accept-encoding",
        "accept-encoding",
    ]
)
@pytest.mark.asyncio
async def test_https2(fly_servers_enc_ssl, ae, enc_type, emerge_log_size_check, access_check):
    headers = {
        ae: enc_type,
    }
    async with httpx.AsyncClient(
            http1=False,
            http2=True,
            verify=False,
            timeout=1) as client:
        res = await client.get(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                headers=headers
        )

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    if enc_type == "*":
        assert(res.headers["content-encoding"] == "gzip")
    else:
        assert(res.headers["content-encoding"] == enc_type)

@pytest.mark.parametrize(
    "enc_type, answer", [
        ("deflate, gzip;q=1.0, *;q=0.5",    "deflate"),
        ("br;q=1.0, gzip;q=0.8, *;q=0.1",   "br"),
        ("br;q=0.9, gzip;q=0, *;q=0",       "br"),
        ("compress;q=0.5, gzip;q=1.0",      "gzip"),
        ("gzip, deflate, br",               "gzip"),
        ("*",                               "gzip"),
    ]
)
@pytest.mark.asyncio
async def test_quality_value(fly_servers_enc, enc_type, answer, emerge_log_size_check, access_check):
    headers = {
        "Accept-Encoding" : enc_type,
    }
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(
                f"{_HTTP}://{_HOST}:{_PORT}/",
                headers=headers
        )

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.headers["content-encoding"] == answer)

@pytest.mark.parametrize(
    "enc_type", [
        "identity; q=0",
        "*; q=0",
    ]
)
@pytest.mark.asyncio
async def test_406(fly_servers_enc, enc_type, emerge_log_size_check, access_check):
    headers = {
        "Accept-Encoding" : enc_type,
    }
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(
                f"{_HTTP}://{_HOST}:{_PORT}/",
                headers=headers
        )

    assert(res.status_code == 406)
    assert(res.http_version == http_scheme())
