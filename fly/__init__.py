import sys
import os
sys.path.append(
	os.path.abspath(os.path.dirname(__file__))
)
from route import FlyRoute

__all__ = [
	FlyRoute.__name__,
]

__version__ = "0.0.1"
