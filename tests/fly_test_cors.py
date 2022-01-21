from fly import Fly
from fly.cors import *


app = Fly()
CORS(app)


@app.get("/")
def index():
    return "Hello, Test"

@app.post("/")
def index_post():
    return "Hello, Test Post"

@allow_cors("*")
@app.get("/user")
def user():
    return "Hello, User"

@allow_cors(
        allow_origin="http://localhost:8080",
        allow_methods=["GET", "POST"],
        allow_headers=["HOST"],
        expose_headers=["User-Agent"],
        allow_credentials=True,
        max_age=1000,
        )
@app.get("/post")
def user():
    return "Hello, User"

@allow_cors("*")
@app.get("/already")
def already():
    res = Response()
    res.add_header("Access-Control-Allow-Origin", "https://localhost:8080")
    return res
