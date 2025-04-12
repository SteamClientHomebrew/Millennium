import {
	callable,
	DialogBody,
	DialogButton,
	DialogFooter,
	DialogHeader,
	Field,
	pluginSelf,
	showModal,
	ShowModalResult,
	Toggle,
} from '@steambrew/client';
import { GenericConfirmDialog } from '../GenericDialog';
import { useState } from 'react';
import { UpdaterOptionProps } from '../../types';
import { locale } from '../../locales';
import { Separator } from '../../components/ISteamComponents';
import Styles from '../../styles';

const SetUserWantsUpdates = callable<[{ wantsUpdates: boolean }], void>('MillenniumUpdater.set_user_wants_updates');
const SetUserWantsNotifications = callable<[{ wantsNotify: boolean }], void>('MillenniumUpdater.set_user_wants_update_notify');

export const PromptSelectUpdaterOptions = () => {
	/** Handle to the security window */
	let SecurityModalWindow: ShowModalResult;

	const RenderOptionSelector = () => {
		const [wantsUpdates, setWantsUpdates] = useState(
			pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.UNSET || pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.YES,
		);
		const [wantsNotify, setWantsNotify] = useState(
			pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.UNSET ||
				pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.YES,
		);

		const OnUpdateChange = (newValue: boolean) => {
			pluginSelf.wantsMillenniumUpdates = newValue;
			setWantsUpdates(newValue);
		};

		const OnNotifyChange = (newValue: boolean) => {
			pluginSelf.wantsMillenniumUpdateNotifications = newValue;
			setWantsNotify(newValue);
		};

		const OnContinue = () => {
			SetUserWantsUpdates({ wantsUpdates: wantsUpdates });
			SetUserWantsNotifications({ wantsNotify: wantsNotify });

			pluginSelf.wantsMillenniumUpdates = wantsUpdates;
			pluginSelf.wantsMillenniumUpdateNotifications = wantsNotify;
			SecurityModalWindow?.Close();
		};

		return (
			<GenericConfirmDialog>
				<DialogHeader>Millennium</DialogHeader>
				<Styles />
				<DialogBody className="MillenniumGenericDialog_DialogBody">
					{locale.message1162025SecurityUpdate}
					<b>{locale.message1162025SecurityUpdateTooltip}</b>

					<Separator />

					<div>
						<Field
							label={locale.toggleWantsMillenniumUpdates}
							description={locale.toggleWantsMillenniumUpdatesTooltip}
							bottomSeparator="none"
						>
							<Toggle value={wantsUpdates} onChange={OnUpdateChange} />
						</Field>
						<Field
							label={locale.toggleWantsMillenniumUpdatesNotifications}
							description={locale.toggleWantsMillenniumUpdatesNotificationsTooltip}
							bottomSeparator="none"
						>
							<Toggle value={wantsNotify} onChange={OnNotifyChange} />
						</Field>
					</div>

					<div className="MillenniumSelectUpdate_SecurityWarning">{locale.updateSecurityWarning}</div>
				</DialogBody>

				<DialogFooter>
					<div className="DialogTwoColLayout _DialogColLayout Panel" style={{ alignItems: 'center' }}>
						<div className="MillenniumSelectUpdate_FooterInfo">{locale.settingsAreChangeableLater}</div>
						<DialogButton className="Primary" onClick={OnContinue}>
							Continue
						</DialogButton>
					</div>
				</DialogFooter>
			</GenericConfirmDialog>
		);
	};

	SecurityModalWindow = showModal(<RenderOptionSelector />, pluginSelf.mainWindow, {
		strTitle: 'Select Update Options',
		popupHeight: 625,
		popupWidth: 750,
	});
};
