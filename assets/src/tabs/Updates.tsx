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
	findClassModule,
} from '@steambrew/client';
import { formatString, locale } from '../locales';
import { ThemeItem } from '../types';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/ISteamComponents';
import Markdown from 'markdown-to-jsx';

interface UpdateProps {
	updates: UpdateItemType[];
	pluginUpdates: any[];
	fetchUpdates: (force?: boolean) => Promise<boolean>;
}

interface UpdateItemType {
	message: string; // Commit message
	date: string; // Humanized since timestamp
	commit: string; // Full GitHub commit URI
	native: string; // Folder name in skins folder.
	name: string; // Common display name
}

const updateListeners = new Set<() => void>();

const RegisterUpdateListener = (callback: () => void) => {
	updateListeners.add(callback);
	return () => updateListeners.delete(callback);
};

export const NotifyUpdateListeners = () => {
	updateListeners.forEach((callback) => callback());
};

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

const HasAnyPluginUpdates = (pluginUpdates: any) => {
	return pluginUpdates?.some((update: any) => update?.hasUpdate);
};

const HasAnyUpdates = (updates: any, pluginUpdates: any) => {
	return updates.length > 0 || HasAnyPluginUpdates(pluginUpdates);
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

const UpdateTheme = callable<[{ native: string }], boolean>('updater.download_theme_update');
const UpdatePlugin = callable<[{ id: string; name: string }], boolean>('updater.download_plugin_update');

const ResyncUpdates = callable<[], string>('updater.resync_updates');
const FindAllPlugins = callable<[], string>('find_all_plugins');

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

const MakeAnchorExternalLink = ({ children, ...props }: { children: any } & React.AnchorHTMLAttributes<HTMLAnchorElement>) => (
	<a {...props} href="#" onClick={() => SteamClient.System.OpenInSystemBrowser(props.href)}>
		{children}
	</a>
);

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates, pluginUpdates, fetchUpdates }) => {
	const [updatingThemes, setUpdatingThemes] = useState<Array<any>>([]);
	const [updatingPlugins, setUpdatingPlugins] = useState<Array<any>>([]);

	const viewMoreClick = (props: UpdateItemType) => SteamClient.System.OpenInSystemBrowser(props?.commit);
	const pluginViewMoreClick = (props: any) =>
		SteamClient.System.OpenInSystemBrowser(
			`https://github.com/${props?.pluginInfo?.repoOwner}/${props?.pluginInfo?.repoName}/tree/${props?.commit}`,
		);

	const IsUpdating = () => {
		/** Check if any theme or plugin is currently being updated. */
		return updatingThemes.some((updating: any) => updating) || updatingPlugins.some((updating: any) => updating);
	};

	const StartThemeUpdate = async (updateObject: UpdateItemType, index: number) => {
		if (IsUpdating()) {
			return;
		}

		setUpdatingThemes({ ...updatingThemes, [index]: true });

		if (await UpdateTheme({ native: updateObject.native })) {
			/** Check for updates */
			await fetchUpdates(true);
			const activeTheme: ThemeItem = pluginSelf.activeTheme;

			/** Check if the updated theme is currently in use, if so reload */
			const reload = await ShowMessageBox(
				formatString(
					activeTheme?.native === updateObject?.native ? locale.updateSuccessfulRestart : locale.updateSuccessful,
					updateObject?.name,
				),
				// @ts-ignore
				LocalizationManager.LocalizeString('#ImageUpload_SuccessCard'),
			);

			if (activeTheme?.native === updateObject?.native && reload) {
				SteamClient.Browser.RestartJSContext();
			}
		} else {
			// @ts-ignore
			ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), LocalizationManager.LocalizeString('#Generic_Error'));
		}

		setUpdatingThemes({ ...updatingThemes, [index]: false });
	};

	const StartPluginUpdate = async (updateObject: any, index: number) => {
		if (IsUpdating()) {
			return;
		}
		/** Check if the plugin is currently enabled, if so reload */
		const plugin = JSON.parse(await FindAllPlugins()).find((plugin: any) => plugin.data.name === updateObject?.pluginInfo?.pluginJson?.name);

		if (plugin?.enabled) {
			await ShowMessageBox(formatString(locale.updateFailedPluginRunning, updateObject?.pluginInfo?.pluginJson?.common_name), locale.HoldOn);
			return;
		}

		setUpdatingPlugins({ ...updatingPlugins, [index]: true });

		if (await UpdatePlugin({ id: updateObject?.id, name: updateObject?.pluginDirectory })) {
			await fetchUpdates(true);
		} else {
			// @ts-ignore
			ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), LocalizationManager.LocalizeString('#Generic_Error'));
		}

		setUpdatingPlugins({ ...updatingPlugins, [index]: false });
	};

	const fieldButtonsStyles: CSSProperties = { display: 'flex', gap: '8px' };
	const updateDescriptionStyles: CSSProperties = { display: 'flex', flexDirection: 'column' };
	const updateLabelStyles: CSSProperties = { display: 'flex', alignItems: 'center', gap: '8px' };

	return (
		<DialogControlsSection>
			<SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
			<DialogBodyText className="_3fPiC9QRyT5oJ6xePCVYz8">{locale.updatePanelHasUpdatesSub}</DialogBodyText>

			{updates?.map((update: UpdateItemType, index: number) => (
				<>
					<Field
						key={index}
						label={
							<div style={updateLabelStyles}>
								<div
									className="update-item-type"
									style={{ color: 'white', fontSize: '12px', padding: '4px 12px', background: '#007eff', borderRadius: '6px' }}
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
									<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
									<Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }}>{update?.message}</Markdown>
								</div>
							</div>
						}
						bottomSeparator={HasAnyPluginUpdates(pluginUpdates) ? 'standard' : updates.length - 1 === index ? 'none' : 'standard'}
					>
						<div style={fieldButtonsStyles}>
							<DialogButton onClick={() => viewMoreClick(update)} className="_3epr8QYWw_FqFgMx38YEEm">
								<IconsModule.Hyperlink style={{ width: '16px', height: '16px' }} />
							</DialogButton>
							<DialogButton
								onClick={() => StartThemeUpdate(update, index)}
								className="_3epr8QYWw_FqFgMx38YEEm"
								style={{ ...(updatingThemes[index] ? { padding: '8px 11px' } : { padding: '8px 14px' }) }}
							>
								{updatingThemes[index] ? (
									<SteamSpinner background={'transparent'} style={{ width: '16px', height: '16px' }} />
								) : (
									<IconsModule.Download style={{ width: '16px', height: '16px' }} />
								)}
							</DialogButton>
						</div>
					</Field>
				</>
			))}

			{pluginUpdates?.map(
				(update: any, index: number) =>
					update?.hasUpdate && (
						<>
							<Field
								key={index}
								label={
									<div style={updateLabelStyles}>
										<div
											className="update-item-type"
											style={{
												color: 'white',
												fontSize: '12px',
												padding: '4px 12px',
												background: '#564688',
												borderRadius: '6px',
											}}
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
											<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
											<Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }}>
												{update?.commitMessage}
											</Markdown>
										</div>
									</div>
								}
								bottomSeparator={pluginUpdates.length - 1 === index ? 'none' : 'standard'}
							>
								<div style={fieldButtonsStyles}>
									<DialogButton onClick={() => pluginViewMoreClick(update)} className="_3epr8QYWw_FqFgMx38YEEm">
										<IconsModule.Hyperlink style={{ width: '16px', height: '16px' }} />
									</DialogButton>
									<DialogButton
										onClick={() => StartPluginUpdate(update, index)}
										className="_3epr8QYWw_FqFgMx38YEEm"
										style={{
											...(updatingPlugins[index] ? { padding: '8px 11px' } : { padding: '8px 14px' }),
											minWidth: 'fit-content',
											display: 'flex',
											justifyContent: 'center',
											alignItems: 'center',
											gap: '11px',
										}}
									>
										{updatingPlugins[index] ? (
											<SteamSpinner background={'transparent'} style={{ width: '16px', height: '16px' }} />
										) : (
											<>
												{update?.pluginInfo?.downloadSize}
												<IconsModule.Download style={{ width: '16px', height: '16px' }} />
											</>
										)}
									</DialogButton>
								</div>
							</Field>
						</>
					),
			)}
		</DialogControlsSection>
	);
};

