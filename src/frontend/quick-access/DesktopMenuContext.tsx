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
import { PluginComponent } from '../types';
import { PyFindAllPlugins } from '../utils/ffi';

interface DesktopMenuContextType {
	isOpen: boolean;
	activePlugin?: PluginComponent;
	plugins?: PluginComponent[];
	toggleMenu: () => void;
	setActivePlugin: (plugin?: PluginComponent) => void;
	closeMenu: () => void;
	openMenu: () => void;
}

const DesktopMenuContext = createContext<DesktopMenuContextType | undefined>(undefined);

interface DesktopMenuProviderProps {
	children: ReactNode;
}

export const DesktopMenuProvider: React.FC<DesktopMenuProviderProps> = ({ children }) => {
	const [isOpen, setIsOpen] = useState(false);
	const [activePlugin, setActivePluginState] = useState<PluginComponent | undefined>(undefined);
	const [plugins, setPlugins] = useState<PluginComponent[] | undefined>(undefined);

	useEffect(() => {
		PyFindAllPlugins().then((pluginsJson: string) => {
			setPlugins(JSON.parse(pluginsJson));
		});
	}, []);

	const openMenu = useCallback(() => {
		setActivePluginState(undefined);
		setIsOpen(true);
	}, []);

	const closeMenu = useCallback(() => {
		setIsOpen(false);
	}, []);

	const toggleMenu = useCallback(() => {
		setIsOpen((prev) => !prev);
	}, [isOpen]);

	const setActivePlugin = useCallback((plugin?: PluginComponent) => {
		setActivePluginState(plugin);
	}, []);

	const value = useMemo<DesktopMenuContextType>(
		() => ({
			isOpen,
			activePlugin,
			plugins,
			toggleMenu,
			setActivePlugin,
			closeMenu,
			openMenu,
		}),
		[isOpen, activePlugin, plugins, toggleMenu, setActivePlugin, closeMenu, openMenu],
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
