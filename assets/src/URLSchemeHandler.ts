import { callable, Millennium } from '@steambrew/client';
import { OpenSettingsTab, SettingsTabs } from './ui/Settings';
import { PluginComponent, ThemeItem } from './types';
import { Logger } from './Logger';

const DEFAULT_THEME_NAME = '__default__';

const ChangeTheme = callable<[{ theme_name: string }]>('cfg.change_theme');
const FindAllThemes = callable<[], string>('find_all_themes');
const FindAllPlugins = callable<[], string>('find_all_plugins');
const UpdatePluginStatus = callable<[{ pluginJson: string }], any>('ChangePluginStatus');

/**
 * steam://millennium URL support.
 *
 * Example:
 * "steam://millennium" -> Open Millennium dialog
 * "steam://millennium/updates" -> Open the "Updates" tab
 * "steam://millennium/themes/disable" -> Use default theme
 * "steam://millennium/themes/enable/aerothemesteam" -> Enable the Office 2007 theme using its internal name
 * "steam://millennium/plugins/disable/steam-db" -> Disable the SteamDB plugin using its internal name
 * "steam://millennium/plugins/disable" -> Disable all plugins
 */
export const OnRunSteamURL = async (_: number, url: string) => {
	const [tab, action, item] = url.split('/').slice(3) as [SettingsTabs, string, string];
	Logger.Log('OnRunSteamURL: Executing %o', url);

	if (!action) {
		const callback = (popup: any) => {
			if (popup.title !== 'Millennium') {
				return;
			}

			// It's async, but it's not used (or needed) here since
			// PopupManager.AddPopupCreatedCallback doesn't support it.
			OpenSettingsTab(popup, tab);
		};

		if (!g_PopupManager.m_rgPopupCreatedCallbacks.some((e: any) => e === callback)) {
			Millennium.AddWindowCreateHook(callback);
		}
		/** FIXME: Since the new Millennium popup window requires it to be rendered from within a React component, we can't do this as of right now */
		// ShowSettingsModal();
		return;
	}

	if ((tab as string) === 'devtools' && action === 'open') {
		// Open the DevTools window
		SteamClient.Browser.OpenDevTools();
	}

	if (tab === 'plugins') {
		// God, why
		const plugins: PluginComponent[] = JSON.parse(await FindAllPlugins()).map((e: PluginComponent) => ({ ...e, plugin_name: e.data.name }));
		if (item) {
			if (!plugins.some((e) => e.data.name === item)) {
				return;
			}

			const neededPlugin = plugins.find((e) => e.data.name === item);
			neededPlugin.enabled = action === 'enable';
		} else {
			// Disable them all
			for (const plugin of plugins) {
				// ..except me, of course
				plugin.enabled = plugin.data.name === 'core';
			}
		}

		UpdatePluginStatus({ pluginJson: JSON.stringify(plugins) });
		SteamClient.Browser.RestartJSContext();
	}

	if (tab === 'themes') {
		const themes: ThemeItem[] = JSON.parse(await FindAllThemes());
		const theme = themes.find((e) => e.native === item);
		const theme_name = !!theme && action === 'enable' ? theme.native : DEFAULT_THEME_NAME;

		ChangeTheme({ theme_name });
		SteamClient.Browser.RestartJSContext();
	}
};
