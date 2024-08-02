import Millennium
import platform
import configparser, os

class Config:

    def __init__(self):
        self.config = configparser.ConfigParser()
        self.config_file = os.path.join(Millennium.steam_path(), "ext", "millennium.ini")

        if os.path.exists(self.config_file):
            try:
                self.config.read(self.config_file)
            except configparser.Error:
                print("error reading config file, resetting...")

        self.setup()

    def set_default(self, section, key, value):

        if section not in self.config:
            self.config[section] = {}

        if key not in self.config[section]:
            self.config[section][key] = value

    def get(self, section, key):
        return self.config[section][key]

    def setup(self):

        LOCALS = os.path.join(Millennium.steam_path(), "ext", "data")

        _platform = platform.system()

        if _platform == "Windows":
            PYTHON_BIN = os.path.join(LOCALS, "cache", "python.exe")  
        elif _platform == "Linux":
            PYTHON_BIN = os.path.join(os.environ.get("HOME"), ".pyenv", "versions", "3.11.8", "bin", "python")

        PACMAN_LOGS      = os.path.join(LOCALS, "logs", "pacman.log")
        PIP_INSTALL_LOGS = os.path.join(LOCALS, "logs", "pip_boot.log")

        self.set_default('PackageManager', 'dev_packages', 'no')
        self.set_default('PackageManager', 'auto_update_dev_packages', 'yes')
        self.set_default('PackageManager', 'use_pip', 'yes')
        self.set_default('PackageManager', 'python', PYTHON_BIN)
        self.set_default('PackageManager', 'pip_logs', PACMAN_LOGS)
        self.set_default('PackageManager', 'pip_boot', PIP_INSTALL_LOGS)

        with open(self.config_file, 'w') as configfile:
            self.config.write(configfile)