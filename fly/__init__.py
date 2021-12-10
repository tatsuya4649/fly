import sys
import os
sys.path.append(
	os.path.abspath(os.path.dirname(__file__))
)
from .app import Fly
from .mount import Mount
from .response import *
from .template import Templates, text_render, file_render

__all__ = [
	Fly.__name__,
	Mount.__name__,
    text_render.__name__,
    file_render.__name__,
    Templates.__name__,
]

__version__ = "1.2.1"
