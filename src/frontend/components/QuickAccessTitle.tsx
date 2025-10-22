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

import { Focusable, joinClassNames, Navigation, quickAccessMenuClasses } from '@steambrew/client';
import { DesktopSideBarFocusedItemType, useDesktopMenu } from '../quick-access/DesktopMenuContext';
import { BsGearFill } from 'react-icons/bs';
import { FaArrowLeft } from 'react-icons/fa';
import { getPluginView } from '../utils/globals';
import { IconButton } from './IconButton';
import { PluginComponent, ThemeItem } from '../types';

export const TitleView = () => {
	const { closeMenu, focusedItem, focusedItemType, setFocusedItem } = useDesktopMenu();

	const onSettingsClick = () => {
		Navigation.Navigate('/millennium/settings');
		closeMenu();
	};

	if (!focusedItem) {
		return (
			<Focusable className={joinClassNames('MillenniumDesktopSidebar_Title', quickAccessMenuClasses.Title)}>
				<div>Library Settings</div>
				<IconButton onClick={onSettingsClick} style={{ marginLeft: 'auto' }}>
					<BsGearFill />
				</IconButton>
			</Focusable>
		);
	}

	/** If the selected item is a plugin, try to get its titleview prop */
	const libraryItemTitle =
		focusedItemType === DesktopSideBarFocusedItemType.PLUGIN
			? getPluginView((focusedItem as PluginComponent)?.data?.name)?.titleView || <div>{(focusedItem as PluginComponent)?.data?.common_name}</div>
			: (focusedItem as ThemeItem)?.data?.name;

	return (
		<Focusable className={joinClassNames('MillenniumDesktopSidebar_Title', quickAccessMenuClasses.Title)}>
			<IconButton onClick={setFocusedItem.bind(undefined, undefined)}>
				<FaArrowLeft />
			</IconButton>
			{libraryItemTitle}
		</Focusable>
	);
};
