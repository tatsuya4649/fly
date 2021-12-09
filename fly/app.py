import sys
import os
import platform
import ctypes
import inspect
import importlib.machinery as imm
import importlib.util as imu
import asyncio
import traceback
import time
import signal

_os = platform.system()
_libfly_dir = os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            'lib'
        )
)

if _os == 'Linux' or _os == 'FreeBSD':
    _lib = 'libfly.so'
elif _os == 'Darwin':
    _lib = 'libfly.dylib'
else:
    raise RuntimeError(f"your system \"{_os}\" not supported.")

_libfly_abs = os.path.abspath(
        os.path.join(
            _libfly_dir,
            _lib
        )
)
_libfly = _libfly_abs
libfly = ctypes.cdll.LoadLibrary(
    _libfly
)

from enum import Enum
from .mount import Mount
from ._fly_server import _fly_server
from .route import Route
from .response import *
from .method import Method

class _Fly:
    def __new__(cls, *args, **kwargs):
        if not hasattr(cls, "_instance"):
            cls._instance = super().__new__(cls)
        return cls._instance


def _signal_interrupt():
    print("\nReceive SIGINT. teminate fly server.", file=sys.stderr, flush=True)
    sys.exit(1)

#import pdb
#pdb.set_trace()
def _watch_dog(fly):
    _f = os.stat(fly._app_filepath)
    try:
        while True:
            _pen = signal.sigpending()
            for i in _pen:
                if i == signal.SIGINT:
                    raise KeyboardInterrupt
                else:
                    signal.raise_signal(i)

            _nf = os.stat(fly._app_filepath)
            if _nf.st_mtime != _f.st_mtime:
                return

            time.sleep(1)
    except KeyboardInterrupt:
        _nf = os.stat(fly._app_filepath)
        if _nf.st_mtime != _f.st_mtime:
            return
        else:
            _signal_interrupt()
    except Exception as e:
        _t = traceback.format_exc()
        print(_t)
        sys.exit(1)

def _display_fly_error(_e):
    print("", file=sys.stderr)
    print("\033[1m" + '  fly error !' + '\033[0m', file=sys.stderr)
    print(f"    {_e}", file=sys.stderr)
    print("", file=sys.stderr)
    _t = traceback.format_exc()
    print(_t, file=sys.stderr)

def _display_master_configure_error(_e):
    _display_fly_error(_e)
    sys.exit(1)

def _run(fly):
    app_filepath = fly._app_filepath

    if not fly.is_test:
        os.system('clear')
    err = False
    try:
        fly._start_server(fly._daemon)
    except _FLY_MASTER_CONFIGURE_ERROR as e:
        _display_master_configure_error(e)
    except Exception as e:
        _display_fly_error(e)
        err = True

    if err:
        _watch_dog(fly)

    if fly.is_debug and not fly.is_daemon:
        print("\n")
        print("Uploading...")

    _spec = imu.spec_from_file_location("_fly", app_filepath)
    _mod = imu.module_from_spec(_spec)
    _spec.loader.exec_module(_mod)
    for _ele in dir(_mod):
        _instance = getattr(_mod, _ele)
        if _instance.__class__ == Fly:
            fly = _instance
    yield fly

def _run_server(fly):
    for i in _run(fly):
        if i._ran is False:
            i.run()

    sys.exit(1)

_FLY_SIGNAL_END = 1
_FLY_RELOAD = 2

class _FLY_MASTER_CONFIGURE_ERROR(Exception):
    pass

