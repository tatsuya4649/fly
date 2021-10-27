import sys
import os
sys.path.append(
	os.path.abspath(os.path.dirname(__file__))
)
from .app import Fly
from .mount import FlyMount
from .response import *
#from .route import FlyRoute

__all__ = [
	Fly.__name__,
#	FlyRoute.__name__,
	FlyMount.__name__,
]

__version__ = "0.0.1"
