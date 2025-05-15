from util.logger import logger
import json, os
import Millennium
from themes.themes import Colors
from api.config import cfg

def GetSystemColors():
    return Colors.get_accent_color(cfg.get_config()["accentColor"])


def ChangePluginStatus(pluginJson):
    Millennium.change_plugin_status(json.loads(pluginJson))


def GetPluginBackendLogs():
    return Millennium.get_plugin_logs()


def GetEnvironmentVar(variable: str):
    return os.getenv(variable)


def GetOperatingSystem():
    return {"nt": 0, "posix": 1}.get(os.name, ValueError("Unsupported OS"))
    

def SetClipboardContent(data: str):
    try:
        import pyperclip
        pyperclip.copy(data)
        return True
    except Exception as e:
        logger.error(f"Failed to copy to clipboard: {e}")
        return False
    

has_shown_update_modal = False

def ShouldShowUpdateModal():
    global has_shown_update_modal

    if not has_shown_update_modal:
        has_shown_update_modal = True
        return True
    
    return False