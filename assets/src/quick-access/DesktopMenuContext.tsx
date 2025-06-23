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
