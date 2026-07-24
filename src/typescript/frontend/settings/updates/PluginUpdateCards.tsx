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

import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { formatString, locale, SteamLocale, tryResolveLocale } from '../../utils/localization-manager';
import { backend } from '../../utils/ffi';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { UpdateContextProviderState, useUpdateContext } from './useUpdateContext';
import { waitForInstallerComplete } from '../general/Installer';
import { PluginUpdateInfo } from '../../types';

const FindPluginByName = async (pluginName: string) => {
	const allPlugins = await backend.plugins.getPlugins();
	return allPlugins.find((plugin) => plugin.data.name === pluginName);
};

const StartPluginUpdate = async (ctx: UpdateContextProviderState, updateObject: PluginUpdateInfo) => {
	const pluginName = updateObject?.pluginInfo?.pluginJson?.name;
	const commit = updateObject?.pluginInfo?.commitId;
	if (!pluginName || !commit) return;

	const key: string = updateObject?.id ?? updateObject?.pluginDirectory;
	const { setUpdatingPlugin, setPluginProgress, fetchAvailableUpdates } = ctx;
	if (ctx.updatingPlugins[key]) return;

	setUpdatingPlugin(key, true);
	setPluginProgress(key, { statusText: locale.strPreparing, progress: 0 });

	const wasEnabled = (await FindPluginByName(pluginName))?.enabled;

	if (wasEnabled) {
		await backend.plugins.stop(pluginName);
	}

	const result = await backend.plugins.update(updateObject.id, updateObject.pluginDirectory, commit);
	const opId: number = result?.opId ?? 0;

	const updateSuccess = await waitForInstallerComplete(opId, ({ progress, status }) => {
		setPluginProgress(key, { statusText: tryResolveLocale(status), progress });
	});

	if (wasEnabled) {
		if (!updateSuccess) {
			await Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'), { bAlertDialog: true });
		}
		setPluginProgress(key, { statusText: locale.strComplete, progress: 100 });
		sessionStorage.setItem('millennium-settings-tab', '/millennium/settings/updates');
		await backend.plugins.togglePlugin(JSON.stringify([{ plugin_name: pluginName, enabled: true }]));
		return;
	}

	if (updateSuccess) {
		setPluginProgress(key, { statusText: locale.strComplete, progress: 100 });
		await fetchAvailableUpdates(true);
	} else {
		Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'), {
			bAlertDialog: true,
		});
	}

	setUpdatingPlugin(key, false);
	setPluginProgress(key, null);
};

export function PluginUpdateCard({ pluginUpdates }: { pluginUpdates: PluginUpdateInfo[] }) {
	if (!pluginUpdates || !pluginUpdates.length) return null;

	const ctx = useUpdateContext();

	return (
		<>
			<SettingsDialogSubHeader>{locale.settingsPanelPlugins}</SettingsDialogSubHeader>
			{pluginUpdates?.map((update, index) => (
				<UpdateCard
					update={{
						name: update?.pluginInfo?.pluginJson?.common_name,
						message: update?.commitMessage,
						date: Utils.toTimeAgo(update?.pluginInfo?.commitDate),
						commit: update?.pluginInfo?.commitId,
					}}
					index={index}
					isUpdating={ctx.updatingPlugins[update?.id ?? update?.pluginDirectory]}
					progress={ctx.pluginProgress[update?.id ?? update?.pluginDirectory]?.progress ?? 0}
					statusText={ctx.pluginProgress[update?.id ?? update?.pluginDirectory]?.statusText ?? ''}
					onUpdateClick={() => StartPluginUpdate(ctx, update)}
				/>
			))}
		</>
	);
}
