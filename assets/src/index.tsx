import { Millennium, pluginSelf, routerHook } from '@steambrew/client';
import { ThemeItem, SystemAccentColor, SettingsProps, ThemeItemV1 } from './types';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { Logger } from './utils/Logger';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { OnRunSteamURL } from './utils/url-scheme-handler';
import { PyGetRootColors, PyGetStartupConfig } from './utils/ffi';
import { onWindowCreatedCallback, patchMissedDocuments } from './patcher';
import { MillenniumSettings } from './settings';
import { NotificationService } from './update-notification-service';
import { EUIMode } from '@steambrew/client/build/globals/steam-client/shared';
import { MillenniumDesktopSidebar } from './quick-access';
import { DesktopMenuProvider } from './quick-access/DesktopMenuContext';

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
		const rootColors = await PyGetRootColors();
		Logger.Log('RootColors found in theme, dispatching...', rootColors);
		pluginSelf.RootColors = rootColors;
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
		millenniumUpdates: settings?.millenniumUpdates ?? {},
	});

	patchMissedDocuments();

	const notificationService = new NotificationService();
	notificationService.showNotifications();
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	await initializeMillennium(JSON.parse(await PyGetStartupConfig()));
	Millennium.AddWindowCreateHook(onWindowCreatedCallback);

	routerHook.addRoute('/millennium/settings', () => <MillenniumSettings />, { exact: false });

	routerHook.addGlobalComponent(
		'MillenniumDesktopUI',
		() => (
			<DesktopMenuProvider>
				<MillenniumDesktopSidebar />
			</DesktopMenuProvider>
		),
		EUIMode.Desktop,
	);

	// @ts-ignore
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
