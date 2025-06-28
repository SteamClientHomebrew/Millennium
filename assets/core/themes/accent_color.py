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

import Millennium, os, json # type: ignore
from util.logger import logger

class Colors:

    @staticmethod
    def get_accent_color_win32():
        from winrt.windows.ui.viewmanagement import UISettings, UIColorType # type: ignore
        settings = UISettings()

        def hex(accent):
            rgba = settings.get_color_value(accent)
            return "#{:02x}{:02x}{:02x}{:02x}".format(rgba.r, rgba.g, rgba.b, rgba.a)
        
        def rgb(accent):
            rgba = settings.get_color_value(accent)
            return f"{rgba.r}, {rgba.g}, {rgba.b}"

        color_dictionary = {
            'accent': hex(UIColorType.ACCENT),        'accentRgb': rgb(UIColorType.ACCENT),
            'light1': hex(UIColorType.ACCENT_LIGHT1), 'light1Rgb': rgb(UIColorType.ACCENT_LIGHT1),
            'light2': hex(UIColorType.ACCENT_LIGHT2), 'light2Rgb': rgb(UIColorType.ACCENT_LIGHT2),
            'light3': hex(UIColorType.ACCENT_LIGHT3), 'light3Rgb': rgb(UIColorType.ACCENT_LIGHT3),
            'dark1':  hex(UIColorType.ACCENT_DARK1),   'dark1Rgb': rgb(UIColorType.ACCENT_DARK1),
            'dark2':  hex(UIColorType.ACCENT_DARK2),   'dark2Rgb': rgb(UIColorType.ACCENT_DARK2),
            'dark3':  hex(UIColorType.ACCENT_DARK3),   'dark3Rgb': rgb(UIColorType.ACCENT_DARK3),
            'originalAccent': hex(UIColorType.ACCENT)
        }
        return json.dumps(color_dictionary)

    @staticmethod
    def get_accent_color_posix():
        color_dictionary = {
            'accent': '#000',
            'light1': '#000', 'light2': '#000', 'light3': '#000',
            'dark1': '#000',   'dark2': '#000',   'dark3': '#000',
            'accentRgb': '0, 0, 0', 'light1Rgb': '0, 0, 0', 'light2Rgb': '0, 0, 0', 'light3Rgb': '0, 0, 0',
            'dark1Rgb': '0, 0, 0', 'dark2Rgb': '0, 0, 0', 'dark3Rgb': '0, 0, 0',
            'originalAccent': '#000'
        }
        return json.dumps(color_dictionary)
    
    @staticmethod
    def extrap_custom_color(accent_color: str) -> str:
        def adjust_hex_color(hex_color, percent=15):
            hex_color = hex_color.lstrip('#')
            r = int(hex_color[0:2], 16)
            g = int(hex_color[2:4], 16)
            b = int(hex_color[4:6], 16)

            if percent < 0: 
                r = max(0, int(r * (1 + percent / 100)))
                g = max(0, int(g * (1 + percent / 100)))
                b = max(0, int(b * (1 + percent / 100)))
            else:
                r = min(255, int(r + (255 - r) * (percent / 100)))
                g = min(255, int(g + (255 - g) * (percent / 100)))
                b = min(255, int(b + (255 - b) * (percent / 100)))

            adjusted_hex = f'#{r:02x}{g:02x}{b:02x}'
            return adjusted_hex

        def hex_to_rgb(hex_color):
            hex_color = hex_color.lstrip('#')
            if len(hex_color) != 6:
                raise ValueError("Invalid hex color format. Use 6 characters (e.g., #RRGGBB).")

            r = int(hex_color[0:2], 16)
            g = int(hex_color[2:4], 16)
            b = int(hex_color[4:6], 16)

            return (r, g, b)
        
        original_accent = json.loads(Colors.get_accent_color_win32() if os.name == 'nt' else Colors.get_accent_color_posix())["originalAccent"]
        
        return json.dumps({
            'accent': accent_color,
            'light1': adjust_hex_color(accent_color, 15),
            'light2': adjust_hex_color(accent_color, 30),
            'light3': adjust_hex_color(accent_color, 45),
            'dark1': adjust_hex_color(accent_color, -15),
            'dark2': adjust_hex_color(accent_color, -30),
            'dark3': adjust_hex_color(accent_color, -45),
            'accentRgb': hex_to_rgb(accent_color),
            'light1Rgb': hex_to_rgb(adjust_hex_color(accent_color, 15)),
            'light2Rgb': hex_to_rgb(adjust_hex_color(accent_color, 30)),
            'light3Rgb': hex_to_rgb(adjust_hex_color(accent_color, 45)),
            'dark1Rgb': hex_to_rgb(adjust_hex_color(accent_color, -15)),
            'dark2Rgb': hex_to_rgb(adjust_hex_color(accent_color, -30)),
            'dark3Rgb': hex_to_rgb(adjust_hex_color(accent_color, -45)),
            'originalAccent': original_accent
        })

    @staticmethod
    def get_accent_color(accent_color):

        if accent_color == "DEFAULT_ACCENT_COLOR":
            if os.name == 'nt':
                return Colors.get_accent_color_win32()
            else:
                return Colors.get_accent_color_posix()
        else:
            return Colors.extrap_custom_color(accent_color)


def is_valid(theme_native_name: str) -> bool:
    file_path = os.path.join(Millennium.steam_path(), "steamui", "skins", theme_native_name, "skin.json")
    if not os.path.isfile(file_path):
        return False

    try:
        with open(file_path, 'r') as file:
            json.load(file)
        return True
    except json.JSONDecodeError:
        return False
    

def find_all_themes() -> str:
    path = Millennium.steam_path() + "/steamui/skins"
    os.makedirs(path, exist_ok=True)
    
    themes = []
    for theme in sorted([d for d in os.listdir(path) if os.path.isdir(os.path.join(path, d))]):
        skin_json_path = os.path.join(path, theme, "skin.json")
        if not os.path.exists(skin_json_path):
            continue
        try:
            with open(skin_json_path, 'r') as json_file:
                themes.append({"native": theme, "data": json.load(json_file)})
        except json.JSONDecodeError:
            logger.log(f"Error parsing {skin_json_path}. Invalid JSON format.")
    
    return json.dumps(themes)
