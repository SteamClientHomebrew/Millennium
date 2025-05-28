import Millennium, json 
from config.manager import get_config

from util.ipc_functions import GetSystemColors, GetOperatingSystem, SetClipboardContent, GetEnvironmentVar, ChangePluginStatus, GetPluginBackendLogs, ShouldShowUpdateModal

from updater.millennium_updater import *
from plugins.plugin_installer import *
from themes.theme_installer import *
from util.logger import *
from util.decorators import *
from themes.css_analyzer import *
from themes.accent_color import *
from themes.accent_color import *
from plugins.plugins import *
from themes.theme_config import *
from themes.webkit_handler import *

config = get_config()

# Check for updates on plugins and themes.
from updater.updater import Updater
updater = Updater()
# Check for updates on Millennium itself. 
MillenniumUpdater.check_for_updates()

themeInstaller = ThemeInstaller()
pluginInstaller = PluginInstaller()

def GetMillenniumConfig():
    enabledPlugins = [plugin["data"]["name"] for plugin in json.loads(find_all_plugins()) if plugin["enabled"]]

    return {
        "accent_color":              theme_config.get_accent_color(), 
        "conditions":                config["themes.conditions"] if "themes.conditions" in config else None, 
        "active_theme":              theme_config.get_active_theme(),
        "settings":                  config._sanitize(),
        "steamPath":                 Millennium.steam_path(),
        "installPath":               Millennium.get_install_path(),
        "millenniumVersion":         Millennium.version(),
        "enabledPlugins":            enabledPlugins,
        "updates":                   updater.get_cached_updates(),
        "hasCheckedForUpdates":      updater.get_has_checked_for_updates()
    }


class Plugin:
    def _front_end_loaded(self):
        logger.log("SteamUI successfully loaded!")

    # Called when the plugin is initially loaded. 
    def _load(self):     
        logger.log(f"Loading Millennium-Core@{Millennium.version()}")
        Millennium.ready()

    # Called whenever Millennium is unloaded. It can be used to clean up resources, save settings, etc.
    def _unload(self):
        logger.log("Millennium-Core has been unloaded!")
