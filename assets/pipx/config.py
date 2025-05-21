import Millennium # type: ignore
import platform
import configparser, os
from core.util.logger import logger

class Config:

    def __init__(self):
        self.config = configparser.ConfigParser()
        self.config_file = os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), "millennium.ini")

        if os.path.exists(self.config_file):
            try:
                self.config.read(self.config_file)
            except configparser.Error:
                logger.log("error reading config file, resetting...")

        self.setup()

    def set_default(self, section, key, value):

        if section not in self.config:
            self.config[section] = {}

        if key not in self.config[section]:
            self.config[section][key] = value

    def get(self, section, key):
        return self.config[section][key]

    # Get the absolute path to the python binary
    def get_python_path(self):
        return self.PYTHON_BIN
    
    # Get the path to the pip logs
    def get_pip_logs(self):
        return self.PIP_INSTALL_LOGS
    
    # Get the path to the pacman logs
    def get_pacman_logs(self):
        return self.PACMAN_LOGS

    def setup(self):
        _platform = platform.system()

        if _platform == "Windows":
            self.PYTHON_BIN = os.path.join(os.getenv("MILLENNIUM__PYTHON_ENV"), "python.exe")  
        elif _platform == "Linux":
            self.PYTHON_BIN = os.getenv("LIBPYTHON_RUNTIME_BIN_PATH")

            # check if the binary is executable
            if not os.access(self.PYTHON_BIN, os.X_OK):
                # set it as executable
                os.chmod(self.PYTHON_BIN, 0o755)
                logger.log(f"Set {self.PYTHON_BIN} as executable")

        self.PACMAN_LOGS      = os.path.join(os.getenv("MILLENNIUM__LOGS_PATH"), "pacman.log")
        self.PIP_INSTALL_LOGS = os.path.join(os.getenv("MILLENNIUM__LOGS_PATH"), "pip_boot.log")

        self.set_default('PackageManager', 'dev_packages', 'no')
        self.set_default('PackageManager', 'auto_update_dev_packages', 'yes')
        self.set_default('PackageManager', 'use_pip', 'yes')

        with open(self.config_file, 'w') as configfile:
            self.config.write(configfile)