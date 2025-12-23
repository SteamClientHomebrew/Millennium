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
import { DesktopTooltip, SettingsDialogSubHeader } from '../components/SteamComponents';
import { useEffect, useState } from 'react';
import { locale } from '../utils/localization-manager';

interface RenderPluginViewsProps {
	plugins: PluginComponent[];
	pluginName: string;
	pluginView: any;
	editableStore: Array<{ name: string; isEditable: boolean }>;
}

export const NonConfigurablePluginItem = ({ plugin }: { plugin: PluginComponent }) => {
	const reason = !plugin.enabled ? locale.pluginIsNotEnabled : locale.pluginIsNotConfigurable;

	return (
		<DesktopTooltip toolTipContent={reason} direction="left">
			<PanelSectionRow key={plugin.data.name}>
				<DialogButton className="MillenniumButton MillenniumDesktopSidebar_LibraryItemButton" disabled={true}>
					<div>{plugin?.data?.common_name}</div>
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
		return <NonConfigurablePluginItem plugin={plugin} />;
	}

	return (
		<PanelSectionRow key={pluginName}>
			<DialogButton
				className="MillenniumButton MillenniumDesktopSidebar_LibraryItemButton"
				onClick={setFocusedItem.spread(plugin, DesktopSideBarFocusedItemType.PLUGIN)}
			>
				{pluginView?.icon && pluginView?.icon}
				<div>{plugin?.data?.common_name}</div>
			</DialogButton>
		</PanelSectionRow>
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
		getPluginConfigurableStatus().then(setConfigurablePluginStore);
	}, []);

	if (!plugins || !plugins?.length) {
		return null; /** let the parent component handle no plugin/themes */
	}

	return (
		<PanelSection>
			<SettingsDialogSubHeader>Plugin Settings</SettingsDialogSubHeader>
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
