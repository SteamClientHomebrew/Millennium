# ==================================================
#   _____ _ _ _             _                     
#  |     |_| | |___ ___ ___|_|_ _ _____           
#  | | | | | | | -_|   |   | | | |     |          
#  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
# 
# ==================================================
# 
# Copyright (c) 2025 Project Millennium
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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

    def upgrade_old_config(self):
        # Old config path
        themes_json_path = os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), "themes.json")
        old_config_data = None

        if os.path.isfile(themes_json_path):
            try:
                with open(themes_json_path, "r", encoding="utf-8") as f:
                    old_config_data = json.load(f)
            except Exception as ex:
                logger.error(f"Failed to upgrade old config from {themes_json_path}: {ex}")

        if old_config_data is None:
            return

        # Migrate old config data to the new structure
        if "conditions" in old_config_data and isinstance(old_config_data["conditions"], dict):
            # iterate over each theme in the old config
            for theme_name, conditions in old_config_data["conditions"].items():
                # Ensure the themes.conditions structure exists
                if "themes.conditions" not in get_config():
                    get_config().set("themes.conditions", {}, skip_propagation=True)

                if theme_name not in get_config()["themes.conditions"]:
                    get_config().set(f"themes.conditions.{theme_name}", {}, skip_propagation=True)

                # Iterate over each condition in the theme
                for condition_name, condition_value in conditions.items():
                    logger.log(f"Upgrading condition '{condition_name}' for theme '{theme_name}'")
                    # Set the condition in the new config structure
                    get_config().set(f"themes.conditions.{theme_name}.{condition_name}", condition_value, skip_propagation=True)

        # Migrate colors
        if "colors" in old_config_data and isinstance(old_config_data["colors"], dict):
            # iterate over each theme in the old config
            for theme_name, colors in old_config_data["colors"].items():
                # Upgrade colors for the theme
                if "themes.themeColors" not in get_config():
                    get_config().set("themes.themeColors", {}, skip_propagation=True)

                if theme_name not in get_config()["themes.themeColors"]:
                    get_config().set(f"themes.themeColors.{theme_name}", {}, skip_propagation=True)

                # Iterate over each color in the theme
                for color_name, color_value in colors.items():
                    logger.log(f"Upgrading color '{color_name}' for theme '{theme_name}' with value '{color_value}'")
                    # Set the color in the new config structure
                    get_config().set(f"themes.themeColors.{theme_name}.{color_name}", color_value, skip_propagation=True)

        # Migrate active theme
        if "active" in old_config_data:
            logger.log("Migrating active theme from old config: " + str(old_config_data["active"]))
            get_config().set("themes.activeTheme", old_config_data["active"], skip_propagation=True)

        # Migrate accent color
        if "accentColor" in old_config_data:
            logger.log("Migrating accent color from old config: " + str(old_config_data["accentColor"]))
            get_config().set("general.accentColor", old_config_data["accentColor"], skip_propagation=True)

        # Merge CSS and JS settings
        if "styles" in old_config_data:
            logger.log("Migrating styles from old config: " + str(old_config_data["styles"]))
            get_config().set("themes.allowedStyles", old_config_data["styles"], skip_propagation=True)

        if "scripts" in old_config_data:
            logger.log("Migrating scripts from old config: " + str(old_config_data["scripts"]))
            get_config().set("themes.allowedScripts", old_config_data["scripts"], skip_propagation=True)

        # Delete the old config file
        try:
            os.remove(themes_json_path)
            logger.log(f"Successfully removed old config file: {themes_json_path}")
        except Exception as ex:
            logger.error(f"Failed to remove old config file {themes_json_path}: {ex}")


    def __init__(self):
        self.upgrade_old_config()

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


    def change_color(self, theme: str, color_name: str, new_color: str, color_type: int) -> None:
        logger.log(f"Changing color {color_name} to {new_color} in theme {theme}")

        color_type = ColorTypes(color_type)
        for color in get_config()[f"themes.themeColors.{theme}"]:
            if color != color_name:
                continue
            parsed_color = convert_from_hex(new_color, color_type)
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