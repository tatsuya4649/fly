
# fly

![python](https://img.shields.io/badge/python-3.6%20%7C%203.7%20%7C%203.8%20%7C%203.9%20%7C%203.10-blue)
![pypi](https://badge.fury.io/py/fly-server.svg)
![test](https://github.com/tatsuya4649/fly/actions/workflows/fly-test.yaml/badge.svg)

fly is lightweight web application framework. This is a library with Python, but all the core parts of the server are implemented in C language to speed up the operation.

* Event driven architecture.(non-blocking network I/O)

* Usable as Web server and Application server.

* Lightweight and fast.

* Since fly as a server by itself, there is no need to prepare a WSGI server or ASGI server.

fly currently supports the following platforms.

| OS	 |  Python3.6 | Python3.7 | Python3.8 | Python3.9 | Python3.10 |
| :-: | :-: | :-: | :-: | :-: | :-: |
| **Linux** | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/linux-py36.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/linux-py37.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/linux-py38.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/linux-py39.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/linux-py310.yml/badge.svg) |
| **macOS** | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/macos-py36.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/macos-py37.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/macos-py38.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/macos-py39.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/macos-py310.yml/badge.svg) |
| **FreeBSD** | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/freebsd-py36.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/freebsd-py37.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/freebsd-py38.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/freebsd-py39.yml/badge.svg) | ![Test](https://github.com/github/tatsuya4649/fly/actions/workflows/freebsd-py310.yml/badge.svg) |

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
<summary>Result</summary>
<div>

```

    * fly Running on 0.0.0.0:1234 (Press CTRL+C to quit)
    * fly 1 workers
    * Application file: app.py
    * SSL: False
    * Log directory path: -
    * Mount paths: -

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

Interface of fly is as simple as possible. Extracted only the necessary parts as Web framework by referring to various Python web frameworks.

But, a core part of fly is implemented by C language, you can't think of it as Python Web framework.

So, if you're looking for **flexible** and **fast** Web framework, should use fly.
## How fast ?

look at the result of [benchmark](https://github.com/tatsuya4649/fly/blob/develop/bench/README.md).

## HTTP version

HTTP1.1, HTTP1.1 over TLS/SSL, HTTP2 over TLS/SSL.

## Contributing

Let's coding.

1. Fork fly.
2. Create a feature branch. (git checkout -b `new-feature`)
3. Commit your changes. (git commit -m `explain of commit`)
4. Push to the bench. (git push origin my-new-feature)
5. Create new pull request.

## Dependencies

* Python >= 3.6

* Openssl >= 1.1.11

* Zlib >= 1.2.11

* libbrotli(Optional) >= 1.0.9

