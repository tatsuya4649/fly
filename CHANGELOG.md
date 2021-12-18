# Change Log

## 1.3.0

Released: 2021/12/18

## Added

* Custom Path parameter(ex. /user/{ user\_id: int})

* Hint of parameter of route function.

	ex.
		def index(host: Header, userid: Query):
			return None

* Add new class for hint of parameters of route function. (Request, Header, Body, Path, Query, Cookie, FormData in types.py)

## Fixed

* URI syntax check when registering route function.

## 1.2.2

Released: 2021/12/15

## Fixed

* Fixed bug of exceptions in \_base.py.

## 1.2.1

Released: 2021/12/10

## Changed

* configure default value

## Fixed

* Fixed dictionary of main.py

* Fixed encoding bug

* Fixed emergency error handle

## 1.2.0

Released: 2021/12/9

## Added

* Support macOS

* Support FreeBSD

## Changed

* Changed Python UI.

### C

* Change struct event member type(event\_data, ...) void \* to union.

## Fixed

* signal handler of master/worker.

* handling of chnaged the content of mount point.

## 1.1.3

Released: 2021/11/22

## Added

## Changed

## Fixed

* Fixed mount bug.

* Fixed deallocate memory bug of master process.

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
