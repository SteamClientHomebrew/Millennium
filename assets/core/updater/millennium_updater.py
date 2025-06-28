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

import configparser
from enum import Enum
import json
import os
import requests
import Millennium # type: ignore
from config.manager import get_config
from util.logger import logger
import semver

class UpdaterProps(Enum):
    UNSET = 0,
    YES = 1,
    NO = 2

# This module is responsible for checking for updates to the Millennium as a whole, nothing else. 
# i.e this does NOT update themes and/or plugins.
class MillenniumUpdater:

    GITHUB_API_URL = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"
    __has_updates = False
    __latest_version = None

    def parse_version(version: str):
        return version[1:] if version[0] == "v" else version

    def check_for_updates():
        if not get_config()["general.checkForMillenniumUpdates"]:
            logger.warn("User has disabled update checking for Millennium.")
            return 

        logger.log("Checking for updates...")

        try:
            response = requests.get(MillenniumUpdater.GITHUB_API_URL).json()
            
            # Find the latest non-prerelease 
            for release in response:
                if not release["prerelease"]:
                    MillenniumUpdater.__latest_version = release
                    break

            current_version = MillenniumUpdater.parse_version(Millennium.version())
            latest_version = MillenniumUpdater.parse_version(MillenniumUpdater.__latest_version["tag_name"])    
            compare = semver.compare(current_version, latest_version)

            if compare == -1:
                MillenniumUpdater.__has_updates = True
                logger.log("New version of Millennium available: " + MillenniumUpdater.__latest_version["tag_name"])
            
            elif compare == 1:
                logger.warn("Millennium is ahead of the latest release.")
            else:
                logger.log("Millennium is up to date.")


        except requests.exceptions.RequestException as e:
            logger.error(f"Failed to check for updates: {e}")

        except ValueError as e:
            logger.error(f"Failed to parse version: {e}")
    
    def find_asset(release_info: dict):
        if os.name == "nt":
            return next(
                (asset for asset in release_info.get("assets", [])
                if asset.get("name") == f"millennium-{release_info.get('tag_name')}-windows-x86_64.zip"),
                None
            )
        elif os.name == "posix":
            return next(
                (asset for asset in release_info.get("assets", [])
                if asset.get("name") == f"millennium-{release_info.get('tag_name')}-linux-x86_64.tar.gz"),
                None
            )
        else:
            print("Invalid platform, can't find platform specific assets")
            return None

    def has_any_updates():
        # We don't actually handle updates here, that is done from the bootstrap module in %root%/win32
        return {
            "hasUpdate": MillenniumUpdater.__has_updates,
            "newVersion": MillenniumUpdater.__latest_version,
            "platformRelease": MillenniumUpdater.find_asset(MillenniumUpdater.__latest_version) if MillenniumUpdater.__latest_version else None,
        }
    

    def queue_update(downloadUrl: str):
        # This function simply creates a file flag that is read by %root%/win32/main.cc to check if updates should start. 
        with open(os.path.join(Millennium.get_install_path(), "ext", "update.flag"), "w") as update_flag:
            update_flag.write(downloadUrl)

        logger.log("Update queued.")
    
