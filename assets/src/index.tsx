import { IconsModule, Millennium, Navigation, pluginSelf, routerHook, sleep, toaster } from '@steambrew/client';
import { ThemeItem, SystemAccentColor, SettingsProps, ThemeItemV1 } from './types';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { Logger } from './utils/Logger';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { OnRunSteamURL } from './utils/url-scheme-handler';
import { PyGetRootColors, PyGetStartupConfig } from './utils/ffi';
import { onWindowCreatedCallback, patchMissedDocuments } from './patcher';
import { MillenniumSettings } from './settings';

async function notifyUpdates() {
	await sleep(1000); // Wait for the toaster to be ready

	const themeUpdates: any[] = pluginSelf.updates.themes;
	const pluginUpdates: any[] = pluginSelf.updates.plugins;

	const updateCount = themeUpdates?.length + pluginUpdates?.filter?.((update: any) => update?.hasUpdate)?.length || 0;

	if (updateCount === 0) {
		Logger.Log('No updates found, skipping notification.');
		return;
	}

	toaster.toast({
		title: `Updates Available`,
		body: `We've found ${updateCount} updates for items in your library!`,
		logo: <IconsModule.Download />,
		duration: 10000,
		onClick: () => {
			Navigation.Navigate('/millennium/settings/updates');
		},
	});
}

async function initializeMillennium(settings: SettingsProps) {
	notifyUpdates();
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
		accentColor: settings?.accent_color,
		conditionals: settings?.conditions,
		steamPath: settings?.steamPath,
		installPath: settings?.installPath,
		version: settings?.millenniumVersion,
		enabledPlugins: settings?.enabledPlugins ?? [],
		updates: settings?.updates ?? [],
		hasCheckedForUpdates: settings?.hasCheckedForUpdates ?? false,
		buildDate: settings?.buildDate,
	});

	patchMissedDocuments();
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	await initializeMillennium(JSON.parse(await PyGetStartupConfig()));
	Millennium.AddWindowCreateHook(onWindowCreatedCallback);

	routerHook.addRoute('/millennium/settings', () => <MillenniumSettings />, { exact: false });

	// @ts-ignore
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
