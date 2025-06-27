import { pluginSelf, sleep } from '@steambrew/client';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { formatString, locale, SteamLocale } from '../../../locales';
import { UpdateCard, UpdateItemType } from './UpdateCard';
import { UpdateContextProviderState, useUpdateContext } from './useUpdateContext';
import { PyUpdateTheme } from '../../utils/ffi';
import { ThemeItem } from '../../types';
import { Utils } from '../../utils';
import { produce } from 'immer';
import { useState } from 'react';

interface UpdateState {
	statusText: string;
	progress: number;
	uxSleepLength: number;
}

async function StartThemeUpdate(ctx: UpdateContextProviderState, setUpdateState: (newState: UpdateState) => Promise<unknown>, updateObject: UpdateItemType, index: number) {
	const { setUpdatingThemes, isAnyUpdating, fetchAvailableUpdates } = ctx;
	if (isAnyUpdating()) return;

	setUpdatingThemes((prev) =>
		produce(prev, (draft) => {
			draft[index] = true;
		}),
	);

	await setUpdateState({
		statusText: locale.strUpdatingTheme,
		progress: 30,
		uxSleepLength: 1000,
	});

	const updateSuccess = await PyUpdateTheme({ native: updateObject.native });

	if (updateSuccess) {
		await setUpdateState({
			statusText: locale.strFinishedUpdating,
			progress: 100,
			uxSleepLength: 1000,
		});

		await fetchAvailableUpdates(true);
		const activeTheme: ThemeItem = pluginSelf.activeTheme;

		const reload = await Utils.ShowMessageBox(
			formatString(activeTheme?.native === updateObject?.native ? locale.updateSuccessfulRestart : locale.updateSuccessful, updateObject?.name),
			SteamLocale('#ImageUpload_SuccessCard'),
		);

		if (activeTheme?.native === updateObject?.native && reload) {
			SteamClient.Browser.RestartJSContext();
		}
	} else {
		Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'));
	}

	setUpdatingThemes((prev) =>
		produce(prev, (draft) => {
			draft[index] = false;
		}),
	);
}

export function ThemeUpdateCard({ themeUpdates }: { themeUpdates: UpdateItemType[] }) {
	if (!themeUpdates || !themeUpdates.length) return null;

	const ctx = useUpdateContext();
	const [updateState, setUpdateState] = useState<UpdateState>(null);

	const setNewState = async (newState: UpdateState): Promise<unknown> => {
		setUpdateState(newState);
		return await sleep(newState.uxSleepLength);
	};

	return [
		<SettingsDialogSubHeader>Themes</SettingsDialogSubHeader>,
		themeUpdates?.map((update: UpdateItemType, index: number) => (
			<UpdateCard
				update={update}
				index={index}
				totalCount={themeUpdates.length}
				isUpdating={ctx.updatingThemes[index]}
				progress={updateState?.progress}
				statusText={updateState?.statusText}
				onUpdateClick={StartThemeUpdate.spread(ctx, setNewState, update, index)}
			/>
		)),
	];
}
