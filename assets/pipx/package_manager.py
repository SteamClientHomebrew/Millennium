import importlib.metadata
import json
import os
import platform
import subprocess
import threading
from core.api.plugins import find_all_plugins
from logger import logger
import platform

def get_installed_packages():
    package_names = [dist.metadata["Name"] for dist in importlib.metadata.distributions()]
    return package_names

def process_output_handler(proc, outfile, terminate_flag):
    with open(outfile, 'w') as f:
        for line in iter(proc.stdout.readline, b''):
            if terminate_flag[0]:
                break

            line = line.rstrip()
            if line:
                logger.log(line)
                f.write(line + '\n')


def pip(cmd, config):

    python_bin = config.get('PackageManager', 'python')
    pip_logs = config.get('PackageManager', 'pip_logs')
    terminate_flag = [False]

    with open(pip_logs, 'w') as f:
        if platform.system() == 'Windows':
            proc = subprocess.Popen([python_bin, '-m', 'pip'] + cmd + ["--no-warn-script-location"],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    bufsize=1, universal_newlines=True, creationflags=subprocess.CREATE_NO_WINDOW)
        else:
            proc = subprocess.Popen([python_bin, '-m', 'pip'] + cmd + ["--no-warn-script-location"],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    bufsize=1, universal_newlines=True)

        output_handler_thread = threading.Thread(target=process_output_handler, args=(proc, pip_logs, terminate_flag))
        output_handler_thread.start()

        proc.wait()
        terminate_flag[0] = True
        output_handler_thread.join()

        if proc.returncode != 0:
            logger.error(f"PIP failed with exit code {proc.returncode}")


def install_packages(package_names, config):
    pip(["install"] + package_names, config)


def uninstall_packages(package_names, config):
    pip(["uninstall", "-y"] + package_names, config)


_platform = platform.system()

def needed_packages():

    print(f"checking for packages on {_platform}")

    needed_packages = []
    installed_packages = get_installed_packages()

    for plugin in json.loads(find_all_plugins()):
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

                if package_name not in installed_packages:

                    if package_platform == _platform:
                        needed_packages.append(package_name)

    return needed_packages


def audit(config):
    packages = needed_packages()

    if packages:
        logger.log(f"Installing packages: {packages}")
        install_packages(packages, config)
    else:
        logger.log("All required packages are satisfied.")