from ._fly_types import *
from pydantic import BaseModel as _BaseModel

class FormData(_BaseModel):
    pass

__all__ = [
    Request.__name__,
    Header.__name__,
    Body.__name__,
    Cookie.__name__,
    Path.__name__,
    Query.__name__,
    FormData.__name__,
]
