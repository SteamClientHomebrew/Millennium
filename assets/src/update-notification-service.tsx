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

import { IconsModule, pluginSelf, sleep, toaster, Navigation } from '@steambrew/client';
import { Logger } from './utils/Logger';
import { settingsManager } from './settings-manager';
import { OnMillenniumUpdate, OSType } from './types';
import { PyUpdateMillennium } from './utils/ffi';
import { MillenniumIcons } from './components/Icons';
import { deferredSettingLabelClasses } from './utils/classes';
import { updateMillennium } from './updateMillennium';

/**
 * Notify the user about available updates in their library.
 * This method checks for theme and plugin updates and displays a notification if any are found.
 */
export class NotificationService {
	shouldNotifyLibraryUpdates: boolean;
	onMillenniumUpdates: OnMillenniumUpdate;

	get libraryUpdateCount() {
		const themeUpdates: any[] = pluginSelf.updates.themes;
		const pluginUpdates: any[] = pluginSelf.updates.plugins;

		return themeUpdates?.length + pluginUpdates?.filter?.((update: any) => update?.hasUpdate)?.length || 0;
	}

	async notifyLibraryUpdates() {
		await sleep(1000); // Wait for the toaster to be ready

		if (this.libraryUpdateCount === 0) {
			Logger.Log('No updates found, skipping notification.');
			return;
		}

		toaster.toast({
			title: `Updates Available`,
			body: `We've found ${this.libraryUpdateCount} updates for items in your library!`,
			logo: <IconsModule.Download />,
			onClick: () => {
				Navigation.Navigate('/millennium/settings/updates');
			},
		});
	}

	async notifyMillenniumUpdates() {
		await sleep(1000); // Wait for the toaster to be ready

		toaster.toast({
			title: `Millennium Update Available`,
			body: `A new version of Millennium is available! Click here to update.`,
			logo: <MillenniumIcons.SteamBrewLogo />,
			onClick: () => {
				Navigation.Navigate('/millennium/settings/updates');
			},
		});
	}

	constructor() {
		this.shouldNotifyLibraryUpdates = settingsManager.config.general.shouldShowThemePluginUpdateNotifications;
		this.onMillenniumUpdates = settingsManager.config.general.onMillenniumUpdate;
	}

	async showNotifications() {
		await sleep(3000); // Wait for the toaster to be ready

		if (this.shouldNotifyLibraryUpdates) {
			this.notifyLibraryUpdates();
		}

		if (!pluginSelf?.millenniumUpdates?.hasUpdate) {
			Logger.Log('No Millennium updates found, skipping notification.');
			return;
		}

		switch (this.onMillenniumUpdates) {
			case OnMillenniumUpdate.AUTO_INSTALL: {
				updateMillennium();
				break;
			}
			case OnMillenniumUpdate.NOTIFY: {
				this.notifyMillenniumUpdates();
				break;
			}
			case OnMillenniumUpdate.DO_NOTHING:
			default: {
				Logger.Log('Millennium updates are set to do nothing.');
				break;
			}
		}
	}
}
