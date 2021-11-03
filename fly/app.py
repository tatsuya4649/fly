import sys
import os
from enum import Enum
from .mount import FlyMount
from ._fly_server import _fly_server
from .signal import FlySignal
from .route import FlyRoute
from .env import FlyEnv
from .response import *

class FlyMethod(Enum):
    GET = "GET"
    POST = "POST"

class Fly(FlySignal, FlyMount, FlyRoute, _fly_server):
    def __init__(
        self,
        config_path=None,
    ):
        FlySignal.__init__(self)
        FlyMount.__init__(self)
        FlyRoute.__init__(self)

        # socket make, bind, listen
        _fly_server.__init__(
            self,
            config_path = config_path,
        )

    def mount(self, path):
        super().mount(path)

        self._mount(path)

    def route(self, path, method):
        if not isinstance(path, str):
            raise TypeError(
                "path must be str type."
            )
        if not isinstance(method, FlyMethod):
            raise TypeError(
                "method must be FlyMethod."
            )
        def __route(func):
            if not callable(func):
                raise TypeError(
                    "func must be function object."
                )
            self._register_route(
                func,
                path,
                method.value
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

    def run(self, daemon=False):
        if self.mounts_count == 0 and len(self.routes) == 0:
            raise RuntimeError("fly must have one or more mount points.")

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
        if self.mounts_count == 0:
            raise RuntimeError("fly must have one or more mount points.")
        super()._debug_run()
