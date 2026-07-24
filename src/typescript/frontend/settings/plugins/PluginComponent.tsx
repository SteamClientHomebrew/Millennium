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

import { Field, IconsModule, Menu, MenuItem, showContextMenu, Toggle } from '@steambrew/sdk';
import React, { Component } from 'react';
import { IconButton } from '../../components/IconButton';
import { DesktopTooltip, Separator } from '../../components/SteamComponents';
import { useQuickAccessStore } from '../../quick-access/quickAccessStore';
import { PluginComponent, PluginMetrics } from '../../types';
import { Utils } from '../../utils';
import { backend } from '../../utils/ffi';
import { formatString, locale } from '../../utils/localization-manager';
import { showPluginCrashModal } from '../../components/PluginCrashModal';
import { FiCpu } from 'react-icons/fi';
import { FaMemory } from 'react-icons/fa';

interface PluginComponentProps {
	plugin: PluginComponent;
	index: number;
	isEnabled: boolean;
	hasErrors: boolean;
	hasWarnings: boolean;
	onSelectionChange: (index: number) => void;
	refetchPlugins: () => Promise<void>;
	allPlugins: Array<PluginComponent>;
	isPluginConfigurable: boolean;
	isLegacy: boolean;
	metrics?: PluginMetrics;
}

enum TooltipType {
	Error = 'error',
	Warning = 'warning',
	None = 'none',
}

interface RenderPluginComponentState {
	flashKey: number;
}

export class RenderPluginComponent extends Component<PluginComponentProps, RenderPluginComponentState> {
	state: RenderPluginComponentState = { flashKey: 0 };

	constructor(props: PluginComponentProps) {
		super(props);
		this.showCtxMenu = this.showCtxMenu.bind(this);
	}

	componentDidUpdate(prevProps: PluginComponentProps) {
		const prev = prevProps.metrics;
		const curr = this.props.metrics;
		if (curr && prev !== curr) {
			this.setState((s) => ({ flashKey: s.flashKey + 1 }));
		}
	}

	async uninstallPlugin() {
		const { plugin, refetchPlugins } = this.props;

		const shouldUninstall = await Utils.ShowMessageBox(formatString(locale.pluginUninstallConfirm, plugin.data.common_name ?? ''), locale.strHeadsUp);
		if (!shouldUninstall) return;

		const success = await backend.plugins.uninstall(plugin.data.name);

		if (success == false) {
			Utils.ShowMessageBox(formatString(locale.pluginUninstallFailed, plugin.data.common_name ?? ''), locale.errorMessageTitle, {
				bAlertDialog: true,
			});
		} else {
			try {
				const configData = await backend.config.plugins.getAll(plugin.data.name);
				if (configData && Object.keys(configData).length > 0) {
					const shouldDelete = await Utils.ShowMessageBox(
						formatString(locale.pluginDeleteConfigPrompt ?? 'Do you want to delete saved settings for {0}?', plugin.data.common_name ?? ''),
						locale.strHeadsUp,
					);
					if (shouldDelete) {
						await backend.config.plugins.removeAll(plugin.data.name);
					}
				}
			} catch {
				/* no config data or error — skip silently */
			}
		}

		await refetchPlugins();
	}

	openPluginSettings() {
		const { plugin } = this.props;
		useQuickAccessStore.getState().openQuickAccess({ plugin: plugin?.data?.name });
	}

	onExtensionSettings() {}

	showCtxMenu(e: React.MouseEvent<HTMLButtonElement>) {
		const { plugin, isPluginConfigurable } = this.props;
		const reason = !plugin.enabled ? locale.pluginIsNotEnabled : locale.pluginIsNotConfigurable;

		const millenniumPluginConfigMenuItem = () => (
			<DesktopTooltip toolTipContent={reason} direction="left">
				<MenuItem onSelected={this.openPluginSettings.bind(this)} disabled={!isPluginConfigurable}>
					{locale.strConfigure}
				</MenuItem>
			</DesktopTooltip>
		);

		const chromeExtensionConfigMenuItem = () => (
			<DesktopTooltip toolTipContent={reason} direction="left">
				<MenuItem onSelected={this.openPluginSettings.bind(this)}>{locale.strConfigure}</MenuItem>
			</DesktopTooltip>
		);

		const config = plugin?.isChromeExtension ? chromeExtensionConfigMenuItem : millenniumPluginConfigMenuItem;

		showContextMenu(
			<Menu label="MillenniumPluginContextMenu">
				<MenuItem onSelected={() => {}} bInteractableItem={false} tone="emphasis" disabled={true}>
					{plugin.data.common_name}
				</MenuItem>
				<Separator />
				<MenuItem onSelected={() => backend.plugins.reload(plugin.data.name)} disabled={!plugin.enabled || this.props.isLegacy}>
					{locale.strReload}
				</MenuItem>
				{config()}
				<MenuItem onSelected={this.uninstallPlugin.bind(this)}>{locale.uninstall}</MenuItem>
				<MenuItem onSelected={Utils.BrowseLocalFolder.bind(null, plugin.path)}>{locale.optionBrowseLocalFiles}</MenuItem>
			</Menu>,
			e.currentTarget ?? window,
		);
	}

