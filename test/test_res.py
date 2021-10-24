import sys
import os

sys.path.append(
    os.path.join(
        os.path.dirname(__file__),
        "..",
    )
)

from fly import HTMLResponse

a = HTMLResponse()
print(a.header)
print("Hello")
