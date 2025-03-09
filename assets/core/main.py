import time
start_time = time.perf_counter()

from updater.millennium import MillenniumUpdater

import Millennium, json # type: ignore
from util.logger import logger

logger.log(f"Loading Millennium-Core@{Millennium.version()}")

from api.css_analyzer     import *
from api.themes           import *
from api.themes           import *
from api.plugins          import *
from api.config           import *

from util.webkit_handler  import *
from util.theme_installer import *

from config.ini           import *

# This updater module is responsible for updating themes.
# It DOES NOT automatically do so, it is interfaced in the front-end.
from updater.theme_updater import ThemeUpdater
updater = ThemeUpdater()

def get_load_config():
    config = cfg.get_config()

    return json.dumps({
        "accent_color": json.loads(Colors.get_accent_color(config["accentColor"])), 
        "conditions": config["conditions"] if "conditions" in config else None, 
        "active_theme": json.loads(cfg.get_active_theme()),
        "settings": config,

        "steamPath": Millennium.steam_path(),
        "installPath": Millennium.get_install_path(),

        "useInterface": True if IniConfig.get_config('Settings', 'useInterface', fallback='yes') == "yes" else False,
        "millenniumVersion": Millennium.version(),

        "wantsUpdates": MillenniumUpdater.user_wants_updates().value,
        "wantsNotify": MillenniumUpdater.user_wants_update_notify().value,
    })

    
def get_plugins_dir():
    return os.getenv("MILLENNIUM__PLUGINS_PATH")


def _webkit_accent_color():
    return Colors.get_accent_color(cfg.get_config()["accentColor"])


def update_plugin_status(plugin_name: str, enabled: bool):
    Millennium.change_plugin_status(plugin_name, enabled)


def _get_plugin_logs():
    return Millennium.get_plugin_logs()


def _get_env_var(variable: str):
    return os.getenv(variable)


def _get_os_type():
    # Get OS type and translate to enum types on the frontend
    if os.name == "nt":
        return 0
    elif os.name == "posix":
        return 1
    else:
        raise ValueError("Unsupported OS")
    

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


    def StartWebsocket(self):
        self.server = WebSocketServer()

        try:
            logger.log("Starting the websocket for theme installer...")
            self.server.start()
        except Exception as e:
            logger.error("Failed to start the websocket for theme installer! trace: " + str(e))


    def _load(self):     
        cfg.set_theme_cb()
        self.StartWebsocket()

        elapsed_time = time.perf_counter() - start_time
        logger.log(f"Ready in {round(elapsed_time * 1000, 3)} milliseconds!")
        Millennium.ready()

        if os.name == "posix":
            from unix.socket_con import MillenniumSocketServer
            logger.log("Starting UNIX socket server...")

            self.unix_server = MillenniumSocketServer()

            # Function to run the server in a thread
            def start_server():
                self.unix_server.serve()

            # Start the server in a separate thread
            server_thread = threading.Thread(target=start_server, daemon=True)
            server_thread.start()

        # This CHECKS for updates on Millennium given the user has it enabled in settings.
        # It DOES NOT automatically update, it is interfaced in the front-end.
        MillenniumUpdater.check_for_updates()


    def _unload(self):
        logger.log("Millennium-Core is unloading...")
        self.server.stop()
        logger.log("Websocket server has been stopped!")

        if os.name == "posix":
            logger.log("Stopping UNIX socket server...")
            self.unix_server.stop()

        logger.log("Millennium-Core has been unloaded!")
