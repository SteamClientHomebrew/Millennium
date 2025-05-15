import { callable, DialogButton, Field, pluginSelf, TextField, Toggle } from '@steambrew/client';
import React, { useState } from 'react';
import { OpenInstallPrompt } from './Installer';
import { SettingsDialogSubHeader } from '../../components/ISteamComponents';
import { Settings } from '../../Settings';
import { locale } from '../../locales';
import { ErrorModal } from '../../custom_components/ErrorModal';
import { UpdaterOptionProps } from '../../types';

const SetUpdateNotificationStatus = callable<[{ status: boolean }], boolean>('updater.set_update_notifs_status');
const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>('MillenniumUpdater.set_user_wants_updates');
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>('MillenniumUpdater.set_user_wants_update_notify');

export const GeneralViewModal: React.FC = () => {
	const [installID, setInstallID] = useState<string>('dd5a94af5f22');
	const [showUpdateNotifications, setNotifications] = useState<boolean>(pluginSelf.wantsMillenniumPluginThemeUpdateNotify);

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

	const OnPluginAndThemeUpdateNotifyChange = async (enabled: boolean) => {
		const result = await SetUpdateNotificationStatus({ status: enabled });

		if (result) {
			setNotifications(enabled);
			Settings.FetchAllSettings();
		} else {
			console.error('Failed to update settings');
			pluginSelf.connectionFailed = true;
		}
	};

	/** Check if the connection failed, this usually means the backend crashed or couldn't load */
	if (pluginSelf.connectionFailed) {
		return (
			<ErrorModal
				header={locale.errorFailedConnection}
				body={locale.errorFailedConnectionBody}
				options={{
					buttonText: locale.errorFailedConnectionButton,
					onClick: () => {
						SteamClient.System.OpenLocalDirectoryInSystemExplorer([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/'));
					},
				}}
			/>
		);
	}

	const OpenSteamBrew = () => {
		SteamClient.System.OpenInSystemBrowser('https://steambrew.app');
	};

	return (
		<>
			<Field
				label={'Install a plugin or theme'}
				focusable={true}
				description={
					<>
						Install a user plugin or theme from an ID. These ID's can be found at{' '}
						<a href="#" onClick={OpenSteamBrew}>
							https://steambrew.app
						</a>
					</>
				}
				bottomSeparator="none"
			>
				<TextField value={installID} onChange={(e) => setInstallID(e.target.value)}></TextField>
				<DialogButton style={{ height: '100%', width: '80px' }} onClick={OpenInstallPrompt.bind(null, installID)}>
					Install
				</DialogButton>
			</Field>

			<div className="SettingsDialogSubHeader" style={{ marginTop: '20px' }}>
				Millennium Updates
			</div>

			<Field label={locale.updatePanelCheckForUpdates} description={locale.toggleWantsMillenniumUpdatesTooltip}>
				<Toggle value={wantsUpdates} onChange={OnUpdateChange} />
			</Field>
			<Field label={locale.updatePanelShowUpdateNotifications} description={locale.toggleWantsMillenniumUpdatesNotificationsTooltip} bottomSeparator="none">
				<Toggle value={wantsNotify} onChange={OnNotifyChange} />
			</Field>

			<div className="SettingsDialogSubHeader" style={{ marginTop: '20px' }}>
				Plugin & Theme Updates
			</div>
			<Field label={locale.updatePanelUpdateNotifications} description={locale.updatePanelUpdateNotificationsTooltip} bottomSeparator="none">
				{showUpdateNotifications !== undefined && <Toggle value={showUpdateNotifications} onChange={OnPluginAndThemeUpdateNotifyChange} />}
			</Field>
		</>
	);
};
