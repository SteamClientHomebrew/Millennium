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