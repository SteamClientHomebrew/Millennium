/**
 *:=================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 *:=================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import { ffi } from '@steambrew/client';
import {
	BackendOpResult,
	InstallStartResponse,
	MillenniumUpdateChannel,
	MillenniumUpdates,
	OpIdResponse,
	PluginCrashInfo,
	PluginComponent,
	PluginMetrics,
	SettingsProps,
	SystemAccentColor,
	ThemeColorOption,
	ThemeItem,
	UpdatesResponse,
} from '../types';
import { AppConfig } from './AppConfig';
import { LogData } from '../settings/logs';

export const backend = {
	getSteamPath: ffi<[], string>('Core_GetSteamPath'),

	plugins: {
		getPlugins: ffi<[], PluginComponent[]>('Core_FindAllPlugins'),
		togglePlugin: ffi<[pluginJson: string], void>('Core_ChangePluginStatus'),
		install: ffi<[downloadUrl: string, totalSize: number], InstallStartResponse>('Core_InstallPlugin'),
		isInstalled: ffi<[pluginName: string], boolean>('Core_IsPluginInstalled'),
		uninstall: ffi<[pluginName: string], boolean>('Core_UninstallPlugin'),
		update: ffi<[id: string, name: string, commit: string], OpIdResponse>('Core_DownloadPluginUpdate'),
		stop: ffi<[pluginName: string], void>('Core_KillPluginBackend'),
		reload: ffi<[pluginName: string], void>('Core_ReloadPlugin'),
		getBackendLogs: ffi<[], LogData[]>('Core_GetPluginBackendLogs'),
		getMetrics: ffi<[], PluginMetrics[]>('Core_GetAllPluginMetrics'),
	},

	themes: {
		activeTheme: ffi<[], ThemeItem>('Core_GetActiveTheme'),
		setActiveTheme: ffi<[themeName: string], void>('Core_ChangeActiveTheme'),
		getTheme: ffi<[repo: string, owner: string, asString: boolean], ThemeItem | false>('Core_GetThemeFromGitPair'),
		getThemes: ffi<[], ThemeItem[]>('Core_FindAllThemes'),
		install: ffi<[owner: string, repo: string], InstallStartResponse>('Core_InstallTheme'),
		uninstall: ffi<[owner: string, repo: string], BackendOpResult>('Core_UninstallTheme'),
		isInstalled: ffi<[owner: string, repo: string], boolean>('Core_IsThemeInstalled'),
		update: ffi<[native: string], OpIdResponse>('Core_DownloadThemeUpdate'),
		colorOptions: ffi<[themeName: string], ThemeColorOption[]>('Core_GetThemeColorOptions'),
		doesUseAccentColor: ffi<[], boolean>('Core_DoesThemeUseAccentColor'),
		getRootColors: ffi<[], string>('Core_GetRootColors'),
		setThemeConfig: ffi<[theme: string, newData: string, condition: string], BackendOpResult>('Core_ChangeCondition'),
		setThemeColor: ffi<[theme: string, colorName: string, newColor: string, colorType: number], string>('Core_ChangeColor'),
		setAccentColor: ffi<[newColor: string], SystemAccentColor>('Core_ChangeAccentColor'),
	},

	environment: {
		get: ffi<[variable: string], string>('Core_GetEnvironmentVar'),
	},

	config: {
		millennium: {
			getConfig: ffi<[], AppConfig>('Core_GetBackendConfig'),
			setConfig: ffi<[config: string, skipPropagation: boolean], void>('Core_SetBackendConfig'),
		},
		plugins: {
			getAll: ffi<[pluginName: string], Record<string, unknown>>('PluginConfig_GetAll'),
			removeAll: ffi<[pluginName: string], BackendOpResult>('PluginConfig_DeleteAll'),
		},

		getInitService: ffi<[], SettingsProps>('Core_GetStartConfig'),
	},

	quickCss: {
		registerWatcher: ffi<[], void>('Core_WatchQuickCss'),
		destroyWatcher: ffi<[], void>('Core_UnwatchQuickCss'),
	},

	updater: {
		getUpdates: ffi<[force: boolean], UpdatesResponse>('Core_GetUpdates'),
		checkMillenniumUpdate: ffi<[channel: MillenniumUpdateChannel], MillenniumUpdates>('Core_CheckMillenniumUpdate'),

		updateMillennium: ffi<[downloadUrl: string, downloadSize: number, background: boolean], void>('Core_UpdateMillennium'),
		hasPendingMillenniumUpdateRestart: ffi<[], boolean>('Core_HasPendingMillenniumUpdateRestart'),
	},

	crashHandler: {
		getPendingCrashes: ffi<[], PluginCrashInfo[]>('Core_GetPendingCrashes'),
		acknowledgeCrash: ffi<[plugin: string], void>('Core_AcknowledgeCrash'),
	},
};
