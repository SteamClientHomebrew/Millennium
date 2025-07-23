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

import { Component } from 'react';
import { ConfirmModal, DialogButton, DialogControlsSection, IconsModule, joinClassNames, pluginSelf, showModal } from '@steambrew/client';
import { PluginComponent } from '../../types';
import { locale } from '../../../locales';
import { settingsClasses } from '../../utils/classes';
import { FaFolderOpen, FaSave, FaStore } from 'react-icons/fa';
import { PiPlugsFill } from 'react-icons/pi';
import { Utils } from '../../utils';
import { PyFindAllPlugins, PyGetEnvironmentVar, PyGetLogData, PyUpdatePluginStatus } from '../../utils/ffi';
import { showInstallPluginModal } from './PluginInstallerModal';
import { LogData, LogLevel } from '../logs';
import { RenderPluginComponent } from './PluginComponent';
import { Placeholder } from '../../components/Placeholder';

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
		await showInstallPluginModal(this.FetchAllPlugins.bind(this));
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
				<Placeholder icon={<IconsModule.ExclamationPoint />} header={locale.errorFailedConnection} body={locale.errorFailedConnectionBody}>
					<DialogButton
						className={settingsClasses.SettingsDialogButton}
						onClick={() => {
							Utils.BrowseLocalFolder([pluginSelf.steamPath, 'ext', 'data', 'logs'].join('/'));
						}}
					>
						{locale.errorFailedConnectionButton}
					</DialogButton>
				</Placeholder>
			);
		}

		/** Haven't received the plugins yet from the backend */
		if (this.state.plugins === undefined) {
			return null;
		}

		if (!this.state.plugins || !this.state.plugins.length || (this.state.plugins.length === 1 && this.state.plugins[0].data.name === 'core')) {
			return (
				<Placeholder icon={<PiPlugsFill className="SVGIcon_Button" />} header={'No Plugins Found.'} body={"It appears you don't have any plugin yet!"}>
					<DialogButton className={joinClassNames(settingsClasses.SettingsDialogButton, 'MillenniumPlaceholder_Button')} onClick={this.InstallPluginMenu.bind(this)}>
						<FaStore />
						{locale.optionInstallPlugin}
					</DialogButton>
					<DialogButton className={joinClassNames(settingsClasses.SettingsDialogButton, 'MillenniumPlaceholder_Button')} onClick={this.OpenPluginsFolder.bind(this)}>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</Placeholder>
			);
		}

		return (
			<>
				<DialogControlsSection className="MillenniumButtonsSection">
					<DialogButton
						className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.SavePluginChanges.bind(this)}
						disabled={!this.state.updatedPlugins.length}
						data-button-type={'save'}
					>
						<FaSave />
						{locale.optionSaveChanges}
					</DialogButton>
					<DialogButton
						className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.InstallPluginMenu.bind(this)}
						data-button-type={'install-plugin'}
					>
						<FaStore />
						{locale.optionInstallPlugin}
					</DialogButton>
					<DialogButton
						className={`MillenniumButton ${settingsClasses.SettingsDialogButton}`}
						onClick={this.OpenPluginsFolder.bind(this)}
						data-button-type={'browse-plugin-local-files'}
					>
						<FaFolderOpen />
						{locale.optionBrowseLocalFiles}
					</DialogButton>
				</DialogControlsSection>
				{this.state.plugins.map((plugin, index) => this.renderPluginComponent({ plugin, index }))}
			</>
		);
	}
}

export { PluginViewModal };
