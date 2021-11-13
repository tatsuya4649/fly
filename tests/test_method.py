import pytest
from fly.method import *

def test_method_from_name():
    result = method_from_name("GET")
    assert(isinstance(result, Method))

@pytest.mark.parametrize(
    "method", [
    "get",
    "gET"
])
def test_method_from_name_value_error(method):
    with pytest.raises(
        ValueError
    ):
        assert(method_from_name(method));

def test_method_from_name_method():
    result = method_from_name(Method.GET)
    assert(isinstance(result, Method))

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
