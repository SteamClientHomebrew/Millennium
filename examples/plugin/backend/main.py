import Millennium, PluginUtils # type: ignore
from settings import PluginSettings
from logger import logger
import time

class Backend:
    @staticmethod 
    def receive_frontend_message(message: str, status: bool, count: int):
        logger.log(f"received: {[message, status, count]}")

        # accepted return types [str, int, bool]
        if count == 69:
            return True
        else:
            return False

def get_steam_path():
    logger.log("getting steam path")
    return Millennium.steam_path()

class Plugin:

    # if steam reloads, i.e. from a new theme being selected, or for other reasons, this is called. 
    # with the above said, that means this may be called more than once within your backends lifespan 
    def _front_end_loaded(self):
        # The front end has successfully mounted in the steam app. 
        # You can now use Millennium.call_frontend_method()
        logger.log("The front end has loaded!")

        start_time = time.time()
        value = Millennium.call_frontend_method("classname.method", params=[18, "USA", False])
        end_time = time.time()
        
        logger.log(f"classname.method says -> {value} [{round((end_time - start_time) * 1000, 3)}ms]")


    def _load(self):     

        PluginSettings.numberTextInput += 1
        logger.log("PluginSettings.numberTextInput: " + str(PluginSettings.numberTextInput))

        PluginSettings.numberTextInput += 1
        logger.log("PluginSettings.numberTextInput: " + str(PluginSettings.numberTextInput))

        # This code is executed when your plugin loads. 
        # notes: thread safe, running for entire lifespan of millennium
        logger.log(f"bootstrapping example plugin, millennium {Millennium.version()}")

        try:
            # This will fail to call the frontend as it is not yet loaded. It is only safe to call the frontend after _front_end_loaded is called.
            value = Millennium.call_frontend_method("classname.method", params=[18, "USA", False])
            logger.log(f"ponged message -> {value}")

        # Frontend not yet loaded
        except ConnectionError as error:
            logger.error(f"Failed to ping frontend, {error}")
            
        Millennium.ready() # this is required to tell Millennium that the backend is ready.


    def _unload(self):
        logger.log("unloading")