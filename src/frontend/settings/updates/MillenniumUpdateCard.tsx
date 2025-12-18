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

import { ConfirmModal, pluginSelf, ProgressBar, showModal, ShowModalResult } from '@steambrew/client';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';
import { updateMillennium } from '../../utils/updateMillennium';
import { useEffect, useState } from 'react';
import { OSType } from '../../types';
import Markdown from 'markdown-to-jsx';
import { useUpdateContext } from './useUpdateContext';

export const MillenniumUpdateCard = ({ millenniumUpdates }: { millenniumUpdates: any }) => {
	const ctx = useUpdateContext();
	const [state, setState] = useState({ statusText: 'Starting updater...', progress: 0, isComplete: false });

	if (!millenniumUpdates || !millenniumUpdates?.hasUpdate) {
		return null; /** No update for Millennium available */
	}

	useEffect(() => {
		if (pluginSelf.platformType !== OSType.Windows) {
			return; /** we don't want this to fire on linux */
		}

		pluginSelf.InstallerEventEmitter = ({ progress, status, isComplete }: any) => {
			setState({ statusText: status, progress, isComplete });
		};
	}, []);

	function StartUpdateWindows() {
		updateMillennium(false).then(() => {
			ctx.setMillenniumUpdating(false);
		});

		ctx.setMillenniumUpdating(true);
	}

	const StartUpdateLinux = () => {
		showModal(
			<ConfirmModal
				strTitle={'Update Millennium'}
				strDescription={
					<Markdown>{`To update Millennium to ${millenniumUpdates?.newVersion?.tag_name}, run the following command in your terminal:\n\n\`${pluginSelf.millenniumLinuxUpdateScript}\``}</Markdown>
				}
				bAlertDialog={true}
			/>,
			pluginSelf.mainWindow,
			{ bNeverPopOut: false },
		);
	};

	function VersionInformation() {
		return [
			'Millennium',
			<div className="MillenniumItem_Version">
				{pluginSelf.version} {'->'} {millenniumUpdates?.newVersion?.tag_name}
			</div>,
		];
	}

	function StartUpdate() {
		if (pluginSelf.platformType === OSType.Windows) {
			StartUpdateWindows();
		} else if (pluginSelf.platformType === OSType.Linux) {
			StartUpdateLinux();
		}
	}

	return (
		<>
			<SettingsDialogSubHeader>Millennium</SettingsDialogSubHeader>
			<UpdateCard
				update={{
					name: VersionInformation(),
					message: millenniumUpdates?.newVersion?.body,
					date: Utils.toTimeAgo(millenniumUpdates?.newVersion?.published_at),
					commit: millenniumUpdates?.newVersion?.html_url,
				}}
				index={0}
				totalCount={1}
				isUpdating={ctx.isUpdatingMillennium}
				progress={state.progress}
				statusText={state.statusText}
				onUpdateClick={StartUpdate}
				toolTipText={'Millennium to ' + millenniumUpdates?.newVersion?.tag_name}
				disabled={pluginSelf?.millenniumUpdates?.updateInProgress}
			/>
		</>
	);
};
