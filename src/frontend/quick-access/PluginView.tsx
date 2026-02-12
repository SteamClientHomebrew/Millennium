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

import { DialogButton, ErrorBoundary, PanelSection, PanelSectionRow } from '@steambrew/client';
import { PluginComponent } from '../types';
import { DesktopSideBarFocusedItemType, useDesktopMenu } from './DesktopMenuContext';
import { getPluginConfigurableStatus, getPluginRenderers, getPluginView } from '../utils/globals';
import { DesktopTooltip } from '../components/SteamComponents';
import { useEffect, useState } from 'react';
import { locale } from '../utils/localization-manager';

interface RenderPluginViewsProps {
	plugins: PluginComponent[];
	pluginName: string;
	pluginView: any;
	editableStore: Array<{ name: string; isEditable: boolean }>;
}

enum PluginDisabledReason {
	NONE,
	NOT_CONFIGURABLE,
	NOT_ENABLED,
}

interface PluginItemProps {
	disabledReason: PluginDisabledReason;
	onClick?: () => void;
	plugin: PluginComponent;
	pluginView?: any;
}

const PluginItem = ({ disabledReason, onClick, plugin, pluginView }: PluginItemProps) => {
	const disabled = disabledReason !== PluginDisabledReason.NONE;
	const tooltipText = (() => {
		switch (disabledReason) {
			case PluginDisabledReason.NOT_CONFIGURABLE:
				return locale.pluginIsNotConfigurable;

			case PluginDisabledReason.NOT_ENABLED:
				return locale.pluginIsNotEnabled;

			default:
				return '';
		}
	})();

	return (
		<DesktopTooltip toolTipContent={tooltipText} direction="left" bDisabled={!disabled}>
			<PanelSectionRow key={plugin.data.name}>
				<DialogButton
					className="MillenniumButton MillenniumDesktopSidebar_LibraryItemButton"
					disabled={disabled}
					onClick={onClick}
					data-disabled-reason={disabled ? PluginDisabledReason[disabledReason] : undefined}
				>
					{pluginView?.icon && pluginView?.icon}
					{plugin?.data?.common_name}
				</DialogButton>
			</PanelSectionRow>
		</DesktopTooltip>
	);
};

export const RenderPluginViews = ({ plugins, pluginName, pluginView, editableStore }: RenderPluginViewsProps) => {
	const { setFocusedItem } = useDesktopMenu();
	const plugin = plugins?.find((p) => p.data.name === pluginName);
	const isEditable = editableStore?.find?.((p) => p.name === pluginName)?.isEditable || false;

	if (!plugin) {
		console.error('Failed to find plugin by name when rendering plugin view in sidebar.');
		return null;
	}

	if (!isEditable) {
		const disabledReason = !plugin.enabled ? PluginDisabledReason.NOT_ENABLED : PluginDisabledReason.NOT_CONFIGURABLE;
		return <PluginItem disabledReason={disabledReason} plugin={plugin} />;
	}

	return (
		<PluginItem
			disabledReason={PluginDisabledReason.NONE}
			onClick={setFocusedItem.spread(plugin, DesktopSideBarFocusedItemType.PLUGIN)}
			plugin={plugin}
			pluginView={pluginView}
		/>
	);
};

export const RenderPluginView = () => {
	const { focusedItem } = useDesktopMenu();
	const renderer = getPluginView(focusedItem?.data?.name)?.content;

	if (!renderer) {
		return (
			<PanelSection>
				<PanelSectionRow>
					Failed to find a renderer for <b>{focusedItem?.data?.name}</b>. Please check if the plugin is loaded correctly.
				</PanelSectionRow>
			</PanelSection>
		);
	}

	return (
		<ErrorBoundary>
			<div className="MillenniumDesktopSidebar_EditorContent">{renderer}</div>
		</ErrorBoundary>
	);
};

export const PluginSelectorView = () => {
	const { plugins } = useDesktopMenu();
	const configurablePluginRenderers = getPluginRenderers();
	const [configurablePluginStore, setConfigurablePluginStore] = useState<Array<{ name: string; isEditable: boolean }>>();

	useEffect(() => {
		getPluginConfigurableStatus(plugins).then(setConfigurablePluginStore);
	}, []);

	if (!plugins || !plugins?.length) {
		return null; /** let the parent component handle no plugin/themes */
	}

	return (
		<PanelSection title="Plugin Settings">
			{plugins?.map((plugin) => (
				<RenderPluginViews
					plugins={plugins}
					pluginName={plugin.data.name}
					pluginView={configurablePluginRenderers[plugin.data.name]}
					editableStore={configurablePluginStore}
				/>
			))}
		</PanelSection>
	);
};
