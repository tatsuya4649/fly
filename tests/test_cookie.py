import pytest
import httpx
from fly.cookie import *
from conftest import *

def test_header_value_from_cookie():
    res = header_value_from_cookie(
        name="id",
        value=100,
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_max_age():
    res = header_value_from_cookie(
        name="id",
        value=100,
        max_age=1000,
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_max_age_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            max_age="1000",
        )

def test_header_value_from_cookie_secure():
    res = header_value_from_cookie(
        name="id",
        value=100,
        secure=True,
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_secure_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            secure="False",
        )

def test_header_value_from_cookie_http_only():
    res = header_value_from_cookie(
        name="id",
        value=100,
        http_only=True,
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_http_only_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            http_only="True",
        )

def test_header_value_from_cookie_domain():
    res = header_value_from_cookie(
        name="id",
        value=100,
        domain="example.com",
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_domain_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            domain=1,
        )

def test_header_value_from_cookie_path():
    res = header_value_from_cookie(
        name="id",
        value=100,
        path="/",
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_path_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            path=1
        )

def test_header_value_from_samesite():
    res = header_value_from_cookie(
        name="id",
        value=100,
        samesite=SameSite._None
    )
    print(res)
    assert(isinstance(res, str))

def test_header_value_from_cookie_samesite_type_error():
    with pytest.raises(TypeError):
        header_value_from_cookie(
            name="id",
            value=100,
            samesite=1
        )

# cookie setting test
@pytest.mark.asyncio
async def test_cookie_http(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get(
            "http://localhost:1234/cookie",
            cookies={"id": "10000"},
        )

    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == http_scheme())

@pytest.mark.asyncio
async def test_cookie_http_set(fly_server):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.get("http://localhost:1234/set_cookie")

    assert(res.headers.get("set-cookie"))
    assert(res.status_code == 200)
    assert(res.is_error is False)
    assert(res.http_version == http_scheme())

