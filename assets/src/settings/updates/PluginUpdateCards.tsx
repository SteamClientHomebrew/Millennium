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

import { produce } from 'immer';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { formatString, locale, SteamLocale } from '../../../locales';
import { PyFindAllPlugins, PyUpdatePlugin } from '../../utils/ffi';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { UpdateContextProviderState, useUpdateContext } from './useUpdateContext';
import { useState } from 'react';
import { sleep } from '@steambrew/client';

// TODO: Type this
type UpdateItemType = any;

interface UpdateState {
	statusText: string;
	progress: number;
	uxSleepLength: number;
}

const FindPluginByName = async (pluginName: string) => {
	const allPlugins = JSON.parse(await PyFindAllPlugins());
	return allPlugins.find((plugin: any) => plugin.data.name === pluginName);
};

const StartPluginUpdate = async (ctx: UpdateContextProviderState, setUpdateState: (ctx: UpdateState) => Promise<unknown>, updateObject: UpdateItemType, index: number) => {
	const { setUpdatingPlugins, isAnyUpdating, fetchAvailableUpdates } = ctx;
	if (isAnyUpdating()) return;

	setUpdatingPlugins((prev) =>
		produce(prev, (draft) => {
			draft[index] = !draft[index];
		}),
	);

	await setUpdateState({
		statusText: locale.strPreparing,
		progress: 10,
		uxSleepLength: 1000,
	});

	/** Generally unsafe to try to update the plugin when its running, so we prevent that */
	if ((await FindPluginByName(updateObject?.pluginInfo?.pluginJson?.name))?.enabled) {
		await Utils.ShowMessageBox(formatString(locale.updateFailedPluginRunning, updateObject?.pluginInfo?.pluginJson?.common_name), locale.HoldOn, {
			bAlertDialog: true,
		});

		setUpdatingPlugins((prev) =>
			produce(prev, (draft) => {
				draft[index] = false;
			}),
		);
		return;
	}

	await setUpdateState({
		statusText: locale.strUpdatingPlugin,
		progress: 60,
		uxSleepLength: 1000,
	});

	const updateSuccess = await PyUpdatePlugin({ id: updateObject?.id, name: updateObject?.pluginDirectory });

	await setUpdateState({
		statusText: locale.strComplete,
		progress: 100,
		uxSleepLength: 1000,
	});

	if (updateSuccess) {
		await fetchAvailableUpdates(true);
	} else {
		Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'), {
			bAlertDialog: true,
		});
	}

	setUpdatingPlugins((prev) =>
		produce(prev, (draft) => {
			draft[index] = false;
		}),
	);
};

export function PluginUpdateCard({ pluginUpdates }: { pluginUpdates: any[] }) {
	if (!pluginUpdates || !pluginUpdates.length) return null;

	const ctx = useUpdateContext();
	const [updateState, setUpdateState] = useState<UpdateState>(null);

	const setNewState = async (newState: UpdateState): Promise<unknown> => {
		setUpdateState(newState);
		return await sleep(newState.uxSleepLength);
	};

	return [
		<SettingsDialogSubHeader>Plugins</SettingsDialogSubHeader>,
		pluginUpdates?.map((update: any, index: number) => (
			<UpdateCard
				update={{
					name: update?.pluginInfo?.pluginJson?.common_name,
					message: update?.commitMessage,
					date: Utils.toTimeAgo(update?.pluginInfo?.commitDate),
					commit: update?.pluginInfo?.commit,
				}}
				index={index}
				totalCount={pluginUpdates.length}
				isUpdating={ctx.updatingPlugins[index]}
				progress={updateState?.progress}
				statusText={updateState?.statusText}
				onUpdateClick={() => StartPluginUpdate(ctx, setNewState, update, index)}
			/>
		)),
	];
}
