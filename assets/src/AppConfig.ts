import { OnMillenniumUpdate, SystemAccentColor } from './types';

export interface AppConfig {
	general: {
		injectJavascript: boolean;
		injectCSS: boolean;
		checkForMillenniumUpdates: boolean;
		checkForPluginAndThemeUpdates: boolean;
		onMillenniumUpdate: OnMillenniumUpdate;
		shouldShowThemePluginUpdateNotifications: boolean;
		accentColor: SystemAccentColor;
	};
	misc: {
		hasShownWelcomeModal: boolean;
	};
}
