import pytest

from fly.main import *

def test_display_help():
    display_help(fly_command_line, "Hello World")
