import sys
import os
import gc

sys.path.append(
    os.path.join(
        os.path.dirname(__file__),
        "..",
    )
)

from fly import Fly

fly = Fly(config_path="test/test.conf")

print("Hello fly")

@fly.get("/")
def a(request):
    response = dict()
    print(request)
    response["header"] = list()
    response["body"] = bytes()
    return response

print(fly.routes)

#fly.mount("test")
#fly.mount("lib")
fly.mount("mnt")
fly.run()
#del fly
#fly = Fly()
#fly.run()
