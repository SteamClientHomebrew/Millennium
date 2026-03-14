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

import React, { createContext, useContext } from 'react';
import { PyResyncUpdates } from '../../utils/ffi';
import { pluginSelf } from '@steambrew/client';
import { UpdateItemType } from './UpdateCard';

type UpdateContextProviderProps = {
	children: React.ReactNode;
};

export type MillenniumUpdateProgress = {
	statusText: string;
	progress: number;
	isComplete: boolean;
};

export type InstallerProgress = {
	statusText: string;
	progress: number;
};

export type UpdateContextProviderState = {
	isUpdatingMillennium: boolean;
	setMillenniumUpdating: React.Dispatch<React.SetStateAction<boolean>>;
	millenniumUpdateProgress: MillenniumUpdateProgress;
	setMillenniumUpdateProgress: (progress: MillenniumUpdateProgress) => void;
	updatingThemes: Record<string, boolean>;
	setUpdatingTheme: (key: string, updating: boolean) => void;
	updatingPlugins: Record<string, boolean>;
	setUpdatingPlugin: (key: string, updating: boolean) => void;
	themeProgress: Record<string, InstallerProgress | null>;
	setThemeProgress: (key: string, progress: InstallerProgress | null) => void;
	pluginProgress: Record<string, InstallerProgress | null>;
	setPluginProgress: (key: string, progress: InstallerProgress | null) => void;
	isAnyUpdating: () => boolean;
	hasAnyUpdates: () => boolean;
	themeUpdates: Array<UpdateItemType> | null;
	pluginUpdates: any;
	hasReceivedUpdates: boolean;
	hasUpdateError: boolean;
	fetchAvailableUpdates: (force?: boolean) => Promise<boolean>;
};

const updateListeners = new Set<() => void>();

export const RegisterUpdateListener = (callback: () => void) => {
	updateListeners.add(callback);
	return () => updateListeners.delete(callback);
};

export const NotifyUpdateListeners = () => {
	updateListeners.forEach((callback) => callback());
};

const UpdateContext = createContext<UpdateContextProviderState | null>(null);

/**
 * Module-level operation state that survives component unmount/remount.
 * When the user navigates away from settings and comes back, ongoing update
 * state is preserved here and re-read by the newly mounted provider.
 */
const _opState = {
	updatingThemes: {} as Record<string, boolean>,
	updatingPlugins: {} as Record<string, boolean>,
	themeProgress: {} as Record<string, InstallerProgress | null>,
	pluginProgress: {} as Record<string, InstallerProgress | null>,
};

let _syncToComponent: (() => void) | null = null;

function _notifyOpChange() {
	_syncToComponent?.();
}

/** Stable setter functions provided via context — safe to capture in async closures. */
const opSetUpdatingTheme = (key: string, updating: boolean) => {
	_opState.updatingThemes = { ..._opState.updatingThemes, [key]: updating };
	_notifyOpChange();
};

const opSetUpdatingPlugin = (key: string, updating: boolean) => {
	_opState.updatingPlugins = { ..._opState.updatingPlugins, [key]: updating };
	_notifyOpChange();
};

const opSetThemeProgress = (key: string, progress: InstallerProgress | null) => {
	_opState.themeProgress = { ..._opState.themeProgress, [key]: progress };
	_notifyOpChange();
};

const opSetPluginProgress = (key: string, progress: InstallerProgress | null) => {
	_opState.pluginProgress = { ..._opState.pluginProgress, [key]: progress };
	_notifyOpChange();
};

/**
 * Module-level fetchAvailableUpdates — survives unmount so background update
 * completions can refresh the list without calling setState on a dead component.
 */
const _fetchAvailableUpdates = async (force: boolean = false): Promise<boolean> => {
	try {
		if (force || !pluginSelf.hasCheckedForUpdates) {
			const updates = JSON.parse(await PyResyncUpdates({ force: true }));
			pluginSelf.updates.themes = updates.themes;
			pluginSelf.updates.plugins = updates.plugins;
			pluginSelf.hasCheckedForUpdates = true;
			NotifyUpdateListeners();
		}
		pluginSelf.connectionFailed = false;
		_syncToComponent?.();
		return true;
	} catch {
		pluginSelf.connectionFailed = true;
		return false;
	}
};

