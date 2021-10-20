import pytest
from fly.method import *

def test_method_from_name():
    result = method_from_name("get")
    assert(isinstance(result, FlyMethod))

def test_method_from_name_method():
    result = method_from_name(FlyMethod.GET)
    assert(isinstance(result, FlyMethod))

@pytest.mark.parametrize(
    "method", [
    10,
    10.0,
])
def test_method_from_name_type_error(method):
    with pytest.raises(
        TypeError
    ):
        method_from_name(method)

def test_method_from_name_value_error():
    with pytest.raises(
        ValueError
    ):
        method_from_name("get1")
