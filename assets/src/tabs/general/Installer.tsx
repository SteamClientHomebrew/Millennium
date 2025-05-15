import {
	callable,
	ConfirmModal,
	ConfirmModalProps,
	DialogBodyText,
	DialogButton,
	DialogLabel,
	Field,
	Millennium,
	pluginSelf,
	ProgressBar,
	showModal,
	ShowModalResult,
	TextField,
} from '@steambrew/client';
import React, { useEffect, useState } from 'react';
import { StartThemeInstaller } from './ThemeInstaller';
import { IProgressProps } from '../../types';
import { StartPluginInstaller } from './PluginInstaller';
import { API_URL } from '../../globals';

export enum InstallType {
	Plugin = 'plugin',
	Theme = 'theme',
}

const FetchPluginInfo = async (id: string) => {
	try {
		const response = await fetch(`${API_URL}/api/v1/plugin/${id}`);
		if (!response.ok) {
			const errorText = await response.text().catch(() => '');
			throw new Error(`Failed to fetch plugin info: ${response.status} ${response.statusText}${errorText ? ` - ${errorText}` : ''}`);
		}
		const pluginInfo = await response.json();
		return pluginInfo;
	} catch (error) {
		throw new Error('An error occurred while fetching plugin info: ' + error.message);
	}
};

const FetchThemeInfo = async (id: string) => {
	try {
		const response = await fetch(`${API_URL}/api/v2/details/${id}`);
		if (!response.ok) {
			const errorText = await response.text().catch(() => '');
			throw new Error(`Failed to fetch theme info: ${response.status} ${response.statusText}${errorText ? ` - ${errorText}` : ''}`);
		}
		const themeInfo = await response.json();
		return themeInfo;
	} catch (error) {
		throw new Error('An error occurred while fetching theme info: ' + error.message);
	}
};

const FetchInformationFromId = async (id: string) => {
	if (!id) {
		throw new Error('ID is empty');
	}

	switch (id.length) {
		case 20:
			return { type: InstallType.Theme, data: await FetchThemeInfo(id) };
		case 12:
			return { type: InstallType.Plugin, data: await FetchPluginInfo(id) };
		default:
			throw new Error('Invalid ID provided');
	}
};

const PromptInstallation = async (type: InstallType, data: any): Promise<ShowModalResult> => {
	const itemName = (type === InstallType.Plugin ? data?.pluginJson?.common_name : data?.name) ?? 'Unknown';

	return new Promise((resolve) => {
		let modal: ShowModalResult;

		const Renderer: React.FC = () => {
			const [element, setElement] = useState<React.ReactElement>(
				<ConfirmModal
					strTitle={itemName}
					strDescription={'Are you sure you want to proceed with the installation?'}
					onOK={resolve.bind(null, modal)}
					bHideCloseIcon={true}
					closeModal={() => {}}
					onCancel={modal?.Close}
				/>,
			);

			useEffect(() => {
				setElement && (pluginSelf.UpdateInstallerState = setElement);
			}, []);

			return element;
		};

		modal = showModal(<Renderer />, pluginSelf.MillenniumSettingsWindowRef, {
			bNeverPopOut: true,
			strTitle: 'Installer',
			popupWidth: 500,
			popupHeight: 275,
		});
	});
};

const InstallerMessageEmitter = (message: string) => {
	try {
		const { status, progress, isComplete }: IProgressProps = JSON.parse(message);

		if (!pluginSelf.InstallerEventEmitter) {
			return;
		}

		pluginSelf.InstallerEventEmitter({
			progress,
			status,
			isComplete,
		});
	} catch (error) {
		console.error('Failed to parse message:', error);
	}
};

Millennium.exposeObj({ InstallerMessageEmitter });

export interface RendererProps {
	onInstallComplete: () => React.ReactNode;
	onProgressUpdate: ({ progress, status }: { progress: number; status: string }) => React.ReactElement;
}

export const OpenInstallPrompt = async (id: string) => {
	let modal: ShowModalResult;
	let renderProps: RendererProps;

	const ShowMessageBox = (message: React.ReactNode, title: React.ReactNode, props?: ConfirmModalProps) => {
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
					// bAlertDialog={true}
				/>
			);

			if (pluginSelf?.UpdateInstallerState) {
				pluginSelf.UpdateInstallerState(element);
			} else {
				showModal(element, pluginSelf.MillenniumSettingsWindowRef, {
					bNeverPopOut: true,
					strTitle: 'Installer',
					popupWidth: 500,
					popupHeight: 275,
				});
			}
		});
	};

	const OnInstallComplete = () => {
		pluginSelf.UpdateInstallerState(renderProps.onInstallComplete());
	};

	const RenderInstallerProgress: React.FC = () => {
		const [event, setEvent] = useState<IProgressProps>(null);

		useEffect(() => {
			setEvent && (pluginSelf.InstallerEventEmitter = setEvent);
		}, []);

		useEffect(() => {
			event?.isComplete && setTimeout(() => OnInstallComplete(), 1000);
		}, [event]);

		return <renderProps.onProgressUpdate progress={event?.progress ?? 0} status={event?.status ?? 'Unknown'} />;
	};

	try {
		const { type, data } = await FetchInformationFromId(id);
		modal = await PromptInstallation(type, data);

		switch (type) {
			case InstallType.Plugin: {
				const result = await StartPluginInstaller(data, { updateInstallerState: pluginSelf.UpdateInstallerState, ShowMessageBox, modal });
				if (!result) return;

				renderProps = result as RendererProps;
				break;
			}
			case InstallType.Theme: {
				const result = await StartThemeInstaller(data, { updateInstallerState: pluginSelf.UpdateInstallerState, ShowMessageBox, modal });
				if (!result) return;

				renderProps = result as RendererProps;
				break;
			}
		}

		pluginSelf.UpdateInstallerState(<RenderInstallerProgress />);
	} catch (error) {
		ShowMessageBox(error.message, 'Whoops!');
	}
};
