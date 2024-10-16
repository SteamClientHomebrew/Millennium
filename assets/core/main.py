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
from util.theme_installer import start_websocket_server, uninstall_theme

if platform.system() == "Windows":
    from updater.version_control import Updater
    updater = Updater()


def inject_webkit_shim(shim_script: str):
    # write the contents to a file
    with open(os.path.join(Millennium.steam_path(), "steamui", "shim.js"), "w") as f:
        f.write(shim_script)
        f.close()

    add_browser_js(os.path.join(Millennium.steam_path(), "shim.js"))

def get_load_config():
    millennium = configparser.ConfigParser()
    config_path = os.path.join(Millennium.get_install_path(), "ext", "millennium.ini")

    with open(config_path, 'r') as enabled: millennium.read_file(enabled)

    if not 'useInterface' in millennium['Settings']:
        millennium['Settings']['useInterface'] = "yes"

    with open(config_path, 'w') as enabled: millennium.write(enabled)
    config = cfg.get_config()

    return json.dumps({
        "accent_color": json.loads(Colors.get_accent_color()), 
        "conditions": config["conditions"] if "conditions" in config else None, 
        "active_theme": json.loads(cfg.get_active_theme()),
        "settings": config,
        "steamPath": Millennium.steam_path(),
        "useInterface": True if millennium.get('Settings', 'useInterface', fallback='') == "yes" else False,
    })

def update_plugin_status(plugin_name: str, enabled: bool):
    Millennium.change_plugin_status(plugin_name, enabled)

class Plugin:
    def _front_end_loaded(self):
        print("SteamUI successfully loaded!")

    def _load(self):     
        cfg.set_theme_cb()

        try:
            websocket_thread = threading.Thread(target=start_websocket_server)
            websocket_thread.start()
        except Exception as e:
            logger.error("Failed to start the websocket for theme installer! trace: " + str(e))

        elapsed_time = time.perf_counter() - start_time
        print(f"Ready in {round(elapsed_time * 1000, 3)} milliseconds!")
        Millennium.ready()

    def _unload(self):
        logger.log("Millennium-Core is unloading...")
    
