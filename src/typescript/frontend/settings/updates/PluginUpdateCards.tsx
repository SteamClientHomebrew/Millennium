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
import { formatString, locale, SteamLocale } from '../../utils/localization-manager';
import { PyFindAllPlugins, PyKillPluginBackend, PyUpdatePlugin, PyUpdatePluginStatus } from '../../utils/ffi';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { UpdateContextProviderState, useUpdateContext } from './useUpdateContext';
import { waitForInstallerComplete } from '../general/Installer';

// TODO: Type this
type UpdateItemType = any;

const FindPluginByName = async (pluginName: string) => {
	const allPlugins = JSON.parse(await PyFindAllPlugins());
	return allPlugins.find((plugin: any) => plugin.data.name === pluginName);
};

const StartPluginUpdate = async (ctx: UpdateContextProviderState, updateObject: UpdateItemType) => {
	const key: string = updateObject?.id ?? updateObject?.pluginDirectory;
	const { setUpdatingPlugin, setPluginProgress, fetchAvailableUpdates } = ctx;
	if (ctx.updatingPlugins[key]) return;

	setUpdatingPlugin(key, true);
	setPluginProgress(key, { statusText: locale.strPreparing, progress: 0 });

	const pluginName = updateObject?.pluginInfo?.pluginJson?.name;
	const wasEnabled = (await FindPluginByName(pluginName))?.enabled;

	if (wasEnabled) {
		await PyKillPluginBackend({ pluginName });
	}

	const result: any = await PyUpdatePlugin({ id: updateObject?.id, name: updateObject?.pluginDirectory });
	const parsed = typeof result === 'string' ? JSON.parse(result) : result;
	const opId: number = parsed?.opId ?? 0;

	const updateSuccess = await waitForInstallerComplete(opId, ({ progress, status }) => {
		setPluginProgress(key, { statusText: status, progress });
	});

	if (wasEnabled) {
		if (!updateSuccess) {
			await Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'), { bAlertDialog: true });
		}
		setPluginProgress(key, { statusText: locale.strComplete, progress: 100 });
		sessionStorage.setItem('millennium-settings-tab', '/millennium/settings/updates');
		await PyUpdatePluginStatus({ pluginJson: JSON.stringify([{ plugin_name: pluginName, enabled: true }]) });
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

export function PluginUpdateCard({ pluginUpdates }: { pluginUpdates: any[] }) {
	if (!pluginUpdates || !pluginUpdates.length) return null;

	const ctx = useUpdateContext();

	return (
		<>
			<SettingsDialogSubHeader>{locale.settingsPanelPlugins}</SettingsDialogSubHeader>
			{pluginUpdates?.map((update: any, index: number) => (
				<UpdateCard
					update={{
						name: update?.pluginInfo?.pluginJson?.common_name,
						message: update?.commitMessage,
						date: Utils.toTimeAgo(update?.pluginInfo?.commitDate),
						commit: update?.pluginInfo?.commit,
					}}
					index={index}
					totalCount={pluginUpdates.length}
					isUpdating={ctx.updatingPlugins[update?.id ?? update?.pluginDirectory]}
					progress={ctx.pluginProgress[update?.id ?? update?.pluginDirectory]?.progress}
					statusText={ctx.pluginProgress[update?.id ?? update?.pluginDirectory]?.statusText}
					onUpdateClick={() => StartPluginUpdate(ctx, update)}
				/>
			))}
		</>
	);
}
