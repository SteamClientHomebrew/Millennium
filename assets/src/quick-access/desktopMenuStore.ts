// desktopMenuStore.ts

import { PluginComponent } from '../types';

type Listener<T> = (value: T) => void;

// --- desktopMenuOpen ---
let desktopMenuOpen = false;
const desktopMenuListeners = new Set<Listener<boolean>>();

export const getDesktopMenuOpen = () => desktopMenuOpen;
export const setDesktopMenuOpen = (value: boolean) => {
	if (desktopMenuOpen !== value) {
		if (value) activePlugin = null; // Reset active plugin when menu opens

		desktopMenuOpen = value;
		desktopMenuListeners.forEach((fn) => fn(value));
	}
};
export const subscribeDesktopMenu = (fn: Listener<boolean>) => {
	desktopMenuListeners.add(fn);
	return () => desktopMenuListeners.delete(fn);
};

// --- activePlugin ---
let activePlugin: PluginComponent = null;
const activePluginListeners = new Set<Listener<PluginComponent>>();

export const getActivePlugin = () => activePlugin;
export const setActivePlugin = (plugin: PluginComponent) => {
	if (activePlugin !== plugin) {
		activePlugin = plugin;
		activePluginListeners.forEach((fn) => fn(plugin));
	}
};
export const subscribeActivePlugin = (fn: Listener<PluginComponent>) => {
	activePluginListeners.add(fn);
	return () => activePluginListeners.delete(fn);
};
