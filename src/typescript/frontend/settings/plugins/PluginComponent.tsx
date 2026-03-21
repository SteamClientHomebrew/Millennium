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

import { Field, IconsModule, Menu, MenuItem, showContextMenu, Toggle } from '@steambrew/client';
import { Component } from 'react';
import { IconButton } from '../../components/IconButton';
import { DesktopTooltip, Separator } from '../../components/SteamComponents';
import { DesktopSideBarFocusedItemType } from '../../quick-access/DesktopMenuContext';
import { useQuickAccessStore } from '../../quick-access/quickAccessStore';
import { PluginComponent } from '../../types';
import { Utils } from '../../utils';
import { PyUninstallPlugin, PyPluginConfigGetAll, PyPluginConfigDeleteAll } from '../../utils/ffi';
import { formatString, locale } from '../../utils/localization-manager';
import { MillenniumIcons } from '../../components/Icons';
import { showPluginCrashModal } from '../../components/PluginCrashModal';


interface PluginComponentProps {
	plugin: PluginComponent;
	index: number;
	isLastPlugin: boolean;
	isEnabled: boolean;
	hasErrors: boolean;
	hasWarnings: boolean;
	onSelectionChange: (index: number) => void;
	refetchPlugins: () => Promise<void>;
	allPlugins: Array<PluginComponent>;
	isPluginConfigurable: boolean;
}

enum TooltipType {
	Error = 'error',
	Warning = 'warning',
	None = 'none',
}

export class RenderPluginComponent extends Component<PluginComponentProps> {
	constructor(props: PluginComponentProps) {
		super(props);
		this.showCtxMenu = this.showCtxMenu.bind(this);
	}

	async uninstallPlugin() {
		const { plugin, refetchPlugins } = this.props;

		const shouldUninstall = await Utils.ShowMessageBox(formatString(locale.pluginUninstallConfirm, plugin.data.common_name), locale.strHeadsUp);
		if (!shouldUninstall) return;

		const success = JSON.parse(await PyUninstallPlugin({ pluginName: plugin.data.name }));

		if (success == false) {
			Utils.ShowMessageBox(formatString(locale.pluginUninstallFailed, plugin.data.common_name), locale.errorMessageTitle, {
				bAlertDialog: true,
			});
		} else {
			try {
				const configData = JSON.parse(await PyPluginConfigGetAll({ pluginName: plugin.data.name }));
				if (configData && Object.keys(configData).length > 0) {
					const shouldDelete = await Utils.ShowMessageBox(
						formatString(locale.pluginDeleteConfigPrompt ?? 'Do you want to delete saved settings for {0}?', plugin.data.common_name),
						locale.strHeadsUp,
					);
					if (shouldDelete) {
						await PyPluginConfigDeleteAll({ pluginName: plugin.data.name });
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
		useQuickAccessStore.getState().openQuickAccess({
			data: {
				libraryItem: plugin,
				libraryItemType: DesktopSideBarFocusedItemType.PLUGIN,
			},
		});
	}

	onExtensionSettings() {
		const { plugin } = this.props;
	}

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
				<MenuItem onSelected={() => {}}>{locale.strReload}</MenuItem>
				{config()}
				<MenuItem onSelected={this.uninstallPlugin.bind(this)}>{locale.uninstall}</MenuItem>
				<MenuItem onSelected={Utils.BrowseLocalFolder.bind(null, plugin.path)}>{locale.optionBrowseLocalFiles}</MenuItem>
			</Menu>,
			e.currentTarget ?? window,
		);
	}

	getTooltipContent() {
		const { plugin, hasErrors, hasWarnings } = this.props;

		if (plugin.data.__private_browser_extension) {
			return {
				type: TooltipType.None,
				content: (
					<DesktopTooltip toolTipContent={locale.pluginChromeExtension} direction="top">
						<MillenniumIcons.ChromeExtension />
					</DesktopTooltip>
				),
			};
		}

		if (plugin.status === 'crashed' && plugin.crash) {
			return {
				type: TooltipType.Error,
				content: (
					<DesktopTooltip toolTipContent={formatString(locale.pluginCrashedTooltip, plugin.data.common_name)} direction="top">
						<div style={{ cursor: 'pointer' }} onClick={() => showPluginCrashModal(plugin.crash, this.props.refetchPlugins)}>
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
				<DesktopTooltip toolTipContent={formatString(locale[status.localeKey], plugin?.data?.common_name)} direction="top">
					<IconsModule.ExclamationPoint color={status.color} />
				</DesktopTooltip>
			),
		};
	}

	render() {
		const { plugin, index, isLastPlugin, isEnabled, onSelectionChange } = this.props;

		/** Don't render the Millennium plugin */
		if (plugin.data.name === 'core') return null;

		const { type, content } = this.getTooltipContent();

		return (
			<Field
				key={index}
				label={
					<div className="MillenniumPlugins_PluginLabel">
						{plugin.data.common_name}
						{plugin.data.version && <div className="MillenniumItem_Version">{plugin.data.version}</div>}
					</div>
				}
				description={plugin?.data?.description}
				icon={content}
				padding="standard"
				bottomSeparator={isLastPlugin ? 'none' : 'standard'}
				className="MillenniumPlugins_PluginField"
				data-plugin-name={plugin.data.name}
				data-plugin-version={plugin.data.version}
				data-plugin-common-name={plugin.data.common_name}
				data-plugin-status={type}
			>
				<Toggle key={plugin.data.name} disabled={plugin.data.name === 'core'} value={isEnabled} onChange={onSelectionChange.bind(null, index)} />
				<IconButton name="KaratDown" onClick={this.showCtxMenu} text={locale.strShowMenu} />
			</Field>
		);
	}
}
