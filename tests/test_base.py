import pytest
from fly._base import _BaseRoute
from fly.types import *
from fly import Response


@pytest.fixture(scope="function", autouse=False)
def inti_br():
    pass

@pytest.fixture(scope="function", autouse=False)
def http_request():
    pass

def test_base():
    def index(request: Request):
        return "Hello"

    base = _BaseRoute(
            handler=index
            )
    output = base.handler({
    })
    assert output is "Hello"

def test_invalid_int_error():
    def index(user_id: int):
        return "Hello"

    base = _BaseRoute(
            handler=index
            )

    res = base.handler({
        })
    assert isinstance(res, Response)
    assert res.status_code == 500

def test_none_request():
    def index(req: Request):
        return req

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        })
    assert res is None

def test_none_header():
    def index(header: Request):
        assert isinstance(header, list)
        return None

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
            "header": [{'name': 'Host', 'value': 'localhost:1234'}, {'name': 'User-Agent', 'value': 'curl/7.79.1'}, {'name': 'Accept', 'value': '*/*'}]
        })

def test_header():
    def index(host: Header, user_agent):
        assert isinstance(host, str)
        assert isinstance(user_agent, str)
        assert user_agent == 'curl/7.79.1'
        assert host == 'localhost:1234'
        return host

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
            "header": [{'name': 'Host', 'value': 'localhost:1234'}, {'name': 'User-Agent', 'value': 'curl/7.79.1'}, {'name': 'Accept', 'value': '*/*'}]
        })
    assert isinstance(res, str)

def test_cookie():
    def index(name: Cookie, age):
        assert isinstance(name, str)
        assert isinstance(age, str)
        return name

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "cookie": ["name=taro;age=20"]
    })
    assert isinstance(res, str)

def test_path_params():
    def index(user_id: Path, user_pwd):
        assert isinstance(user_id, int)
        assert isinstance(user_pwd, str)
        return str(user_id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "path_params": [{'param': 'user_id', 'type': 0, 'value': '100'},{'param': 'user_pwd', 'type': 3, 'value': 'secret'}]
    })
    assert isinstance(res, str)

def test_query_params():
    def index(user_id: Query, user_pwd):
        assert isinstance(user_id, str)
        assert isinstance(user_pwd, str)
        return str(user_id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "query": "user_id=10&user_pwd=secret"
    })
    assert isinstance(res, str)

def test_key_error():
    def index(_test):
        return None

    base = _BaseRoute(
            handler=index
            )

    with pytest.raises(KeyError) as e:
        base._parse_func_args({
        })

def test_same_item():
    def index(user_id):
        print(user_id)
        assert isinstance(user_id, int)
        return str(user_id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "path_params": [{'param': 'user_id', 'type': 0, 'value': '1'},{'param': 'user_pwd', 'type': 3, 'value': 'secret'}],
        "header": [{'name': 'user_id', 'value': '2'}],
        "query": "user_id=3",
        "cookie": "user_id=4",
    })
    assert isinstance(res, str)

# Test multipart/form-data
def test_form_data_without_header():
    class User(FormData):
        id: int
        name: str
        email: str

    def index(user: User):
        assert isinstance(user.id, int)
        assert isinstance(user.name, str)
        assert isinstance(user.email, str)
        return str(user.id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "body": [{'content': bytearray(b'10'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="id";'}]}, {'content': bytearray(b'hello'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="name"'}]}, {'content': bytearray(b'~~~~@gmail.com'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="email"; filename=\'file\''}]}]
    })
    assert isinstance(res, Response)
    assert res.status_code == 400

def test_form_data():
    class User(FormData):
        id: int
        name: str
        email: str

    def index(user: User):
        assert isinstance(user.id, int)
        assert isinstance(user.name, str)
        assert isinstance(user.email, str)
        return str(user.id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "header": [{'name': 'Content-Type', 'value': 'multipart/form-data; boundary=------------------------c4ce68e096ded4d6'}],
        "body": [{'content': bytearray(b'10'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="id";'}]}, {'content': bytearray(b'hello'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="name"'}]}, {'content': bytearray(b'~~~~@gmail.com'), 'header': [{'name': 'Content-Disposition', 'value': 'form-data; name="email"; filename=\'file\''}]}]
    })
    assert isinstance(res, str)
    assert res == str(10)

# Test application/x-www-form-urlencoded
def test_data_without_header():
    class User(FormData):
        id: int
        name: str
        email: str

    def index(user: User):
        assert isinstance(user.id, int)
        assert isinstance(user.name, str)
        assert isinstance(user.email, str)
        return str(user.id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "body": b'id%3D10%26name%3D13%26email%3D~~~~~%40gmail.com'
    })
    assert isinstance(res, Response)
    assert res.status_code == 400

def test_data():
    class User(FormData):
        id: int
        name: str
        email: str

    def index(user: User):
        assert isinstance(user.id, int)
        assert isinstance(user.name, str)
        assert isinstance(user.email, str)
        return str(user.id)

    base = _BaseRoute(
            handler=index
            )
    res = base.handler({
        "header": [{'name': 'Content-Type', 'value': 'application/x-www-form-urlencoded'}],
        "body": b'id%3D10%26name%3D13%26email%3D~~~~~%40gmail.com'
    })
    assert isinstance(res, str)
    assert res == str(10)
