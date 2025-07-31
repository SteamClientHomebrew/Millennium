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

import { ConfirmModal, Millennium, pluginSelf, routerHook, showModal } from '@steambrew/client';
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
import { MillenniumDesktopSidebar } from './quick-access';
import { DesktopMenuProvider } from './quick-access/DesktopMenuContext';
import { EUIMode } from '@steambrew/client';
import { useEffect } from 'react';
import { settingsManager } from './settings-manager';
import { WelcomeModalComponent } from './components/WelcomeModal';

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
		platformType: settings?.platformType,
		millenniumLinuxUpdateScript: settings?.millenniumLinuxUpdateScript,
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

	/** Render welcome modal for new users */
	routerHook.addGlobalComponent('MillenniumWelcomeModal', () => <WelcomeModalComponent />, EUIMode.Desktop);

	// @ts-ignore
	SteamClient.URL.RegisterForRunSteamURL('millennium', OnRunSteamURL);
}
