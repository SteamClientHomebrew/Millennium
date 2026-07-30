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

import { ConfirmModal, Field } from '@steambrew/sdk';
import { InstallerProps } from '../types';
import { OnProgressUpdate, RendererProps } from './InstallerProgress';
import { API_URL, PLUGINS_URL } from '../utils/globals';
import { backend } from '../utils/ffi';
import { formatString, locale } from '../utils/localization-manager';
import { Utils } from '../utils';

interface DependencyStatus {
	name: string;
	range?: string;
	installed: boolean;
	enabled: boolean;
}

/** Resolve the plugin's declared dependencies ("name" or "name@<range>")
 *  against what is actually on disk. */
const ResolveDependencies = async (dependencies: string[]): Promise<DependencyStatus[]> => {
	const installedPlugins = await backend.plugins.getPlugins();

	return dependencies
		.filter((spec): spec is string => typeof spec === 'string' && spec.length > 0)
		.map((spec) => {
			const atIndex = spec.indexOf('@');
			const name = atIndex === -1 ? spec : spec.slice(0, atIndex);
			const range = atIndex === -1 ? undefined : spec.slice(atIndex + 1);
			const plugin = installedPlugins?.find((installed) => installed?.data?.name === name);

			return { name, range, installed: !!plugin, enabled: !!plugin?.enabled };
		});
};

const DependencyStateLabel = (dependency: DependencyStatus): string => {
	if (dependency.enabled) return locale.dependencyStateEnabled;
	return dependency.installed ? locale.dependencyStateDisabled : locale.dependencyStateMissing;
};

/** "min version 1.2.0" instead of the raw ">=1.2.0" spec syntax */
const DependencyVersionLabel = (range: string): string => {
	if (range.startsWith('>')) return formatString(locale.dependencyVersionMin, range.replace(/^>=?/, ''));
	if (range.startsWith('<')) return formatString(locale.dependencyVersionMax, range.replace(/^<=?/, ''));
	return formatString(locale.dependencyVersionExact, range.replace(/^=/, ''));
};

const ShowDependencyWarning = (data: any, dependencies: DependencyStatus[], props: InstallerProps): Promise<boolean> => {
	return new Promise((resolve) => {
		props?.ShowMessageBox(
			<>
				<Field description={locale.dependencyModalBody} />
				{dependencies.map((dependency) => (
					<Field
						key={dependency.name}
						label={
							<div className="MillenniumPlugins_PluginLabel">
								{dependency.range ? `${dependency.name},` : dependency.name}
								{dependency.range && <div className="MillenniumItem_Version">{DependencyVersionLabel(dependency.range)}</div>}
							</div>
						}
						description={DependencyStateLabel(dependency)}
					/>
				))}
				<Field label={locale.dependencyBrowsePlugins} description={<Utils.URLComponent url={PLUGINS_URL} />} bottomSeparator="none" />
			</>,
			formatString(locale.dependencyModalTitle, data?.pluginJson?.common_name ?? data?.pluginJson?.name),
			{
				strOKButtonText: locale.dependencyModalInstallAnyway,
				strCancelButtonText: locale.strNeverMind,
				onOK: () => resolve(true),
				onCancel: () => {
					resolve(false);
					props?.modal?.Close?.();
				},
			},
		);
	});
};

const OnInstallComplete = (data: any, props: InstallerProps) => {
	const EnablePlugin = async () => {
		await backend.plugins.togglePlugin(JSON.stringify([{ plugin_name: data?.pluginJson?.name, enabled: true }]));
	};

	/** Refetch plugin data after installation */
	props?.refetchDataCb?.();

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

export const StartPluginInstaller = async (data: any, props: InstallerProps): Promise<RendererProps | boolean> => {
	/** Plugin build is failing */
	if (!data?.hasValidBuild) {
		props?.ShowMessageBox(locale.strInvalidPluginBuildMessage, locale.strInvalidPluginBuild);
		return false;
	}

	const pluginName = data?.pluginJson?.name;
	if (!pluginName || typeof pluginName !== 'string') {
		props?.ShowMessageBox(locale.errorInvalidID, locale.errorMessageTitle);
		return false;
	}

	const isInstalled = await backend.plugins.isInstalled(pluginName);

	if (isInstalled) {
		props?.ShowMessageBox(formatString(locale.strAlreadyInPluginLibrary, data?.pluginJson?.common_name ?? locale.strUnknown), locale.strAlreadyInstalled, {
			bAlertDialog: true,
			onOK: () => {
				props?.modal?.Close?.();
			},
		});
		return false;
	}

	/** Warn about missing or disabled dependencies. Purely informational — the user can
	 *  always continue, and nothing is ever installed on their behalf. */
	const declaredDependencies: string[] = Array.isArray(data?.pluginJson?.dependencies) ? data.pluginJson.dependencies : [];

	if (declaredDependencies.length) {
		const dependencies = await ResolveDependencies(declaredDependencies);

		/** the dialog lists every dependency with its state, but only appears when at least one is unmet */
		if (dependencies.some((dependency) => !dependency.enabled) && !(await ShowDependencyWarning(data, dependencies, props))) {
			return false;
		}
	}

	const downloadUrl = API_URL + data?.downloadUrl;

	/** Start installer and extract opId for per-operation progress tracking */
	let opId = 0;
	try {
		const result = await backend.plugins.install(downloadUrl, data?.fileSize);
		if (!result?.success) {
			const message = result?.error ?? result?.message ?? locale.errorFailedToStartThemeInstaller;
			props?.ShowMessageBox(formatString(locale.errorFailedToDownloadPlugin, message), locale.errorMessageTitle);
			return false;
		}
		opId = result.opId ?? 0;
	} catch (error: unknown) {
		props?.ShowMessageBox(formatString(locale.errorFailedToDownloadPlugin, String(error)), locale.errorMessageTitle);
		return false;
	}

	return {
		onInstallComplete: OnInstallComplete.bind(null, data, props),
		onProgressUpdate: OnProgressUpdate,
		opId,
	};
};
