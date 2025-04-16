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
	findClassModule,
} from '@steambrew/client';
import { formatString, locale } from '../locales';
import { ThemeItem } from '../types';
import { ConnectionFailed } from '../custom_components/ConnectionFailed';
import { SettingsDialogSubHeader } from '../components/ISteamComponents';
import Markdown from 'markdown-to-jsx';
import { settingsClasses } from '../classes';

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
		<div className="MillenniumUpToDate_Container">
			<IconsModule.Checkmark width="64" />
			<div className="MillenniumUpToDate_Header">{locale.updatePanelNoUpdatesFound}</div>
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

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ updates: themeUpdates, pluginUpdates, fetchUpdates }) => {
	const [updatingThemes, setUpdatingThemes] = useState<boolean[]>([]);
	const [updatingPlugins, setUpdatingPlugins] = useState<boolean[]>([]);

	const viewMoreClick = (props: UpdateItemType) => SteamClient.System.OpenInSystemBrowser(props?.commit);
	const pluginViewMoreClick = (props: any) =>
		SteamClient.System.OpenInSystemBrowser(`https://github.com/${props?.pluginInfo?.repoOwner}/${props?.pluginInfo?.repoName}/tree/${props?.commit}`);

	const IsUpdating = () => {
		/** Check if any theme or plugin is currently being updated. */
		return updatingThemes.some((updating) => updating) || updatingPlugins.some((updating) => updating);
	};

	const StartThemeUpdate = async (updateObject: UpdateItemType, index: number) => {
		if (IsUpdating()) {
			return;
		}

		const newUpdatingThemes = [...updatingThemes];
		newUpdatingThemes[index] = true;
		setUpdatingThemes(newUpdatingThemes);

		if (await UpdateTheme({ native: updateObject.native })) {
			/** Check for updates */
			await fetchUpdates(true);
			const activeTheme: ThemeItem = pluginSelf.activeTheme;

			/** Check if the updated theme is currently in use, if so reload */
			const reload = await ShowMessageBox(
				formatString(activeTheme?.native === updateObject?.native ? locale.updateSuccessfulRestart : locale.updateSuccessful, updateObject?.name),
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

		const newUpdatingThemesAfter = [...updatingThemes];
		newUpdatingThemesAfter[index] = false;
		setUpdatingThemes(newUpdatingThemesAfter);
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

		const newUpdatingPlugins = [...updatingPlugins];
		newUpdatingPlugins[index] = true;
		setUpdatingPlugins(newUpdatingPlugins);

		if (await UpdatePlugin({ id: updateObject?.id, name: updateObject?.pluginDirectory })) {
			await fetchUpdates(true);
		} else {
			// @ts-ignore
			ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), LocalizationManager.LocalizeString('#Generic_Error'));
		}

		const newUpdatingPluginsAfter = [...updatingPlugins];
		newUpdatingPluginsAfter[index] = false;
		setUpdatingPlugins(newUpdatingPluginsAfter);
	};

	return (
		<DialogControlsSection>
			<SettingsDialogSubHeader>{locale.updatePanelHasUpdates}</SettingsDialogSubHeader>
			<DialogBodyText className="_3fPiC9QRyT5oJ6xePCVYz8">{locale.updatePanelHasUpdatesSub}</DialogBodyText>

			{themeUpdates?.map((update: UpdateItemType, index: number) => (
				<Field
					key={index}
					className="MillenniumUpdateField"
					label={
						<div className="MillenniumUpdates_Label">
							<div className="MillenniumUpdates_LabelType" data-type="theme">
								Theme
							</div>
							{update.name}
						</div>
					}
					description={
						<div className="MillenniumUpdates_Description">
							<div>
								<b>{locale.updatePanelReleasedTag}</b> {update?.date}
							</div>
							<div>
								<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
								<Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }}>{update?.message}</Markdown>
							</div>
						</div>
					}
					bottomSeparator={pluginUpdates.length > 0 ? 'standard' : index === themeUpdates.length - 1 ? 'none' : 'standard'}
				>
					<DialogButton onClick={() => viewMoreClick(update)} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
						<IconsModule.Hyperlink />
					</DialogButton>
					<DialogButton onClick={() => StartThemeUpdate(update, index)} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
						{updatingThemes[index] ? <SteamSpinner background={'transparent'} /> : <IconsModule.Download />}
					</DialogButton>
				</Field>
			))}

			{pluginUpdates?.map((update: any, index: number) => (
				<Field
					key={index}
					label={
						<div className="MillenniumUpdates_Label">
							<div className="MillenniumUpdates_LabelType" data-type="plugin">
								Plugin
							</div>
							{update?.pluginInfo?.pluginJson?.common_name}
						</div>
					}
					description={
						<div className="MillenniumUpdates_Description">
							<div>
								<b>{locale.updatePanelReleasedTag}</b> {timeAgo(update?.pluginInfo?.commitDate)}
							</div>
							<div>
								<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
								<Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }}>{update?.commitMessage}</Markdown>
							</div>
						</div>
					}
					bottomSeparator={index === pluginUpdates.length - 1 ? 'none' : 'standard'}
				>
					<DialogButton onClick={() => pluginViewMoreClick(update)} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
						<IconsModule.Hyperlink style={{ width: '16px', height: '16px' }} />
					</DialogButton>
					<DialogButton onClick={() => StartPluginUpdate(update, index)} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
						{updatingPlugins[index] ? (
							<SteamSpinner background={'transparent'} />
						) : (
							<>
								{update?.pluginInfo?.downloadSize}
								<IconsModule.Download />
							</>
						)}
					</DialogButton>
				</Field>
			))}
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

	if (!HasAnyUpdates(updates, pluginUpdates)) {
		return <UpToDateModal />;
	}

	return <RenderAvailableUpdates updates={updates} fetchUpdates={FetchAvailableUpdates} pluginUpdates={pluginUpdates?.filter((update: any) => update?.hasUpdate)} />;
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

	const sidebarTitleClass = (findClassModule((m) => m.ReturnToPageListButton && m.PageListItem_Title && m.HidePageListButton) as any)?.PageListItem_Title;

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
				<div className="FriendMessageCount MillenniumUpdateCount" style={{ display: 'none' }}>
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
