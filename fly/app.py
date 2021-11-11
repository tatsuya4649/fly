import sys
import os
import ctypes

ctypes.cdll.LoadLibrary(
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__),
            "lib/libfly.so"
        )
    )
)
from enum import Enum
from .mount import FlyMount
from ._fly_server import _fly_server
from .route import FlyRoute
from .response import *
from .method import FlyMethod

class _Fly:
    def __new__(cls, *args, **kwargs):
        if not hasattr(cls, "_instance"):
            cls._instance = super().__new__(cls)
        return cls._instance

class Fly(_Fly, FlyMount, FlyRoute, _fly_server):
    def __init__(
        self,
        config_path=None,
        **kwargs,
    ):
        FlyMount.__init__(self)
        FlyRoute.__init__(self)

        if config_path is not None:
            if not isinstance(config_path, str):
                raise TypeError("config_path must be str type.")

            if not os.path.isfile(config_path):
                raise ValueError("config_path not exiswt.")
            if not os.access(config_path, os.R_OK):
                raise ValueError("config_path read permission denied.")

        self.config_path = config_path
        # for Singleton
        if not hasattr(self, "_run_server"):
            setattr(self, "_run_server", True)
            # socket make, bind, listen
            _fly_server.__init__(self)


    def route(self, path, method):
        if not isinstance(path, str) or not isinstance(method, FlyMethod):
            raise TypeError(
                "path must be str type and FlyMethod."
            )

        def __route(func):
            if not callable(func):
                raise TypeError(
                    "func must be function object."
                )
            self.register_route(
                uri=path,
                func=func,
                method=method.value
            )
            return func
        return __route

    def get(self, path):
        return self.route(path, FlyMethod.GET)

    def post(self, path):
        return self.route(path, FlyMethod.POST)

    def head(self, path):
        return self.route(path, FlyMethod.HEAD)

    def options(self, path):
        return self.route(path, FlyMethod.OPTIONS)

    def put(self, path):
        return self.route(path, FlyMethod.PUT)

    def delete(self, path):
        return self.route(path, FlyMethod.DELETE)

    def connect(self, path):
        return self.route(path, FlyMethod.CONNECT)

    def trace(self, path):
        return self.route(path, FlyMethod.TRACE)

    def patch(self, path):
        return self.route(path, FlyMethod.PATCH)

    def run(self, daemon=False):
        if self.mounts_count == 0 and len(self.routes) == 0:
            raise RuntimeError("fly must have one or more mount points.")
        if self.config_path is not None and not isinstance(self.config_path, str):
            raise TypeError("config_path must be str type.")

        try:
            super()._configure(self.config_path, self.routes)
        except Exception as e:
            print(e)
            return

        for __p in self.mounts:
            self._mount(__p)

        print("\n", file=sys.stderr)
        print(f"    \033[1m*\033[0m fly Running on \033[1m{self._host}:{self._port}\033[0m (Press CTRL+C to quit)", file=sys.stderr)
        print(f"    \033[1m*\033[0m fly \033[1m{self._reqworker}\033[0m workers", file=sys.stderr)
        print(f"    \033[1m*\033[0m SSL: \033[1m{self._ssl}\033[0m")
        print(f"    \033[1m*\033[0m SSL certificate path: \033[1m{self._ssl_crt_path}\033[0m")
        print(f"    \033[1m*\033[0m SSL key path: \033[1m{self._ssl_key_path}\033[0m")
        print(f"    \033[1m*\033[0m Log directory path: \033[1m{os.path.abspath(self._log)}\033[0m")

        print(f"    \033[1m*\033[0m Mount paths (\033[1m{','.join(self.mounts)}\033[0m)", file=sys.stderr)
        max_len = 0
        for mount in self.mounts:
            max_len = len(mount) if max_len < len(mount) else max_len

        for mount in self.mounts:
            __mn = self._mount_number(mount)
            __mfc = self._mount_files(__mn)
            print("        - {:<{width}s}: files \033[1m{}\033[0m, mount_number \033[1m{mn}\033[0m".format(mount, __mfc, width=max_len, mn=__mn), file=sys.stderr)
        print("\n", file=sys.stderr)
        super().run(daemon)

    def _debug_run(self):
        if self.mounts_count == 0 and len(self.routes) == 0:
            raise RuntimeError("fly must have one or more mount points.")
        super()._debug_run()
