import { create } from 'zustand';

type QuickAccessState = {
	openQuickAccess: () => void;
	closeQuickAccess: () => void;
	subscribeToOpen: (callback: () => void) => () => void;
	subscribeToClose: (callback: () => void) => () => void;
};

const openListeners = new Set<() => void>();
const closeListeners = new Set<() => void>();

export const useQuickAccessStore = create<QuickAccessState>(() => ({
	openQuickAccess: () => {
		openListeners.forEach((cb) => cb());
	},
	closeQuickAccess: () => {
		closeListeners.forEach((cb) => cb());
	},
	subscribeToOpen: (cb) => {
		openListeners.add(cb);
		return () => openListeners.delete(cb);
	},
	subscribeToClose: (cb) => {
		closeListeners.add(cb);
		return () => closeListeners.delete(cb);
	},
}));
