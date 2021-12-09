import click
import os
import sys
import typing
from fly import Fly
import tempfile
import importlib.machinery as imm
import signal
from fly.__init__ import __version__ as __version__

class FlyNotFoundError(Exception):
    pass

@click.command()
@click.version_option(__version__)
@click.argument(
    "app",
    type=click.Path(exists=True),
)
@click.option(
    "-c",
    "--conf-path",
    'conf_path',
    default=None,
    type=click.Path(exists=True),
    help="Configure path of server",
    show_default=False
)
@click.option(
    "--mount-max",
    default=None,
    type=int,
    help="Number of max mount",
    show_default=False
)
@click.option(
    "-w",
    "--workers",
    "workers",
    default=None,
    type=int,
    help="Number of worker processes",
    show_default=False
)
@click.option(
    "--encode_threshold",
    default=None,
    type=int,
    help="Number of encoding threshold value. If content length over this value, content will be encoded.",
    show_default=False
)
@click.option(
    "-h",
    "--host",
    "host",
    default=None,
    type=str,
    help="Bind socket to this host",
    show_default=False
)
@click.option(
    "-p",
    "--port",
    "port",
    default=None,
    type=int,
    help="Bind socket to this port",
    show_default=False
)
@click.option(
    "-s",
    "--ssl",
    "ssl",
    is_flag=True,
    default=None,
    help="Whether to use SSL/TLS Protocol",
    show_default=False
)
@click.option(
    "-x",
    "--ssl-crt-path",
    "ssl_crt_path",
    type=click.Path(exists=True),
    default=None,
    help="SSL/TLS certificate file path",
    show_default=False
)
@click.option(
    "-k",
    "--ssl-key-path",
    "ssl_key_path",
    type=click.Path(exists=True),
    default=None,
    help="SSL/TLS key file path",
    show_default=False
)
@click.option(
    "--pidfile-path",
    type=str,
    default=None,
    help="Path of directory that is made pid file",
    show_default=False
)
@click.option(
    "-i",
    "--index_path",
    "index_path",
    type=str,
    default=None,
    help="Index path of URI",
    show_default=False
)
@click.option(
    "-l",
    "--log_path",
    "log_path",
    type=click.Path(exists=True),
    default=None,
    help="Path os directory that is made log files",
    show_default=False
)
@click.option(
    "--stdout",
    type=str,
    is_flag=True,
    default=None,
    help="Whether to display logs on stdout",
    show_default=False
)
@click.option(
    "--stderr",
    type=str,
    is_flag=True,
    default=None,
    help="Whether to display logs on stderr",
    show_default=False
)
@click.option(
    "-b",
    "--backlog",
    "backlog",
    type=int,
    default=None,
    help="Number of TCP backlog",
    show_default=False
)
@click.option(
    "-m",
    "--max_response_len",
    "max_response_len",
    type=int,
    default=None,
    help="Size of maximum response content length",
    show_default=False
)
@click.option(
    "-r",
    "--max_request_len",
    "max_request_len",
    type=int,
    default=None,
    help="Size of maximum request content length. If request content length over this value, response HTTP 413 (Payload Too Large).",
    show_default=False
)
@click.option(
    "-t",
    "--request_timeout",
    "request_timeout",
    type=int,
    default=None,
    help="Request timeout. If request time over this value, a connection will be forcibly disconnected.",
    show_default=False
)
@click.option(
    "-d",
    "--daemon",
    "daemon",
    is_flag=True,
    default=False,
    help="Whether to treat daemon process.",
    show_default=False
)
@click.option(
    "--test",
    "test",
    is_flag=True,
    default=False,
    show_default=False
)
def fly_command_line(
    app:                str,
    conf_path:          str,
    mount_max:          int,
    workers:            int,
    encode_threshold:   int,
    host:               str,
    port:               int,
    ssl:                bool,
    ssl_crt_path:       str,
    ssl_key_path:       str,
    pidfile_path:       str,
    index_path:         str,
    log_path:           str,
    stdout:             bool,
    stderr:             bool,
    backlog:            int,
    max_response_len:   int,
    max_request_len:    int,
    request_timeout:    int,
    daemon:             bool,
    test:               bool,
):
    """This is fly operation script.\n
    If the same name is specified in the configure file and the argument,
    the argument value will be selected. \n

    detail: https://github.com/tatsuya4649/fly
    """
    kwargs = {
        'app':                  app,
        "conf_path":            os.path.abspath(conf_path) \
                if conf_path is not None else None,
        "daemon":               daemon,
        "mount_max":            mount_max,
        "workers":              workers,
        "encode_threshold":     encode_threshold,
        "host":                 host,
        "port":                 port,
        "ssl":                  ssl,
        "ssl_crt_path":         os.path.abspath(ssl_crt_path) \
                if ssl_crt_path is not None else None,
        "ssl_key_path":         os.path.abspath(ssl_key_path) \
                if ssl_key_path is not None else None,
        "pidfile_path":         pidfile_path,
        "index_path":           index_path,
        "log_path":             os.path.abspath(log_path) \
                if log_path is not None else None,
        "log_stdout":           stdout,
        "log_stderr":           stderr,
        "backlog":              backlog,
        "max_response_content_length":     max_response_len,
        "max_request_length":      max_request_len,
        "request_timeout":      request_timeout,
        "test":                 test,
    }
    run(**kwargs)

