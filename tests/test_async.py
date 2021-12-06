from fly import Fly
import pytest
import asyncio
import httpx
import conftest
from conftest import *


@pytest.fixture(scope="function", autouse=True)
def access_check(newlog):
    access_log = conftest._LOGPATH + "/fly_access.log"
    if os.path.isfile(access_log):
        _tmp_access_log_size = os.path.getsize(access_log)
    else:
        _tmp_access_log_size = 0
    print(f"~~~~~ now access log size {_tmp_access_log_size} ~~~~~")
    yield _tmp_access_log_size
    assert(_tmp_access_log_size < os.path.getsize(access_log))
    print(f"~~~~~ access log size {os.path.getsize(access_log)} ~~~~~")


"""
GET method test
"""
_HOST="localhost"
_PORT=1234
_HTTP="http"
_HTTPS="https"
@pytest.mark.asyncio
async def test_http_get_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/")

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_get_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        verify=False,
        timeout=1
    ) as client:
        try:
            res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_get_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        verify = False,
        http1  = False,
        http2  = True,
        timeout=1
    ) as client:
        try:
            res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())

"""
POST method test
"""
@pytest.mark.asyncio
async def test_http_post_data_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.post(
            f"{_HTTP}://{_HOST}:{_PORT}/",
            data = {"key": "value"},
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_post_data_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1
    ) as client:
        try:
            res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                data = {"key": "value"},
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_post_data_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        verify=False,
        http1=False,
        http2=True,
        timeout=1
    ) as client:
        try:
            res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                data = {"key": "value"},
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_http_post_file_index(fly_servers, emerge_log_size_check):
    # send with multipart/form-data
    with open("tests/fly_dummy", 'rb') as _f:
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=True,
            timeout=60
        ) as client:
            res = await client.post(
                f"{_HTTP}://{_HOST}:{_PORT}/",
                files = files,
            )
        print(res.content)
        assert(res.status_code == 200)
        assert(res.http_version == http_scheme())
        assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_post_file_index(fly_servers_ssl, emerge_log_size_check):
    # send with multipart/form-data
    with open("tests/fly_dummy", 'rb') as _f:
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=True,
            verify=False,
            timeout=60
        ) as client:
            try:
                res = await client.post(
                    f"{_HTTPS}://{_HOST}:{_PORT}/",
                    files = files,
                )
            except ssl.SSLWantReadError:
                print(traceback.format_exc())
            except ssl.SSLWantWriteError:
                print(traceback.format_exc())

        print(res.content)
        assert(res.status_code == 200)
        assert(res.http_version == http_scheme())
        assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_post_file_index(fly_servers_ssl, emerge_log_size_check):
    with open("tests/fly_dummy", 'rb') as _f:
        # send with multipart/form-data
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=False,
            http2=True,
            verify=False,
            timeout=10
        ) as client:
            try:
                res = await client.post(
                    f"{_HTTPS}://{_HOST}:{_PORT}/",
                    files = files,
                )
            except ssl.SSLWantReadError:
                print(traceback.format_exc())
            except ssl.SSLWantWriteError:
                print(traceback.format_exc())

        assert(res.status_code == 200)
        assert(res.http_version == http2_scheme())
        assert(res.content.decode("utf-8") == "Success, POST")

@pytest.mark.asyncio
async def test_http_post_files_index(fly_servers, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        timeout=60
    ) as client:
        res = await client.post(
            f"{_HTTP}://{_HOST}:{_PORT}/",
            files = files,
        )
    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_post_files_index(fly_servers_ssl, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=60,
    ) as client:
        try:
            res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                files = files,
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_post_files_index(fly_servers_ssl, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    # send with multipart/form-data
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=10
    ) as client:
        try:
            res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}/",
                files = files,
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, POST")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
async def test_http_get_empty(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(f"{_HTTP}://{_HOST}:{_PORT}/empty")

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_get_empty(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = True,
        verify = False,
        timeout=1,
    ) as client:
        try:
            res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/empty")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_get_empty(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1,
    ) as client:
        try:
            res = await client.get(f"{_HTTPS}://{_HOST}:{_PORT}/empty")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_head_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.head(f"{_HTTP}://{_HOST}:{_PORT}")

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_head_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = True,
        verify = False,
        timeout=1
    ) as client:
        try:
            res = await client.head(f"{_HTTPS}://{_HOST}:{_PORT}")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_head_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1
    ) as client:
        try:
            res = await client.head(f"{_HTTPS}://{_HOST}:{_PORT}")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_head_500(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.head(f"{_HTTP}://{_HOST}:{_PORT}/head_body")

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_head_500(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify = False,
        timeout=1
    ) as client:
        try:
            res = await client.head(f"{_HTTPS}://{_HOST}:{_PORT}/head_body")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == http_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_head_500(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify = False,
        timeout=1
    ) as client:
        try:
            res = await client.head(f"{_HTTPS}://{_HOST}:{_PORT}/head_body")
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res)
    assert(res.status_code == 500)
    assert(res.is_server_error is True)
    assert(res.http_version == http2_scheme())
    assert(len(res.content) == 0)

