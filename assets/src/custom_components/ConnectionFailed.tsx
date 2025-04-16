import { DialogButton, IconsModule, pluginSelf } from '@steambrew/client';

export const ConnectionFailed = () => {
	const OpenLogsFolder = () => {
		const logsPath = [pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/');
		SteamClient.System.OpenLocalDirectoryInSystemExplorer(logsPath);
	};

	return (
		<div className="MillenniumUpToDate_Container">
			<IconsModule.Caution width="64" />

			<div className="MillenniumUpToDate_Header">Failed to connect to Millennium!</div>
			<div className="MillenniumUpToDate_Text">
				This issue isn't network related, you're most likely missing a file millennium needs, or are experiencing an unexpected bug.
			</div>

			<DialogButton onClick={OpenLogsFolder}>Open Logs Folder</DialogButton>
		</div>
	);
};
