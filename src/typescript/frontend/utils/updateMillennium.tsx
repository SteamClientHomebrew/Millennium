import { IconsModule, pluginSelf, toaster } from '@steambrew/sdk';
import { backend } from './ffi';
import { deferredSettingLabelClasses } from './classes';
import { locale } from './localization-manager';

export async function updateMillennium(background: boolean) {
	const downloadUrl = pluginSelf.millenniumUpdates?.platformRelease?.browser_download_url;
	const downloadSize = pluginSelf.millenniumUpdates?.platformRelease?.size || 0;

	if (!downloadUrl) {
		toaster.toast({
			title: locale.toastMillenniumUpdateErrorTitle,
			body: locale.toastMillenniumUpdateNoDownloadUrl,
			logo: <IconsModule.ExclamationPoint className={deferredSettingLabelClasses.Icon} />,
			duration: 5000,
			critical: true,
			sound: 3,
		});
		return;
	}

	backend.updater.updateMillennium(downloadUrl, downloadSize, background);
}
