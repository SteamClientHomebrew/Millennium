# This file is responsible for parsing CSS files and extracting color metadata from them.
# Learn more: https://docs.steambrew.app/developers/themes/theme-colors

import cssutils, enum, json
from util.logger import logger

# Supported color types
class ColorTypes(enum.Enum):
    RawRGB = 1 # 255, 255, 255
    RGB = 2 # rgb(255, 255, 255)
    RawRGBA = 3 # 255, 255, 255, 1.0
    RGBA = 4 # rgba(255, 255, 255, 1.0)
    Hex = 5 # #ffffff
    Unknown = 6 # Unknown


def convert_from_hex(color, type: ColorTypes):
    if type == ColorTypes.Unknown:
        return None

    color = color.lstrip('#')
    r, g, b = [int(color[i:i+2], 16) for i in (0, 2, 4)]

    if type in {ColorTypes.RawRGB, ColorTypes.RGB}:
        if type == ColorTypes.RawRGB:
            return f"{r}, {g}, {b}"
        return f"rgb({r}, {g}, {b})"

    if type in {ColorTypes.RawRGBA, ColorTypes.RGBA}:
        a = int(color[6:8], 16) / 255.0
        if type == ColorTypes.RawRGBA:
            return f"{r}, {g}, {b}, {a:.2f}"
        return f"rgba({r}, {g}, {b}, {a:.2f})"

    return "#" + color


def convert_to_hex(color, type: ColorTypes):
    if type == ColorTypes.Hex:
        return color

    if type in {ColorTypes.RawRGB, ColorTypes.RawRGBA}:
        values = [int(x.strip()) for x in color.split(',')]
    elif type in {ColorTypes.RGB, ColorTypes.RGBA}:
        values = [int(x.strip()) for x in color[color.index('(')+1:-1].split(',')]
    
    if type in {ColorTypes.RawRGB, ColorTypes.RGB}:
        r, g, b = values[:3]
        return f'#{r:02x}{g:02x}{b:02x}'

    if type in {ColorTypes.RawRGBA, ColorTypes.RGBA}:
        r, g, b, a = values
        a = int(float(a) * 255)
        return f'#{r:02x}{g:02x}{b:02x}{a:02x}'

    return None


# Expand short hex color codes (provided by the CSS parser) to full hex color codes
def expand_hex_color(short_hex):
    if len(short_hex) == 4 and short_hex[0] == '#':
        return '#' + ''.join([char*2 for char in short_hex[1:]])
    else:
        return short_hex


# Attempt to parse a raw color value (e.g. "255, 255, 255")
def try_raw_parse(color):
    if ", " not in color:
        return ColorTypes.Unknown

    channels = color.split(", ")

    if len(channels) == 3:
        return ColorTypes.RawRGB
    elif len(channels) == 4:
        return ColorTypes.RawRGBA


# Parse a color value and determine its type
def parse_color(color):
    if color.startswith(('rgb(', 'rgba(', '#')):
        return ColorTypes.RGB if color.startswith('rgb(') else (ColorTypes.RGBA if color.startswith('rgba(') else ColorTypes.Hex)
    return ColorTypes.Unknown if color.startswith(('hsl(', 'hsla(', 'hsv(', 'hsva(')) else try_raw_parse(color)


# Lexically analyze the CSS properties of a style rule and construct a map of property names to their respective comments
def lexically_analyze(rule: cssutils.css.CSSStyleRule):
    propertyMap = {}
    for item in rule.style.cssText.split(";"):
        if "/*" in item and "*/" in item:
            comment = item[item.index("/*"):item.index("*/")+2]
            item = item.replace(comment, "").strip()
            
            name, description = (None, None)
            for line in comment.split("\n"):
                if "@name" in line: name = line.split("@name")[1].strip()
                if "@description" in line: description = line.split("@description")[1].strip()
        else:
            item = item.strip().split(":")[0]
            name, description = (None, None)
        
        propertyMap[item] = (name, description)
    
    return propertyMap


# Generate metadata for each color property in the CSS style rule
def generate_color_metadata(style_rule, property_map):
    result = []
    for property_rule in style_rule.style:
        value = expand_hex_color(property_rule.value)
        type = parse_color(value)
        
        if type == ColorTypes.Unknown:
            logger.error(f"ERROR: Could not parse color value '{value}'")
            continue
        
        name, description = property_map.get(property_rule.name, (None, None)) 
        result.append({
            "color": property_rule.name,
            "name": name,
            "description": description,
            "type": type.value,
            "defaultColor": convert_to_hex(value, type)
        })
    
    return result


# Find the root CSS component in a CSS file
def find_root(file_path):
    # Suppress CSS parsing warnings
    cssutils.log.setLevel(50)
    parsed_style_sheet = cssutils.parseFile(file_path)

    for style_rule in parsed_style_sheet:
        if not isinstance(style_rule, cssutils.css.CSSStyleRule) or style_rule.selectorText != ':root':
            continue
        return style_rule
    return None


# Parse the root CSS component and generate metadata for each color property
def parse_root(file_path):
    root_component = find_root(file_path)
    property_map = lexically_analyze(root_component)
    result = generate_color_metadata(root_component, property_map)

    return json.dumps(result, indent=4)
