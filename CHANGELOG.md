# Change Log

## 1.1.2

Released: 2021/11/21

## Added

* Debug mode (realtime automatic reload)

* Detection of master process app file updates

* configure.ac test update

## Changed

* Change SIGWINCH signal handler.

## Fixed

* Tests code.

* Dockerfile update.

* github action udpate.

### C

* Fixed signal handler.

* Fixed mount handle bug.

* Fixed buffer bug.


* etc...

## 1.1.1

Released: 2021/11/18

## Added

* Added error handling test.

### Python

* HTTPException (fly/exceptions.py). if error occurred or raise HTTPExceptio return it's status code and error content as response body.

* HTTP4xxException

* HTTP5xxException

* debug mode and production mode in Fly (fly/app.py)

* \_BaseResponse class (fly/\_base.py). this is wrapper function of route function.

### C

## Changed

### C

* Changed handling when there was no memory.

## Fixed

### C

* More flexible error handling.

## 1.1.0

Released: 2021/11/13

### Added

### Python

* `fly` command to operate fly server.

* 'cookie' request key.

* Templaets class with Jinja2 to support template response.  * Templates helper function.  * Route function(@app.get, @app.post, ...) cat receive return value of str type, bytes type.

* set\_cookie function in Response class.

* Route function can handle all HTTP method. (GET, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH)

### C

### Changed

### Python

* Delete 'Fly' prefix from some Python class

### C

* Handle of 413 response(Payload Too Large)

- Updated configure.ac

### Fixed

### C

* Fixed daemon process.

* Fixed POST handle.

* Fixed master spawn handler.

* Fixed a bug where parsing Query parameter.

* etc...
