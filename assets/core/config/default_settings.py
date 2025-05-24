from enum import Enum
import json
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
		"hasShownWelcomeModal": False,
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