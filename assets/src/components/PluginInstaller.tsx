/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { ConfirmModal, ProgressBarWithInfo } from '@steambrew/client';
import { InstallerProps } from '../types';
import Styles from '../utils/styles';
import { RendererProps } from '../settings/general/Installer';
import { API_URL } from '../utils/globals';
import { PyIsPluginInstalled, PyUpdatePluginStatus, PyInstallPlugin } from '../utils/ffi';
import { formatString, locale } from '../../locales';

const OnInstallComplete = (data: any, props: InstallerProps) => {
	const EnablePlugin = async () => {
		await PyUpdatePluginStatus({ pluginJson: JSON.stringify([{ plugin_name: data?.pluginJson?.name, enabled: true }]) });
	};

	/** Refetch plugin data after installation */
	props?.refetchDataCb();

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
					key={`installer-progress-${progress}`}
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
