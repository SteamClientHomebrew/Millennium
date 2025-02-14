import Millennium # type: ignore
import configparser, os, json

try:
    from util.logger import logger
except ImportError:
    pass

def is_enabled(plugin_name: str) -> bool:
    config = configparser.ConfigParser()
    config_path = os.path.join(Millennium.get_install_path(), "ext", "millennium.ini")
    
    with open(config_path, 'r') as enabled:
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
    for subdir in ["ext/data", "plugins"]: # ext/data is internal plugins, plugins is user plugins
        search_dirs(os.path.join(Millennium.get_install_path(), subdir), plugins, logger if _logger is None else _logger)
    return json.dumps(plugins)
