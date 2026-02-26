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

import React, { createContext, useContext, ReactNode, useState, useEffect, useCallback, useMemo } from 'react';
import { PluginComponent, ThemeItem } from '../types';
import { PyFindAllPlugins, PyFindAllThemes } from '../utils/ffi';
import { pluginSelf } from '@steambrew/client';
import { Utils } from '../utils';
import { locale } from '../utils/localization-manager';

export enum DesktopSideBarFocusedItemType {
	PLUGIN,
	THEME,
}

interface DesktopMenuContextType {
	isOpen: boolean;
	focusedItem?: PluginComponent | ThemeItem;
	focusedItemType?: DesktopSideBarFocusedItemType;
	plugins?: PluginComponent[];
	themes?: ThemeItem[];
	toggleMenu: () => void;
	setFocusedItem: (item?: PluginComponent | ThemeItem, type?: DesktopSideBarFocusedItemType) => void;
	closeMenu: () => void;
	openMenu: (userdata?: any) => void;
}

const DesktopMenuContext = createContext<DesktopMenuContextType | undefined>(undefined);

interface DesktopMenuProviderProps {
	children: ReactNode;
}

export const DesktopMenuProvider: React.FC<DesktopMenuProviderProps> = ({ children }) => {
	const [isOpen, setIsOpen] = useState(false);
	const [focusedItem, setFocusedLibraryItem] = useState<PluginComponent | ThemeItem | undefined>(undefined);
	const [plugins, setPlugins] = useState<PluginComponent[] | undefined>(undefined);
	const [themes, setThemes] = useState<ThemeItem[] | undefined>(undefined);
	const [focusedItemType, setFocusedItemType] = useState<DesktopSideBarFocusedItemType | undefined>(undefined);

	useEffect(() => {
		PyFindAllPlugins().then((pluginsJson: string) => {
			setPlugins(JSON.parse(pluginsJson));
		});
		PyFindAllThemes().then((themesJson: string) => {
			setThemes(JSON.parse(themesJson));
		});
	}, []);

	const openMenu = useCallback((userdata?: any) => {
		if (userdata?.data?.hasOwnProperty('libraryItem') && userdata?.data?.hasOwnProperty('libraryItemType')) {
			/** open the sidebar to a specfic library item */
			setFocusedItem(userdata?.data?.libraryItem, userdata?.data?.libraryItemType);
		} else {
			/** if we aren't explicitly asked to open the sidebar on a specific item, clear the last focused item to force go home */
			setFocusedLibraryItem(undefined);
		}

		setIsOpen(true);
	}, []);

	const closeMenu = useCallback(() => {
		setIsOpen(false);
	}, []);

	const toggleMenu = useCallback(() => {
		setIsOpen((prev) => !prev);
	}, [isOpen]);

	const setFocusedItem = useCallback((item?: PluginComponent | ThemeItem, type?: DesktopSideBarFocusedItemType) => {
		if (!item && pluginSelf.ConditionConfigHasChanged) {
			console.log('Theme config changed, prompting to reload the UI...');
			Utils.PromptReload(
				(shouldReload) => {
					if (shouldReload) {
						SteamClient.Browser.RestartJSContext();
					}
				},
				locale.themeConfigChangeReloadHeader,
				locale.themeConfigChangeReloadBody,
				locale.strConfirm,
			);
		}
		setFocusedLibraryItem(item);
		setFocusedItemType(type);
	}, []);

	const value = useMemo<DesktopMenuContextType>(
		() => ({
			isOpen,
			focusedItem,
			plugins,
			themes,
			toggleMenu,
			setFocusedItem,
			closeMenu,
			openMenu,
			focusedItemType,
		}),
		[isOpen, focusedItem, plugins, themes, toggleMenu, setFocusedItem, closeMenu, openMenu, focusedItemType],
	);

	return <DesktopMenuContext.Provider value={value}>{children}</DesktopMenuContext.Provider>;
};

export const useDesktopMenu = (): DesktopMenuContextType => {
	const context = useContext(DesktopMenuContext);
	if (context === undefined) {
		throw new Error('useDesktopMenu must be used within a DesktopMenuProvider');
	}
	return context;
};

export { DesktopMenuContext };
