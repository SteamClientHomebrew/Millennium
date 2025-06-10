import React, { createContext, useContext, useEffect, useState } from 'react';
import { getDesktopMenuOpen, setDesktopMenuOpen, subscribeDesktopMenu, getActivePlugin, setActivePlugin, subscribeActivePlugin } from './desktopMenuStore';
import { PluginComponent } from '../types';

type DesktopMenuContextType = {
	desktopMenuOpen: boolean;
	setDesktopMenuOpen: (value: boolean) => void;
	activePlugin: PluginComponent;
	setActivePlugin: (plugin: PluginComponent) => void;
};

const DesktopMenuContext = createContext<DesktopMenuContextType | undefined>(undefined);

export const DesktopMenuProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
	const [desktopMenuOpen, setLocalDesktopMenuOpen] = useState(getDesktopMenuOpen());
	const [activePlugin, setLocalActivePlugin] = useState(getActivePlugin());

	useEffect(() => {
		const unsubMenu = subscribeDesktopMenu(setLocalDesktopMenuOpen);
		const unsubPlugin = subscribeActivePlugin(setLocalActivePlugin);
		return () => {
			unsubMenu();
			unsubPlugin();
		};
	}, []);

	return (
		<DesktopMenuContext.Provider
			value={{
				desktopMenuOpen,
				setDesktopMenuOpen,
				activePlugin,
				setActivePlugin,
			}}
		>
			{children}
		</DesktopMenuContext.Provider>
	);
};

export const useDesktopMenu = () => {
	const context = useContext(DesktopMenuContext);
	if (!context) {
		throw new Error('useDesktopMenu must be used within a DesktopMenuProvider');
	}
	return context;
};
