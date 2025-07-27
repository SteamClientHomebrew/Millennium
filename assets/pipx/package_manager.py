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

import importlib.metadata
import json
import os
import platform
import subprocess
import threading
from plugins.plugins import find_all_plugins
from core.util.logger import logger
import platform
from config import Config

def get_installed_packages():
    logger.log("Installed packages:")
    
    package_map = {dist.metadata["Name"]: dist.version for dist in importlib.metadata.distributions()}
    str_packages = [f"{name}=={version}" for name, version in package_map.items()]

    logger.log(" ".join(str_packages))
    return package_map

def process_output_handler(proc, outfile, terminate_flag):
    with open(outfile, 'w') as f:
        for line in iter(proc.stdout.readline, b''):
            if terminate_flag[0]:
                break

            line = line.rstrip()
            if line:
                logger.log(line)
                f.write(line + '\n')


def pip(cmd, config: Config):

    python_bin = config.get_python_path()
    pip_logs = config.get_pip_logs()
    terminate_flag = [False]

    os.makedirs(os.path.dirname(pip_logs), exist_ok=True)

    with open(pip_logs, 'w') as f:
        command = [python_bin, '-m', 'pip'] + cmd + ["--no-warn-script-location"]
        logger.log(f"Running command: {' '.join(command)}")

        proc = subprocess.Popen(
            command,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=1, universal_newlines=True,
            creationflags=subprocess.CREATE_NO_WINDOW if platform.system() == 'Windows' else 0
        )

        output_handler_thread = threading.Thread(target=process_output_handler, args=(proc, pip_logs, terminate_flag))
        output_handler_thread.start()

        proc.wait()
        terminate_flag[0] = True
        output_handler_thread.join()

        if proc.returncode != 0:
            logs_file = os.path.join(os.environ.get("MILLENNIUM__LOGS_PATH", pip_logs), "pipx_log.log")
            str_error = f"Millennium's package manager failed with exit code {proc.returncode}. This is likely a fatal issue, and will cause breaking side effects on loaded plugins, and or Millennium. Please check the log file at {logs_file} for more details."

            if platform.system() == 'Windows':
                import ctypes
                ctypes.windll.user32.MessageBoxW(
                    None,
                    str_error,
                    "Millennium - Package manager error",
                    0x10 | 0x0
                )

            logger.error(f"PIP failed with exit code {proc.returncode}")


def install_packages(package_names, config: Config):
    pip(["install", "--upgrade", "pip", "setuptools", "wheel"], config)
    pip(["install"] + package_names, config)


def uninstall_packages(package_names, config: Config):
    pip(["uninstall", "-y"] + package_names, config)


_platform = platform.system()

def needed_packages():

    logger.log(f"checking for packages on {_platform}")

    needed_packages = []
    installed_packages = get_installed_packages()

    for plugin in json.loads(find_all_plugins(logger)):
        requirements_path = os.path.join(plugin["path"], "requirements.txt")

        if not os.path.exists(requirements_path):
            continue

        with open(requirements_path, "r") as f:
            for package in f.readlines():

                package_name = package.strip() 
                package_platform = _platform

                if "|" in package_name:
                    split = package_name.split(" | ")

                    package_name = split[0]
                    package_platform = split[1]

                # Maintain backwards compatibility with old format that doesn't contain a version
                if package_name.find("==") == -1:
                    if package_name not in installed_packages:
                        if package_platform == _platform:
                            logger.log(f"Package {package_name} is not installed, installing...")
                            needed_packages.append(package_name)   
                # New format, offer better version control support
                else:
                    if package_name.split("==")[0] not in installed_packages:

                        if package_platform == _platform:
                            logger.log(f"Package {package_name} is not installed, installing...")
                            needed_packages.append(package_name)
                
                    elif package_name.split("==")[0] in installed_packages:

                        if package_platform == _platform:
                            if package_name.split("==")[1] != installed_packages[package_name.split("==")[0]]:
                                logger.log(f"Package {package_name} is outdated. Current version: {installed_packages[package_name.split('==')[0]]}, required version: {package_name.split('==')[1]}")
                                needed_packages.append(package_name)


    for package in needed_packages:
        logger.log(f"Package {package} is needed.")

    return needed_packages


def audit(config: Config):
    packages = needed_packages()

    if packages:
        logger.log(f"Installing packages: {packages}")
        install_packages(packages, config)
    else:
        logger.log("All required packages are satisfied.")