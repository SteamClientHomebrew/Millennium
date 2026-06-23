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

import { IconsModule, pluginSelf, sleep, toaster, Navigation } from '@steambrew/sdk';
import { settingsManager } from './settings-manager';
import { OnMillenniumUpdate } from '../types';
import { MillenniumIcons } from '../components/Icons';

let _libraryNotified = false;

export const notificationService = {
	async notifyLibraryUpdates() {
		if (_libraryNotified) return;
		const themeUpdates: any[] = pluginSelf.updates?.themes;
		const pluginUpdates: any[] = pluginSelf.updates?.plugins;
		const count = (themeUpdates?.length ?? 0) + (pluginUpdates?.filter?.((u: any) => u?.hasUpdate)?.length ?? 0);
		if (count === 0) return;
		_libraryNotified = true;
		toaster.toast({
			title: `Updates Available`,
			body: `We've found ${count} updates for items in your library!`,
			logo: <IconsModule.Download />,
			onClick: () => Navigation.Navigate('/millennium/settings/updates'),
		});
	},

	async notifyMillenniumUpdates() {
		await sleep(1000);
		toaster.toast({
			title: `Millennium Update Available`,
			body: `A new version of Millennium is available! Click here to update.`,
			logo: <MillenniumIcons.SteamBrewLogo />,
			onClick: () => Navigation.Navigate('/millennium/settings/updates'),
		});
	},

	async showNotifications() {
		await sleep(3000);
		if (!pluginSelf?.millenniumUpdates?.hasUpdate) return;
		if (settingsManager.config.general.onMillenniumUpdate === OnMillenniumUpdate.NOTIFY) {
			notificationService.notifyMillenniumUpdates();
		}
	},
};
