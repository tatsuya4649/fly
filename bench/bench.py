import os
import sys
sys.path.append(
    os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)
    ))
)
from fly import Fly

app = Fly(config_path="bench/bench.conf")
app.mount("bench/html")
app._debug_run()
