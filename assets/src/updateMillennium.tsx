import { IconsModule, pluginSelf, toaster } from '@steambrew/client';
import { PyUpdateMillennium } from './utils/ffi';
import { deferredSettingLabelClasses } from './utils/classes';

export async function updateMillennium(background: boolean) {
	const downloadUrl = pluginSelf.millenniumUpdates?.platformRelease?.browser_download_url;
	const downloadSize = pluginSelf.millenniumUpdates?.platformRelease?.size || 0;

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

	await PyUpdateMillennium({ downloadUrl, downloadSize, background });
	pluginSelf.millenniumUpdates.updateInProgress = true;
}
