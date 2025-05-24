from util.logger import logger
import json, os
import Millennium
from themes.accent_color import Colors
from config.manager import get_config

def GetSystemColors():
    return Colors.get_accent_color(get_config()["general.accentColor"])


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