class Fly(_Fly, Mount, Route, _fly_server):
    def __init__(
        self,
        config_path=None,
        **kwargs,
    ):
        Mount.__init__(self)
        Route.__init__(self)

        if config_path is not None:
            if not isinstance(config_path, str):
                raise TypeError("config_path must be str type.")

            if not os.path.isfile(config_path):
                raise ValueError("config_path not exiswt.")
            if not os.access(config_path, os.R_OK):
                raise ValueError("config_path read permission denied.")

        self.config_path = config_path
        # for Singleton
        if not hasattr(self, "_fly_server"):
            setattr(self, "_fly_server", True)
            _fly_server.__init__(self)

        if kwargs.get("debug") is not None:
            if not isinstance(kwargs.get("debug"), bool):
                raise TypeError("debug must be bool type.")

            self._debug = True if kwargs.get("debug") is True else False
        else:
            self._debug = True

        self._app_filepath = self._get_application_file_path()
        self._ran = False

    def route(self, path, method):
        if not isinstance(path, str) or not isinstance(method, Method):
            raise TypeError(
                "path must be str type and Method."
            )

        def _route(func):
            if not callable(func):
                raise TypeError(
                    "func must be function object."
                )
            setattr(func, "_application", self)
            setattr(func, "route", {
                "uri": path,
                "func": func,
                "method": method,
            })
            self.register_route(
                uri=path,
                func=func,
                method=method.value,
                debug=self.is_debug
            )
            return func
        return _route

    def get(self, path):
        return self.route(path, Method.GET)

    def post(self, path):
        return self.route(path, Method.POST)

    def head(self, path):
        return self.route(path, Method.HEAD)

    def options(self, path):
        return self.route(path, Method.OPTIONS)

    def put(self, path):
        return self.route(path, Method.PUT)

    def delete(self, path):
        return self.route(path, Method.DELETE)

    def connect(self, path):
        return self.route(path, Method.CONNECT)

    def trace(self, path):
        return self.route(path, Method.TRACE)

    def patch(self, path):
        return self.route(path, Method.PATCH)

    def _get_application_file_path(self):
        stack = inspect.stack()
        for s in stack[1:]:
            m = inspect.getmodule(s[0])
            if m and (__file__ != m.__file__):
                return os.path.abspath(m.__file__)
        return None

    # XXX: require debug
    def run(self, daemon=False, test=False):
        self._daemon = daemon
        self._test = test
        self._loaded = True
        self._ran = True
        self._run()

    def _start_server(self, daemon=False):
        if not isinstance(daemon, bool):
            raise TypeError(
                "daemon must be bool type."
            )
        self._daemon = daemon
        if self.mounts_count == 0 and len(self.routes) == 0:
            raise RuntimeError("fly must have one or more mount points.")
        if self.config_path is not None and not isinstance(self.config_path, str):
            raise TypeError("config_path must be str type.")

        try:
            super()._configure(self.config_path, self.routes)
        except Exception as e:
            raise _FLY_MASTER_CONFIGURE_ERROR(e) from e

        for _p in self.mounts:
            self._mount(_p)

        self._display_explain()
        result = super().run(
            self._app_filepath,
            self.is_debug,
            self.is_daemon,
        )

        if self.is_daemon:
            sys.exit(result)

        if result == _FLY_RELOAD:
            return
            sys.exit(result)
        elif result == _FLY_SIGNAL_END:
            sys.exit(result)
        else:
            # not come here
            print(result)
            sys.exit(result)

    @property
    def is_debug(self):
        return self._debug

    @property
    def is_daemon(self):
        return self._daemon

    @property
    def is_test(self):
        return self._test

    def _run(self):
        _run_server(self)

    def _display_explain(self):
        print("\n", file=sys.stderr)
        print(f"    \033[1m*\033[0m fly Running on \033[1m{self._host}:{self._port}\033[0m (Press CTRL+C to quit)", file=sys.stderr)
        print(f"    \033[1m*\033[0m fly \033[1m{self._reqworker}\033[0m workers", file=sys.stderr)
        if self._app_filepath:
            print(f"    \033[1m*\033[0m Application file: \033[1m{self._app_filepath}\033[0m", file=sys.stderr)
        print(f"    \033[1m*\033[0m SSL: \033[1m{self._ssl}\033[0m", file=sys.stderr)
        if self._ssl:
            print(f"    \033[1m*\033[0m SSL certificate path: \033[1m{self._ssl_crt_path}\033[0m", file=sys.stderr)
            print(f"    \033[1m*\033[0m SSL key path: \033[1m{self._ssl_key_path}\033[0m", file=sys.stderr)
        if self._log is not None:
            print(f"    \033[1m*\033[0m Log directory path: \033[1m{os.path.abspath(self._log)}\033[0m", file=sys.stderr)
            print(f"        \033[1m-\033[0m Access log path(\033[1m fly_access.log \033[0m)", file=sys.stderr)
            print(f"        \033[1m-\033[0m Error log path(\033[1m fly_error.log \033[0m)", file=sys.stderr)
            print(f"        \033[1m-\033[0m Notice log path(\033[1m fly_notice.log \033[0m)", file=sys.stderr)
        else:
            print(f"    \033[1m*\033[0m Log directory path: \033[1m-\033[0m", file=sys.stderr)

        if len(self.mounts) > 0:
            print(f"    \033[1m*\033[0m Mount paths (\033[1m{','.join(self.mounts)}\033[0m)", file=sys.stderr)
            max_len = 0
            for mount in self.mounts:
                max_len = len(mount) if max_len < len(mount) else max_len

                _mn = self._mount_number(mount)
                _mfc = self._mount_files(_mn)
                print("        - {:<{width}s}: files \033[1m{}\033[0m, mount_number \033[1m{mn}\033[0m".format(mount, _mfc, width=max_len, mn=_mn), file=sys.stderr)
        else:
            print(f"    \033[1m*\033[0m Mount paths: \033[1m-\033[0m", file=sys.stderr)

        print("\n", file=sys.stderr)
