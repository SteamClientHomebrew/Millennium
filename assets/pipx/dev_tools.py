import importlib
import json
import subprocess
import urllib.request, urllib.error
from logger import logger

class mpc:

    def __init__(self, m_python_bin):
        self.__python_bin = m_python_bin

    def update_millennium(self):
        logger.log("Updating Millennium Dev Tools...")
        subprocess.run([self.__python_bin, "-m", "pip", "install", "--upgrade", "millennium"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def start(self, config):
        if (config.get('PackageManager', 'dev_packages') != 'yes'):
            return

        try:
            if config.get('PackageManager', 'auto_update_dev_packages') == 'no':
                logger.log("Millennium Dev Tools auto-update is disabled.")
                return

            self.update_millennium()
                
        except importlib.metadata.PackageNotFoundError as e:
            logger.warn("Installing Millennium Dev Tools...")
            self.update_millennium()