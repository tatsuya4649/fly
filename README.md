
# fly

![issue](https://img.shields.io/github/issues/tatsuya4649/fly)
![fork](https://img.shields.io/github/forks/tatsuya4649/fly)
![star](https://img.shields.io/github/stars/tatsuya4649/fly)
![license](https://img.shields.io/github/license/tatsuya4649/fly)
![python](https://img.shields.io/badge/python-3.5%7C3.6%7C3.7%7C3.8%7C3.9%7C3.10-blue)
![pypi](https://badge.fury.io/py/fly-server.svg)

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

@app.get("/")
def index(request):
	return "Hello, fly!"

```

```
$ fly app.py
```

<details>
<summary>result</summary>
<div>

```
    * fly Running on 0.0.0.0:1234 (Press CTRL+C to quit)
    * fly 5 workers
    * SSL: False
    * SSL certificate path: conf/server.crt
    * SSL key path: conf/server.key
    * Log directory path: ~/log
    * Mount paths ()
```


</div>
</details>

<details>
<summary>mount vs route</summary>
<div>

* mount: use for static content(css, html, js)

* route: use for dynamic content(like CGI)

</div>
</details>

## Why fly ?

Interface of fly is as simple as possible. Extracted only the necessary parts as Web framework by referring to variout Python web frameworks.

But, a core part of fly is implemented by C language, you can't think of it as Python Web framework.

So, if you're looking for **flexible** and **fast** Web framework, should use fly.
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

## Running tests

```
# move to fly project directory and run following command.
$ python3 -m pytest
```
