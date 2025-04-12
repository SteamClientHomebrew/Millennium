import { useEffect, useState } from 'react';
import { pluginSelf, Toggle, SteamSpinner, Field, callable, DialogControlsSection } from '@steambrew/client';
import { locale } from '../locales';
import { UpdaterOptionProps } from '../types';
import { Settings } from '../Settings';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/ISteamComponents';

const GetUpdateList = callable<[{ force: boolean }], any>('updater.get_update_list');
const SetUpdateNotificationStatus = callable<[{ status: boolean }], boolean>('updater.set_update_notifs_status');
const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>('MillenniumUpdater.set_user_wants_updates');
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>('MillenniumUpdater.set_user_wants_update_notify');

const SettingsViewModal: React.FC = () => {
	const [showUpdateNotifications, setNotifications] = useState<boolean>(undefined);
	const [hasReceivedUpdates, setHasReceivedUpdates] = useState<boolean>(false);

	const [wantsUpdates, setWantsUpdates] = useState(pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.YES);
	const [wantsNotify, setWantsNotify] = useState(pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.YES);

	const OnUpdateChange = (newValue: boolean) => {
		setWantsUpdates(newValue);
		pluginSelf.wantsMillenniumUpdates = newValue;
		SetUserWantsUpdates({ wantsUpdates: newValue });
	};

	const OnNotifyChange = (newValue: boolean) => {
		setWantsNotify(newValue);
		pluginSelf.wantsMillenniumUpdateNotifications = newValue;
		SetUserWantsNotifications({ wantsNotify: newValue });
	};

	const FetchSettings = async () => {
		try {
			const updateList = JSON.parse(await GetUpdateList({ force: false }));
			pluginSelf.connectionFailed = false;

			setNotifications(updateList.notifications ?? false);
			setHasReceivedUpdates(true);
		} catch (exception) {
			console.error('Failed to fetch updates');
			pluginSelf.connectionFailed = true;
		}
	};

	const OnNotificationsChange = async (enabled: boolean) => {
		const result = await SetUpdateNotificationStatus({ status: enabled });

		if (result) {
			setNotifications(enabled);
			Settings.FetchAllSettings();
		} else {
			console.error('Failed to update settings');
			pluginSelf.connectionFailed = true;
		}
	};

	useEffect(() => {
		FetchSettings();
	}, []);

	/** Check if the connection failed, this usually means the backend crashed or couldn't load */
	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />;
	}

	if (!hasReceivedUpdates) {
		return <SteamSpinner background={'transparent'} />;
	}

	return (
		<>
			<DialogControlsSection>
				<SettingsDialogSubHeader>Millennium Updates</SettingsDialogSubHeader>

				<Field label={locale.updatePanelCheckForUpdates} description={locale.toggleWantsMillenniumUpdatesTooltip}>
					<Toggle value={wantsUpdates} onChange={OnUpdateChange} />
				</Field>
				<Field label={locale.updatePanelShowUpdateNotifications} description={locale.toggleWantsMillenniumUpdatesNotificationsTooltip}>
					<Toggle value={wantsNotify} onChange={OnNotifyChange} />
				</Field>
			</DialogControlsSection>
			<DialogControlsSection>
				<SettingsDialogSubHeader>Plugin & Theme Updates</SettingsDialogSubHeader>
				<Field label={locale.updatePanelUpdateNotifications} description={locale.updatePanelUpdateNotificationsTooltip}>
					{showUpdateNotifications !== undefined && <Toggle value={showUpdateNotifications} onChange={OnNotificationsChange} />}
				</Field>
			</DialogControlsSection>
		</>
	);
};

export { SettingsViewModal };
