import Millennium

import time
import json

class Backend:
    @staticmethod 
    def receive_frontend_message(message: str, status: bool, count: int):
        print(f"received: {[message, status, count]}")

        # accepted return types [str, int, bool]
        if count == 69:
            return True
        else:
            return 

def get_steam_path():
    print("getting steam path")
    return Millennium.steam_path()

class Plugin:

    # if steam reloads, i.e. from a new theme being selected, or for other reasons, this is called. 
    # with the above said, that means this may be called more than once within your backends lifespan 
    def _front_end_loaded(self):
        # The front end has successfully mounted in the steam app. 
        # You can now use Millennium.call_frontend_method()

        print("The front end has loaded!")

        start_time = time.time()
        # This throws an error related to the params argument (This is an assumption, Dont hate me :c)
        # value = Millennium.call_frontend_method("classname.method", params=[18, "USA", False])
        value = "nil"
        end_time = time.time()
        
        print(f"classname.method says -> {value} [{round((end_time - start_time) * 1000, 3)}ms]")


    def _load(self):     
        # This code is executed when your plugin loads. 
        # notes: thread safe, running for entire lifespan of millennium

        print(f"bootstrapping plugin, millennium v{Millennium.version()}")
        print(f"Steam Path -> {Millennium.steam_path()}")

        # print("pinging frontend")  
        try:
            # value = Millennium.call_frontend_method("classname.method", params=[18, "USA", False])
            value = "nil"
            print(f"ponged message -> {value}")
        except ConnectionError as error:
            print(error) # "frontend is not loaded!"
        Millennium.ready()


    def _unload(self):
        print("unloading")
