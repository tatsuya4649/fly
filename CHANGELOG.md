# Change Log

## 1.1.0

Released: 2021/11/13

### Added

### Python

* 'cookie' request key.

* Templaets class with Jinja2 to support template response.

* Templates helper function.

* Route function(@app.get, @app.post, ...) cat receive return value of str type, bytes type.

* set_cookie function in Response class.

* Route function can handle all HTTP method. (GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH)

### C

### Changed

### Python

* delete 'Fly' prefix from some Python class

### C

* handle of 413 response(Payload Too Large)


### Fixed

### C

* Fixed POST handle.

* Fixed a bug where parsing Query parameter.

* etc...