export class UpdateContextProvider extends React.Component<UpdateContextProviderProps, Partial<UpdateContextProviderState>> {
	constructor(props: UpdateContextProviderProps) {
		super(props);
		this.state = {
			updatingThemes: _opState.updatingThemes,
			updatingPlugins: _opState.updatingPlugins,
			themeProgress: _opState.themeProgress,
			pluginProgress: _opState.pluginProgress,
			isUpdatingMillennium: false,
			millenniumUpdateProgress: { statusText: 'Starting updater...', progress: 0, isComplete: false },
			themeUpdates: null,
			pluginUpdates: null,
			hasReceivedUpdates: false,
			hasUpdateError: false,
		};
	}

	private syncFromOpState = () => {
		this.setState({
			updatingThemes: _opState.updatingThemes,
			updatingPlugins: _opState.updatingPlugins,
			themeProgress: _opState.themeProgress,
			pluginProgress: _opState.pluginProgress,
			themeUpdates: pluginSelf.updates?.themes ?? null,
			pluginUpdates: pluginSelf.updates?.plugins ?? null,
			hasUpdateError: Boolean(pluginSelf?.updates?.themes?.error || pluginSelf?.updates?.plugins?.error),
			hasReceivedUpdates: Boolean(pluginSelf.hasCheckedForUpdates),
		});
	};

	setMillenniumUpdating = (newState: boolean) => {
		return this.setState({ isUpdatingMillennium: newState });
	};

	setMillenniumUpdateProgress = (progress: MillenniumUpdateProgress) => {
		return this.setState({ millenniumUpdateProgress: progress });
	};

	componentDidMount() {
		_syncToComponent = this.syncFromOpState;
		// Re-sync in case operations completed while unmounted.
		this.syncFromOpState();
		_fetchAvailableUpdates();
	}

	componentWillUnmount() {
		_syncToComponent = null;
	}

	isAnyUpdating = () => {
		return Object.values(_opState.updatingThemes).some((u) => u) || Object.values(_opState.updatingPlugins).some((u) => u);
	};

	hasAnyUpdates = () => {
		const { themeUpdates, pluginUpdates } = this.state;

		const hasThemeUpdates = (themeUpdates?.length ?? 0) > 0;
		const hasPluginUpdates = pluginUpdates?.some((update: any) => update?.hasUpdate) ?? false;
		const hasMillenniumUpdate = pluginSelf?.millenniumUpdates?.hasUpdate;

		const hasAnyUpdate = hasThemeUpdates || hasPluginUpdates || hasMillenniumUpdate;
		return hasAnyUpdate;
	};

	render() {
		const { updatingThemes, updatingPlugins } = this.state;

		return (
			<UpdateContext.Provider
				value={{
					updatingThemes,
					updatingPlugins,
					themeUpdates: this.state.themeUpdates,
					pluginUpdates: this.state.pluginUpdates,
					isUpdatingMillennium: this.state.isUpdatingMillennium,
					millenniumUpdateProgress: this.state.millenniumUpdateProgress,
					themeProgress: this.state.themeProgress,
					pluginProgress: this.state.pluginProgress,
					hasReceivedUpdates: this.state.hasReceivedUpdates,
					hasUpdateError: this.state.hasUpdateError,
					setUpdatingTheme: opSetUpdatingTheme,
					setUpdatingPlugin: opSetUpdatingPlugin,
					setMillenniumUpdating: this.setMillenniumUpdating,
					setMillenniumUpdateProgress: this.setMillenniumUpdateProgress,
					setThemeProgress: opSetThemeProgress,
					setPluginProgress: opSetPluginProgress,
					isAnyUpdating: this.isAnyUpdating,
					hasAnyUpdates: this.hasAnyUpdates,
					fetchAvailableUpdates: _fetchAvailableUpdates,
				}}
			>
				{this.props.children}
			</UpdateContext.Provider>
		);
	}
}
export const useUpdateContext = () => {
	const context = useContext(UpdateContext);
	if (!context) throw new Error('Millennium: useUpdateContext must be used inside UpdateContextProvider');
	return context;
};
