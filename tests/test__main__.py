import pytest
import sys
from fly.__main__ import fly_command_line
from fly import __main__ as fm
import os
from runpy import run_path, run_module

@pytest.fixture(scope="function", autouse=False)
def fly_sys_help():
    args = sys.argv
    new_args = list()
    new_args.append("fly")
    new_args.append("--help")
    sys.argv = new_args
    yield
    sys.argv = args

def test__main__(fly_sys_help):
    with pytest.raises(SystemExit) as e:
        fly_command_line()

    assert(e.value.code == 0)
