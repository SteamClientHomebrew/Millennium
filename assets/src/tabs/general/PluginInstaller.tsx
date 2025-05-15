import { callable, ConfirmModal, ProgressBarWithInfo } from '@steambrew/client';
import { InstallerProps } from '../../types';
import { useEffect, useState } from 'react';
import Styles from '../../styles';
import { RendererProps } from './Installer';
import { UpdatePluginStatus } from '../Plugins';

const StartPluginInstallerBackend = callable<[{ download_url: string; total_size: number }], void>('pluginInstaller.install_plugin');
const StartThemeUninstallerBackend = callable<[{ download_url: string }], void>('pluginInstaller.uninstall_plugin');
const IsPluginInstalled = callable<[{ plugin_name: string }], boolean>('pluginInstaller.check_install');

const OnInstallComplete = (data: any, props: InstallerProps) => {
	console.log(data);

	const EnablePlugin = async () => {
		await UpdatePluginStatus({ pluginJson: JSON.stringify([{ plugin_name: data?.pluginJson?.name, enabled: true }]) });
	};

	return (
		<ConfirmModal
			strTitle={'Installation Complete'}
			strDescription={`Successfully installed ${data?.pluginJson?.common_name ?? 'unknown'}!`}
			bHideCloseIcon={true}
			strOKButtonText={'Enable Plugin (Requires Reload)'}
			onOK={() => {
				// ChangeActiveTheme(data.name, UIReloadProps.Force);
				EnablePlugin();
				props?.modal?.Close?.();
			}}
			onCancel={() => {
				props?.modal?.Close?.();
			}}
		/>
	);
};

const OnProgressUpdate = ({ progress, status }: { progress: number; status: string }) => {
	const [timeRemaining, setTimeRemaining] = useState(10000);

	useEffect(() => {
		setInterval(() => {
			setTimeRemaining((prev) => {
				if (prev <= 0) {
					return 0;
				}
				return prev - 10;
			});
		}, 10);
	}, []);

	const RenderBody = () => {
		return (
			<>
				<Styles />
				<ProgressBarWithInfo
					/* @ts-ignore */
					className="InstallerProgressBar"
					rtEstimatedCompletionTime={timeRemaining}
					sOperationText={status}
					nProgress={progress}
					nTransitionSec={0.5}
				/>
			</>
		);
	};

	return (
		<ConfirmModal
			strTitle={'Installation Progress'}
			strDescription={<RenderBody />}
			bHideCloseIcon={true}
			onCancel={() => {}}
			bAlertDialog={true}
			bCancelDisabled={true}
			bDisableBackgroundDismiss={true}
		/>
	);
};

export const StartPluginInstaller = async (data: any, props: InstallerProps): Promise<RendererProps | boolean> => {
	console.log('Starting plugin installer...', data);

	if (!data?.hasValidBuild) {
		props?.ShowMessageBox('This plugin does not have a valid build for your OS.', 'Invalid Build');
		return false;
	}

	const isInstalled = await IsPluginInstalled({ plugin_name: data?.pluginJson?.name });

	if (isInstalled) {
		props?.ShowMessageBox('This plugin is already installed!', 'Plugin Already Installed');
		return false;
	}

	console.log('Plugin is not installed, continuing...');

	const downloadUrl = 'https://steambrew.app' + data?.downloadUrl;

	StartPluginInstallerBackend({ download_url: downloadUrl, total_size: data?.fileSize }).then((result: any) => {
		try {
			const jsonResponse = JSON.parse(result);

			if (!jsonResponse?.success) {
				throw new Error(jsonResponse?.message);
			}
		} catch (error) {
			// console.error('Failed to parse plugin installer response:', error);
			props?.ShowMessageBox('Failed to download plugin: ' + error, 'Whoops!');
		}
	});

	// const theme: Theme = data?.skin_data;

	// const owner = theme?.github?.owner;
	// const repo = theme?.github?.repo_name;

	// const { ShowMessageBox } = props;

	// const UninstallTheme = (resolve: (value: unknown) => void) => {
	// 	StartThemeUninstallerBackend({ repo, owner }).then((result: any) => {
	// 		const response = JSON.parse(result);

	// 		if (!response || !response?.success) {
	// 			ShowMessageBox('Failed to uninstall theme', 'Whoops!');
	// 			resolve(false);
	// 		}

	// 		resolve(true);
	// 	});
	// };

	// if (isInstalled) {
	// 	const shouldContinueInstall = await new Promise((resolve) => {
	// 		ShowMessageBox(
	// 			"You already have this theme installed! Would you like to reinstall it? If you've added any custom files their data will be lost.",
	// 			'Conflicting Files',
	// 			{
	// 				strCancelButtonText: 'Never mind',
	// 				strOKButtonText: 'Reinstall',
	// 				onOK: UninstallTheme.bind(null, resolve),
	// 				onCancel: () => {
	// 					resolve(false);
	// 					props?.modal?.Close?.();
	// 				},
	// 			},
	// 		);
	// 	});

	// 	if (!shouldContinueInstall) {
	// 		return false;
	// 	}
	// }

	// /** Run installer in the background */
	// StartThemeInstallerBackend({ repo, owner }).then((result: any) => {
	// 	!result && ShowMessageBox('Failed to start internal installer module...', 'Whoops!');
	// });

	return {
		onInstallComplete: OnInstallComplete.bind(null, data, props),
		onProgressUpdate: OnProgressUpdate,
	};
};