	getTooltipContent() {
		const { plugin, hasErrors, hasWarnings, isLegacy } = this.props;

		if (isLegacy) {
			return {
				type: TooltipType.Error,
				content: (
					<DesktopTooltip toolTipContent={locale.legacyPluginDisabledTooltip} direction="top">
						<IconsModule.ExclamationPoint color="red" />
					</DesktopTooltip>
				),
			};
		}

		if (plugin.status === 'crashed' && plugin.crash) {
			const crash = plugin.crash;
			return {
				type: TooltipType.Error,
				content: (
					<DesktopTooltip toolTipContent={formatString(locale.pluginCrashedTooltip, plugin.data.common_name ?? '')} direction="top">
						<div style={{ cursor: 'pointer' }} onClick={() => showPluginCrashModal(crash, this.props.refetchPlugins)}>
							<IconsModule.ExclamationPoint color="red" />
						</div>
					</DesktopTooltip>
				),
			};
		}

		const statusMap = [
			{ condition: hasErrors, color: '#ffc82c', localeKey: 'pluginEncounteredErrors' as keyof typeof locale, type: TooltipType.Error },
			{ condition: hasWarnings, color: '#ffc82c', localeKey: 'pluginIssuedWarnings' as keyof typeof locale, type: TooltipType.Warning },
		];

		const status = statusMap.find((entry) => entry.condition);
		if (!status) return { type: TooltipType.None, content: null };

		return {
			type: status.type,
			content: (
				<DesktopTooltip toolTipContent={formatString(locale[status.localeKey] ?? '', plugin?.data?.common_name ?? '')} direction="top">
					<IconsModule.ExclamationPoint color={status.color} />
				</DesktopTooltip>
			),
		};
	}

	render() {
		const { plugin, index, isEnabled, onSelectionChange, metrics } = this.props;
		const { flashKey } = this.state;

		/** Don't render the Millennium plugin */
		if (plugin.data.name === 'core') return null;

		const { type, content } = this.getTooltipContent();

		const formatBytes = (bytes: number) => (bytes >= 1024 * 1024 ? `${(bytes / (1024 * 1024)).toFixed(1)} MB` : `${(bytes / 1024).toFixed(1)} KB`);

		const showMetrics = isEnabled && metrics && metrics.rss_bytes > 0;

		return (
			<Field
				key={index}
				label={
					<div className="MillenniumPlugins_PluginLabel">
						{plugin.data.common_name}
						{plugin.data.version && (
							<div className="MillenniumItem_Version">
								<span>v{plugin.data.version}</span>
							</div>
						)}
						{showMetrics && (
							<div className="MillenniumPlugins_Metrics" key={flashKey}>
								<span>
									<FaMemory />
									{formatBytes(metrics.heap_bytes)}
								</span>
								<span>
									<FiCpu />
									{metrics.cpu_percent.toFixed(1)}% CPU
								</span>
							</div>
						)}
					</div>
				}
				description={plugin?.data?.description}
				icon={content}
				padding="standard"
				className="MillenniumPlugins_PluginField"
				data-plugin-name={plugin.data.name}
				data-plugin-version={plugin.data.version}
				data-plugin-common-name={plugin.data.common_name}
				data-plugin-status={type}
			>
				<Toggle
					key={plugin.data.name}
					disabled={plugin.data.name === 'core' || this.props.isLegacy}
					value={this.props.isLegacy ? false : isEnabled}
					onChange={() => onSelectionChange(index)}
				/>
				<IconButton name="KaratDown" onClick={(ev) => this.showCtxMenu(ev as React.MouseEvent<HTMLButtonElement>)} tooltip={locale.strShowMenu} />
			</Field>
		);
	}
}
