
# fly

Python lightweight Web application framework.

* Event driven architecture.

* Usable as Web server and Application server.

* Lightweight and fast.

## Install

```
$ pip install fly-server
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

```

<details>
<summary>mount vs route</summary>
<div>

* mount: use for static content(css, html, js)

* route: use for dynamic content(like CGI)

</div>
</details>

## How fast ?

look at [benchmark](https://github.com/tatsuya4649/fly/blob/develop/bench/README.md).

## HTTP version

HTTP1.1, HTTP1.1 over TLS/SSL, HTTP2 over TLS/SSL.

## Contributing

Let's coding.

1. Fork fly.
2. Create a feature branch. (git checkout -b `new-feature`)
3. Commit your changes. (git commit -m `explain of commit`)
4. Push to the bench. (git push origin my-new-feature)
5. Create new pull request.

