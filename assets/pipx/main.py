import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pipx.config as config
import time
import importlib.metadata
import dev_tools, pip_setup, package_manager
from logger import logger

logger.log("Starting Package Manager...")
config = config.Config()

def get_installed_packages():
    package_names = [dist.metadata["Name"] for dist in importlib.metadata.distributions()]
    return package_names

def main():

    start_time = time.perf_counter()
    pip_setup.verify_pip(config)

    # keep millennium module up to date
    watchdog = dev_tools.mpc(config.get('PackageManager', 'python'))
    watchdog.start(config)

    if config.get('PackageManager', 'use_pip') == 'yes':
        # install missing packages
        package_manager.audit(config)

    elapsed_time_ms = (time.perf_counter()  - start_time) * 1000 
    logger.log(f"Finished in {elapsed_time_ms:.2f} ms")

main()