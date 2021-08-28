from _fly_signal import _fly_signal

class FlySignal(_fly_signal):
	def __init__(
		self,
		*args,
		**kwargs,
	):
		super().__init__()

if __name__ == "__main__":
	s = FlySignal()
	print(dir(s))
	while(True):
		...
