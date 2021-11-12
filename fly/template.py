import jinja2 as j2
import os

"""
Helper function for templating.
"""
def text_render(text, data):
    if text is None or not isinstance(text, str):
        raise TypeError(
            "text must be str type."
        )
    if data is None or not isinstance(data, dict):
        raise TypeError(
            "data must be dict type."
        )

    __tmp = j2.Template(text)
    return __tmp.render(data)

def file_render(file_path, data):
    if file_path is None or os.path.isfile(file_path):
        raise TypeError(
            "file_path must exist."
        )
    if data is None or not isinstance(data, dict):
        raise TypeError(
            "data must be dict type."
        )

    __env = j2.Environment(loader=j2.FileSystemLoader)
    __tmp = __env.get_template(file_path)
    return __tmp.render(data)


class Templates:
    def __init__(self, ):
        pass