const UpdatesViewModal: React.FC = () => {
	const [updates, setThemeUpdates] = useState<Array<UpdateItemType>>(null);
	const [pluginUpdates, setPluginUpdates] = useState<any>(null);
	const [hasReceivedUpdates, setHasReceivedUpdates] = useState<boolean>(false);

	const ForceFetchUpdates = async () => {
		const updates = JSON.parse(await ResyncUpdates());

		pluginSelf.updates.themes = updates.themes;
		pluginSelf.updates.plugins = updates.plugins;

		NotifyUpdateListeners();
	};

	const FetchAvailableUpdates = async (force: boolean = false): Promise<boolean> => {
		try {
			if (force) {
				await ForceFetchUpdates();
			}

			pluginSelf.connectionFailed = false;

			setThemeUpdates(pluginSelf.updates.themes);
			setPluginUpdates(pluginSelf.updates.plugins);
			setHasReceivedUpdates(true);

			return true;
		} catch (exception) {
			console.error('Failed to fetch updates');
			pluginSelf.connectionFailed = true;
			return false;
		}
	};

	useEffect(() => {
		FetchAvailableUpdates();
	}, []);

	/** Check if the connection failed, this usually means the backend crashed or couldn't load */
	if (pluginSelf.connectionFailed) {
		return <ConnectionFailed />;
	}

	/** Check if the updates have been received, if not show a loading spinner */
	if (!hasReceivedUpdates) {
		return <SteamSpinner background={'transparent'} />;
	}

	return (
		<>
			<style>{`
				img[alt="Steam Spinner"] {
					height: 22px !important;
					width: auto !important;
				}

				img[alt="Steam Spinner"] + div {
					display: none !important;
				}

				.DialogControlsSection {
					margin-top: 0px !important;
				}
			`}</style>

			{!HasAnyUpdates(updates, pluginUpdates) ? (
				<UpToDateModal />
			) : (
				<RenderAvailableUpdates updates={updates} fetchUpdates={FetchAvailableUpdates} pluginUpdates={pluginUpdates} />
			)}
		</>
	);
};

