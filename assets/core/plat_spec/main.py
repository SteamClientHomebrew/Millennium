import os
import Millennium # type: ignore

if os.name == "nt":
    config_path = os.path.join(Millennium.get_install_path(), "ext", "millennium.ini")
elif os.name == "posix":
    config_path = os.path.join(os.path.expanduser("~/.config/millennium/millennium.ini"))