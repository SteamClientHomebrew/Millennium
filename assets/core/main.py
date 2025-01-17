import threading
import time
import base64

import psutil

from updater.millennium import MillenniumUpdater
start_time = time.perf_counter()

import Millennium, json, os, configparser # type: ignore
from util.logger import logger
import platform

logger.log(f"Loading Millennium-Core@{Millennium.version()}")

from api.css_analyzer import parse_root
from api.themes import Colors
from api.themes import find_all_themes
from api.plugins import find_all_plugins
from api.config import Config, cfg
from util.webkit_handler import WebkitStack, add_browser_css, add_browser_js
from util.theme_installer import WebSocketServer

# This updater module is responsible for updating themes.
# It DOES NOT automatically do so, it is interfaced in the front-end.
from updater.version_control import Updater
updater = Updater()

def get_load_config():
    millennium = configparser.ConfigParser()
    config_path = os.path.join(Millennium.get_install_path(), "ext", "millennium.ini")

    with open(config_path, 'r') as enabled: millennium.read_file(enabled)

    if not 'useInterface' in millennium['Settings']:
        millennium['Settings']['useInterface'] = "yes"

    with open(config_path, 'w') as enabled: millennium.write(enabled)
    config = cfg.get_config()

    return json.dumps({
        "accent_color": json.loads(Colors.get_accent_color(config["accentColor"])), 
        "conditions": config["conditions"] if "conditions" in config else None, 
        "active_theme": json.loads(cfg.get_active_theme()),
        "settings": config,

        "steamPath": Millennium.steam_path(),
        "installPath": Millennium.get_install_path(),

        "useInterface": True if millennium.get('Settings', 'useInterface', fallback='') == "yes" else False,
        "millenniumVersion": Millennium.version(),

        "wantsUpdates": MillenniumUpdater.user_wants_updates().value,
        "wantsNotify": MillenniumUpdater.user_wants_update_notify().value,
    })

def _webkit_accent_color():
    config = cfg.get_config()

    return Colors.get_accent_color(config["accentColor"])

def update_plugin_status(plugin_name: str, enabled: bool):
    Millennium.change_plugin_status(plugin_name, enabled)

def _get_plugin_logs():
    return Millennium.get_plugin_logs()

def _copy_to_clipboard(data: str):
    try:
        import pyperclip
        pyperclip.copy(data)
        return True
    except Exception as e:
        logger.error(f"Failed to copy to clipboard: {e}")
        return False


class Plugin:
    def _front_end_loaded(self):
        logger.log("SteamUI successfully loaded!")

    def _load(self):     
        cfg.set_theme_cb()
        self.server = WebSocketServer()

        try:
            logger.log("Starting the websocket for theme installer...")
            self.server.start()
        except Exception as e:
            logger.error("Failed to start the websocket for theme installer! trace: " + str(e))

        elapsed_time = time.perf_counter() - start_time
        logger.log(f"Ready in {round(elapsed_time * 1000, 3)} milliseconds!")
        Millennium.ready()

        # This CHECKS for updates on Millennium given the user has it enabled in settings.
        # It DOES NOT automatically update, it is interfaced in the front-end.
        MillenniumUpdater.check_for_updates()


    def _unload(self):
        logger.log("Millennium-Core is unloading...")
        self.server.stop()
        logger.log("Millennium-Core has been unloaded!")
