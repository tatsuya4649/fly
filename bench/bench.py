import os
import sys
sys.path.append(
    os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)
    ))
)
from fly import Fly, HTMLResponse

app = Fly(config_path="bench/bench.conf")
app.mount("bench/html")

@app.get("/")
def a(request):
    b = 1/0
    return HTMLResponse(200, [], "Hello World, fly!!!")

app.run()
