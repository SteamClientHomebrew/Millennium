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

from enum import Enum
import os, platform
from themes.accent_color import Colors

class OnMillenniumUpdate(Enum):
	DO_NOTHING = 0
	NOTIFY = 1
	AUTO_INSTALL = 2

default_config = {
    "general": {
		"injectJavascript": True,
		"injectCSS": True,
		"checkForMillenniumUpdates": True,
		"checkForPluginAndThemeUpdates": True,
		"onMillenniumUpdate": OnMillenniumUpdate.AUTO_INSTALL if platform.system() == "Windows" else OnMillenniumUpdate.NOTIFY,
		"shouldShowThemePluginUpdateNotifications": True,
		"accentColor": "DEFAULT_ACCENT_COLOR",
	},
	"misc": {
		# This used to be an old file Millennium used. its no longer used anymore so its a good indicator on whether Millennium was just installed.
		"hasShownWelcomeModal": os.path.exists(os.path.join(os.getenv("MILLENNIUM__CONFIG_PATH"), "themes.json")),
    },
	"themes": {
        "activeTheme": "default",
		"allowedStyles": True,
		"allowedScripts": True,
    },
	"notifications": {
        "showNotifications": True,
        "showUpdateNotifications": True,
        "showPluginNotifications": True,
    },
}