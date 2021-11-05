
# fly

C/Python lightweight and fast WEP(Web/App) framework.

* Event driven architecture.

* Usable as Web server and Application server.

* Lightweight and fast.

## Install

```
$ pip install fly
```

## Hello World

It is so easy to use fly.

1. import fly and make fly instance.

2. mount directory and registery route.(option)

3. run fly.

```python

from fly import Fly

app = Fly()

# register index route
@app.get("/")
def index(request):
	return HTMLResponse(
		200,
		[],
		"Hello World, fly"
	)

# start server
app.run()

# Output
#    * fly Running on 0.0.0.0:1234 (Press CTRL+C to quit)
#    * fly 1 workers
#    * SSL: False
#    * SSL certificate path: conf/server.crt
#    * SSL key path: conf/server.key
#    * Log directory path: /home/user/fly/log
#    * Mount paths ()

```

<details>
<summary>mount vs route</summary>
<div>

* mount: use for static content(css, html, js)

* route: use for dynamic content(like CGI)

</div>
</details>

## How fast ?

look [benchmark](bench/README.md).

## Why fast ?

## HTTP version

HTTP1.1, HTTP1.1 over TLS/SSL, HTTP2 over TLS/SSL.
