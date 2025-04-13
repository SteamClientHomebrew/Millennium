import { CSSProperties, useEffect, useState } from 'react';
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
	pluginUpdates: any;
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
		<div
			className="__up-to-date-container"
			style={{
				display: 'flex',
				flexDirection: 'column',
				alignItems: 'center',
				gap: '20px',
				height: '100%',
				justifyContent: 'center',
			}}
		>
			<IconsModule.Checkmark width="64" />
			<div className="__up-to-date-header" style={{ marginTop: '-120px', color: 'white', fontWeight: '500', fontSize: '15px' }}>
				{locale.updatePanelNoUpdatesFound}
			</div>
		</div>
	);
};

const timeAgo = (dateString: string) => {
	const rtf = new Intl.RelativeTimeFormat('en', { numeric: 'auto' });
	const date = new Date(dateString);
	const now = new Date();
	const seconds = Math.floor((now.getTime() - date.getTime()) / 1000);

	const intervals = {
		year: 31536000,
		month: 2592000,
		week: 604800,
		day: 86400,
		hour: 3600,
		minute: 60,
		second: 1,
	} as const;

	for (const [unit, value] of Object.entries(intervals)) {
		const diff = Math.floor(seconds / value);
		if (diff >= 1) {
			return rtf.format(-diff, unit as Intl.RelativeTimeFormatUnit);
		}
	}

	return 'just now';
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

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates, pluginUpdates, fetchUpdates }) => {
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
	const fieldButtonsStyles: CSSProperties = { display: 'flex', gap: '8px' };
	const updateButtonStyles: CSSProperties = { minWidth: '80px' };
	const updateDescriptionStyles: CSSProperties = { display: 'flex', flexDirection: 'column' };
	const updateLabelStyles: CSSProperties = { display: 'flex', alignItems: 'center', gap: '8px' };

	console.log(IconsModule);

	return (
		<DialogControlsSection>
			<SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
			<DialogBodyText className="_3fPiC9QRyT5oJ6xePCVYz8">{locale.updatePanelHasUpdatesSub}</DialogBodyText>

			{updates.map((update: UpdateItemType, index: number) => (
				<>
					<Field
						key={index}
						label={
							<div style={updateLabelStyles}>
								<div
									className="update-item-type"
									style={{ color: 'white', fontSize: '12px', padding: '4px', background: '#007eff', borderRadius: '6px' }}
								>
									Theme
								</div>
								{update.name}
							</div>
						}
						description={
							<div style={updateDescriptionStyles}>
								<div>
									<b>{locale.updatePanelReleasedTag}</b> {update?.date}
								</div>
								<div>
									<b>{locale.updatePanelReleasePatchNotes}</b> {update?.message}
								</div>
							</div>
						}
						bottomSeparator={pluginUpdates.length > 0 ? 'standard' : updates.length - 1 === index ? 'none' : 'standard'}
					>
						<div style={fieldButtonsStyles}>
							<DialogButton onClick={() => viewMoreClick(update)} className="_3epr8QYWw_FqFgMx38YEEm">
								<IconsModule.Hyperlink style={{ width: '16px', height: '16px' }} />
							</DialogButton>
							<DialogButton
								onClick={() => updateItemMessage(update, index)}
								className="_3epr8QYWw_FqFgMx38YEEm"
								style={{ ...(updating[index] ? { padding: '8px 11px' } : { padding: '8px 14px' }) }}
							>
								{updating[index] ? (
									<SteamSpinner background={'transparent'} style={{ width: '16px', height: '16px' }} />
								) : (
									<IconsModule.Download style={{ width: '16px', height: '16px' }} />
								)}
							</DialogButton>
						</div>
					</Field>
				</>
			))}

			{pluginUpdates.map((update: any, index: number) => (
				<>
					<Field
						key={index}
						label={
							<div style={updateLabelStyles}>
								<div
									className="update-item-type"
									style={{ color: 'white', fontSize: '12px', padding: '4px', background: '#564688', borderRadius: '6px' }}
								>
									Plugin
								</div>
								{update?.pluginInfo?.pluginJson?.common_name}
							</div>
						}
						description={
							<div style={updateDescriptionStyles}>
								<div>
									<b>{locale.updatePanelReleasedTag}</b> {timeAgo(update?.pluginInfo?.commitDate)}
								</div>
								<div>
									<b>{locale.updatePanelReleasePatchNotes}</b> {update?.message}
								</div>
							</div>
						}
						bottomSeparator={pluginUpdates.length - 1 === index ? 'none' : 'standard'}
					>
						<div style={fieldButtonsStyles}>
							<DialogButton onClick={() => viewMoreClick(update)} className="_3epr8QYWw_FqFgMx38YEEm">
								<IconsModule.Hyperlink style={{ width: '16px', height: '16px' }} />
							</DialogButton>
							<DialogButton
								onClick={() => updateItemMessage(update, index)}
								className="_3epr8QYWw_FqFgMx38YEEm"
								style={{ ...(updating[index] ? { padding: '8px 11px' } : { padding: '8px 14px' }) }}
							>
								{updating[index] ? (
									<SteamSpinner background={'transparent'} style={{ width: '16px', height: '16px' }} />
								) : (
									<IconsModule.Download style={{ width: '16px', height: '16px' }} />
								)}
							</DialogButton>
						</div>
					</Field>
				</>
			))}
		</DialogControlsSection>
	);
};

const GetUpdateList = callable<[{ force: boolean }], any>('updater.get_update_list');
const GetPluginUpdates = callable<[], any>('plugin_updater.check_for_updates');

const UpdatesViewModal: React.FC = () => {
	const [updates, setUpdates] = useState<Array<UpdateItemType>>(null);
	const [pluginUpdates, setPluginUpdates] = useState<any>(null);
	const [hasReceivedUpdates, setHasReceivedUpdates] = useState<boolean>(false);

	const FetchAvailableUpdates = async (): Promise<boolean> =>
		new Promise(async (resolve, reject) => {
			try {
				const updateList = JSON.parse(await GetUpdateList({ force: false }));
				const pluginUpdates = JSON.parse(await GetPluginUpdates());

				console.log(`Plugin updates:`, pluginUpdates);
				console.log(`Theme updates:`, updateList);

				pluginSelf.connectionFailed = false;

				setUpdates(updateList.updates);
				setPluginUpdates(pluginUpdates);
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
		<>
			<style>{`
				img[alt="Steam Spinner"] {
					height: 22px !important;
					width: auto !important;
				}

				img[alt="Steam Spinner"] + div {
					display: none !important;
				}
			`}</style>

			{updates &&
				(!updates.length && !pluginUpdates.length ? (
					<UpToDateModal />
				) : (
					<RenderAvailableUpdates updates={updates} fetchUpdates={FetchAvailableUpdates} pluginUpdates={pluginUpdates} />
				))}
		</>
	);
};

export { UpdatesViewModal };
