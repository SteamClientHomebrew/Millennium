import Markdown from 'markdown-to-jsx';
import {
	callable,
	DialogBody,
	DialogBodyText,
	DialogButton,
	DialogFooter,
	DialogHeader,
	Field,
	pluginSelf,
	showModal,
	TextField,
	Toggle,
} from '@steambrew/client';
import { useEffect, useState } from 'react';
import { OSType, UpdaterOptionProps } from '../types';
import { locale } from '../locales';
import { GenericConfirmDialog } from './GenericDialog';
import { ShowDownloadInformation } from './modals/UpdateInfoModal';
import { PromptSelectUpdaterOptions } from './modals/SelectUpdatePrefs';
import { OnDoNotShowAgainChange } from './modals/DisableUpdates';
import { Separator } from '../components/ISteamComponents';

const UpdateMillennium = callable<[{ downloadUrl: string }], void>('MillenniumUpdater.queue_update');
const ShouldShowUpdateInfo = callable<[], boolean>('should_show_update_modal');

const ContainerURLStyles = {
	display: 'flex',
	justifyContent: 'flex-end',
	fontSize: '13px',
	marginBottom: '10px',
};

const LOG_UPDATE_INFO = (...params: any[]) => {
	console.log(`%c[Millennium Updater]%c`, 'background: lightblue; color: black;', 'background: inherit; color: inherit;', ...params);
};

const MakeAnchorExternalLink = ({ children, ...props }: { children: any; props: any }) => (
	<a target="_blank" {...props}>
		{children}
	</a>
);

const UpdateAvailablePopup = ({ props, targetAsset, currentOSType }: { props: any; targetAsset: any; currentOSType: OSType }) => {
	const [doNotShowAgain, setDoNotShowAgain] = useState(false);
	const [updateScript, setUpdateScript] = useState<string>();

	const SetupLinux = async () => {
		if (currentOSType == OSType.Linux) {
			const GetEnvironmentVar = callable<[{ variable: string }], string>('_get_env_var');
			const updateScript = await GetEnvironmentVar({ variable: 'MILLENNIUM__UPDATE_SCRIPT_PROMPT' });

			setUpdateScript(updateScript);
		}
	};

	useEffect(() => {
		SetupLinux();
	}, []);

	const OpenDiffInExternalBrowser = () => {
		SteamClient.System.OpenInSystemBrowser(
			`https://github.com/shdwmtr/millennium/compare/${pluginSelf.version}...${props.tag_name}#files_bucket`,
		);
	};

	const CloseWindow = () => {
		pluginSelf?.windows?.['Millennium Updater']?.SteamClient?.Window?.Close();
	};

	const StartUpdateProcess = () => {
		UpdateMillennium({ downloadUrl: targetAsset.browser_download_url });
		CloseWindow();
	};

	const RenderWindowsFooter = () => (
		<>
			<div className="UpdateLinksContainer" style={{ display: 'flex', gap: '10px', justifyContent: 'flex-end' }}>
				<a className="viewDiffLink" onClick={OpenDiffInExternalBrowser} style={ContainerURLStyles}>
					{locale.strViewUpdateDiffInBrowser}
				</a>
				<a className="viewDownloadInfo" onClick={ShowDownloadInformation.bind(null, props, targetAsset)} style={ContainerURLStyles}>
					{locale.strViewDownloadInfo}
				</a>
			</div>

			<div className="DialogTwoColLayout _DialogColLayout Panel">
				<Field label={locale.strDontShowAgain} bottomSeparator="none">
					<Toggle value={doNotShowAgain} onChange={(newValue) => OnDoNotShowAgainChange(newValue, setDoNotShowAgain)} />
				</Field>

				<DialogButton className="Primary" onClick={StartUpdateProcess}>
					{locale.strUpdateNextStartup}
				</DialogButton>
				<DialogButton className="Secondary" onClick={CloseWindow}>
					{locale.strUpdateReject}
				</DialogButton>
			</div>
		</>
	);

	const RenderLinuxFooter = () => (
		<>
			<div className="DialogTwoColLayout _DialogColLayout Panel" style={{ alignItems: 'baseline', justifyContent: 'space-between' }}>
				<Field label={locale.strDontShowAgain} bottomSeparator="none">
					<Toggle value={doNotShowAgain} onChange={(newValue) => OnDoNotShowAgainChange(newValue, setDoNotShowAgain)} />
				</Field>

				<div className="UpdateLinksContainer" style={{ display: 'flex', gap: '10px', justifyContent: 'flex-end' }}>
					<a className="viewDiffLink" onClick={OpenDiffInExternalBrowser} style={ContainerURLStyles}>
						{locale.strViewUpdateDiffInBrowser}
					</a>
					<a className="viewDownloadInfo" onClick={ShowDownloadInformation.bind(null, props, targetAsset)} style={ContainerURLStyles}>
						{locale.strViewDownloadInfo}
					</a>
				</div>
			</div>

			<div className="DialogTwoColLayout _DialogColLayout Panel">
				<TextField value={updateScript} />

				<DialogButton className="Secondary" onClick={CloseWindow}>
					{locale.strUpdateReject}
				</DialogButton>
			</div>
		</>
	);

	return (
		<GenericConfirmDialog>
			<DialogHeader> Update Available! 💫 </DialogHeader>
			<DialogBody>
				<DialogBodyText style={{ overflowY: 'scroll', marginBottom: 'unset' }}>
					<p className="updateInfoTidbit">{locale.strAnUpdateIsAvailable}</p>
					<Separator />
					<Markdown options={{ overrides: { a: { component: MakeAnchorExternalLink } } }} className="MillenniumReleaseMarkdown">
						{props.body}
					</Markdown>
				</DialogBodyText>

				<DialogFooter style={{ flexDirection: 'column' }}>
					{currentOSType == OSType.Linux ? <RenderLinuxFooter /> : <RenderWindowsFooter />}
				</DialogFooter>
			</DialogBody>
		</GenericConfirmDialog>
	);
};

