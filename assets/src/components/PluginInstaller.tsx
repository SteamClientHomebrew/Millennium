import { callable, ConfirmModal, ProgressBarWithInfo } from '@steambrew/client';
import { InstallerProps } from '../types';
import { useEffect, useState } from 'react';
import Styles from '../utils/styles';
import { RendererProps } from '../settings/general/Installer';
import { API_URL } from '../utils/globals';
import { PyIsPluginInstalled, PyUpdatePluginStatus, PyInstallPlugin } from '../utils/ffi';
import { formatString, locale } from '../../locales';

const OnInstallComplete = (data: any, props: InstallerProps) => {
	const EnablePlugin = async () => {
		await PyUpdatePluginStatus({ pluginJson: JSON.stringify([{ plugin_name: data?.pluginJson?.name, enabled: true }]) });
	};

	return (
		<ConfirmModal
			strTitle={locale.strInstallComplete}
			strDescription={formatString(locale.strSuccessfulInstall, data?.pluginJson?.common_name ?? locale.strUnknown)}
			bHideCloseIcon={true}
			strOKButtonText={locale.strEnablePlugin}
			onOK={() => {
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
	const RenderBody = () => {
		return (
			<>
				<Styles />
				<ProgressBarWithInfo
					/* @ts-ignore */
					className="MillenniumInstallerDialog_ProgressBar"
					sOperationText={status}
					nProgress={progress}
					nTransitionSec={0.5}
				/>
			</>
		);
	};

	return (
		<ConfirmModal
			className="MillenniumInstallerDialog"
			strTitle={locale.strInstallProgress}
			strDescription={<RenderBody />}
			bHideCloseIcon={true}
			onCancel={() => {}}
			bAlertDialog={true}
			bCancelDisabled={true}
			bDisableBackgroundDismiss={true}
			bOKDisabled={true}
		/>
	);
};

export const StartPluginInstaller = async (data: any, props: InstallerProps): Promise<RendererProps | boolean> => {
	/** Plugin build is failing */
	if (!data?.hasValidBuild) {
		props?.ShowMessageBox(locale.strInvalidPluginBuildMessage, locale.strInvalidPluginBuild);
		return false;
	}

	const isInstalled = await PyIsPluginInstalled({ plugin_name: data?.pluginJson?.name });

	if (isInstalled) {
		props?.ShowMessageBox(formatString(locale.strAlreadyInPluginLibrary, data?.pluginJson?.common_name ?? locale.strUnknown), locale.strAlreadyInstalled, {
			bAlertDialog: true,
			onOK: () => {
				props?.modal?.Close?.();
			},
		});
		return false;
	}

	const downloadUrl = API_URL + data?.downloadUrl;

	PyInstallPlugin({ download_url: downloadUrl, total_size: data?.fileSize }).then((result: any) => {
		try {
			const jsonResponse = JSON.parse(result);

			if (!jsonResponse?.success) {
				throw new Error(jsonResponse?.message);
			}
		} catch (error) {
			props?.ShowMessageBox(formatString(locale.errorFailedToDownloadPlugin, error), locale.errorMessageTitle);
		}
	});

	return {
		onInstallComplete: OnInstallComplete.spread(data, props),
		onProgressUpdate: OnProgressUpdate,
	};
};
