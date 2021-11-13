import jinja2 as j2
import os

"""
Helper function for templating.
"""
def text_render(text, **data):
    if text is None or not isinstance(text, str):
        raise TypeError(
            "text must be str type."
        )

    __tmp = j2.Template(text)
    return __tmp.render(data)

def file_render(file_path, **data):
    if file_path is None or not os.path.isfile(file_path):
        raise TypeError(
            "file_path must exist."
        )

    __env = j2.Environment(loader=j2.FileSystemLoader(
            os.path.dirname(file_path)
    ))
    __tmp = __env.get_template(os.path.basename(file_path))
    return __tmp.render(data)


"""

fly template engine is based on Jinja2.

detail of Jinja2: https://github.com/pallets/jinja

"""
class Templates:
    def __init__(self, dir_path):
        self._dir_path = dir_path
        self._loader = j2.FileSystemLoader(dir_path)
        self._env = j2.Environment(loader=self._loader, autoescape=True)

    def get_template(self, filename):
        return self._env.get_template(filename)

    def render(self, filename, **data):
        _tmp = self.get_template(filename)

        return _tmp.render(data)
