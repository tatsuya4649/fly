
# Benchmarks

List of benchmarks compared "fly" and popular Web/App server.

## Condition

* CPU: Intel(R) Core(TM) i7-4770HQ CPU @ 2.20GHz
* Memory: 15GB
* HTTP benchmarking tool: [wrk](https://github.com/wg/wrk "github of wrk")(HTTP/1.1)
* HTML file: html/bench.html(147bytes)
* SSL cert/key file: conf/server.crt, conf/server.key
* Measurement time: 1m
* Thread: 1
* Connections: 10

## Result of HTTP Server

### HTTP/1.1

| server | Req/Sec(avg) | Transfer/Sec(avg) | Request in 1.0ms | Read in 1.0ms | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | k | MB | 0 | MB | us |
| nginx | 36.01k | 13.63MB | 2153443 | 819.41MB | 277.44us |
| Apache | 42.85k | 15.17MB | 2557572 | 696.66MB | 405.36us |

### HTTP/1.1 over SSL/TLS

| server | Req/Sec(avg) | Transfer/Sec(avg) | Request in 1.0ms | Read in 1.0ms | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 30.35k | 11.49MB | 1812167 | 689.55MB | 329.63us |
| nginx | 30.35k | 11.49MB | 1812167 | 689.55MB | 329.63us |
| Apache | 31.47k | 11.59MB | 1881810 | 696.66MB | 405.36us |

### HTTP/2 over SSL/TLS

## Result of Application Server

use simple app server that just returns "Hello World"(HTML Response).

## "Hello World" source code of some server

```python: fly
from fly import Fly

app = Fly(config_path="bench/bench.conf")

@app.get("/")
def hello():
    return HTMLResponse(200, [], "Hello World")

```

```python: flask
from flask import Flask, render_template
import os

app = Flask(__name__)

@app.route("/")
def hello():
    return "Hello World"
```

```python: Django
from django.http import HttpResponse

def hello(request):
    return HttpResponse("Hello World")
```

```python: Django

from fastapi import FastAPI
from fastapi.responses import HTMLResponse

app = FastAPI()

@app.get("/")
async def root():
    return HTMLResponse(content="Hello World", status_code=200)

```

### 1Thread, 10Connection, 100Req/Sec (1minute)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | k | MB | 0 | MB | us |
| gunicorn+flask | 99.86 | 15.99KB | 5992 | 71.62ms |
| gunicorn+Django | 99.86 | 24.38KB | 5992 | 74.07ms |
| uvicorn+FastAPI | 100.01 | 14.06KB | 6001 | 1.95ms |

### 1Thread, 100Connection, 1000Req/Sec (1minute)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| gunicorn+flask | 995.07 | 159.37KB | 59706 | 69.60ms |
| gunicorn+Django | 995.07 | 242.94KB | 59706 | 79.88ms |
| uvicorn+FastAPI | 983.35 | 138.28| 59004 | 18.11ms |

### 1Thread, 500Connection, 5000Req/Sec (1minute)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| gunicorn+flask | 2543.33 | 407.33KB | 152601 | 16.64s |
| gunicorn+Django | 1635.38 | 399.26KB | 98124 | 23.11s |
| uvicorn+FastAPI | 4869.90 | 684.83KB | 292379 | 333.22ms |

### 1Thread, 1000Connection, 10000Req/Sec (1minute)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| gunicorn+flask | 2542.29 | 407.16KB | 152540 | 25.40s  |
| gunicorn+Django | 1643.89 | 401.34KB | 98635 | 28.87s |
| uvicorn+FastAPI | 4873.64 | 685.35KB | 292780 | 15.19s |
