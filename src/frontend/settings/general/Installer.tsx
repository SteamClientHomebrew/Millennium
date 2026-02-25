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

import { ConfirmModal, ConfirmModalProps, Millennium, pluginSelf, showModal, ShowModalResult } from '@steambrew/client';
import React, { useEffect, useState } from 'react';
import { StartThemeInstaller } from '../../components/ThemeInstaller';
import { IProgressProps } from '../../types';
import { StartPluginInstaller } from '../../components/PluginInstaller';
import { API_URL } from '../../utils/globals';
import { formatString, locale } from '../../utils/localization-manager';
import { Logger } from '../../utils/Logger';
import { IncrementPluginDownloadFromId, IncrementThemeDownloadFromId } from '../../utils/update-bump';

export enum InstallType {
	Plugin,
	Theme,
}

export interface RendererProps {
	onInstallComplete: () => React.ReactNode;
	onProgressUpdate: ({ progress, status }: { progress: number; status: string }) => React.ReactElement;
}

/**
 * Module-level progress listener for installer events dispatched from C++.
 * Use registerInstallerProgressListener / unregisterInstallerProgressListener
 * rather than reaching into pluginSelf directly.
 */
let _progressListener: ((event: IProgressProps) => void) | null = null;
let _pendingEvent: IProgressProps | null = null;

export function registerInstallerProgressListener(listener: (event: IProgressProps) => void) {
	_progressListener = listener;
	// Drain any event that arrived before the listener was registered.
	if (_pendingEvent) {
		listener(_pendingEvent);
		_pendingEvent = null;
	}
}

export function unregisterInstallerProgressListener() {
	_progressListener = null;
}

function InstallerMessageEmitter(status: string, progress: number, isComplete: boolean) {
	try {
		const event: IProgressProps = { progress, status, isComplete };
		if (_progressListener) {
			_progressListener(event);
		} else {
			// Buffer the latest event so it can be replayed once a listener registers.
			_pendingEvent = event;
		}
	} catch (error) {
		console.error('Failed to emit installer progress:', error);
	}
}

Millennium.exposeObj({ InstallerMessageEmitter });

export class Installer {
	async FetchPluginInfo(id: string) {
		try {
			const response = await fetch(`${API_URL}/api/v1/plugin/${id}`);
			if (!response.ok) {
				const errorText = await response.text().catch(() => '');
				throw new Error(`${locale.errorFailedToFetchPlugin}${response.status} ${response.statusText}${errorText ? ` - ${errorText}` : ''}`);
			}
			const pluginInfo = await response.json();
			return pluginInfo;
		} catch (error) {
			throw new Error(locale.errorFailedToFetchPlugin + error.message);
		}
	}

	async FetchThemeInfo(id: string) {
		try {
			const response = await fetch(`${API_URL}/api/v2/details/${id}`);
			if (!response.ok) {
				const errorText = await response.text().catch(() => '');
				throw new Error(`${locale.errorFailedToFetchTheme}${response.status} ${response.statusText}${errorText ? ` - ${errorText}` : ''}`);
			}
			const themeInfo = await response.json();
			return themeInfo;
		} catch (error) {
			throw new Error(locale.errorFailedToFetchTheme + error.message);
		}
	}

	async FetchInformationFromId(id: string) {
		if (!id) {
			throw new Error(locale.errorInvalidID);
		}

		switch (id.length) {
			case 20:
				return { type: InstallType.Theme, data: await this.FetchThemeInfo(id) };
			case 12:
				return { type: InstallType.Plugin, data: await this.FetchPluginInfo(id) };
			default:
				throw new Error(locale.errorInvalidID);
		}
	}

