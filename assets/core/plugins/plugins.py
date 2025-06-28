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

import Millennium # type: ignore
import configparser, os, json
try:
    from util.logger import logger
except ImportError:
    pass

def is_enabled(plugin_name: str) -> bool:
    config = configparser.ConfigParser()

    with open(os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), "millennium.ini"), 'r') as enabled:
        config.read_file(enabled)
    
    enabled_plugins = config.get('Settings', 'enabled_plugins', fallback='').split('|')
    return plugin_name in enabled_plugins


def search_dirs(m_path: str, plugins: list, _logger = None) -> None:
    for theme in [d for d in os.listdir(m_path) if os.path.isdir(os.path.join(m_path, d))]:
        skin_json_path = os.path.join(m_path, theme, "plugin.json")
        if not os.path.exists(skin_json_path):
            continue
        try:
            with open(skin_json_path, 'r') as json_file:
                skin_data = json.load(json_file)
                plugin_name = skin_data.get("name", "undefined_plugin_name")
                plugins.append({'path': os.path.join(m_path, theme), 'enabled': is_enabled(plugin_name), 'data': skin_data})
        except json.JSONDecodeError:
            _logger.error(f"Error parsing {skin_json_path}. Invalid JSON format.")
            

def find_all_plugins(_logger = None) -> str:
    plugins = []

    # MILLENNIUM__DATA_LIB is internal plugins, MILLENNIUM__PLUGINS_PATH is user plugins
    for subdir in [os.getenv("MILLENNIUM__DATA_LIB"), os.getenv("MILLENNIUM__PLUGINS_PATH")]: 
        search_dirs(subdir, plugins, logger if _logger is None else _logger)

    return json.dumps(plugins)


def get_plugin_from_name(plugin_name: str):
    plugins = json.loads(find_all_plugins())

    for plugin in plugins:
        if "data" in plugin and "name" in plugin["data"] and plugin["data"]["name"] == plugin_name:
            return plugin
        
    return None