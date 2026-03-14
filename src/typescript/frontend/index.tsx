/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
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

import { EUIMode, Millennium, pluginSelf, routerHook, sleep } from '@steambrew/client';
import { WelcomeModalComponent } from './components/WelcomeModal';
import { onWindowCreatedCallback, patchMissedDocuments } from './patcher';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { MillenniumDesktopSidebar } from './quick-access';
import { DesktopMenuProvider } from './quick-access/DesktopMenuContext';
import { handleSettingsReturnNavigation, MillenniumSettings } from './settings';
import { MillenniumQuickCssEditor } from './settings/quickcss';
import { SettingsProps, SystemAccentColor, ThemeItem, ThemeItemV1 } from './types';
import { PyGetRootColors, PyGetStartupConfig } from './utils/ffi';
import { Logger } from './utils/Logger';
import { useQuickCssState } from './utils/quick-css-state';
import { NotificationService } from './utils/update-notification-service';
import { OnRunSteamURL } from './utils/url-scheme-handler';
import { showPluginCrashToast } from './components/PluginCrashModal';
import { PyGetPendingCrashes } from './utils/ffi';

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
		try {
			const rootColors = JSON.parse(await PyGetRootColors());
			pluginSelf.RootColors = rootColors;
		} catch (error) {
			Logger.Error('Failed to load root colors from backend', error);
		}
	}

	Object.assign(pluginSelf, {
		activeTheme: settings?.active_theme,
		accentColor: settings?.accent_color,
		conditionals: settings?.conditions,
		steamPath: settings?.steamPath,
		installPath: settings?.installPath,
		themesPath: settings?.themesPath,
		version: settings?.millenniumVersion,
		enabledPlugins: settings?.enabledPlugins ?? [],
		updates: settings?.updates ?? [],
		hasCheckedForUpdates: settings?.hasCheckedForUpdates ?? false,
		buildDate: settings?.buildDate,
		millenniumUpdates: settings?.millenniumUpdates ?? {},
		platformType: settings?.platformType,
		millenniumLinuxUpdateScript: settings?.millenniumLinuxUpdateScript,
		quickCss: settings?.quickCss ?? '',
	});

	patchMissedDocuments();

	const notificationService = new NotificationService();
	notificationService.showNotifications();

	window.addEventListener('millennium-plugin-crash', (e: Event) => {
		const detail = (e as CustomEvent).detail;
		setTimeout(() => showPluginCrashToast(detail), 3000);
	});

	setTimeout(async () => {
		try {
			const pending = JSON.parse(await PyGetPendingCrashes()) as Array<{ plugin: string; exitCode: number; displayName?: string; crashDumpDir?: string }>;
			pending?.forEach?.(showPluginCrashToast);
		} catch {}
	}, 5000);
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	try {
		await initializeMillennium(JSON.parse(await PyGetStartupConfig()));
	} catch (error) {
		Logger.Error('Millennium frontend initialization failed, continuing with route registration.', error);
	}
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

	/** Render welcome modal for new users */
	routerHook.addGlobalComponent('MillenniumWelcomeModal', () => <WelcomeModalComponent />, EUIMode.Desktop);

	/** Render Quick CSS Editor */
	routerHook.addGlobalComponent(
		'MillenniumQuickCssEditor',
		() => {
			const { isMillenniumOpen } = useQuickCssState();
			if (!isMillenniumOpen) {
				return null;
			}

			return <MillenniumQuickCssEditor />;
		},
		EUIMode.Desktop,
	);

	// @ts-ignore
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);

	// If the user was in Millennium Settings when a JS context restart happened,
	// navigate back to settings and restore the original start_page.
	handleSettingsReturnNavigation();
}
