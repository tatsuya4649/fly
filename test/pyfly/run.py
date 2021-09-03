import sys
import os
import gc

sys.path.append(
    os.path.join(
        os.path.dirname(__file__),
        "../..",
    )
)

from fly import Fly

fly = Fly()

@fly.get("/")
def a():
    print("a")

del fly
fly = Fly()
#fly.run()
