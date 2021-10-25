import sys
import os
import gc

sys.path.append(
    os.path.join(
        os.path.dirname(__file__),
        "..",
    )
)

from fly import HTMLResponse

a = HTMLResponse()
print("Hello")
gc.collect()
print(gc.garbage)
