import pytest
from fly import Fly
from fly.response import *

@pytest.fixture(scope="function", autouse=False)
def init_fly():
    app = Fly()

    yield app
@pytest.fixture(scope="function", autouse=False)
def init_fly_nodebug():
    app = Fly(debug=False)
    yield app

def _get_base_route(app, uri, method):
    for i in app.routes:
        if i["uri"] == uri and i["method"] == method:
            return i["func"]

def test_HTTPException(init_fly):
    rr = HTTP400Exception(
        err_content="hello"
    )
    @init_fly.get("/")
    def index(request):
        raise rr

    _b = _get_base_route(init_fly, "/", "GET")
    res = _b({})
    assert res.status_code == 400
    assert res.body == b"hello"

def test_unexpected_error(init_fly):
    @init_fly.get("/")
    def index(request):
        1/0

    _b = _get_base_route(init_fly, "/", "GET")
    res = _b({})
    assert res.status_code == 500
    print(res.body.decode("utf-8"))

def test_unexpected_error(init_fly_nodebug):
    @init_fly_nodebug.get("/")
    def index(request):
        1/0

    _b = _get_base_route(init_fly_nodebug, "/", "GET")
    res = _b({})
    assert res.status_code == 500
    assert len(res.body) == 0
