import sys
import os

sys.path.append(
    os.path.dirname(__file__)
)
from fly import Fly, Response
from fly.response import *
from fly.types import *

app = Fly()
app.mount("tests/mnt")
app.mount("tests/mnt2")

@app.get("/")
def index(request):
    print(request)
    return "Hello, Test"

@app.get("/user")
def index(request):
    print(request)
    return "Hello, user"

@app.head("/")
def index_head(request):
    return None

@app.head("/head_body")
def head_body(request):
    return "500 Error"

@app.get("/empty")
def empty(request):
    return None

@app.post("/")
def index_post(request):
    print(request)
    return "Success, POST"

@app.put("/")
def index_put(request):
    return "Success, PUT"

@app.delete("/")
def index_delete(request):
    return "Success, DELETE"

@app.patch("/")
def index_patch(request):
    return "Success, PATCH"

@app.options("/")
def index_option(request):
    return "Success, OPTIONS"

@app.get("/query")
def query(request):
    if request.get("query") is None:
        raise RuntimeError
    print(request.get("query"))
    return None

@app.get("/cookie")
def cookie(request):
    print(request)
    cookie = request.get("cookie")
    if cookie is None or len(cookie) == 0:
        raise RuntimeError

    return "OK, Cookie"

@app.get("/set_cookie")
def set_cookie(request):
    res = Response(200)
    res.set_cookie("id", 100)
    return res

@app.get("/raise_404")
def raise_404(request):
    raise HTTP404Exception

class User(FormData):
    userid: int
    username: str

@app.get("/types")
def types_index(request: Request):
    return "Success, request"

@app.get("/types/body")
def types_body(body: Request):
    return "Success, body"

@app.get("/types/header")
def types_header(header: Request):
    return "Success, header"

@app.get("/types/cookie")
def types_cookie(cookie: Request):
    return "Success, cookie"

@app.get("/types/query")
def types_cookie(query: Request):
    return "Success, query"

@app.get("/types/path_params")
def types_cookie(path_params: Request):
    return "Success, path_params"

@app.get("/types/no_request")
def types_norequest(norequest: Request):
    assert norequest is None
    return "Success, norequest"

@app.get("/types/no_request_error")
def types_norequest(norequest):
    return "Success, norequest"

@app.get("/types/multi")
def types_multi(request: Request, body: Request, header: Request):
    return "Success, Multi"

@app.get("/types/header_item")
def types_header_item(hello: Header):
    return f"Success, Header item {hello}"

@app.post("/types/body_item")
def types_body_item(user: User):
    return f"Success, Header item {user.userid}:{user.username}"

@app.get("/types/cookie_item")
def types_cookie_item(user_id: Cookie, username: Cookie, userpwd):
    assert user_id is not None
    assert username is not None
    assert userpwd is not None
    return f"Success, Cookie item {user_id}"

@app.get("/types/query_item")
def types_query_item(user_id: Query, username: Query, userpwd):
    print(user_id)
    print(username)
    print(userpwd)
    assert user_id is not None
    assert username is not None
    assert userpwd is not None
    return f"Success, Query item {user_id}"

@app.get("/types/path_param_item/{user_id: int}")
def types_path_param_item(user_id: Path):
    return f"Success, path_param item: {user_id}"

@app.get("/types/path_param_item_default/{postid}")
def types_path_param_default(postid: Path):
    return f"Success, path_param_default: {postid}"

