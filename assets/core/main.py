import builtins
import time, Millennium, json 
initialStartTime = time.perf_counter()

from plugins.plugin_installer import PluginInstaller
from IPC                    import GetSystemColors, GetOperatingSystem, SetClipboardContent, GetEnvironmentVar, ChangePluginStatus, GetPluginBackendLogs, ShouldShowUpdateModal
from updater.millennium_updater   import MillenniumUpdater
from util.logger                  import logger

logger.log(f"Loading Millennium-Core@{Millennium.version()}")

from util.decorators      import *
from api.css_analyzer     import *
from themes.themes           import *
from themes.themes           import *
from plugins.plugins          import *
from api.config           import *
from themes.webkit_handler  import *
from themes.theme_installer import *
from config.ini           import *

# Check for updates on plugins and themes.
from updater.updater import Updater
updater = Updater()
# Check for updates on Millennium itself. 
MillenniumUpdater.check_for_updates()

themeInstaller = ThemeInstaller()
pluginInstaller = PluginInstaller()

def GetMillenniumConfig():
    config = cfg.get_config()
    enabledPlugins = [plugin["data"]["name"] for plugin in json.loads(find_all_plugins()) if plugin["enabled"]]

    return json.dumps({
        "accent_color":              json.loads(Colors.get_accent_color(config["accentColor"])), 
        "conditions":                config["conditions"] if "conditions" in config else None, 
        "active_theme":              json.loads(cfg.get_active_theme()),
        "settings":                  config,
        "steamPath":                 Millennium.steam_path(),
        "installPath":               Millennium.get_install_path(),
        "useInterface":              IniConfig.get_config('Settings', 'useInterface', fallback='yes') == "yes",
        "wantsThemeAndPluginNotify": IniConfig.get_config('Settings', 'updateNotifications', fallback='yes') == "yes",
        "millenniumVersion":         Millennium.version(),
        "wantsUpdates":              MillenniumUpdater.user_wants_updates().value,
        "wantsNotify":               MillenniumUpdater.user_wants_update_notify().value,
        "enabledPlugins":            enabledPlugins,
        "updates":                   updater.check_for_updates(),
    })


class Plugin:
    def _front_end_loaded(self):
        logger.log("SteamUI successfully loaded!")

    def StopWebsocket(self):
        logger.log("Stopping the websocket server...")
        self.server.stop()

    @linux_only
    def StartUnixSocket(self):
        from unix.socket_con import MillenniumSocketServer
        logger.log("Starting UNIX socket server...")

        self.unix_server = MillenniumSocketServer()

        # Function to run the server in a thread
        def start_server():
            self.unix_server.serve()

        # Start the server in a separate thread
        server_thread = threading.Thread(target=start_server, daemon=True)
        server_thread.start()

    @linux_only 
    def StopUnixSocket(self):
        logger.log("Stopping UNIX socket server...")
        self.unix_server.stop()

    # Called when the plugin is initially loaded. 
    def _load(self):     
        self.StartUnixSocket()

        logger.log(f"Ready in {round((time.perf_counter() - initialStartTime) * 1000, 3)} milliseconds!")
        Millennium.ready()

    # Called whenever Millennium is unloaded. It can be used to clean up resources, save settings, etc.
    def _unload(self):
        logger.log("Millennium-Core is unloading...")
        self.StopUnixSocket()

        logger.log("Millennium-Core has been unloaded!")