@pytest.mark.asyncio
async def test_http_put_index_data(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.put(
            f"{_HTTP}://{_HOST}:{_PORT}",
            data = {"key": "value"},
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_put_index_data(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1
    ) as client:
        try:
            res = await client.put(
                f"{_HTTPS}://{_HOST}:{_PORT}",
                data = {"key": "value"},
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_put_index_data(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1 = False,
        http2 = True,
        verify=False,
        timeout=1
    ) as client:
        try:
            res = await client.put(
                f"{_HTTPS}://{_HOST}:{_PORT}",
                data = {"key": "value"},
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")

@pytest.mark.asyncio
async def test_http_put_index_files(fly_servers, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        timeout=60,
    ) as client:
        res = await client.put(
            f"{_HTTP}://{_HOST}:{_PORT}",
            files = files,
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_put_index_files(fly_servers_ssl, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=60
    ) as client:
        try:
            res = await client.put(
                f"{_HTTPS}://{_HOST}:{_PORT}",
                files = files,
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_put_index_files(fly_servers_ssl, emerge_log_size_check):
    _f1 = open("tests/fly_dummy", 'rb')
    _f2 = open("tests/fly_dummy2", 'rb')
    files = {
        'dummy1':
            ('fly_dummy', _f1.read(), 'text/plain'),
        'dummy2':
            ('fly_dummy2', _f2.read(), 'text/html'),
    }
    async with httpx.AsyncClient(
        http1=False,
        http2 = True,
        verify=False,
        timeout=60
    ) as client:
        try:
            res = await client.put(
                f"{_HTTPS}://{_HOST}:{_PORT}",
                files = files,
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, PUT")
    _f1.close()
    _f2.close()

@pytest.mark.asyncio
async def test_http_delete_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.delete(
            f"{_HTTP}://{_HOST}:{_PORT}",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_delete_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify = False,
        timeout=1,
    ) as client:
        try:
            res = await client.delete(
                f"{_HTTPS}://{_HOST}:{_PORT}",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_delete_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        try:
            res = await client.delete(
                f"{_HTTPS}://{_HOST}:{_PORT}",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, DELETE")

@pytest.mark.asyncio
async def test_http_patch_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.patch(
            f"{_HTTP}://{_HOST}:{_PORT}",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, PATCH")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_patch_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        try:
            res = await client.patch(
                f"{_HTTPS}://{_HOST}:{_PORT}",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, PATCH")

@pytest.mark.asyncio
async def test_http_options_index(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.options(
            f"{_HTTP}://{_HOST}:{_PORT}",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_options_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1,
    ) as client:
        try:
            res = await client.options(
                f"{_HTTPS}://{_HOST}:{_PORT}",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_options_index(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        timeout=1,
    ) as client:
        try:
            res = await client.options(
                f"{_HTTPS}://{_HOST}:{_PORT}",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
    assert(res.content.decode("utf-8") == "Success, OPTIONS")

@pytest.mark.asyncio
async def test_http_return_query(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        params={"key1": "value1", "key2": "value2"},
        timeout=60
    ) as client:
        res = await client.get(
            f"{_HTTP}://{_HOST}:{_PORT}/query",
        )

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https_return_query(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        params={"key1": "value1", "key2": "value2"},
        timeout=60
    ) as client:
        try:
            res = await client.get(
                f"{_HTTPS}://{_HOST}:{_PORT}/query",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_https2_return_query(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=False,
        http2=True,
        verify=False,
        params={"key1": "value1", "key2": "value2"},
        timeout=60
    ) as client:
        try:
            res = await client.get(
                f"{_HTTPS}://{_HOST}:{_PORT}/query",
            )
        except ssl.SSLWantReadError:
            print(traceback.format_exc())
        except ssl.SSLWantWriteError:
            print(traceback.format_exc())

    print(res.content)
    assert(res.status_code == 200)
    assert(res.http_version == http2_scheme())
"""
Illeagl test
"""
# SSL Server but, HTTP request
@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_illegal_http(fly_servers_ssl, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        timeout=1,
    ) as client:
        res = await client.get(
            f"{_HTTP}://{_HOST}:{_PORT}",
        )

    assert(res.status_code == 400)
    assert(res.http_version == http_scheme())

# HTTP Server but, HTTPS request
@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_illegal_https(fly_servers, emerge_log_size_check):
    async with httpx.AsyncClient(
        http1=True,
        verify=False,
        timeout=1,
    ) as client:
        with pytest.raises(
            httpx.ConnectError
        ) as e:
            try:
                res = await client.get(
                    f"{_HTTPS}://{_HOST}:{_PORT}",
                )
            except ssl.SSLWantReadError:
                print(traceback.format_exc())
            except ssl.SSLWantWriteError:
                print(traceback.format_exc())

@pytest.mark.asyncio
async def test_request_over(fly_mini_servers, emerge_log_size_check):
    with open("tests/fly_dummy", 'rb') as _f:
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=True,
            timeout=60
        ) as client:
            res = await client.post(
                f"{_HTTP}://{_HOST}:{_PORT}",
                files = files,
            )
        assert(res.status_code == 413)
        assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_ssl_request_over(fly_mini_servers_ssl, emerge_log_size_check):
    with open("tests/fly_dummy", 'rb') as _f:
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=True,
            http2=False,
            verify=False,
            timeout=60
        ) as client:
            res = await client.post(
                f"{_HTTPS}://{_HOST}:{_PORT}",
                files = files,
            )

        assert(res.status_code == 413)
        assert(res.http_version == http_scheme())

@pytest.mark.asyncio
@pytest.mark.skipif(not_have_ssl_crt_key_file(), reason=conftest.ssl_reason)
async def test_http2_ssl_request_over(fly_mini_servers_ssl, emerge_log_size_check):
    with open("tests/fly_dummy", 'rb') as _f:
        files = {'upload-file': ('fly_dummy', _f.read(), 'text/plain')}
        async with httpx.AsyncClient(
            http1=False,
            http2=True,
            verify=False,
            timeout=60
        ) as client:
            try:
                res = await client.post(
                    f"{_HTTPS}://{_HOST}:{_PORT}",
                    files = files,
                )
            except ssl.SSLWantReadError:
                print(traceback.format_exc())
            except ssl.SSLWantWriteError:
                print(traceback.format_exc())

        assert(res.status_code == 413)
        assert(res.http_version == http2_scheme())

