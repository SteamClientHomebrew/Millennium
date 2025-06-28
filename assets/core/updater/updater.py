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

import json
import requests
from config.manager import get_config
from plugins.plugin_updater import PluginUpdater
from themes.theme_updater import ThemeUpdater
from util.logger import logger

class Updater: 
    def __init__(self):
        self.api_url = "https://steambrew.app/api/checkupdates"

        self.theme_updater = ThemeUpdater()
        self.plugin_updater = PluginUpdater()

        self.cached_updates = None
        self.has_checked_for_updates = False

        if not get_config()["general.checkForPluginAndThemeUpdates"]:
            logger.warn("User has disabled update checking for plugins and themes.")
            return

        self.cached_updates = self.check_for_updates()
        self.has_checked_for_updates = True

    def download_plugin_update(self, id: str, name: str) -> bool:
        return self.plugin_updater.download_plugin_update(id, name)
    
    def download_theme_update(self, native: str) -> bool:
        return self.theme_updater.download_theme_update(native)

    def get_cached_updates(self) -> dict:
        return self.cached_updates
    
    def get_has_checked_for_updates(self) -> bool:
        return self.has_checked_for_updates

    def check_for_updates(self, force = False) -> None:
        try:
            if not force and self.cached_updates:
                logger.log("Using cached updates.")
                return self.cached_updates

            plugins = self.plugin_updater.get_request_body()
            update_query, themes  = self.theme_updater.get_request_body()

            request_body = {}

            if plugins is not None:
                request_body["plugins"] = plugins

            if themes is not None:
                request_body["themes"] = themes

            if not request_body:
                logger.log("No themes or plugins to update!")   
                return { "themes": [], "plugins": [] }

            response = requests.post(self.api_url, json=request_body)
            response.raise_for_status()  # Raise an error for bad responses

            json = response.json()
            
            if "themes" in json and json["themes"] and not "error" in json["themes"]:
                json["themes"] = self.theme_updater.process_updates(update_query, json["themes"])

            return json
        
        except Exception as e:
            logger.log(f"An error occurred while checking for updates: {e}")
            return { "themes": { "error": str(e) } }
        
    def resync_updates(self):
        logger.log("Resyncing updates...")
        self.cached_updates = self.check_for_updates(force=True)
        self.has_checked_for_updates = True
        logger.log("Resync complete.")

        return json.dumps(self.cached_updates)