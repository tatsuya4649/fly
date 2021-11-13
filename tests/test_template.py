import pytest
from fly.template import *

def test_text_render():
    txt = text_render("Hello {{ world }}", world="World")
    assert(isinstance(txt, str))
    assert(txt == "Hello World")

def test_file_render():
    txt = file_render("tests/fly_test.j2", world="World")
    assert(isinstance(txt, str))
    assert(txt == "Hello World")

def test_template_init():
    Templates(dir_path="tests")

@pytest.fixture(scope="function", autouse=False)
def template_init():
    tmp = Templates(dir_path="tests")
    yield tmp

def test_get_template(template_init):
    tmp = template_init.get_template("fly_test.j2")
    print(tmp)
    print(dir(tmp))

def test_render(template_init):
    res = template_init.render("fly_test.j2", world="World")
    assert(isinstance(res, str))
