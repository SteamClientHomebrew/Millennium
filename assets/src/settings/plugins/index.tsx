import React, { Component, useEffect, useState } from 'react';
import {
	ClassModule,
	ConfirmModal,
	DialogButton,
	Field,
	IconsModule,
	Menu,
	MenuItem,
	Toggle,
	findClassModule,
	pluginSelf,
	showContextMenu,
	showModal,
} from '@steambrew/client';
import { PluginComponent } from '../../types';
import { locale } from '../../../locales';
import { DialogControlSectionClass, settingsClasses } from '../../utils/classes';
import { ErrorModal } from '../../components/ErrorModal';
import { RenderComponents } from '../../components/PluginEditor';
import { FaEllipsisH, FaFolderOpen, FaSave, FaStore } from 'react-icons/fa';
import { DesktopTooltip, Separator } from '../../components/SteamComponents';
import { Utils } from '../../utils';
import { PyFindAllPlugins, PyGetEnvironmentVar, PyGetLogData, PyUpdatePluginStatus } from '../../utils/ffi';
import { showInstallPluginModal } from './PluginInstallerModal';
import { LogData, LogLevel } from '../logs';
import { RenderPluginComponent } from './PluginComponent';
import Styles from '../../utils/styles';

declare global {
	interface Window {
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
	}
}

interface PluginStatusProps {
	errors: number;
	warnings: number;
}

interface UpdatedPluginProps {
	plugin_name: string;
	enabled: boolean;
}

interface PluginViewModalState {
	plugins: PluginComponent[];
	checkedItems: { [key: number]: boolean };
	pluginsWithLogs?: Map<string, PluginStatusProps>;
	updatedPlugins: UpdatedPluginProps[];
}

class PluginViewModal extends Component<{}, PluginViewModalState> {
	state: PluginViewModalState = {
		plugins: undefined,
		checkedItems: {},
		pluginsWithLogs: undefined,
		updatedPlugins: [],
	};

	componentDidMount() {
		this.FetchAllPlugins();
	}

	getEnabledPlugins(plugins: PluginComponent[]) {
		return plugins
			.map((plugin: PluginComponent, index: number) => ({ plugin, index }))
			.filter(({ plugin }) => plugin.enabled)
			.reduce((acc, { index }) => ({ ...acc, [index]: true }), {});
	}

	async FetchAllPlugins() {
		const plugins: PluginComponent[] = JSON.parse(await PyFindAllPlugins());
		const checkedItems = this.getEnabledPlugins(plugins);
		const pluginNames = plugins.map((p) => p.data.common_name);
		const pluginsWithLogs = new Map<string, PluginStatusProps>();

		const result = await PyGetLogData();
		const logData: LogData[] = JSON.parse(result as any);

		for (let plugin of logData) {
			if (pluginNames.includes(plugin.name)) {
				const errors = plugin.logs.filter((l) => l.level === LogLevel.ERROR).length;
				const warnings = plugin.logs.filter((l) => l.level === LogLevel.WARNING).length;
				pluginsWithLogs.set(plugin.name, { errors, warnings });
			}
		}

		this.setState({ plugins, checkedItems, pluginsWithLogs });
	}

	handleCheckboxChange(index: number) {
		const plugin = this.state.plugins[index];
		const originalChecked = plugin.enabled;
		const updated = !this.state.checkedItems[index] || plugin.data.name === 'core';

		const checkedItems = { ...this.state.checkedItems, [index]: updated };
		const filtered = this.state.updatedPlugins.filter((p) => p.plugin_name !== plugin.data.name);
		const updatedPlugins = updated !== originalChecked ? [...filtered, { plugin_name: plugin.data.name, enabled: updated }] : filtered;

		this.setState({ checkedItems, updatedPlugins });
	}

	SavePluginChanges() {
		const onOK = () => {
			PyUpdatePluginStatus({ pluginJson: JSON.stringify(this.state.updatedPlugins) });
		};

		showModal(
			<ConfirmModal strTitle={locale.optionReloadRequired} strDescription={locale.optionPluginNeedsReload} strOKButtonText={locale.optionReloadNow} onOK={onOK} />,
			pluginSelf.mainWindow,
			{ bNeverPopOut: false },
		);
	}

	async OpenPluginsFolder() {
		const path = await PyGetEnvironmentVar({ variable: 'MILLENNIUM__PLUGINS_PATH' });
		Utils.BrowseLocalFolder(path);
	}

	async InstallPluginMenu() {
		await showInstallPluginModal();
	}

	renderPluginComponent({ plugin, index }: { plugin: PluginComponent; index: number }) {
		const logState = this.state.pluginsWithLogs?.get(plugin.data.common_name);

		return (
			<RenderPluginComponent
				plugin={plugin}
				index={index}
				isLastPlugin={index === this.state.plugins.length - 1}
				isEnabled={this.state.checkedItems[index]}
				hasErrors={logState?.errors > 0}
				hasWarnings={logState?.warnings > 0}
				onSelectionChange={(index: number) => this.handleCheckboxChange(index)}
				refetchPlugins={this.FetchAllPlugins.bind(this)}
			/>
		);
	}

	render() {
		if (pluginSelf.connectionFailed) {
			return (
				<ErrorModal
					header={locale.errorFailedConnection}
					body={locale.errorFailedConnectionBody}
					options={{
						buttonText: locale.errorFailedConnectionButton,
						onClick: () => {
							Utils.BrowseLocalFolder([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/'));
						},
					}}
				/>
			);
		}

		/** Haven't received the plugins yet from the backend */
		if (this.state.plugins === undefined) {
			return null;
		}

		if (!this.state.plugins || !this.state.plugins.length) {
			return (
				<ErrorModal
					header={'No Plugins Found.'}
					body={"It appears you don't have any plugin yet!"}
					showIcon={false}
					options={{
						buttonText: 'Install a Plugin',
						onClick: this.InstallPluginMenu.bind(this),
					}}
				/>
			);
		}

		return (
			<>
				<Styles />
				<div
					className={`DialogControlsSection ${DialogControlSectionClass} MillenniumPluginsDialogControlsSection`}
					style={{ marginBottom: '10px', marginTop: '10px' }}
				>
					<DialogButton
						className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.SavePluginChanges.bind(this)}
						disabled={!this.state.updatedPlugins.length}
						data-button-type={'save'}
					>
						<FaSave />
						{locale.optionSaveChanges}
					</DialogButton>
					<DialogButton
						className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.InstallPluginMenu.bind(this)}
						data-button-type={'install-plugin'}
					>
						<FaStore />
						{locale.optionInstallPlugin}
					</DialogButton>
					<DialogButton
						className={`MillenniumSpanningIconButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.OpenPluginsFolder.bind(this)}
						data-button-type={'browse-plugin-local-files'}
					>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</div>
				{this.state.plugins.map((plugin, index) => this.renderPluginComponent({ plugin, index }))}
			</>
		);
	}
}

export { PluginViewModal };
