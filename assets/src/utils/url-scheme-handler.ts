/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { callable, Millennium, Navigation } from '@steambrew/client';
import { PluginComponent, ThemeItem } from '../types';
import { Logger } from './Logger';

const DEFAULT_THEME_NAME = '__default__';

const ChangeTheme = callable<[{ theme_name: string }]>('theme_config.change_theme');
const FindAllThemes = callable<[], string>('find_all_themes');
const FindAllPlugins = callable<[], string>('find_all_plugins');
const UpdatePluginStatus = callable<[{ pluginJson: string }], any>('ChangePluginStatus');

/**
 * steam://millennium URL support.
 *
 * Example:
 * "steam://millennium/settings" -> Open Millennium dialog
 * "steam://millennium/settings/general" -> Open Millennium dialog
 * "steam://millennium/settings/updates" -> Open the "Updates" tab
 * "steam://millennium/settings/themes/disable" -> Use default theme
 * "steam://millennium/settings/themes/enable/aerothemesteam" -> Enable the Office 2007 theme using its internal name
 * "steam://millennium/settings/plugins/disable/steam-db" -> Disable the SteamDB plugin using its internal name
 * "steam://millennium/settings/plugins/disable" -> Disable all plugins
 * "steam://millennium/settings/devtools/open" -> Open the DevTools window
 */
export const OnRunSteamURL = async (_: number, url: string) => {
	const [context, action, option, parameter] = url
		.replace(/^steam:\/{1,2}/, '/')
		.split('/')
		.filter((r) => r)
		.slice(1);

	if (context !== 'settings') {
		Logger.Log('OnRunSteamURL: Invalid context %o, expected "settings"', context);
		return;
	}

	if (!option) {
		Logger.Log('OnRunSteamURL: No option specified, navigating to settings');
		Navigation.Navigate('/millennium/settings/' + action);
		return;
	}

	if (action === 'devtools' && parameter === 'open') {
		// Open the DevTools window
		SteamClient.Browser.OpenDevTools();
	}

	if (action === 'plugins') {
		// God, why
		const plugins: PluginComponent[] = JSON.parse(await FindAllPlugins()).map((e: PluginComponent) => ({ ...e, plugin_name: e.data.name }));
		if (parameter) {
			if (!plugins.some((e) => e.data.name === parameter)) {
				return;
			}

			const neededPlugin = plugins.find((e) => e.data.name === parameter);
			neededPlugin.enabled = option === 'enable';
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

	if (action === 'themes') {
		const themes: ThemeItem[] = JSON.parse(await FindAllThemes());
		const theme = themes.find((e) => e.native === parameter);
		const theme_name = !!theme && option === 'enable' ? theme.native : DEFAULT_THEME_NAME;

		ChangeTheme({ theme_name });
		SteamClient.Browser.RestartJSContext();
	}
};
