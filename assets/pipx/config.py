import Millennium # type: ignore
import platform
import configparser, os
from core.util.logger import logger
from core.plat_spec.main import config_path

class Config:

    def __init__(self):
        self.config = configparser.ConfigParser()
        self.config_file = config_path

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

    def setup(self):

        _platform = platform.system()

        if _platform == "Windows":
            PYTHON_BIN = os.path.join(os.getenv("MILLENNIUM__PYTHON_ENV"), "python.exe")  
        elif _platform == "Linux":
            PYTHON_BIN = os.path.join(os.getenv("MILLENNIUM__PYTHON_ENV"), "bin", "python3.11")

            # check if the binary is executable
            if not os.access(PYTHON_BIN, os.X_OK):
                # set it as executable
                os.chmod(PYTHON_BIN, 0o755)
                logger.log(f"Set {PYTHON_BIN} as executable")

        PACMAN_LOGS      = os.path.join(os.getenv("MILLENNIUM__LOGS_PATH"), "pacman.log")
        PIP_INSTALL_LOGS = os.path.join(os.getenv("MILLENNIUM__LOGS_PATH"), "pip_boot.log")

        self.set_default('PackageManager', 'dev_packages', 'no')
        self.set_default('PackageManager', 'auto_update_dev_packages', 'yes')
        self.set_default('PackageManager', 'use_pip', 'yes')
        self.set_default('PackageManager', 'python', PYTHON_BIN)
        self.set_default('PackageManager', 'pip_logs', PACMAN_LOGS)
        self.set_default('PackageManager', 'pip_boot', PIP_INSTALL_LOGS)

        with open(self.config_file, 'w') as configfile:
            self.config.write(configfile)