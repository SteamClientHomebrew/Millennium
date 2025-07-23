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

import { DialogButton, Field, IconsModule, Menu, MenuItem, showContextMenu, Toggle } from '@steambrew/client';
import { DesktopTooltip, Separator } from '../../components/SteamComponents';
import { PluginComponent } from '../../types';
import { FaEllipsisH } from 'react-icons/fa';
import { Utils } from '../../utils';
import { Component } from 'react';
import { PyUninstallPlugin } from '../../utils/ffi';
import { IconButton } from '../../components/IconButton';

interface PluginComponentProps {
	plugin: PluginComponent;
	index: number;
	isLastPlugin: boolean;
	isEnabled: boolean;
	hasErrors: boolean;
	hasWarnings: boolean;
	onSelectionChange: (index: number) => void;
	refetchPlugins: () => Promise<void>;
}

enum TooltipType {
	Error = 'error',
	Warning = 'warning',
	None = 'none',
}

export class RenderPluginComponent extends Component<PluginComponentProps> {
	async uninstallPlugin() {
		const { plugin, refetchPlugins } = this.props;

		const shouldUninstall = await Utils.ShowMessageBox(`Are you sure you want to uninstall ${plugin.data.common_name}?`, 'Heads up!');
		if (!shouldUninstall) return;

		const success = await PyUninstallPlugin({ pluginName: plugin.data.name });

		if (success == false) {
			Utils.ShowMessageBox(`Failed to uninstall ${plugin.data.common_name}. Check the logs tab for more details.`, 'Whoops', {
				bAlertDialog: true,
			});
		}

		await refetchPlugins();
	}

	showCtxMenu = (e: MouseEvent | GamepadEvent) => {
		const { plugin } = this.props;
		showContextMenu(
			<Menu label="MillenniumPluginContextMenu">
				<MenuItem onSelected={() => {}} bInteractableItem={false} tone="emphasis" disabled={true}>
					{plugin.data.common_name}
				</MenuItem>
				<Separator />
				<MenuItem onSelected={() => {}}>Reload</MenuItem>
				<MenuItem onSelected={this.uninstallPlugin.bind(this)}>Uninstall</MenuItem>
				<MenuItem onSelected={Utils.BrowseLocalFolder.bind(null, plugin.path)}>Browse local files</MenuItem>
			</Menu>,
			e.currentTarget ?? window,
		);
	};

	getTooltipContent() {
		const { plugin, hasErrors, hasWarnings } = this.props;

		const statusMap = [
			{ condition: hasErrors, color: 'red', message: 'encountered errors', type: TooltipType.Error },
			{ condition: hasWarnings, color: '#ffc82c', message: 'issued warnings', type: TooltipType.Warning },
		];

		const status = statusMap.find((entry) => entry.condition);
		if (!status) return { type: TooltipType.None, content: null };

		return {
			type: status.type,
			content: (
				<DesktopTooltip toolTipContent={`${plugin?.data?.common_name} ${status.message}. Please check the logs tab for more details.`} direction="top">
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
				<IconButton onClick={this.showCtxMenu}>
					<FaEllipsisH />
				</IconButton>
			</Field>
		);
	}
}