const RenderUpdatesSettingsTab = () => {
	const [updateCount, setUpdateCount] = useState(0);

	useEffect(() => {
		const unregister = RegisterUpdateListener(() => {
			setUpdateCount(GetUpdateCount());
		});

		setUpdateCount(GetUpdateCount());
		return () => unregister();
	}, []);

	const sidebarTitleClass = (findClassModule((m) => m.ReturnToPageListButton && m.PageListItem_Title && m.HidePageListButton) as any)
		?.PageListItem_Title;

	/** Styles to display the updates count in the sidebar */
	const style = `
	.PageListColumn .sideBarUpdatesItem {
		display: flex;
		gap: 10px;
		justify-content: space-between;
		align-items: center;
		overflow: visible !important;
	}

	.${sidebarTitleClass} {
		overflow: visible !important;
		width: calc(100% - 32px);
	}

	.PageListColumn .FriendMessageCount {
		display: flex !important;
		margin-top: 0px !important;
		position: initial !important;

		line-height: 20px;
		height: fit-content !important;
		width: fit-content !important;
	}`;

	return (
		<div className="sideBarUpdatesItem unreadFriend">
			<style>{style}</style>

			{locale.settingsPanelUpdates}
			{updateCount > 0 && (
				<div className="FriendMessageCount" style={{ display: 'none' }}>
					{updateCount}
				</div>
			)}
		</div>
	);
};

const GetUpdateCount = () => {
	const updates = pluginSelf.updates.themes;
	const pluginUpdates = pluginSelf.updates.plugins;

	return updates.length + pluginUpdates.filter((update: any) => update?.hasUpdate).length;
};

export { UpdatesViewModal, RenderUpdatesSettingsTab, GetUpdateCount };
