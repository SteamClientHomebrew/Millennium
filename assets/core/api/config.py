import Millennium, json, os # type: ignore

from api.css_analyzer import ColorTypes, convert_from_hex, convert_to_hex, parse_root
from api.themes import is_valid
from api.watchdog import SteamUtils
from util.webkit_handler import WebkitStack, add_browser_css
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class ConfigFileHandler(FileSystemEventHandler):
    def __init__(self, config_instance):
        self.config_instance = config_instance

    def on_modified(self, event):
        self.config_instance.steam_utils.handle_dispatch(event)

        if event.src_path == self.config_instance.config_path:
            self.config_instance.reload_config()

    def on_created(self, event):
        self.config_instance.steam_utils.handle_dispatch(event)


class Config:
    def __init__(self):
        self.steam_utils = SteamUtils()

        self.config_path = os.path.join(Millennium.steam_path(), "ext", "themes.json")
        self.config = self.get_config()

        self.create_default("active", "default", str)
        self.create_default("scripts", True, bool)
        self.create_default("styles", True, bool)
        self.create_default("updateNotifications", True, bool)
    
        # Check if the active them
        self.validate_theme()

        # Setup file watcher
        self.setup_file_watcher()
        self.start_watch()

        self.set_config(json.dumps(self.config, indent=4))
        self.set_theme_cb()
    

    def validate_theme(self):
        active = self.config["active"]
        if active != "default" and not is_valid(active):
            print(f"Theme '{active}' is invalid. Resetting to default.")
            self.config["active"] = "default"


    def setup_file_watcher(self):
        self.event_handler = ConfigFileHandler(self)
        self.observer = Observer()


    def start_watch(self):
        self.observer.schedule(self.event_handler, os.path.dirname(self.config_path), recursive=False)
        self.observer.start()


    def reload_config(self):
        self.config = self.get_config()
        self.validate_theme()
        self.set_config(json.dumps(self.config, indent=4))

        WebkitStack().unregister_all()
        self.set_theme_cb()


    def set_config_keypair(self, key: str, value) -> None:
        self.config[key] = value
        self.set_config(json.dumps(self.config, indent=4))


    def get_config(self) -> dict:
        if not os.path.exists(self.config_path):
            os.makedirs(os.path.dirname(self.config_path), exist_ok=True)
            self.set_config(json.dumps({}, indent=4))

        with open(self.config_path, 'r') as config:
            try:
                return json.loads(config.read())
            except json.JSONDecodeError:
                return {}


    def get_config_str(self) -> str:
        return json.dumps(self.config, indent=4)


    def set_config(self, dumps: str) -> None:
        try:
            self.observer.stop()
            self.observer.join()
        except Exception:
            pass

        with open(self.config_path, 'w') as config:
            config.write(dumps)

        self.config = self.get_config()
        self.setup_file_watcher()
        self.start_watch()
        

    def change_theme(self, theme_name: str) -> None:
        self.config["active"] = theme_name
        self.set_config(json.dumps(self.config, indent=4))
        self.reload_config()


    def create_default(self, key: str, value, type):
        if key not in self.config or not isinstance(self.config[key], type):
            self.config[key] = value


    def get_active_theme_name(self) -> str:
        return self.config["active"]


    def get_active_theme(self) -> str:
        active_theme = self.get_active_theme_name()
        if not is_valid(active_theme):
            return json.dumps({"failed": True})

        file_path = os.path.join(Millennium.steam_path(), "steamui", "skins", active_theme, "skin.json")
        try:
            with open(file_path, 'r') as file:
                return json.dumps({"native": active_theme, "data": json.load(file)})
        except Exception as ex:
            return json.dumps({"failed": True})


    def start_webkit_hook(self, theme, name):
        if "failed" in theme:
            return

        if "Steam-WebKit" in theme["data"] and isinstance(theme["data"]["Steam-WebKit"], str):
            add_browser_css(os.path.join(Millennium.steam_path(), "skins", name, theme["data"]["Steam-WebKit"]))

        if "RootColors" in theme["data"] and isinstance(theme["data"]["RootColors"], str):
            add_browser_css(os.path.join(Millennium.steam_path(), "skins", name, theme["data"]["RootColors"]))


    def setup_colors(self, file_path):
        self.colors = json.loads(parse_root(file_path))
        if "colors" not in self.config:
            self.config["colors"] = {}

        if self.name not in self.config["colors"]:
            self.config["colors"][self.name] = {}

        for color in self.colors:
            color_name = color["color"]
            color_value = convert_from_hex(color["defaultColor"], ColorTypes(color["type"]))
            if color_name not in self.config["colors"][self.name]:
                self.config["colors"][self.name][color_name] = color_value

        self.set_config(json.dumps(self.config, indent=4))


    def get_colors(self):
        def create_root(data: str):
            return f":root {{{data}}}"

        if "colors" not in self.config:
            return create_root("")

        if self.name not in self.config["colors"]:
            return create_root("")

        root: str = ""
        for color in self.config["colors"][self.name]:
            root += f"{color}: {self.config['colors'][self.name][color]};"

        return create_root(root)


    def get_color_opts(self):
        root_colors = self.colors
        for saved_color in self.config["colors"][self.name]:
            for color in root_colors:
                if color["color"] == saved_color:
                    color["hex"] = convert_to_hex(self.config["colors"][self.name][saved_color], ColorTypes(color["type"]))
        return json.dumps(root_colors, indent=4)


    def change_color(self, color_name: str, new_color: str, type: int) -> None:
        type = ColorTypes(type)
        for color in self.config["colors"][self.name]:
            if color != color_name:
                continue
            parsed_color = convert_from_hex(new_color, type)
            self.config["colors"][self.name][color] = parsed_color
            self.set_config(json.dumps(self.config, indent=4))
            return parsed_color


    def set_theme_cb(self):
        self.theme = json.loads(self.get_active_theme())
        self.name = self.get_active_theme_name()

        self.start_webkit_hook(self.theme, self.name)
        self.setup_conditionals(self.theme, self.name, self.config)

        if "data" in self.theme and "RootColors" in self.theme["data"]:
            root_colors = os.path.join(Millennium.steam_path(), "steamui", "skins", self.name, self.theme["data"]["RootColors"])
            self.setup_colors(root_colors)


    def get_conditionals(self):
        return json.dumps(self.config["Conditions"])


    def is_invalid_condition(self, conditions: dict, name: str, condition_name: str, condition_value):
        default_value = condition_value["default"] if "default" in condition_value else list(condition_value["values"].keys())[0]

        if name not in conditions.keys():
            conditions[name] = {}
        if condition_name not in conditions[name].keys():
            conditions[name][condition_name] = default_value
        elif conditions[name][condition_name] not in condition_value["values"].keys():
            conditions[name][condition_name] = default_value


    def change_condition(self, theme, newData, condition):
        try:
            self.config["conditions"][theme][condition] = newData
            self.set_config(json.dumps(self.config, indent=4))
            return json.dumps({"success": True})
        except Exception as ex:
            return json.dumps({"success": False, "message": str(ex)})


    def setup_conditionals(self, theme, name, config):
        if "conditions" not in config:
            config["conditions"] = {}
        if "failed" in theme or "Conditions" not in theme["data"]:
            return
        for condition_name, condition_value in theme["data"]["Conditions"].items():
            self.is_invalid_condition(config["conditions"], name, condition_name, condition_value)

        self.set_config(json.dumps(config, indent=4))


cfg = Config()