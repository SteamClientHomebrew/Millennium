/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import { ConfirmModal } from '@steambrew/client';
import { InstallerProps, Theme } from '../types';
import { OnProgressUpdate, RendererProps } from './InstallerProgress';
import { backend } from '../utils/ffi';
import { formatString, locale } from '../utils/localization-manager';
import { ChangeActiveTheme, UIReloadProps } from '../settings/themes/ThemeComponent';

const OnInstallComplete = (data: any, props: InstallerProps) => {
	const UseNewTheme = async () => {
		try {
			const themeData = await backend.themes.getTheme(data?.skin_data?.github?.repo_name, data?.skin_data?.github?.owner, true);
			if (themeData) {
				await ChangeActiveTheme(themeData.native, UIReloadProps.Force);
			}
		} catch (error) {
			console.error('Error finding theme on disk:', error);
		}
	};

	/** Refetch theme data after installation */
	props?.refetchDataCb?.();

	return (
		<ConfirmModal
			strTitle={locale.strInstallComplete}
			strDescription={formatString(locale.strSuccessfulInstall, data?.name ?? locale.strUnknown)}
			bHideCloseIcon={true}
			strOKButtonText={locale.strUseThemeRequiresReload}
			onOK={() => {
				props?.modal?.Close?.();
				// Let the modal finish closing before restarting Steam's JS context.
				setTimeout(() => {
					void UseNewTheme();
				}, 150);
			}}
			onCancel={() => {
				props?.modal?.Close?.();
			}}
		/>
	);
};

export const StartThemeInstaller = async (data: any, props: InstallerProps): Promise<RendererProps | boolean> => {
	const theme: Theme = data?.skin_data;

	const owner = theme?.github?.owner ?? '';
	const repo = theme?.github?.repo_name ?? '';

	const isInstalled = await backend.themes.isInstalled(owner, repo);
	const { ShowMessageBox } = props;

	const UninstallTheme = (resolve: (value: unknown) => void) => {
		backend.themes.uninstall(owner, repo).then((response) => {
			if (!response?.success) {
				ShowMessageBox(formatString(locale.errorFailedToUninstallTheme, response?.message ?? locale.strUnknown), locale.errorMessageTitle, {
					onOK: () => props?.modal?.Close?.(),
					onCancel: () => props?.modal?.Close?.(),
				});
				resolve(false);
				return;
			}

			resolve(true);
		});
	};

	if (isInstalled) {
		const shouldContinueInstall = await new Promise((resolve) => {
			ShowMessageBox(locale.warningThemeAlreadyInstalled, locale.warningConflictingFiles, {
				strCancelButtonText: locale.strNeverMind,
				strOKButtonText: locale.strReinstall,
				onOK: UninstallTheme.bind(null, resolve),
				onCancel: () => {
					resolve(false);
					props?.modal?.Close?.();
				},
			});
		});

		if (!shouldContinueInstall) {
			return false;
		}
	}

	/** Start installer and extract opId for per-operation progress tracking */
	let opId = 0;
	try {
		const response = await backend.themes.install(owner, repo);
		if (!response?.success) {
			ShowMessageBox(response?.message ?? locale.errorFailedToStartThemeInstaller, locale.errorMessageTitle, {
				onOK: () => props?.modal?.Close?.(),
				onCancel: () => props?.modal?.Close?.(),
			});
			return false;
		}
		opId = response?.opId ?? 0;
	} catch {
		ShowMessageBox(locale.errorFailedToStartThemeInstaller, locale.errorMessageTitle, {
			onOK: () => props?.modal?.Close?.(),
			onCancel: () => props?.modal?.Close?.(),
		});
		return false;
	}

	return {
		onInstallComplete: OnInstallComplete.bind(null, data, props),
		onProgressUpdate: OnProgressUpdate,
		opId,
	};
};
