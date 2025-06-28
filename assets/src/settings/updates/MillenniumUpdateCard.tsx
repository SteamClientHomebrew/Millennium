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

import { pluginSelf } from '@steambrew/client';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';

export const MillenniumUpdateCard = ({ millenniumUpdates }: { millenniumUpdates: any }) => {
	if (!millenniumUpdates || !millenniumUpdates?.hasUpdate) return null;

	function VersionInformation() {
		return [
			'Millennium',
			<div className="MillenniumItem_Version">
				{pluginSelf.version} {'->'} {millenniumUpdates?.newVersion?.tag_name}
			</div>,
		];
	}

	return [
		<SettingsDialogSubHeader>Millennium</SettingsDialogSubHeader>,
		<UpdateCard
			update={{
				name: VersionInformation(),
				message: millenniumUpdates?.newVersion?.body,
				date: Utils.toTimeAgo(millenniumUpdates?.newVersion?.published_at),
				commit: millenniumUpdates?.newVersion?.html_url,
			}}
			index={0}
			totalCount={1}
			isUpdating={false}
			progress={0}
			statusText={String()}
			onUpdateClick={() => {}}
		/>,
	];
};
