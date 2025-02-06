import re
import cssutils
import Millennium, json, os # type: ignore

from api.css_analyzer import ColorTypes, convert_from_hex, convert_to_hex, parse_root
from api.themes import Colors, is_valid
from util.webkit_handler import WebkitHookStore, add_browser_css, add_browser_js, add_conditional_data, parse_conditional_patches
from util.logger import logger


class Config:
    def __init__(self):
        self.config_path = os.path.join(Millennium.get_install_path(), "ext", "themes.json")
        self.config = self.get_config()

        self.create_default("active", "default", str)
        self.create_default("scripts", True, bool)
        self.create_default("styles", True, bool)
        self.create_default("updateNotifications", True, bool)
        self.create_default("accentColor", "DEFAULT_ACCENT_COLOR", str)
    
        # Check if the active them
        self.validate_theme()


        self.set_config(json.dumps(self.config, indent=4))
        self.set_theme_cb()
    

    def validate_theme(self):
        active = self.config["active"]
        if active != "default" and not is_valid(active):
            logger.log(f"Theme '{active}' is invalid. Resetting to default.")
            self.config["active"] = "default"


    def reload_config(self):
        self.config = self.get_config()
        self.validate_theme()
        self.set_config(json.dumps(self.config, indent=4))

        WebkitHookStore().unregister_all()
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
        with open(self.config_path, 'w') as config:
            config.write(dumps)

        self.config = self.get_config()
        

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

        keys = ["Steam-WebKit", "webkitCSS", "RootColors"]
        theme_path = os.path.join(Millennium.steam_path(), "steamui", "skins", name)

        for key in keys:
            if key in theme["data"] and isinstance(theme["data"][key], str):
                add_browser_css(os.path.join(theme_path, theme["data"][key]))

        if "webkitJS" in theme["data"] and isinstance(theme["data"]["webkitJS"], str):
            add_browser_js(os.path.join(theme_path, theme["data"]["webkitJS"]))

        add_conditional_data(theme_path, theme["data"])


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
        
    def change_accent_color(self, new_color: str) -> None:
        self.config["accentColor"] = new_color
        self.set_config(json.dumps(self.config, indent=4))

        return Colors.get_accent_color(self.config["accentColor"])
    
    def reset_accent_color(self) -> None:
        self.config["accentColor"] = "DEFAULT_ACCENT_COLOR"
        self.set_config(json.dumps(self.config, indent=4))

        return Colors.get_accent_color(self.config["accentColor"])


    def set_theme_cb(self):
        self.theme = json.loads(self.get_active_theme())
        self.name = self.get_active_theme_name()

        self.start_webkit_hook(self.theme, self.name)
        self.setup_conditionals(self.theme, self.name, self.config)

        if "data" in self.theme and "RootColors" in self.theme["data"]:
            root_colors = os.path.join(Millennium.steam_path(), "steamui", "skins", self.name, self.theme["data"]["RootColors"])
            self.setup_colors(root_colors)

    def does_theme_use_accent_color(self) -> bool:

        if "data" not in self.theme:
            return False

        parsed_data = parse_conditional_patches(self.theme["data"])

        if self.theme["data"].get("UseDefaultPatches", False):

            # Default patch CSS files. 
            parsed_data.append({ "fileType": "TargetCss", "targetPath": "libraryroot.custom.css" })
            parsed_data.append({ "fileType": "TargetCss", "targetPath": "bigpicture.custom.css"  })
            parsed_data.append({ "fileType": "TargetCss", "targetPath": "friends.custom.css"     })

        def get_all_imports(css_path, visited=None):
            if visited is None:
                visited = set()
            
            # Normalize the file path
            css_path = os.path.abspath(css_path)
            
            # Avoid re-processing files
            if css_path in visited:
                return visited
            
            visited.add(css_path)
            
            # Read the CSS content
            try:
                with open(css_path, "r", encoding="utf-8") as file:
                    css_content = file.read()
            except FileNotFoundError:
                return visited

            # Regex to match @import rules
            import_pattern = re.compile(r'@import\s+(?:url\()?["\']?(.*?)["\']?\)?;')

            # Find all @import paths in the CSS
            for match in import_pattern.finditer(css_content):
                imported_path = match.group(1)
                if not imported_path:
                    continue
                
                # Resolve relative paths
                if not imported_path.startswith(("http://", "https://")):
                    imported_path = os.path.join(os.path.dirname(css_path), imported_path)
                    imported_path = os.path.abspath(imported_path)
                
                get_all_imports(imported_path, visited)
            
            return visited
                

        for patch in parsed_data:
            if patch['fileType'] == 'TargetCss':
                css_path = os.path.join(Millennium.steam_path(), "steamui", "skins", self.name, patch['targetPath'])
                imports = get_all_imports(css_path)

                for import_path in imports:
                    try:
                        with open(import_path, "r", encoding="utf-8") as file:
                            css_content = file.read()

                            if "--SystemAccentColor" in css_content:
                                return True
                    except FileNotFoundError:
                        continue

        return False
                

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