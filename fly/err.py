import sys
import traceback

def display_fly_error(_e):
    print("", file=sys.stderr)
    print("\033[1m" + '  fly error !' + '\033[0m', file=sys.stderr)
    print(f"    {_e}", file=sys.stderr)
    print("", file=sys.stderr)
    _t = traceback.format_exc()
    print(_t, file=sys.stderr)
