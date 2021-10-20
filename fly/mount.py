import os

class FlyMount:
    def __init__(self):
        self._mounts = list()

    @property
    def mounts(self):
        return self._mounts

    @property
    def mounts_count(self):
        return len(self.mounts)

    def mount(self, path):
        if not isinstance(path, str):
            raise TypeError(
                "path must be str type."
            )

        abspath = os.path.abspath(path)
        if not os.path.isdir(abspath):
            raise ValueError(
                f"invalid path \"{path}\"."
            )
        self._mounts.append(abspath)
