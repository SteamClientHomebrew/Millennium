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

import React, { createContext, useContext } from 'react';
import { PyResyncUpdates } from '../../utils/ffi';
import { DialogButton, IconsModule, pluginSelf, SteamSpinner } from '@steambrew/client';
import { UpdateItemType } from './UpdateCard';
import { locale } from '../../../locales';
import { Placeholder } from '../../components/Placeholder';
import { settingsClasses } from '../../utils/classes';

type UpdateContextProviderProps = {
	children: React.ReactNode;
};

export type UpdateContextProviderState = {
	updatingThemes: boolean[];
	setUpdatingThemes: React.Dispatch<React.SetStateAction<boolean[]>>;
	updatingPlugins: boolean[];
	setUpdatingPlugins: React.Dispatch<React.SetStateAction<boolean[]>>;
	isAnyUpdating: () => boolean;
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

export class UpdateContextProvider extends React.Component<UpdateContextProviderProps, Partial<UpdateContextProviderState>> {
	constructor(props: UpdateContextProviderProps) {
		super(props);
		this.state = {
			updatingThemes: [],
			updatingPlugins: [],
			themeUpdates: null,
			pluginUpdates: null,
			hasReceivedUpdates: false,
			hasUpdateError: false,
		};
	}

	setUpdatingPlugins = (updater: (prev: boolean[]) => boolean[]) => {
		return this.setState((prevState) => {
			return { updatingPlugins: updater(prevState.updatingPlugins) };
		});
	};

	setUpdatingThemes = (updater: (prev: boolean[]) => boolean[]) => {
		return this.setState((prevState) => ({
			updatingThemes: updater(prevState.updatingThemes),
		}));
	};

	componentDidMount() {
		this.fetchAvailableUpdates();
	}

	isAnyUpdating = () => {
		const { updatingThemes, updatingPlugins } = this.state;
		return updatingThemes.some((u) => u) || updatingPlugins.some((u) => u);
	};

	hasAnyUpdates = () => {
		const { themeUpdates, pluginUpdates } = this.state;
		return (themeUpdates?.length ?? 0) > 0 || pluginUpdates?.some((update: any) => update?.hasUpdate);
	};

	forceFetchUpdates = async () => {
		const updates = JSON.parse(await PyResyncUpdates());
		pluginSelf.updates.themes = updates.themes;
		pluginSelf.updates.plugins = updates.plugins;
		NotifyUpdateListeners();
	};

	parseUpdateErrorMessage = () => {
		const themeError = pluginSelf?.updates?.themes?.error || '';
		const pluginError = pluginSelf?.updates?.plugins?.error || '';
		if (themeError && pluginError) return `${themeError}\n${pluginError}`;
		return themeError || pluginError;
	};

	fetchAvailableUpdates = async (force: boolean = false): Promise<boolean> => {
		try {
			if (force || !pluginSelf.hasCheckedForUpdates) {
				await this.forceFetchUpdates();
				pluginSelf.hasCheckedForUpdates = true;
			}
			pluginSelf.connectionFailed = false;

			this.setState({
				themeUpdates: pluginSelf.updates.themes,
				pluginUpdates: pluginSelf.updates.plugins,
				hasUpdateError: Boolean(pluginSelf?.updates?.themes?.error || pluginSelf?.updates?.plugins?.error),
				hasReceivedUpdates: true,
			});

			return true;
		} catch (exception) {
			pluginSelf.connectionFailed = true;
			return false;
		}
	};

	render() {
		const { updatingThemes, updatingPlugins, hasReceivedUpdates, hasUpdateError } = this.state;

		if (hasUpdateError) {
			return (
				<Placeholder
					icon={<IconsModule.ExclamationPoint />}
					header={locale.updatePanelErrorHeader}
					body={locale.updatePanelErrorBody + this.parseUpdateErrorMessage()}
				>
					<DialogButton className={settingsClasses.SettingsDialogButton} onClick={this.fetchAvailableUpdates.spread(true)}>
						{locale.updatePanelErrorButton}
					</DialogButton>
				</Placeholder>
			);
		}

		if (!hasReceivedUpdates) return <SteamSpinner background="transparent" />;
		if (!this.hasAnyUpdates()) {
			return <Placeholder icon={<IconsModule.Checkmark />} header={locale.updatePanelNoUpdatesFoundHeader} body={locale.updatePanelNoUpdatesFound} />;
		}

		return (
			<UpdateContext.Provider
				value={{
					updatingThemes,
					updatingPlugins,
					themeUpdates: this.state.themeUpdates,
					pluginUpdates: this.state.pluginUpdates,
					hasReceivedUpdates: this.state.hasReceivedUpdates,
					hasUpdateError: this.state.hasUpdateError,
					setUpdatingThemes: this.setUpdatingThemes,
					setUpdatingPlugins: this.setUpdatingPlugins,
					isAnyUpdating: this.isAnyUpdating,
					fetchAvailableUpdates: this.fetchAvailableUpdates,
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
