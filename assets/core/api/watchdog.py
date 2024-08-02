# This file is used in connection with the Millennium CLI
# brief: Restart and Reload Steam from a flag file
import Millennium, os  # type: ignore
from util.logger import logger

class SteamUtils:

    def handle_message(self, file, event_func):
        try:
            print(f"dispatching {event_func} from {file}")
            Millennium.call_frontend_method(event_func)
            os.remove(file)
        except Exception as e:
            logger.error(f"Failed to reload Steam from CLI flag. Exception: {e}")


    def handle_dispatch(self, event):
        eventMap = {
            self.reload_flag: "WatchDog.startReload",
            self.restart_flag: "WatchDog.startRestart",
            self.restart_force_flag: "WatchDog.startRestartForce"
        }

        if event.src_path in eventMap:
            self.handle_message(event.src_path, eventMap[event.src_path])


    def __init__(self):
        steam_path = Millennium.steam_path()
        self.reload_flag = os.path.join(steam_path, "ext", "reload.flag")
        self.restart_force_flag = os.path.join(steam_path, "ext", "restart_force.flag")
        self.restart_flag = os.path.join(steam_path, "ext", "restart.flag")
        pass
