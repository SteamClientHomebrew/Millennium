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

import { Component } from 'react';
import { ConfirmModal, DialogButton, DialogControlsSection, joinClassNames, pluginSelf, showModal } from '@steambrew/sdk';
import { PluginComponent, PluginMetrics, ThemeItem } from '../../types';
import { formatString, locale } from '../../utils/localization-manager';
import { settingsClasses } from '../../utils/classes';
import { FaFolderOpen, FaSave, FaStore } from 'react-icons/fa';
import { PiPlugsFill } from 'react-icons/pi';
import { Utils } from '../../utils';
import { backend } from '../../utils/ffi';
import { showInstallPluginModal } from './PluginInstallerModal';
import { LogData, LogLevel } from '../logs';
import { RenderPluginComponent } from './PluginComponent';
import { Placeholder } from '../../components/Placeholder';
import { getPluginConfigurableStatus } from '../../utils/globals';

declare global {
	interface Window {
		MILLENNIUM_PLUGIN_SETTINGS_STORE: any;
	}
}

function isLegacyPlugin(plugin: PluginComponent): boolean {
	return plugin.data.name !== 'core' && plugin.data.useBackend !== false && plugin.data.backendType !== 'lua';
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
	plugins: PluginComponent[] | undefined;
	checkedItems: { [key: number]: boolean };
	pluginsWithLogs?: Map<string, PluginStatusProps>;
	updatedPlugins: UpdatedPluginProps[];
	configurablePluginStore: Array<{ name: string; isEditable: boolean }>;
	metrics: Map<string, PluginMetrics>;
	activeTheme?: ThemeItem;
}

class PluginViewModal extends Component<{}, PluginViewModalState> {
	state: PluginViewModalState = {
		plugins: undefined,
		checkedItems: {},
		pluginsWithLogs: undefined,
		updatedPlugins: [],
		configurablePluginStore: [],
		metrics: new Map(),
	};

	private crashEventHandler = () => this.FetchAllPlugins();
	private metricsInterval: ReturnType<typeof setInterval> | null = null;

	componentDidMount() {
		this.FetchAllPlugins();
		backend.themes.activeTheme().then((activeTheme) => this.setState({ activeTheme }));
		this.fetchMetrics();
		this.metricsInterval = setInterval(this.fetchMetrics.bind(this), 2000);
		window.addEventListener('millennium-plugin-crash', this.crashEventHandler);
	}

	componentWillUnmount() {
		if (this.metricsInterval) clearInterval(this.metricsInterval);
		window.removeEventListener('millennium-plugin-crash', this.crashEventHandler);
	}

	async fetchMetrics() {
		try {
			const list = await backend.plugins.getMetrics();
			const map = new Map<string, PluginMetrics>();
			for (const m of list) map.set(m.name, m);
			this.setState({ metrics: map });
		} catch {
			/* metrics are best-effort */
		}
	}

	getEnabledPlugins(plugins: PluginComponent[]) {
		return plugins
			.map((plugin: PluginComponent, index: number) => ({ plugin, index }))
			.filter(({ plugin }) => plugin.enabled)
			.reduce((acc, { index }) => ({ ...acc, [index]: true }), {});
	}

	async FetchAllPlugins() {
		const plugins: PluginComponent[] = await backend.plugins.getPlugins();
		const checkedItems = this.getEnabledPlugins(plugins);
		const pluginNames = plugins.map((p) => p.data.common_name);
		const pluginsWithLogs = new Map<string, PluginStatusProps>();
		const logData: LogData[] = await backend.plugins.getBackendLogs();

		for (let plugin of logData) {
			if (pluginNames.includes(plugin.name)) {
				const errors = plugin?.logs?.filter((l) => l.level === LogLevel.ERROR).length;
				const warnings = plugin?.logs?.filter((l) => l.level === LogLevel.WARNING).length;
				pluginsWithLogs.set(plugin.name, { errors, warnings });
			}
		}

		const configurablePluginStore = await getPluginConfigurableStatus(plugins);
		this.setState({ plugins, checkedItems, pluginsWithLogs, configurablePluginStore });
	}

	/** Plugins that declare the given plugin in "requires" and are currently checked. */
	getDependents(pluginName: string) {
		return (this.state.plugins ?? []).filter((candidate, candidateIndex) => {
			const requirements = Array.isArray(candidate.data.requires) ? candidate.data.requires : [];
			return this.state.checkedItems[candidateIndex] && requirements.some((spec) => spec.split('@')[0] === pluginName);
		});
	}

	/** True when the active theme lists the given plugin in its "requires". */
	isRequiredByActiveTheme(pluginName: string) {
		const requirements = Array.isArray(this.state.activeTheme?.data?.requires) ? this.state.activeTheme.data.requires : [];
		return requirements.some((spec) => spec.split('@')[0] === pluginName);
	}

