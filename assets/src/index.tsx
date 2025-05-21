import { Millennium, pluginSelf } from '@steambrew/client';
import { ThemeItem, SystemAccentColor, SettingsProps, ThemeItemV1 } from './types';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { Logger } from './utils/Logger';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { OnRunSteamURL } from './utils/url-scheme-handler';
import { PyGetRootColors, PyGetStartupConfig } from './utils/ffi';
import { onWindowCreatedCallback, patchMissedDocuments } from './patcher';

async function initializeMillennium(settings: SettingsProps) {
	Logger.Log(`Received props`, settings);

	const theme: ThemeItem = settings.active_theme;
	const systemColors: SystemAccentColor = settings.accent_color;

	ParseLocalTheme(theme);
	DispatchSystemColors(systemColors);

	const themeV1: ThemeItemV1 = settings?.active_theme?.data as ThemeItemV1;

	if (themeV1?.GlobalsColors) {
		DispatchGlobalColors(themeV1?.GlobalsColors);
	}

	if (theme?.data?.hasOwnProperty('RootColors')) {
		Logger.Log('RootColors found in theme, dispatching...');
		pluginSelf.RootColors = await PyGetRootColors();
	}

	Object.assign(pluginSelf, {
		conditionals: settings?.conditions,
		steamPath: settings?.steamPath,
		installPath: settings?.installPath,
		version: settings?.millenniumVersion,
		enabledPlugins: settings?.enabledPlugins ?? [],
		updates: settings?.updates ?? [],
	});

	patchMissedDocuments();
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	await initializeMillennium(JSON.parse(await PyGetStartupConfig()));
	Millennium.AddWindowCreateHook(onWindowCreatedCallback);

	// @ts-ignore
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
