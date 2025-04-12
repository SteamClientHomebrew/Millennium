import { useEffect, useState } from 'react';
import {
	DialogBodyText,
	DialogButton,
	DialogControlsSection,
	IconsModule,
	pluginSelf,
	SteamSpinner,
	Field,
	callable,
	ConfirmModal,
	showModal,
} from '@steambrew/client';
import { locale } from '../locales';
import { ThemeItem } from '../types';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/ISteamComponents';

interface UpdateProps {
	updates: UpdateItemType[];
	fetchUpdates: () => Promise<boolean>;
}

interface UpdateItemType {
	message: string; // Commit message
	date: string; // Humanized since timestamp
	commit: string; // Full GitHub commit URI
	native: string; // Folder name in skins folder.
	name: string; // Common display name
}

const UpToDateModal: React.FC = () => {
	return (
		<div className="MillenniumUpToDate_Container">
			<IconsModule.Checkmark width="64" />
			<div className="MillenniumUpToDate_Header">{locale.updatePanelNoUpdatesFound}</div>
		</div>
	);
};

const UpdateTheme = callable<[{ native: string }], boolean>('updater.update_theme');

const ShowMessageBox = (message: string, title?: string) => {
	return new Promise((resolve) => {
		const onOK = () => resolve(true);
		const onCancel = () => resolve(false);

		showModal(
			<ConfirmModal
				// @ts-ignore
				strTitle={title ?? LocalizationManager.LocalizeString('#InfoSettings_Title')}
				strDescription={message}
				onOK={onOK}
				onCancel={onCancel}
			/>,
			pluginSelf.windows['Millennium'],
			{
				bNeverPopOut: false,
			},
		);
	});
};

interface FormatString {
	(template: string, ...args: string[]): string;
}

const formatString: FormatString = (template, ...args) => {
	return template.replace(/{(\d+)}/g, (match, index) => {
		return index < args.length ? args[index] : match; // Replace {index} with the corresponding argument or leave it unchanged
	});
};

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates, fetchUpdates }) => {
	const [updating, setUpdating] = useState<Array<any>>([]);
	const viewMoreClick = (props: UpdateItemType) => SteamClient.System.OpenInSystemBrowser(props?.commit);

	const updateItemMessage = async (updateObject: UpdateItemType, index: number) => {
		setUpdating({ ...updating, [index]: true });

		const success = await UpdateTheme({ native: updateObject.native });

		if (success) {
			/** Check for updates */
			await fetchUpdates();
			const activeTheme: ThemeItem = pluginSelf.activeTheme;

			/** Check if the updated theme is currently in use, if so reload */
			if (activeTheme?.native === updateObject?.native) {
				SteamClient.Browser.RestartJSContext();

				const reload = await ShowMessageBox(
					formatString(locale.updateSuccessfulRestart, updateObject?.name),
					// @ts-ignore
					LocalizationManager.LocalizeString('#ImageUpload_SuccessCard'),
				);

				if (reload) {
					SteamClient.Browser.RestartJSContext();
				}
			} else {
				const reload = await ShowMessageBox(
					formatString(locale.updateSuccessful, updateObject?.name),
					// @ts-ignore
					LocalizationManager.LocalizeString('#ImageUpload_SuccessCard'),
				);

				if (reload) {
					SteamClient.Browser.RestartJSContext();
				}
			}
		} else {
			// @ts-ignore
			ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), LocalizationManager.LocalizeString('#Generic_Error'));
		}
	};

	return (
		<DialogControlsSection>
			<SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
			<DialogBodyText className="_3fPiC9QRyT5oJ6xePCVYz8">{locale.updatePanelHasUpdatesSub}</DialogBodyText>

			{updates.map((update: UpdateItemType, index: number) => (
				<>
					<Field
						key={index}
						className="MillenniumUpdateField"
						label={
							<div className="MillenniumUpdates_Label">
								<div className="MillenniumUpdates_LabelType">Theme</div>
								{update.name}
							</div>
						}
						description={
							<div className="MillenniumUpdates_Description">
								<div>
									<b>{locale.updatePanelReleasedTag}</b> {update?.date}
								</div>
								<div>
									<b>{locale.updatePanelReleasePatchNotes}</b> {update?.message}
								</div>
							</div>
						}
					>
						<DialogButton onClick={() => viewMoreClick(update)} className="MillenniumUpdates_ThemeButton _3epr8QYWw_FqFgMx38YEEm">
							{locale.ViewMore}
						</DialogButton>
						<DialogButton
							onClick={() => updateItemMessage(update, index)}
							className="MillenniumUpdates_ThemeButton _3epr8QYWw_FqFgMx38YEEm"
						>
							{updating[index] ? locale.updatePanelIsUpdating : locale.updatePanelUpdate}
						</DialogButton>
					</Field>
				</>
			))}
		</DialogControlsSection>
	);
};

const GetUpdateList = callable<[{ force: boolean }], any>('updater.get_update_list');

const UpdatesViewModal: React.FC = () => {
	const [updates, setUpdates] = useState<Array<UpdateItemType>>(null);
	const [hasReceivedUpdates, setHasReceivedUpdates] = useState<boolean>(false);

	const FetchAvailableUpdates = async (): Promise<boolean> =>
		new Promise(async (resolve, reject) => {
			try {
				const updateList = JSON.parse(await GetUpdateList({ force: false }));
				pluginSelf.connectionFailed = false;

				setUpdates(updateList.updates);
				setHasReceivedUpdates(true);
				resolve(true);
			} catch (exception) {
				console.error('Failed to fetch updates');
				pluginSelf.connectionFailed = true;
				reject(false);
			}
			resolve(true);
		});

	useEffect(() => {
		FetchAvailableUpdates();
	}, []);

	/** Check if the connection failed, this usually means the backend crashed or couldn't load */
	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />;
	}

	return !hasReceivedUpdates ? (
		<SteamSpinner background={'transparent'} />
	) : (
		<>{updates && (!updates.length ? <UpToDateModal /> : <RenderAvailableUpdates updates={updates} fetchUpdates={FetchAvailableUpdates} />)}</>
	);
};

export { UpdatesViewModal };
