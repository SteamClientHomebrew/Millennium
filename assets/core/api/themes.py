import Millennium, os, json # type: ignore

class Colors:

    @staticmethod
    # Get user preferred color scheme on windows
    def get_accent_color_win32():
        from winrt.windows.ui.viewmanagement import UISettings, UIColorType # type: ignore
        # Get the accent color
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
        }
        return json.dumps(color_dictionary)

    @staticmethod
    def get_accent_color_posix():
        print("[posix] get_accent_color has no implementation")

        color_dictionary = {
            'accent': '#000',
            'light1': '#000', 'light2': '#000', 'light3': '#000',
            'dark1': '#000',   'dark2': '#000',   'dark3': '#000',
        }
        return json.dumps(color_dictionary)

    @staticmethod
    def get_accent_color():
        if os.name == 'nt':
            return Colors.get_accent_color_win32()
        else:
            return Colors.get_accent_color_posix()


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
            print(f"Error parsing {skin_json_path}. Invalid JSON format.")
    
    return json.dumps(themes)