const GetPlatformSpecificAsset = async (releaseInfo: any, currentOSType: OSType) => {
	switch (currentOSType) {
		case OSType.Windows:
			return releaseInfo.assets.find((asset: any) => asset.name === `millennium-${releaseInfo.tag_name}-windows-x86_64.zip`);
		case OSType.Linux:
			return releaseInfo.assets.find((asset: any) => asset.name === `millennium-${releaseInfo.tag_name}-linux-x86_64.tar.gz`);

		default: {
			console.error("Invalid platform, can't find platform specific assets");
			return null;
		}
	}
};

export const ShowUpdaterModal = async () => {
	if (!(await ShouldShowUpdateInfo())) {
		LOG_UPDATE_INFO('Already shown update info, skipping...');
		return;
	}

	/** If the user hasn't specified their update options, prompt them */
	if (pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.UNSET || pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.UNSET) {
		LOG_UPDATE_INFO("User hasn't specified update options yet, prompting...");
		PromptSelectUpdaterOptions();
		return;
	}

	/** If the user doesn't want updates, cancel */
	if (pluginSelf.wantsMillenniumUpdates == UpdaterOptionProps.NO) {
		LOG_UPDATE_INFO('User has disabled updates, skipping...');
		return;
	}

	/** Pull updates from the backend in %root%/assets/core/updater/millennium.py */
	const GetAvailableUpdates = callable<[], any>('MillenniumUpdater.has_any_updates');
	const updates = JSON.parse(await GetAvailableUpdates());

	/** If there are no updates, cancel */
	if (updates.hasUpdate === false) {
		LOG_UPDATE_INFO('No updates available, skipping...');
		return;
	}

	const GetOSType = callable<[], OSType>('_get_os_type');
	const currentOSType = await GetOSType();

	const targetAsset = await GetPlatformSpecificAsset(updates.newVersion, currentOSType);

	if (pluginSelf.wantsMillenniumUpdateNotifications == UpdaterOptionProps.NO) {
		// Start the update process without showing the modal; as they want updates, but not notifications
		LOG_UPDATE_INFO('User has disabled update notifications, but updates are enabled. Starting update process...');
		UpdateMillennium({ downloadUrl: targetAsset.browser_download_url });
		return;
	}

	LOG_UPDATE_INFO('Updates available, showing modal with props: ', updates);

	showModal(<UpdateAvailablePopup props={updates.newVersion} targetAsset={targetAsset} currentOSType={currentOSType} />, pluginSelf.mainWindow, {
		strTitle: 'Millennium Updater',
		popupHeight: 500,
		popupWidth: 725,
	});

	const intervalId = setInterval(() => {
		const updaterWindow = pluginSelf?.windows?.['Millennium Updater'];

		if (!updaterWindow) {
			return;
		}

		updaterWindow.SteamClient?.Window?.MarkLastFocused();
		updaterWindow.SteamClient?.Window?.FlashWindow();
		updaterWindow.SteamClient?.Window?.BringToFront();
		clearInterval(intervalId);
	}, 1);
};
