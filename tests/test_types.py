import pytest
import httpx
import conftest
from conftest import *


_HOST="localhost"
_PORT=1234
_HTTP="http"
_HTTPS="https"
@pytest.mark.asyncio
async def test_request(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_request(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_request(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_body(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/body")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_body(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/body")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_body(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/body")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_header(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/header")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_header(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/header")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_header(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1,
        http1=False,
        http2=True,
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/header")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_cookie(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/cookie")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_cookie(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/cookie")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_cookie(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/cookie")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_query(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/query")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_query(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/query")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_query(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/query")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_path_params(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/path_params")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_path_params(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_params")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_path_params(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_params")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())


@pytest.mark.asyncio
async def test_no_request(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/no_request")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_no_request(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/no_request")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_no_request(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/no_request")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_no_request_error(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/no_request_error")

    assert(res.status_code == 500)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_no_request_error(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/no_request_error")

    assert(res.status_code == 500)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_no_request_error(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/no_request_error")

    assert(res.status_code == 500)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_multi(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/multi")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_multi(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/multi")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_multi(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/multi")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_header_item(fly_servers, emerge_log_size_check, access_check):
    headers = {
        "hello": "header_item",
    }
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/header_item", headers=headers)

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_header_item(fly_servers_ssl, emerge_log_size_check, access_check):
    headers = {
        "hello": "header_item",
    }
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/header_item", headers=headers)

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_header_item(fly_servers_ssl, emerge_log_size_check, access_check):
    headers = {
        "hello": "header_item",
    }
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/header_item", headers=headers)

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_body_item(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.post(
                f"{_HTTP}://{_HOST}:{_PORT}/types/body_item",
                data={
                    "userid": 1,
                    "username": "user",
                }
        )

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_body_item(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            http1=True,
            verify=False,
            timeout=1) as client:
        res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/types/body_item",
                data={
                    "userid": 1,
                    "username": "user",
                }
        )

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_body_item(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            verify=False,
            http1=False,
            http2=True,
            timeout=1
    ) as client:
        res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/types/body_item",
                data={
                    "userid": 1,
                    "username": "user",
                }
        )

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_path_param_item(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/path_param_item/10")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_path_param_item(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            http1=True,
            verify=False,
            timeout=1) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_param_item/10")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_path_param_item(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            verify=False,
            http1=False,
            http2=True,
            timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_param_item/10")

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

@pytest.mark.asyncio
async def test_path_param_item_type_error(fly_servers, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/types/path_param_item/error")

    assert(res.status_code == 404)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_path_param_item_type_error(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            http1=True,
            verify=False,
            timeout=1) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_param_item/error")

    assert(res.status_code == 404)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_path_param_item_type_error(fly_servers_ssl, emerge_log_size_check, access_check):
    async with httpx.AsyncClient(
            verify=False,
            http1=False,
            http2=True,
            timeout=1
    ) as client:
        res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/types/path_param_item/error")

    assert(res.status_code == 404)
    assert(res.http_version == http2_scheme())