	/**
	 * Shows the initial "proceed with installation?" modal.
	 * @param onStateSetter Called with the modal's state setter once the component
	 *   mounts — lets the caller swap content without touching pluginSelf.
	 */
	async PromptInstallation(type: InstallType, data: any, onStateSetter: (setter: (el: React.ReactElement) => void) => void): Promise<ShowModalResult> {
		const itemName = (type === InstallType.Plugin ? data?.pluginJson?.common_name : data?.name) ?? locale.strUnknown;

		return new Promise((resolve) => {
			let modal: ShowModalResult;

			const Renderer: React.FC = () => {
				const [element, setElement] = useState<React.ReactElement>(
					<ConfirmModal
						strTitle={itemName}
						strDescription={locale.warnProceedInstallation}
						onOK={() => resolve(modal)}
						bHideCloseIcon={true}
						closeModal={() => {}}
						onCancel={() => modal?.Close()}
					/>,
				);

				useEffect(() => {
					onStateSetter(setElement);
				}, []);

				return element;
			};

			modal = showModal(<Renderer />, pluginSelf.mainWindow, {
				bNeverPopOut: true,
				popupWidth: 500,
				popupHeight: 275,
			});
		});
	}

	async OpenInstallPrompt(id: string, refetchDataCb: () => void) {
		let modal: ShowModalResult;
		let renderProps: RendererProps;
		// Local state setter for the modal — avoids mutable pluginSelf properties.
		let updateInstallerState: ((element: React.ReactElement) => void) | null = null;

		function ShowMessageBox(message: React.ReactNode, title: React.ReactNode, props?: ConfirmModalProps) {
			return new Promise((resolve) => {
				const handle = (result: boolean, cb?: () => void) => {
					cb?.();
					resolve(result);
				};

				const element = (
					<ConfirmModal
						{...props}
						strTitle={title}
						strDescription={message}
						bHideCloseIcon={true}
						onOK={handle.bind(null, true, props?.onOK)}
						onCancel={handle.bind(null, false, props?.onCancel)}
					/>
				);

				if (updateInstallerState) {
					updateInstallerState(element);
				} else {
					showModal(element, pluginSelf.mainWindow, {
						bNeverPopOut: true,
						popupWidth: 500,
						popupHeight: 275,
					});
				}
			});
		}

		function OnInstallComplete() {
			const element = renderProps.onInstallComplete();
			if (React.isValidElement(element)) {
				updateInstallerState?.(element);
			}
		}

		function RenderInstallerProgress() {
			const [event, setEvent] = useState<IProgressProps>(null);

			useEffect(() => {
				registerInstallerProgressListener(setEvent);
				return () => unregisterInstallerProgressListener();
			}, []);

			useEffect(() => {
				event?.isComplete && setTimeout(() => OnInstallComplete(), 1000);
			}, [event]);

			return <renderProps.onProgressUpdate progress={event?.progress ?? 0} status={event?.status ?? locale.strUnknown} />;
		}

		try {
			const { type, data } = await this.FetchInformationFromId(id);
			modal = await this.PromptInstallation(type, data, (setter) => {
				updateInstallerState = setter;
			});

			// updateInstallerState is guaranteed set here — the Renderer useEffect
			// fires before the user can click OK.
			const installerProps = { updateInstallerState: updateInstallerState!, ShowMessageBox, modal, refetchDataCb };

			switch (type) {
				case InstallType.Plugin: {
					Logger.Log(`Installing plugin with ID: ${id}`);
					IncrementPluginDownloadFromId(id);

					const result = await StartPluginInstaller(data, installerProps);
					if (!result) return;

					renderProps = result as RendererProps;
					break;
				}
				case InstallType.Theme: {
					Logger.Log(`Installing theme with ID: ${id}`);
					IncrementThemeDownloadFromId(id);

					const result = await StartThemeInstaller(data, installerProps);
					if (!result) return;

					renderProps = result as RendererProps;
					break;
				}
			}

			updateInstallerState!(<RenderInstallerProgress />);
		} catch (error) {
			ShowMessageBox(error.message, locale.errorMessageTitle);
		}
	}
}
