import re
import Millennium, json, os # type: ignore
import traceback

from config.manager import get_config
from themes.css_analyzer import ColorTypes, convert_from_hex, convert_to_hex, parse_root
from themes.accent_color import Colors, find_all_themes, is_valid
from themes.webkit_handler import WebkitHookStore, add_browser_css, add_browser_js, add_conditional_data, parse_conditional_patches
from util.logger import logger


class Config:

    def on_config_change(self, *args, **kwargs):
        try:
            self.validate_theme()

            # Remove all previous patches
            WebkitHookStore().unregister_all()
            self.setup_theme_hooks()
        except Exception as ex:
            logger.error(f"Error in theme config change: {ex}\nTraceback:\n{traceback.format_exc()}")

    def __init__(self):
        self.themes_path = os.path.join(Millennium.steam_path(), "steamui", "skins")
        self.colors = {}

        self.on_config_change() # Initial validation
        get_config().register_listener(self.on_config_change)
    
    def validate_theme(self):
        if "themes.activeTheme" not in get_config():
            get_config().set("themes.activeTheme", "default")
            return

        active = get_config()["themes.activeTheme"]
        if active != "default" and not is_valid(active):
            logger.log(f"Theme '{active}' is invalid. Resetting to default.")
            get_config().set("themes.activeTheme", "default")

    def get_config(self) -> dict:
        return get_config().get_all()


    def set_config(self, key: str, value) -> None:
        get_config().set(key, value)
        

    def change_theme(self, theme_name: str) -> None:
        self.set_config("themes.activeTheme", theme_name)

    def get_accent_color(self) -> str:
        return json.loads(Colors.get_accent_color(get_config()["general.accentColor"]))

    def get_active_theme(self) -> str:
        active_theme = get_config()["themes.activeTheme"]
        if not is_valid(active_theme):
            return {"failed": True}

        file_path = os.path.join(self.themes_path, active_theme, "skin.json")
        try:
            with open(file_path, 'r') as file:
                return {"native": active_theme, "data": json.load(file)}
        except Exception as ex:
            return {"failed": True}


    def start_webkit_hook(self, theme, name):
        if "failed" in theme:
            return

        keys = ["Steam-WebKit", "webkitCSS", "RootColors"]
        theme_path = os.path.join(self.themes_path, name)

        for key in keys:
            if key in theme["data"] and isinstance(theme["data"][key], str):
                add_browser_css(os.path.join(theme_path, theme["data"][key]))

        if "webkitJS" in theme["data"] and isinstance(theme["data"]["webkitJS"], str):
            add_browser_js(os.path.join(theme_path, theme["data"]["webkitJS"]))

        add_conditional_data(theme_path, theme["data"], name)


    def setup_colors(self):
        themes = json.loads(find_all_themes())
        for theme in themes:
            if "failed" in theme:
                continue

            if "data" in theme and "RootColors" in theme["data"]:
                root_colors = os.path.join(Millennium.steam_path(), "steamui", "skins", theme['native'], theme["data"]["RootColors"])

                self.colors[theme['native']] = json.loads(parse_root(root_colors))

                if "themes.themeColors" not in get_config():
                    get_config().set("themes.themeColors", {})

                if theme['native'] not in get_config()["themes.themeColors"]:
                    get_config().set(f"themes.themeColors.{theme['native']}", {})

                for color in self.colors[theme['native']] :
                    color_name = color["color"]
                    color_value = convert_from_hex(color["defaultColor"], ColorTypes(color["type"]))
                    if color_name not in get_config()[f"themes.themeColors.{theme['native']}"]:
                        get_config().set(f"themes.themeColors.{theme['native']}.{color_name}", color_value)


    def get_colors(self):
        def create_root(data: str):
            return f":root {{{data}}}"

        if "themes.themeColors" not in get_config():
            return create_root("")

        if self.name not in get_config()["themes.themeColors"]:
            return create_root("")

        root: str = ""
        for color in get_config()[f"themes.themeColors.{self.name}"]:
            root += f"{color}: {get_config()[f'themes.themeColors.{self.name}.{color}']};"

        return create_root(root)


    def get_color_opts(self, theme_name: str):
        root_colors = self.colors[theme_name]
        for saved_color in get_config()[f"themes.themeColors.{theme_name}"]:
            for color in root_colors:
                if color["color"] == saved_color:
                    color["hex"] = convert_to_hex(get_config()[f"themes.themeColors.{theme_name}.{saved_color}"], ColorTypes(color["type"]))
        return json.dumps(root_colors, indent=4)


    def change_color(self, theme: str, color_name: str, new_color: str, type: int) -> None:
        logger.log(f"Changing color {color_name} to {new_color} in theme {theme}")

        type = ColorTypes(type)
        for color in get_config()[f"themes.themeColors.{theme}"]:
            if color != color_name:
                continue
            parsed_color = convert_from_hex(new_color, type)
            get_config().set(f"themes.themeColors.{theme}.{color}", parsed_color, skip_propagation=True)
            return parsed_color
        
    def change_accent_color(self, new_color: str) -> None:
        get_config()["general.accentColor"] = new_color
        get_config()._auto_save()

        return Colors.get_accent_color(get_config()["general.accentColor"])
    
    def reset_accent_color(self) -> None:
        get_config()["general.accentColor"] = "DEFAULT_ACCENT_COLOR"
        get_config()._auto_save()

        return Colors.get_accent_color(get_config()["general.accentColor"])


    def setup_theme_hooks(self):
        self.theme = self.get_active_theme()
        self.name = get_config()["themes.activeTheme"]

        self.setup_conditionals(self.theme, get_config())
        self.start_webkit_hook(self.theme, self.name)
        self.setup_colors()

    def does_theme_use_accent_color(self) -> bool:

        if "data" not in self.theme:
            return False

        parsed_data = parse_conditional_patches(self.theme["data"], self.name)

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
            except (FileNotFoundError, UnicodeDecodeError, OSError):
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
                    except (FileNotFoundError, UnicodeDecodeError, OSError):
                        continue

        return False
                

    def get_conditionals(self):
        return json.dumps(get_config()["themes.conditions"] if "conditions" in get_config()["themes"] else {}, indent=4)


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
            get_config().set(f"themes.conditions.{theme}.{condition}", newData, skip_propagation=True)
            return json.dumps({"success": True})
        except Exception as ex:
            return json.dumps({"success": False, "message": str(ex)})


    def setup_conditionals(self, theme, config):
        if "themes.conditions" not in config:
            get_config().set("themes.conditions", {})

        themes = json.loads(find_all_themes())
        for theme in themes:
            if "failed" in theme or "Conditions" not in theme["data"]:
                continue

            for condition_name, condition_value in theme["data"]["Conditions"].items():
                self.is_invalid_condition(config["themes.conditions"], theme["native"], condition_name, condition_value)

            get_config().save_to_file()


theme_config = Config()