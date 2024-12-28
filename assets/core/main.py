import threading
import time
start_time = time.perf_counter()

import Millennium, json, os, configparser # type: ignore
from util.logger import logger
import platform

print(f"Loading Millennium-Core@{Millennium.version()}")

from api.css_analyzer import parse_root
from api.themes import Colors
from api.themes import find_all_themes
from api.plugins import find_all_plugins
from api.config import Config, cfg
from util.webkit_handler import WebkitStack, add_browser_css, add_browser_js
from util.theme_installer import WebSocketServer

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
        "useInterface": True if millennium.get('Settings', 'useInterface', fallback='') == "yes" else False,
    })

def _webkit_accent_color():
    config = cfg.get_config()

    return Colors.get_accent_color(config["accentColor"])

def update_plugin_status(plugin_name: str, enabled: bool):
    Millennium.change_plugin_status(plugin_name, enabled)

class Plugin:
    def _front_end_loaded(self):
        print("SteamUI successfully loaded!")

    def _load(self):     
        cfg.set_theme_cb()
        self.server = WebSocketServer()

        try:
            logger.log("Starting the websocket for theme installer...")
            self.server.start()
        except Exception as e:
            logger.error("Failed to start the websocket for theme installer! trace: " + str(e))

        elapsed_time = time.perf_counter() - start_time
        print(f"Ready in {round(elapsed_time * 1000, 3)} milliseconds!")
        Millennium.ready()

    def _unload(self):
        logger.log("Millennium-Core is unloading...")
        self.server.stop()
        logger.log("Millennium-Core has been unloaded!")