def display_help(command, message):
    with click.Context(command) as ctx:
        click.echo(command.get_help(ctx))
        print("")
        print(message)

def run(**kwargs):
    app = kwargs.get("app")
    if app is None:
        raise KeyError("must have 'app' key.")

    _abs_app = os.path.abspath(app)
    try:
        _ml = imm.SourceFileLoader("_app", _abs_app)
        _m = _ml.load_module("_app")
    except SyntaxError as e:
        display_help(fly_command_line, f"\"{app}\" can't import as module.")
        sys.exit(1)
    except RuntimeError as e:
        print(e)
        sys.exit(1)
    except Exception as e:
        print(e)
        sys.exit(1)

    daemon = kwargs.get("daemon")
    if daemon is None:
        raise KeyError("must have 'daemon' key.")
    test = kwargs.get("test")
    config_path = kwargs.get("conf_path")

    _fp = tempfile.NamedTemporaryFile("w+", delete=True)
    try:
        _fp.seek(0, os.SEEK_SET)
        if config_path is not None:
            with open(config_path, "r") as _cf:
                content = _cf.read()
                _fp.write(content)
                _fp.flush()

        # argument parameter write to tempolary file
        CONF_PARAMETES = [
            "mount_max",
            "file_max",
            "workers",
            "workers_max",
            "encode_threshold",
            "host",
            "port",
            "ssl",
            "ssl_crt_path",
            "ssl_key_path",
            "pidfile_path",
            "index_path",
            "log_path",
            "log_stdout",
            "log_stderr",
            "backlog",
            "max_response_content_length",
            "max_request_length",
            "request_timeout"
        ]
        _fp.flush()
        for key in kwargs.keys():
            if key in CONF_PARAMETES and \
                    kwargs.get(key) is not None:
                _par = f"{key} = {kwargs.get(key)}\n"
                _fp.write(_par)
                _fp.flush()

        for _ele in dir(_m):
            _instance = getattr(_m, _ele)
            if _instance.__class__ == Fly:
                if config_path is not None and _instance.config_path is not None and \
                        os.path.abspath(_instance.config_path) != os.path.abspath(config_path):
                    raise ValueError(
                        f"ambiguous config_path. Fly instance config_path({os.path.abspath(_instance.config_path)}) or command line config_path({os.path.abspath(config_path)})."
                    )
                _instance.config_path = os.path.abspath(_fp.name)
                _instance.run(daemon=daemon, test=test)
                sys.exit(1)

        display_help(fly_command_line, f"\"{app}\" can't find Fly instance.")
        sys.exit(1)

    except Exception as e:
        print(e)
    finally:
        _fp.close()
        sys.exit(0)

    display_help(fly_command_line, f"\"{app}\" can't find Fly instance.")

if __name__ == "__main__":
    fly_command_line()
