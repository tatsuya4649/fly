import sys
import os
sys.path.append(
	os.path.abspath(os.path.dirname(__file__))
)
from .app import Fly
from .response import *

__all__ = [
	Fly.__name__,
]

__version__ = "1.0.0"
