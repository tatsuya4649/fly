import os
import yaml

class fly_env:
    """
    configure naming rules:

    environment variable name is "FLY_" + yaml item name(upper)
    """
    def __init__(self, name, env_name, default):
        self.name = name
        self.env_name = env_name
        self.default = default

class fly_config:
    """
    configure of fly store in environment variables.

    parameters:
    @path: configure yaml file.
    """
    def __init__(self, path="conf/fly.yaml"):
        self.path = path
        self.parse_config()

    def parse_config(self):
        with open(self.path, "r") as file:
            obj = yaml.safe_load(file)

        for __item in obj["config"]:
            item_name = list(__item.keys())[0]
            env_name = "FLY_" + str.upper(item_name)
            os.environ[env_name] = f"{__item[item_name]}"

FLY_ENVIRONS = [
    #       ITEM_NAME       ENV_NAME            DEFAULT_VALUE
    fly_env("mount_max",    "FLY_MOUNT_MAX",    10),
    fly_env("file_max",     "FLY_FILE_MAX",     1000),
    fly_env("worker",       "FLY_WORKER_MAX",   100),
]

if __name__ == "__main__":
    __a = fly_config()
    print(os.environ)
