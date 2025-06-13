export const API_URL = 'https://steambrew.app';
export const PLUGINS_URL = [API_URL, 'plugins'].join('/');
export const THEMES_URL = [API_URL, 'themes'].join('/');

export const getPluginView = (pluginName: string) => getPluginRenderers()?.[pluginName];

export function getPluginRenderers() {
	if (typeof window?.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS === 'undefined') {
		return {};
	}

	return window?.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS;
}
