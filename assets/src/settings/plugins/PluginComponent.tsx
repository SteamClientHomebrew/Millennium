import { DialogButton, Field, IconsModule, Menu, MenuItem, showContextMenu, Toggle } from '@steambrew/client';
import { DesktopTooltip, Separator } from '../../components/SteamComponents';
import { PluginComponent } from '../../types';
import { FaEllipsisH } from 'react-icons/fa';
import { Utils } from '../../utils';
import { Component } from 'react';
import { PyUninstallPlugin } from '../../utils/ffi';

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
				data-icon-status={type}
			>
				<Toggle key={plugin.data.name} disabled={plugin.data.name === 'core'} value={isEnabled} onChange={onSelectionChange.bind(null, index)} />
				<DialogButton onClick={this.showCtxMenu} style={{ width: '32px' }}>
					<FaEllipsisH />
				</DialogButton>
			</Field>
		);
	}
}
