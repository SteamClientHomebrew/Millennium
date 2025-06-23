from enum import Enum
import json, os
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
		"onMillenniumUpdate": OnMillenniumUpdate.AUTO_INSTALL,
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