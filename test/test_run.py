import sys
import os
import gc
import enum

class FlyContentType(enum.Enum):
    TEXT_HTML = 0

sys.path.append(
    os.path.join(
        os.path.dirname(__file__),
        "..",
    )
)

from fly import Fly, HTMLResponse

fly = Fly(config_path="test/test.conf")

print("Hello fly")

@fly.get("/")
def a(request):
    return HTMLResponse(200, [], "Hello World, fly!!!")

print(fly.routes)

#fly.mount("test")
#fly.mount("lib")
fly.mount("mnt")
fly.run()
#del fly
#fly = Fly()
#fly.run()
