from fly import Fly
import pytest
import asyncio
import httpx
from signal import SIGINT

"""
"""

__HTTP = "HTTP/1.1"
__HTTP2 = "HTTP/2"
# make fly server (HTTP/1.1)
@pytest.fixture(scope="function", autouse=False)
async def fly_server():
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/http_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)


# make fly server (HTTP1.1/2 over SSL)
@pytest.fixture(scope="function", autouse=False)
async def fly_server_ssl():
    process = await asyncio.create_subprocess_shell("python -m fly tests/fly_test.py -c tests/https_test.conf")
    await asyncio.sleep(0.5)
    yield process
    process.send_signal(SIGINT)

"""
GET method test
"""
@pytest.mark.asyncio
async def test_http_get_index(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get("http://localhost:1234/")

    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)

@pytest.mark.asyncio
async def test_https_get_index(fly_server_ssl):
    async with httpx.AsyncClient(verify=False, timeout=1) as client:
        res = await client.get("https://localhost:1234/")

    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)

@pytest.mark.asyncio
async def test_https2_get_index(fly_server_ssl):
    async with httpx.AsyncClient(
        verify = False,
        http1  = False,
        http2  = True,
        timeout=1
    ) as client:
        res = await client.get("https://localhost:1234/")

    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)

"""
POST method test
"""
@pytest.mark.asyncio
async def test_http_post_data_index(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.post(
            "http://localhost:1234",
            data = {"key": "value"},
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https_post_data_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1
    ) as client:
        res = await client.post(
            "https://localhost:1234",
            data = {"key": "value"},
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https2_post_data_index(fly_server_ssl):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        res = await client.post(
            "https://localhost:1234/",
            data = {"key": "value"},
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_http_post_file_index(fly_server):
    # send with multipart/form-data
    files = {'upload-file': ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain')}
    async with httpx.AsyncClient(
        http1=True,
        timeout=60
    ) as client:
        res = await client.post(
            "http://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https_post_file_index(fly_server_ssl):
    # send with multipart/form-data
    files = {'upload-file': ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain')}
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=60
    ) as client:
        res = await client.post(
            "https://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https2_post_file_index(fly_server_ssl):
    # send with multipart/form-data
    files = {'upload-file': ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain')}
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=60
    ) as client:
        res = await client.post(
            "https://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_http_post_files_index(fly_server):
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        timeout=60
    ) as client:
        res = await client.post(
            "http://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https_post_files_index(fly_server_ssl):
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=60,
    ) as client:
        res = await client.post(
            "https://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_https2_post_files_index(fly_server_ssl):
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=60
    ) as client:
        res = await client.post(
            "https://localhost:1234",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_http_get_empty(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get("http://localhost:1234/empty")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https_get_empty(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = True,
        verify = False,
        timeout=1,
    ) as client:
        res = await client.get("https://localhost:1234/empty")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https2_get_empty(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1,
    ) as client:
        res = await client.get("https://localhost:1234/empty")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_head_index(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.head("http://localhost:1234")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https_head_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = True,
        verify = False,
        timeout=1
    ) as client:
        res = await client.head("https://localhost:1234")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https2_head_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1
    ) as client:
        res = await client.head("https://localhost:1234")

    print(res)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_head_500(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.head("http://localhost:1234/head_body")

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https_head_500(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify = False,
        timeout=1
    ) as client:
        res = await client.head("https://localhost:1234/head_body")

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == __HTTP)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_https2_head_500(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1
    ) as client:
        res = await client.head("https://localhost:1234/head_body")

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == __HTTP2)
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_put_index_data(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.put(
            "http://localhost:1234",
            data = {"key": "value"},
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_https_put_index_data(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1
    ) as client:
        res = await client.put(
            "https://localhost:1234",
            data = {"key": "value"},
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_https2_put_index_data(fly_server_ssl):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify=False,
        timeout=1
    ) as client:
        res = await client.put(
            "https://localhost:1234",
            data = {"key": "value"},
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_http_put_index_files(fly_server):
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        timeout=60,
    ) as client:
        res = await client.put(
            "http://localhost:1234",
            files = files,
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_https_put_index_files(fly_server_ssl):
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=60
    ) as client:
        res = await client.put(
            "https://localhost:1234",
            files = files,
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_https2_put_index_files(fly_server_ssl):
    files = {
        'dummy1':
            ('fly_dummy', open("tests/fly_dummy", 'rb'), 'text/plain'),
        'dummy2':
            ('fly_dummy2', open("tests/fly_dummy2", 'rb'), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=False,
        http2 = True,
        verify=False,
        timeout=60
    ) as client:
        res = await client.put(
            "https://localhost:1234",
            files = files,
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_http_delete_index(fly_server):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.delete(
            "http://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
async def test_https_delete_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify = False,
        timeout=1,
    ) as client:
        res = await client.delete(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
async def test_https2_delete_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        res = await client.delete(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
async def test_http_patch_index(fly_server):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.patch(
            "http://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PATCH")

@pytest.mark.asyncio
async def test_https_patch_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1,
    ) as client:
        res = await client.patch(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, PATCH")

@pytest.mark.asyncio
async def test_https2_patch_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        res = await client.patch(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, PATCH")

@pytest.mark.asyncio
async def test_http_options_index(fly_server):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.options(
            "http://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

@pytest.mark.asyncio
async def test_https_options_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1,
    ) as client:
        res = await client.options(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP)
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

@pytest.mark.asyncio
async def test_https2_options_index(fly_server_ssl):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        res = await client.options(
            "https://localhost:1234",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == __HTTP2)
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

