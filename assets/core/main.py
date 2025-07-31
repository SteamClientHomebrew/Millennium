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

import Millennium, json, platform
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

    platform_type = {"Windows": 0, "Linux": 1, "Darwin": 2}.get(platform.system(), 0)

    return {
        "accent_color":                theme_config.get_accent_color(), 
        "conditions":                  config["themes.conditions"] if "themes.conditions" in config else None, 
        "active_theme":                theme_config.get_active_theme(),
        "settings":                    config._sanitize(),
        "steamPath":                   Millennium.steam_path(),
        "installPath":                 Millennium.get_install_path(),
        "millenniumVersion":           Millennium.version(),
        "enabledPlugins":              enabledPlugins,
        "updates":                     updater.get_cached_updates(),
        "hasCheckedForUpdates":        updater.get_has_checked_for_updates(),
        "buildDate":                   Millennium.__internal_get_build_date(),
        "millenniumUpdates":           MillenniumUpdater.has_any_updates(),
        "platformType":                platform_type,
        "millenniumLinuxUpdateScript": os.getenv("MILLENNIUM__UPDATE_SCRIPT_PROMPT", "unknown"),
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
