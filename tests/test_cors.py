import pytest
from fly import Fly
from fly.cors import *
import httpx
import asyncio

def test_cors():
    app = Fly()

    @allow_cors()
    @app.get("/")
    def index():
        return None

    print(app.routes)
    assert(len(app.routes) == 2)

def test_cors_debug_only():
    app = Fly()

    @allow_cors(only_debug=True)
    @app.get("/")
    def index():
        return None

    assert(len(app.routes) == 2)
    app.remove_only_debug_headers(False)
    assert(len(app.routes) == 1)

def test_cors_default_cors():
    app = Fly()
    CORS(app)

    @app.get("/")
    def index():
        return None

    app.default_cors_apply_routes()
    assert(len(app.routes) == 2)


@pytest.mark.asyncio
@pytest.fixture(scope="function", autouse=False)
async def fly_server_cors(fly_remove_pid, remove_already_in_use):
    process = await asyncio.create_subprocess_shell("python3 -m fly tests/fly_test_cors.py -c tests/http_test.conf --test")
    await asyncio.sleep(1.5)
    yield process
    await asyncio.create_subprocess_shell("lsof -i:1234 -t | xargs kill -KILL")

@pytest.mark.asyncio
async def test_index_cors(fly_server_cors):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.options(
                "http://localhost:1234/"
                )

        print(res.headers)
        print(res.content)
        assert res.status_code == 200
        headers = res.headers
        assert headers.get("access-control-allow-origin") is not None
        assert headers.get("access-control-allow-methods") is not None
        assert headers.get("access-control-allow-credentials") is not None

@pytest.mark.asyncio
async def test_user_cors(fly_server_cors):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.options(
                "http://localhost:1234/user"
                )

        print(res.headers)
        print(res.content)
        assert res.status_code == 200
        headers = res.headers
        assert headers.get("access-control-allow-origin") is not None
        assert headers.get("access-control-allow-origin") == "*"

@pytest.mark.asyncio
async def test_post_cors(fly_server_cors):
    async with httpx.AsyncClient(http1=True, timeout=1) as client:
        res = await client.options(
                "http://localhost:1234/post"
                )

        print(res.headers)
        print(res.content)
        assert res.status_code == 200
        headers = res.headers
        assert headers.get("access-control-allow-origin") is not None
        assert headers.get("access-control-allow-methods") is not None
        assert headers.get("access-control-allow-origin") == "http://localhost:8080"
        assert headers.get("access-control-allow-methods") == "GET,POST"
        assert headers.get("access-control-allow-headers") == "HOST"
        assert headers.get("access-control-expose-headers") == "User-Agent"
        assert headers.get("access-control-allow-credentials") == "true"
        assert headers.get("access-control-max-age") == "1000"

