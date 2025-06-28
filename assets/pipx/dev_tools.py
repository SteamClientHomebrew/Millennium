# ==================================================
#   _____ _ _ _             _                     
#  |     |_| | |___ ___ ___|_|_ _ _____           
#  | | | | | | | -_|   |   | | | |     |          
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
# 
# ==================================================
# 
# Copyright (c) 2025 Project Millennium
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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