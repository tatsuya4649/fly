
# Benchmarks

List of benchmarks compared "fly" and popular HTTP/App server.

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

### 1Thread, 100Connection, 1000Req/Sec (1minute)

![sev1000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/serv1000.png)
| server | Req/Sec(avg) | Transfer/Sec(avg) | Request in 1m | Timeout error | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 996.66 | 359.15KB | 59801 | 0 | 1.10ms |
| nginx | 1.04k | 373.75KB | 59801 | 0 | 0.93ms |
| Apache(prefork) | 996.66 | 363.20KB | 59801 | 201 | 1.07ms |

### 1Thread, 500Connection, 5000Req/Sec (1minute)
![sev5000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/serv5000.png)
| server | Req/Sec(avg) | Transfer/Sec(avg) | Request in 1m | Timeout error | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 4881.61 | 1.72MB | 292901 | 0 | 3.78ms |
| nginx | 4881.61 | 1.79MB | 292907 | 0 | 2.09ms |
| Apache(prefork) | 4569.96 | 1.63MB | 274210 | 5636 | 1.98s |

### 1Thread, 1000Connection, 10000Req/Sec (1minute)

![sev10000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/serv10000.png)
| server | Req/Sec(avg) | Transfer/Sec(avg) | Request in 1m | Timeout error | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 9572.07 | 3.37MB | 574413 | 0 | 5.06ms |
| nginx | 9048.77 | 3.31MB | 542972 | 1539 | 2.65ms |
| Apache(prefork) | 6944.70 | 2.47MB | 416693 | 16304 | 3.25s |

## Result of Application Server

use simple app server that just returns "Hello World"(HTML Response).

## "Hello World" source code of some server

```python

# fly
from fly import Fly

app = Fly(config_path="bench/bench.conf")

@app.get("/")
def hello():
    return HTMLResponse(200, [], "Hello World")

```

```python

# Flask
from flask import Flask, render_template
import os

app = Flask(__name__)

@app.route("/")
def hello():
    return "Hello World"
```

```python

# Django
from django.http import HttpResponse

def hello(request):
    return HttpResponse("Hello World")
```

```python

# FastAPI
from fastapi import FastAPI
from fastapi.responses import HTMLResponse

app = FastAPI()

@app.get("/")
async def root():
    return HTMLResponse(content="Hello World", status_code=200)

```

### 1Thread, 10Connection, 100Req/Sec (1minute)

![req100](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/req100.png)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 100.01 | 8.11KB | 6001 | 1.05ms |
| uvicorn+FastAPI | 100.01 | 14.06KB | 6001 | 1.95ms |
| gunicorn+flask | 99.86 | 15.99KB | 5992 | 71.62ms |
| gunicorn+Django | 99.86 | 24.38KB | 5992 | 74.07ms |

### 1Thread, 100Connection, 1000Req/Sec (1minute)

![req1000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/req1000.png)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 996.66 | 80.78KB | 59801 | 1.18ms |
| uvicorn+FastAPI | 983.35 | 138.28| 59004 | 18.11ms |
| gunicorn+flask | 995.07 | 159.37KB | 59706 | 69.60ms |
| gunicorn+Django | 995.07 | 242.94KB | 59706 | 79.88ms |

### 1Thread, 500Connection, 5000Req/Sec (1minute)

![req5000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/req5000.png)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 4881.48 | 395.67KB | 292901 | 3.31ms |
| uvicorn+FastAPI | 4869.90 | 684.83KB | 292379 | 333.22ms |
| gunicorn+flask | 2543.33 | 407.33KB | 152601 | 16.64s |
| gunicorn+Django | 1635.38 | 399.26KB | 98124 | 23.11s |

### 1Thread, 1000Connection, 10000Req/Sec (1minute)

![req10000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/req10000.png)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 9572.90 | 775.93KB | 574382 | 4.80ms |
| uvicorn+FastAPI | 4873.64 | 685.35KB | 292780 | 15.19s |
| gunicorn+flask | 2542.29 | 407.16KB | 152540 | 25.40s  |
| gunicorn+Django | 1643.89 | 401.34KB | 98635 | 28.87s |

### 1Thread, 1000Connection, 20000Req/Sec (1minute)

![req20000](https://raw.githubusercontent.com/tatsuya4649/fly/develop/bench/asset/req20000.png)

| server | Req/Sec | Transfer/Sec(avg) | Request in 1m | Latency(avg) |
|:--------:|:---------:|:---------:|:---------:|:---------:|
| **fly** | 18902.98 | 1.50MB | 1134190 | 603.78ms |
| uvicorn+FastAPI | 4828.42 | 679.00KB | 289835 | 25.00s |
| gunicorn+flask | 2569.28 | 411.49KB | 154159 | 30.12s  |
| gunicorn+Django | 1626.83 | 397.18KB | 97612 | 31.88s |
