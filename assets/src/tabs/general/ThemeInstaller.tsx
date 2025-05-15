import { callable, ConfirmModal, ProgressBarWithInfo } from '@steambrew/client';
import { InstallerProps, Theme, ThemeItem } from '../../types';
import { RendererProps } from './Installer';
import { ChangeActiveTheme, UIReloadProps } from '../../utils/BackendFunctions';
import { useEffect, useState } from 'react';
import Styles from '../../styles';

const StartThemeInstallerBackend = callable<[{ repo: string; owner: string }], void>('themeInstaller.install_theme');
const StartThemeUninstallerBackend = callable<[{ repo: string; owner: string }], void>('themeInstaller.uninstall_theme');
const IsThemeInstalled = callable<[{ repo: string; owner: string }], boolean>('themeInstaller.check_install');
const FindThemeOnDisk = callable<[{ repo: string; owner: string; asString: boolean }], string>('themeInstaller.get_theme_from_gitpair');

const OnInstallComplete = (data: any, props: InstallerProps) => {
	console.log(data);

	const UseNewTheme = async () => {
		try {
			const themeData: ThemeItem = JSON.parse(
				await FindThemeOnDisk({ repo: data?.skin_data?.github?.repo_name, owner: data?.skin_data?.github?.owner, asString: true }),
			);
			ChangeActiveTheme(themeData?.native, UIReloadProps.Force);
		} catch (error) {
			console.error('Error finding theme on disk:', error);
		}
	};

	return (
		<ConfirmModal
			strTitle={'Installation Complete'}
			strDescription={`Successfully installed ${data.name}!`}
			bHideCloseIcon={true}
			strOKButtonText={'Use Theme (Restart Reload)'}
			onOK={() => {
				// ChangeActiveTheme(data.name, UIReloadProps.Force);
				UseNewTheme();
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

export const StartThemeInstaller = async (data: any, props: InstallerProps): Promise<RendererProps | boolean> => {
	const theme: Theme = data?.skin_data;

	const owner = theme?.github?.owner;
	const repo = theme?.github?.repo_name;

	const { ShowMessageBox } = props;

	const isInstalled = await IsThemeInstalled({ owner, repo });

	const UninstallTheme = (resolve: (value: unknown) => void) => {
		StartThemeUninstallerBackend({ repo, owner }).then((result: any) => {
			const response = JSON.parse(result);

			if (!response || !response?.success) {
				ShowMessageBox('Failed to uninstall theme', 'Whoops!');
				resolve(false);
			}

			resolve(true);
		});
	};

	if (isInstalled) {
		const shouldContinueInstall = await new Promise((resolve) => {
			ShowMessageBox(
				"You already have this theme installed! Would you like to reinstall it? If you've added any custom files their data will be lost.",
				'Conflicting Files',
				{
					strCancelButtonText: 'Never mind',
					strOKButtonText: 'Reinstall',
					onOK: UninstallTheme.bind(null, resolve),
					onCancel: () => {
						resolve(false);
						props?.modal?.Close?.();
					},
				},
			);
		});

		if (!shouldContinueInstall) {
			return false;
		}
	}

	/** Run installer in the background */
	StartThemeInstallerBackend({ repo, owner }).then((result: any) => {
		!result && ShowMessageBox('Failed to start internal installer module...', 'Whoops!');
	});

	return {
		onInstallComplete: OnInstallComplete.bind(null, data, props),
		onProgressUpdate: OnProgressUpdate,
	};
};
