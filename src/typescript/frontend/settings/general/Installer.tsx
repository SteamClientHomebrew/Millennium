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

import { ConfirmModal, ConfirmModalProps, Millennium, pluginSelf, ProgressBar, showModal, ShowModalResult } from '@steambrew/client';
import React, { useEffect, useState } from 'react';
import { StartThemeInstaller } from '../../components/ThemeInstaller';
import { IProgressProps } from '../../types';
import { StartPluginInstaller } from '../../components/PluginInstaller';
import { API_URL } from '../../utils/globals';
import { locale } from '../../utils/localization-manager';
import { Logger } from '../../utils/Logger';
import { IncrementPluginDownloadFromId, IncrementThemeDownloadFromId } from '../../utils/update-bump';
import Styles from 'utils/styles';

export enum InstallType {
	Plugin,
	Theme,
}

export interface RendererProps {
	onInstallComplete: () => React.ReactNode;
	onProgressUpdate: ({ status }: { status: string }) => React.ReactElement;
	opId: number;
}

/**
 * Per-operation progress listeners for installer events dispatched from C++.
 * Each concurrent install/update operation is identified by a unique opId.
 * opId=0 is reserved for Millennium's own self-updater.
 */
const _progressListeners = new Map<number, (event: IProgressProps) => void>();
const _pendingEvents = new Map<number, IProgressProps>();

export function registerInstallerProgressListener(opId: number, listener: (event: IProgressProps) => void) {
	_progressListeners.set(opId, listener);
	// Drain any event that arrived before the listener was registered.
	const pending = _pendingEvents.get(opId);
	if (pending) {
		listener(pending);
		_pendingEvents.delete(opId);
	}
}

export function unregisterInstallerProgressListener(opId: number) {
	_progressListeners.delete(opId);
	_pendingEvents.delete(opId);
}

export function waitForInstallerComplete(opId: number, onProgress: (event: IProgressProps) => void): Promise<boolean> {
	return new Promise((resolve) => {
		registerInstallerProgressListener(opId, (event) => {
			onProgress(event);
			if (event.isComplete) {
				unregisterInstallerProgressListener(opId);
				resolve(event.success ?? true);
			}
		});
	});
}

function InstallerMessageEmitter(status: string, progress: number, isComplete: boolean, success: boolean = true, opId: number = 0) {
	try {
		const event: IProgressProps = { progress, status, isComplete, success, opId };
		const listener = _progressListeners.get(opId);
		if (listener) {
			listener(event);
		} else {
			// Buffer the latest event so it can be replayed once a listener registers.
			_pendingEvents.set(opId, event);
		}
	} catch (error) {
		console.error('Failed to emit installer progress:', error);
	}
}

Millennium.exposeObj({ InstallerMessageEmitter });

export const OnProgressUpdate = ({ status }: { status: string }) => {
	const RenderBody = () => {
		return (
			<>
                <Styles />
                <style>
                    {`.MillenniumInstallerDialog .DialogButton { display: none; }
                    .MillenniumInstallerDialog_ProgressStatus { font-weight: 500; float: right; margin-bottom: 10px; }
                    .DialogBodyText { margin-bottom: unset; }`}
                </style>
				<div className='MillenniumInstallerDialog_ProgressStatus'>{status}</div>
				<ProgressBar
                    /* @ts-ignore */
					className="MillenniumInstallerDialog_ProgressBar"
                    indeterminate
                    nProgress={0}
				/>
			</>
		);
	};

	return (
		<ConfirmModal
			className="MillenniumInstallerDialog"
			strTitle={locale.strInstallProgress}
			strDescription={<RenderBody />}
			onCancel={() => {}}
			bHideCloseIcon
            bAlertDialog
			bOKDisabled
			bCancelDisabled
			bDisableBackgroundDismiss
		/>
	);
};

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
			throw new Error(locale.errorFailedToFetchPlugin + (error as Error).message);
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
			throw new Error(locale.errorFailedToFetchTheme + (error as Error).message);
		}
	}

	async FetchInformationFromId(id: string, type: InstallType) {
        if (!id) {
            throw new Error(locale.errorInvalidID);
        }
        const map = {
            [InstallType.Theme]: {
                length: 20,
                fetch: this.FetchThemeInfo,
            },
            [InstallType.Plugin]: {
                length: 12,
                fetch: this.FetchPluginInfo,
            },
        };
        if (id.length !== map[type].length) {
            throw new Error(locale.errorInvalidID);
        }
        return await map[type].fetch.call(this, id);
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

	async OpenInstallPrompt(id: string, type: InstallType, refetchDataCb: () => void) {
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
			const [event, setEvent] = useState<IProgressProps | null>(null);
			const opId = renderProps.opId;

			useEffect(() => {
				registerInstallerProgressListener(opId, setEvent);
				return () => unregisterInstallerProgressListener(opId);
			}, []);

			useEffect(() => {
				event?.isComplete && setTimeout(() => OnInstallComplete(), 1000);
			}, [event]);

			return <renderProps.onProgressUpdate status={event?.status ?? locale.strUnknown} />;
		}

		try {
			const data = await this.FetchInformationFromId(id, type);
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
			ShowMessageBox((error as Error).message, locale.errorMessageTitle);
		}
	}
}
