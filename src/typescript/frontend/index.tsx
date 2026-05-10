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

import { EUIMode, Millennium, pluginSelf, routerHook } from '@steambrew/client';
import { WelcomeModalComponent } from './components/WelcomeModal';
import { onWindowCreatedCallback, patchMissedDocuments } from './patcher';
import { DispatchSystemColors } from './patcher/SystemColors';
import { ParseLocalTheme } from './patcher/ThemeParser';
import { DispatchGlobalColors } from './patcher/v1/GlobalColors';
import { MillenniumDesktopSidebar } from './quick-access';
import { DesktopMenuProvider } from './quick-access/DesktopMenuContext';
import { handleSettingsReturnNavigation, MillenniumSettings } from './settings';
import { MillenniumQuickCssEditor } from './settings/quickcss';
import { PluginComponent, PluginCrashInfo, SettingsProps, SystemAccentColor, ThemeItem, ThemeItemV1 } from './types';
import { backend } from './utils/ffi';
import { Logger } from './utils/Logger';
import { useQuickCssState } from './utils/quick-css-state';
import { NotificationService } from './utils/update-notification-service';
import { OnRunSteamURL } from './utils/url-scheme-handler';
import { showPluginCrashModal } from './components/PluginCrashModal';
import { showLegacyPluginModal } from './components/LegacyPluginModal';
import { showSupersededPluginModal, SUPERSEDED_PLUGIN_NAMES } from './components/SupersededPluginModal';

async function initializeMillennium(settings: SettingsProps) {
	Logger.Log(`Initialized Millennium Frontend Settings Store:`, settings);

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
			const rootColors = await backend.themes.getRootColors();
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
		gitCommitOid: settings?.gitCommitOid,
		millenniumUpdates: settings?.millenniumUpdates ?? {},
		platformType: settings?.platformType,
		millenniumLinuxUpdateScript: settings?.millenniumLinuxUpdateScript,
		quickCss: settings?.quickCss ?? '',
	});

	patchMissedDocuments();

	const notificationService = new NotificationService();
	notificationService.showNotifications();

	const crashQueue: PluginCrashInfo[] = [];
	let mainWindowReady = false;
	let mainWindowReadyAt = 0;

	const showAfterDelay = (detail: PluginCrashInfo) => {
		const elapsed = Date.now() - mainWindowReadyAt;
		const remaining = Math.max(0, 5000 - elapsed);
		setTimeout(() => showPluginCrashModal(detail), remaining);
	};

	const checkLegacyPlugins = async () => {
		try {
			const plugins: PluginComponent[] = await backend.plugins.getPlugins();
			const legacy = plugins.filter((p) => p.enabled && p.data.name !== 'core' && p.data.useBackend !== false && p.data.backendType !== 'lua');

			if (legacy.length) {
				const disablePayload = legacy.map((p) => ({ plugin_name: p.data.name, enabled: false }));
				await backend.plugins.togglePlugin(JSON.stringify(disablePayload));
			}

			const superseded = plugins.filter((p) => SUPERSEDED_PLUGIN_NAMES.includes(p.data.name));
			const nonSupersededLegacy = legacy.filter((p) => !SUPERSEDED_PLUGIN_NAMES.includes(p.data.name));

			if (superseded.length && nonSupersededLegacy.length) {
				showSupersededPluginModal(superseded, () => showLegacyPluginModal(nonSupersededLegacy));
			} else if (superseded.length) {
				showSupersededPluginModal(superseded);
			} else if (legacy.length) {
				showLegacyPluginModal(legacy);
			}
		} catch (e) {
			Logger.Error('Failed to check for legacy plugins', e);
		}
	};

	const flushCrashQueue = () => {
		mainWindowReady = true;
		mainWindowReadyAt = Date.now();
		crashQueue.splice(0).forEach(showAfterDelay);
		setTimeout(checkLegacyPlugins, 5000);
	};

	window.addEventListener('millennium-main-window-ready', flushCrashQueue, { once: true });

	window.addEventListener('millennium-plugin-crash', (e: Event) => {
		const detail = (e as CustomEvent).detail;
		Logger.Log('Received real-time crash event for plugin:', detail?.plugin);
		if (mainWindowReady) showAfterDelay(detail);
		else crashQueue.push(detail);
	});

	if (settings?.pendingCrashes?.length) {
		Logger.Log(`Startup config contains ${settings.pendingCrashes.length} pending crash(es)`);
		settings.pendingCrashes.forEach((d) => crashQueue.push(d));
	}
}

// Entry point on the front end of your plugin
export default async function PluginMain() {
	try {
		await initializeMillennium(await backend.config.getInitService());
	} catch (error) {
		Logger.Error('Millennium frontend initialization failed, continuing with route registration.', error);
	}
	Millennium.AddWindowCreateHook?.(onWindowCreatedCallback);

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
