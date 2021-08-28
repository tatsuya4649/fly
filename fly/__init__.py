import sys
import os
sys.path.append(
	os.path.abspath(os.path.dirname(__file__))
)
from .fly import Fly
from .fs import FlyFs
from .route import FlyRoute

__all__ = [
	Fly.__name__,
	FlyRoute.__name__,
	FlyFs.__name__,
]

__version__ = "0.0.1"
