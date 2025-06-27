import { IconsModule, pluginSelf, sleep, toaster, Navigation } from '@steambrew/client';
import { Logger } from './utils/Logger';
import { settingsManager } from './settings-manager';
import { OnMillenniumUpdate, OSType } from './types';
import { PyUpdateMillennium } from './utils/ffi';
import { MillenniumIcons } from './components/Icons';
import { deferredSettingLabelClasses } from './utils/classes';

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

	async updateMillennium() {
		const downloadUrl = pluginSelf.millenniumUpdates?.platformRelease?.browser_download_url;

		if (!downloadUrl) {
			toaster.toast({
				title: `Millennium Update Error`,
				body: `No download URL found for update.`,
				logo: <IconsModule.ExclamationPoint className={deferredSettingLabelClasses.Icon} />,
				duration: 5000,
				critical: true,
				sound: 3,
			});
		}

		PyUpdateMillennium({ downloadUrl });
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
				this.updateMillennium();
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
