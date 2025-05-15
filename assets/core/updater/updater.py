import json
import requests
from plugins.plugin_updater import PluginUpdater
from themes.theme_updater import ThemeUpdater
from util.logger import logger
from config.ini import IniConfig

class Updater: 
    def __init__(self):
        self.api_url = "https://steambrew.app/api/checkupdates"

        self.theme_updater = ThemeUpdater()
        self.plugin_updater = PluginUpdater()

        self.cached_updates = None
        self.cached_updates = self.check_for_updates()

    def download_plugin_update(self, id: str, name: str) -> bool:
        return self.plugin_updater.download_plugin_update(id, name)
    
    def download_theme_update(self, native: str) -> bool:
        return self.theme_updater.download_theme_update(native)

    def set_update_notifs_status(self, status: bool):
        IniConfig.set_config("Settings", "updateNotifications", "yes" if status else "no")
        return True

    def check_for_updates(self, force = False) -> None:
        try:
            if not force and self.cached_updates:
                logger.log("Using cached updates.")
                return self.cached_updates

            plugins = self.plugin_updater.get_request_body()
            update_query, themes  = self.theme_updater.get_request_body()

            request_body = {
                "plugins": plugins,
                "themes": themes
            }

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
        logger.log("Resync complete.")

        return json.dumps(self.cached_updates)