	handleCheckboxChange(index: number) {
		const plugin = this.state.plugins?.[index];
		if (!plugin) return;

		if (isLegacyPlugin(plugin)) return;

		const updated = !this.state.checkedItems[index] || plugin.data.name === 'core';

		/** Anything that requires this plugin cannot run without it, so turning it off turns
		    those off too - dependent plugins, and the active theme if it requires it. */
		const dependents = updated ? [] : this.getDependents(plugin.data.name);
		const revertsTheme = !updated && this.isRequiredByActiveTheme(plugin.data.name);

		if (dependents.length || revertsTheme) {
			const affected = dependents.map((dependent) => dependent.data.common_name ?? dependent.data.name);

			if (revertsTheme) {
				affected.push(this.state.activeTheme?.data?.name ?? this.state.activeTheme?.native ?? '');
			}

			showModal(
				<ConfirmModal
					strTitle={locale.pluginDisableDependentsTitle}
					strDescription={formatString(locale.pluginDisableDependentsBody, plugin.data.common_name ?? plugin.data.name, affected.join(', '))}
					strOKButtonText={locale.pluginDisableDependentsConfirm}
					strCancelButtonText={locale.strNeverMind}
					onOK={() => this.applyCheckboxChange(index, updated, dependents, revertsTheme)}
				/>,
				pluginSelf.mainWindow,
				{ bNeverPopOut: false },
			);
			return;
		}

		this.applyCheckboxChange(index, updated, []);
	}

	applyCheckboxChange(index: number, updated: boolean, dependents: PluginComponent[], revertsTheme = false) {
		const plugin = this.state.plugins?.[index];
		if (!plugin) return;

		if (revertsTheme) {
			backend.themes.setActiveTheme('default');
			this.setState({ activeTheme: undefined });
		}

		const changes = [{ plugin, index, enabled: updated }];

		for (const dependent of dependents) {
			const dependentIndex = (this.state.plugins ?? []).indexOf(dependent);
			if (dependentIndex !== -1) {
				changes.push({ plugin: dependent, index: dependentIndex, enabled: false });
			}
		}

		const checkedItems = { ...this.state.checkedItems };
		let updatedPlugins = this.state.updatedPlugins;

		for (const change of changes) {
			checkedItems[change.index] = change.enabled;
			updatedPlugins = updatedPlugins.filter((p) => p.plugin_name !== change.plugin.data.name);

			if (change.enabled !== change.plugin.enabled) {
				updatedPlugins = [...updatedPlugins, { plugin_name: change.plugin.data.name, enabled: change.enabled }];
			}
		}

		this.setState({ checkedItems, updatedPlugins });
	}

	SavePluginChanges() {
		const onOK = () => {
			backend.plugins.togglePlugin(JSON.stringify(this.state.updatedPlugins));
		};

		showModal(
			<ConfirmModal strTitle={locale.optionReloadRequired} strDescription={locale.optionPluginNeedsReload} strOKButtonText={locale.optionReloadNow} onOK={onOK} />,
			pluginSelf.mainWindow,
			{ bNeverPopOut: false },
		);
	}

	async OpenPluginsFolder() {
		const path = await backend.environment.get('MILLENNIUM__PLUGINS_PATH');
		Utils.BrowseLocalFolder(path);
	}

	async InstallPluginMenu() {
		await showInstallPluginModal(this.FetchAllPlugins.bind(this));
	}

	renderPluginComponent({ plugin, index }: { plugin: PluginComponent; index: number }) {
		const logState = this.state.pluginsWithLogs?.get(plugin.data.common_name ?? '');

		return (
			<RenderPluginComponent
				plugin={plugin}
				index={index}
				isEnabled={this.state.checkedItems[index]}
				hasErrors={(logState?.errors ?? 0) > 0}
				hasWarnings={(logState?.warnings ?? 0) > 0}
				onSelectionChange={(index: number) => this.handleCheckboxChange(index)}
				refetchPlugins={this.FetchAllPlugins.bind(this)}
				allPlugins={this.state.plugins ?? []}
				isPluginConfigurable={this.state.configurablePluginStore?.find((p) => p.name === plugin.data.name)?.isEditable ?? false}
				isLegacy={isLegacyPlugin(plugin)}
				metrics={this.state.metrics.get(plugin.data.name)}
			/>
		);
	}

	render() {
		/** Haven't received the plugins yet from the backend */
		if (this.state.plugins === undefined) {
			return null;
		}

		if (!this.state.plugins || !this.state.plugins.length || (this.state.plugins.length === 1 && this.state.plugins[0].data.name === 'core')) {
			return (
				<Placeholder icon={<PiPlugsFill className="SVGIcon_Button" />} header={locale.pluginPanelNoPluginsHeader} body={locale.pluginPanelNoPluginsBody}>
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
