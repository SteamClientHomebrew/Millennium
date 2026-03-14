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

import { pluginSelf } from '@steambrew/client';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { formatString, locale, SteamLocale } from '../../utils/localization-manager';
import { UpdateCard, UpdateItemType } from './UpdateCard';
import { UpdateContextProviderState, useUpdateContext } from './useUpdateContext';
import { PyUpdateTheme } from '../../utils/ffi';
import { ThemeItem } from '../../types';
import { Utils } from '../../utils';
import { waitForInstallerComplete } from '../general/Installer';

async function StartThemeUpdate(ctx: UpdateContextProviderState, updateObject: UpdateItemType) {
	const key = updateObject.native;
	const { setUpdatingTheme, setThemeProgress, fetchAvailableUpdates } = ctx;
	if (ctx.updatingThemes[key]) return;

	setUpdatingTheme(key, true);
	setThemeProgress(key, { statusText: locale.strPreparing, progress: 0 });

	const result: any = await PyUpdateTheme({ native: updateObject.native });
	const parsed = typeof result === 'string' ? JSON.parse(result) : result;
	const opId: number = parsed?.opId ?? 0;

	const updateSuccess = await waitForInstallerComplete(opId, ({ progress, status }) => {
		setThemeProgress(key, { statusText: status, progress });
	});

	if (updateSuccess) {
		setThemeProgress(key, { statusText: locale.strFinishedUpdating, progress: 100 });
		await fetchAvailableUpdates(true);
		const activeTheme: ThemeItem = pluginSelf.activeTheme;

		if (activeTheme?.native === updateObject?.native) {
			const reload = await Utils.ShowMessageBox(formatString(locale.updateSuccessfulRestart, updateObject?.name), SteamLocale('#Settings_RestartRequired_Title'));
			reload && SteamClient.Browser.RestartJSContext();
		}
	} else {
		Utils.ShowMessageBox(formatString(locale.updateFailed, updateObject?.name), SteamLocale('#Generic_Error'));
	}

	setUpdatingTheme(key, false);
	setThemeProgress(key, null);
}

export function ThemeUpdateCard({ themeUpdates }: { themeUpdates: UpdateItemType[] }) {
	if (!themeUpdates || !themeUpdates.length) return null;

	const ctx = useUpdateContext();

	return (
		<>
			<SettingsDialogSubHeader>{locale.settingsPanelThemes}</SettingsDialogSubHeader>
			{themeUpdates?.map((update: UpdateItemType, index: number) => (
				<UpdateCard
					update={update}
					index={index}
					totalCount={themeUpdates.length}
					isUpdating={ctx.updatingThemes[update.native]}
					progress={ctx.themeProgress[update.native]?.progress}
					statusText={ctx.themeProgress[update.native]?.statusText}
					onUpdateClick={() => StartThemeUpdate(ctx, update)}
				/>
			))}
		</>
	);
}
