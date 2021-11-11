import sys
import os

sys.path.append(
    os.path.dirname(__file__)
)
from fly import Fly

app = Fly()

@app.get("/")
def index(request):
    print(request)
    return "Hello, Test"

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

