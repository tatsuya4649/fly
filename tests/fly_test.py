import sys
import os

sys.path.append(
    os.path.dirname(__file__)
)
from fly import Fly, Response
from fly.response import *

app = Fly()

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
